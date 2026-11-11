# BSD-3-Clause
# Copyright (c) 2022, Rochet2 <rochet2@post.com>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# * Neither the name of the copyright holder nor the names of its
#   contributors may be used to endorse or promote products derived from
#   this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

project ( lua C )

# CoA: only lua52 is allowed, from a local tarball. No URL on the Internet,
# so no configure step can download anything. The tarball is placed by the
# Builder; its SHA256 is checked by FetchContent.
if (NOT LUA_VERSION STREQUAL "lua52")
  message(FATAL_ERROR "CoA: LUA_VERSION='${LUA_VERSION}' is not supported, only lua52.")
endif()

set(COA_LUA52_TARBALL "${CMAKE_SOURCE_DIR}/deps/lua/lua-5.2.4.tar.gz")
if (NOT EXISTS "${COA_LUA52_TARBALL}")
  message(FATAL_ERROR "CoA: COA_WITH_ALE=ON needs the Lua 5.2.4 source tarball at\n"
                      "  ${COA_LUA52_TARBALL}\n"
                      "(SHA256 b9e2e4aad6789b3b63a056d442f7b39f0ecfca3ae0f1fc0ae4e9614401b69f4b).\n"
                      "Place it there, or configure with -DCOA_WITH_ALE=OFF.")
endif()

include(FetchContent)
FetchContent_Declare(
  lua52
  URL      ${COA_LUA52_TARBALL}
  URL_HASH SHA256=b9e2e4aad6789b3b63a056d442f7b39f0ecfca3ae0f1fc0ae4e9614401b69f4b
)
FetchContent_MakeAvailable(lua52)

# Easen warnings - disable Werror for Lua sources
string(REGEX REPLACE "( |^)/W[0-9]( |$)" "\\1/W2\\2" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "( |^)/W[0-9]( |$)" "\\1/W2\\2" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
# Remove -Werror from flags for Lua compilation
string(REGEX REPLACE "-Werror" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-Werror" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
# Also remove any warning-as-error flags
string(REGEX REPLACE "-Wno-error=[^ ]*" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "-Wno-error=[^ ]*" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

set(LUA_SOURCE_FOLDER "${${LUA_VERSION}_SOURCE_DIR}/src")

file(GLOB LOCAL_SOURCES_H ${LUA_SOURCE_FOLDER}/*.h)
file(GLOB LOCAL_SOURCES_C ${LUA_SOURCE_FOLDER}/*.c)
# Compile lua as C++ so it uses exceptions instead of longjmp
# Disabled for now as some libraries expect lua to be C
# set_source_files_properties(${LOCAL_SOURCES_H} ${LOCAL_SOURCES_C} PROPERTIES LANGUAGE CXX )
list(REMOVE_ITEM LOCAL_SOURCES_C ${LUA_SOURCE_FOLDER}/lua.c)
list(REMOVE_ITEM LOCAL_SOURCES_C ${LUA_SOURCE_FOLDER}/luac.c)

# CoA: static library only
add_library(lualib STATIC ${LOCAL_SOURCES_H} ${LOCAL_SOURCES_C})
set_property(TARGET lualib PROPERTY POSITION_INDEPENDENT_CODE ON)

set_target_properties(lualib PROPERTIES LINKER_LANGUAGE C)
target_include_directories(lualib PUBLIC "${LUA_SOURCE_FOLDER}" "${CMAKE_CURRENT_SOURCE_DIR}")

if (WIN32)
  set_target_properties(lualib PROPERTIES OUTPUT_NAME ${LUA_VERSION})
  install(TARGETS lualib DESTINATION "${CMAKE_INSTALL_PREFIX}")
  if (NOT LUA_STATIC)
    install(FILES $<TARGET_PDB_FILE:lualib> DESTINATION "${CMAKE_INSTALL_PREFIX}" OPTIONAL)
  endif()
else()
  set_target_properties(lualib PROPERTIES PUBLIC_HEADER "${LOCAL_SOURCES_H};${CMAKE_CURRENT_SOURCE_DIR}/lua.hpp")
  install(TARGETS lualib
          DESTINATION "${CMAKE_INSTALL_PREFIX}/lib"
          PUBLIC_HEADER DESTINATION "${CMAKE_INSTALL_PREFIX}/include"
  )
endif()

if (WIN32)
  target_compile_definitions(lualib PRIVATE _CRT_SECURE_NO_WARNINGS)
  if (NOT LUA_STATIC)
    target_compile_definitions(lualib PRIVATE LUA_BUILD_AS_DLL)
  endif()
elseif (APPLE)
  target_compile_definitions(lualib PUBLIC LUA_USE_MACOSX)
  target_compile_options(lualib PRIVATE -Wno-deprecated-declarations -Wno-empty-body -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-string-plus-int)
  target_link_libraries(lualib readline)
elseif (UNIX)
  target_compile_definitions(lualib PUBLIC LUA_USE_LINUX)
  target_compile_options(lualib PRIVATE -Wno-empty-body -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -Wno-sign-compare -Wno-string-plus-int)
  target_link_libraries(lualib ${CMAKE_DL_LIBS} m readline)
  set_target_properties(lualib PROPERTIES OUTPUT_NAME ${LUA_VERSION})
endif()

# CoA: the standalone lua interpreter and luac compiler are not built
# (no unsandboxed Lua executable is shipped next to the server).
