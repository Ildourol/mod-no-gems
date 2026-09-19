AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src")

# Automatically deploy mod-no-gems Lua script to the server lua_scripts directory during build
if(WIN32)
    add_custom_command(TARGET modules POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$(Configuration)/lua_scripts/mod-no-gems"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_LIST_DIR}/lua_scripts/mod-no-gems" "${CMAKE_BINARY_DIR}/bin/$(Configuration)/lua_scripts/mod-no-gems"
    )
endif()
