// Copyright Contributors to the DNF5 project
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <dnf5/iplugin.hpp>
#include <libdnf5-cli/argument_parser.hpp>

#include <cstdlib>
#include <cstring>

using namespace dnf5;

namespace {

constexpr const char * PLUGIN_NAME{"repo_snapshot"};
constexpr PluginVersion PLUGIN_VERSION{.major = 1, .minor = 0, .micro = 0};
constexpr PluginAPIVersion REQUIRED_PLUGIN_API_VERSION{.major = 2, .minor = 0};

constexpr const char * attrs[]{"author.name", "author.email", "description", nullptr};
constexpr const char * attrs_value[]{
    "Microsoft",
    "azlinux@microsoft.com",
    "Registers --snapshot-time and --snapshot-exclude-repos global CLI options."};


class RepoSnapshotCmdPlugin : public IPlugin {
public:
    using IPlugin::IPlugin;

    PluginAPIVersion get_api_version() const noexcept override { return REQUIRED_PLUGIN_API_VERSION; }
    const char * get_name() const noexcept override { return PLUGIN_NAME; }
    PluginVersion get_version() const noexcept override { return PLUGIN_VERSION; }
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
    std::vector<std::unique_ptr<Command>> create_commands() override;
    void finish() noexcept override {}
};


void RepoSnapshotCmdPlugin::init() {
    auto & ctx = get_context();
    auto & parser = ctx.get_argument_parser();
    auto & root_cmd = *parser.get_root_command();

    // Get the global options group (registered by main.cpp).
    auto & global_options_group = root_cmd.get_group("global_options");

    // Register --snapshot-time global option.
    auto snapshot_time = parser.add_new_named_arg("snapshot-time");
    snapshot_time->set_long_name("snapshot-time");
    snapshot_time->set_has_value(true);
    snapshot_time->set_arg_value_help("POSIX_TIME");
    snapshot_time->set_description(
        "POSIX time to be used for client-side repository snapshot");
    snapshot_time->set_parse_hook_func(
        [](libdnf5::cli::ArgumentParser::NamedArg *,
           [[maybe_unused]] const char * option,
           const char * value) {
            // Bridge the CLI value to an environment variable for the libdnf5 plugin.
#ifdef _WIN32
            _putenv_s("DNF5_SNAPSHOT_TIME", value);
#else
            setenv("DNF5_SNAPSHOT_TIME", value, 1);
#endif
            return true;
        });
    global_options_group.register_argument(snapshot_time);

    // Register --snapshot-exclude-repos global option.
    auto snapshot_exclude = parser.add_new_named_arg("snapshot-exclude-repos");
    snapshot_exclude->set_long_name("snapshot-exclude-repos");
    snapshot_exclude->set_has_value(true);
    snapshot_exclude->set_arg_value_help("REPO_ID,...");
    snapshot_exclude->set_description(
        "Repos to exclude from client-side repository snapshot");
    snapshot_exclude->set_parse_hook_func(
        [](libdnf5::cli::ArgumentParser::NamedArg *,
           [[maybe_unused]] const char * option,
           const char * value) {
            // Bridge the CLI value to an environment variable for the libdnf5 plugin.
#ifdef _WIN32
            _putenv_s("DNF5_SNAPSHOT_EXCLUDE_REPOS", value);
#else
            setenv("DNF5_SNAPSHOT_EXCLUDE_REPOS", value, 1);
#endif
            return true;
        });
    global_options_group.register_argument(snapshot_exclude);
}


std::vector<std::unique_ptr<Command>> RepoSnapshotCmdPlugin::create_commands() {
    // No new commands — this plugin only adds global options.
    return {};
}


std::exception_ptr last_exception;

}  // namespace


PluginAPIVersion dnf5_plugin_get_api_version(void) {
    return REQUIRED_PLUGIN_API_VERSION;
}

const char * dnf5_plugin_get_name(void) {
    return PLUGIN_NAME;
}

PluginVersion dnf5_plugin_get_version(void) {
    return PLUGIN_VERSION;
}

IPlugin * dnf5_plugin_new_instance(
    [[maybe_unused]] ApplicationVersion application_version, Context & context) try {
    return new RepoSnapshotCmdPlugin(context);
} catch (...) {
    last_exception = std::current_exception();
    return nullptr;
}

void dnf5_plugin_delete_instance(IPlugin * plugin_object) {
    delete plugin_object;
}

std::exception_ptr * dnf5_plugin_get_last_exception(void) {
    return &last_exception;
}
