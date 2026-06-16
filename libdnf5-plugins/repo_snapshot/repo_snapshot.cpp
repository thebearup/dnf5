// Copyright Contributors to the DNF5 project
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <libdnf5/base/base.hpp>
#include <libdnf5/plugin/iplugin.hpp>
#include <libdnf5/repo/repo_query.hpp>
#include <libdnf5/rpm/package_query.hpp>
#include <libdnf5/rpm/package_sack.hpp>
#include <libdnf5/rpm/package_set.hpp>

#include <cerrno>
#include <cstring>
#include <set>
#include <string>

using namespace libdnf5;

namespace {

constexpr const char * PLUGIN_NAME{"repo_snapshot"};
constexpr plugin::Version PLUGIN_VERSION{.major = 1, .minor = 0, .micro = 0};
constexpr PluginAPIVersion REQUIRED_PLUGIN_API_VERSION{.major = 2, .minor = 0};

constexpr const char * attrs[]{"author.name", "author.email", "description", nullptr};
constexpr const char * attrs_value[]{
    "Microsoft",
    "azlinux@microsoft.com",
    "Client-side repository snapshot: filters packages by publish time."};


class RepoSnapshotPlugin : public plugin::IPlugin {
public:
    RepoSnapshotPlugin(libdnf5::plugin::IPluginData & data, libdnf5::ConfigParser &)
        : IPlugin(data) {}
    ~RepoSnapshotPlugin() override = default;

    PluginAPIVersion get_api_version() const noexcept override { return REQUIRED_PLUGIN_API_VERSION; }
    const char * get_name() const noexcept override { return PLUGIN_NAME; }
    plugin::Version get_version() const noexcept override { return PLUGIN_VERSION; }
    const char * const * get_attributes() const noexcept override { return attrs; }

    const char * get_attribute(const char * attribute) const noexcept override {
        for (size_t i = 0; attrs[i]; ++i) {
            if (std::strcmp(attribute, attrs[i]) == 0) {
                return attrs_value[i];
            }
        }
        return nullptr;
    }

    void init() override;
    void repos_loaded() override;

private:
    time_t snapshot_time{0};
    std::set<std::string> exclude_repos;
};


void RepoSnapshotPlugin::init() {
    // Read snapshot_time from environment variable (set by the dnf5 CLI plugin
    // when --snapshot-time is passed on the command line).
    std::string time_str;

    const char * env = std::getenv("DNF5_SNAPSHOT_TIME");
    if (env) {
        time_str = env;
    }

    if (time_str.empty()) {
        return;
    }

    // Parse the POSIX timestamp.
    errno = 0;
    char * end = nullptr;
    snapshot_time = static_cast<time_t>(std::strtoll(time_str.c_str(), &end, 10));
    if (errno || end == time_str.c_str() || snapshot_time <= 0) {
        auto & logger = *get_base().get_logger();
        logger.warning("repo_snapshot plugin: invalid snapshot_time '{}', disabling snapshot", time_str);
        snapshot_time = 0;
        return;
    }

    // Read excluded repos from environment variable (set by the dnf5 CLI plugin
    // when --snapshot-exclude-repos is passed on the command line).
    std::string exclude_str;
    const char * exclude_env = std::getenv("DNF5_SNAPSHOT_EXCLUDE_REPOS");
    if (exclude_env) {
        exclude_str = exclude_env;
    }

    if (!exclude_str.empty()) {
        // Parse comma-separated repo IDs.
        std::string::size_type start = 0;
        while (start < exclude_str.size()) {
            auto pos = exclude_str.find(',', start);
            if (pos == std::string::npos) {
                pos = exclude_str.size();
            }
            auto token = exclude_str.substr(start, pos - start);
            // Trim whitespace.
            auto first = token.find_first_not_of(" \t");
            if (first != std::string::npos) {
                auto last = token.find_last_not_of(" \t");
                exclude_repos.insert(token.substr(first, last - first + 1));
            }
            start = pos + 1;
        }
    }

    auto & logger = *get_base().get_logger();
    logger.info("repo_snapshot plugin: snapshot_time={}", snapshot_time);
    if (!exclude_repos.empty()) {
        std::string repos_list;
        for (const auto & r : exclude_repos) {
            if (!repos_list.empty()) repos_list += ", ";
            repos_list += r;
        }
        logger.info("repo_snapshot plugin: excluding repos: {}", repos_list);
    }
}


void RepoSnapshotPlugin::repos_loaded() {
    if (snapshot_time <= 0) {
        return;
    }

    auto & base = get_base();
    auto & logger = *base.get_logger();

    // Build a PackageQuery over all available packages (including excluded ones
    // so we don't miss packages that other plugins may have excluded).
    rpm::PackageQuery all_packages(base, rpm::PackageQuery::ExcludeFlags::IGNORE_EXCLUDES);

    // Collect packages whose file time (publish time) exceeds the snapshot time.
    // Falls back to build_time if file_time is not available (libsolv not patched).
    rpm::PackageSet to_exclude(base);

    for (const auto & pkg : all_packages) {
        // Skip system (installed) repo.
        if (pkg.is_installed()) {
            continue;
        }

        // Skip repos in the exclusion list.
        if (exclude_repos.count(pkg.get_repo_id())) {
            continue;
        }

        // Prefer file_time (repo publish time) over build_time.
        auto file_time = static_cast<time_t>(pkg.get_file_time());
        time_t effective_time;
        if (file_time > 0) {
            effective_time = file_time;
        } else {
            effective_time = static_cast<time_t>(pkg.get_build_time());
        }

        if (effective_time > 0 && effective_time > snapshot_time) {
            to_exclude.add(pkg);
        }
    }

    if (to_exclude.empty()) {
        logger.info("repo_snapshot plugin: no packages excluded by snapshot filter");
        return;
    }

    // Use add_user_excludes to hide the filtered packages from dependency resolution.
    base.get_rpm_package_sack()->add_user_excludes(to_exclude);

    logger.info("repo_snapshot plugin: excluded {} packages published after snapshot time {}", to_exclude.size(), snapshot_time);
}


std::exception_ptr last_exception;

}  // namespace


PluginAPIVersion libdnf_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

const char * libdnf_plugin_get_name(void) {
    return PLUGIN_NAME;
}

plugin::Version libdnf_plugin_get_version(void) {
    return PLUGIN_VERSION;
}

plugin::IPlugin * libdnf_plugin_new_instance(
    [[maybe_unused]] LibraryVersion library_version,
    libdnf5::plugin::IPluginData & data,
    libdnf5::ConfigParser & parser) try {
    return new RepoSnapshotPlugin(data, parser);
} catch (...) {
    last_exception = std::current_exception();
    return nullptr;
}

void libdnf_plugin_delete_instance(plugin::IPlugin * plugin_object) {
    delete plugin_object;
}

std::exception_ptr * libdnf_plugin_get_last_exception(void) {
    return &last_exception;
}
