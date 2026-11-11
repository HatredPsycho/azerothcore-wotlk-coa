/*
 * CoA: sandbox for the ALE Lua state (written for Conquest of Azeroth).
 * This file is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 3 as published by the Free
 * Software Foundation.
 */

#ifndef ALE_SANDBOX_H
#define ALE_SANDBOX_H

extern "C"
{
#include "lua.h"
};

namespace ALESandbox
{
    // CoA: true when ALE.Sandbox.Enable = 1 (default).
    bool IsEnabled();

    // CoA: name of the ALE.Sandbox.Allow* option that must be 1 for the Lua method
    // `methodName` to run, or nullptr when the method is not restricted.
    char const* GetRequiredOption(char const* methodName);

    // CoA: pushes a C closure that raises a Lua error explaining that `methodName`
    // is disabled by the sandbox and which option enables it.
    void PushDeniedMethod(lua_State* L, char const* methodName, char const* option);

    // CoA: removes io, os, debug, dofile, loadfile, package.loadlib, package.cpath,
    // the C searchers of package.searchers/loaders, and forces load() to text chunks.
    // Call after luaL_openlibs and after package.path/cpath have been set.
    void Apply(lua_State* L);
}

#endif // ALE_SANDBOX_H
