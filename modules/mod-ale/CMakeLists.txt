# CoA: mod-ale is built only with -DCOA_WITH_ALE=ON (see conf/dist/config.cmake).
# CoA: Lua 5.2 only, linked statically, built from a local tarball
# (deps/lua/lua-5.2.4.tar.gz). No variant may download anything.
set(LUA_VERSION "lua52" CACHE STRING "Lua version to use (CoA: lua52 only)")
set_property(CACHE LUA_VERSION PROPERTY STRINGS lua52)
if (NOT LUA_VERSION STREQUAL "lua52")
  MESSAGE(FATAL_ERROR "CoA: LUA_VERSION='${LUA_VERSION}' is not supported. "
                      "Only lua52 (local tarball, static) is allowed; luajit, lua51, lua53 and lua54 are disabled.")
endif()
MESSAGE(STATUS "Lua version: ${LUA_VERSION}")

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# CoA: static linking only
set(LUA_STATIC ON CACHE BOOL "link lua statically (CoA: always ON)" FORCE)
MESSAGE(STATUS "Lua linking: static")

add_subdirectory(src/lualib/lua)
