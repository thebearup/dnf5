if(NOT WITH_PLUGIN_REPO_SNAPSHOT)
    return()
endif()

add_library(repo_snapshot MODULE repo_snapshot.cpp)

# disable the 'lib' prefix in order to create repo_snapshot.so
set_target_properties(repo_snapshot PROPERTIES PREFIX "")

target_link_libraries(repo_snapshot PRIVATE libdnf5)

install(TARGETS repo_snapshot LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/libdnf5/plugins/")
install(FILES "repo_snapshot.conf" DESTINATION "${CMAKE_INSTALL_FULL_SYSCONFDIR}/dnf/libdnf5-plugins")
