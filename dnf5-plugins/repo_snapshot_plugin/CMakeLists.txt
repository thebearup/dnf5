add_library(repo_snapshot_cmd_plugin MODULE repo_snapshot_cmd_plugin.cpp)

# disable the 'lib' prefix in order to create repo_snapshot_cmd_plugin.so
set_target_properties(repo_snapshot_cmd_plugin PROPERTIES PREFIX "")

target_link_libraries(repo_snapshot_cmd_plugin PRIVATE libdnf5-cli)
target_link_libraries(repo_snapshot_cmd_plugin PRIVATE dnf5)

install(TARGETS repo_snapshot_cmd_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_FULL_LIBDIR}/dnf5/plugins/)
