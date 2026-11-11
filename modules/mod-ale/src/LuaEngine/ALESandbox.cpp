/*
 * CoA: sandbox for the ALE Lua state (written for Conquest of Azeroth).
 * This file is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 3 as published by the Free
 * Software Foundation.
 */

#include "ALESandbox.h"
#include "ALEConfig.h"

extern "C"
{
#include "lualib.h"
#include "lauxlib.h"
};

#include <cstring>
#include <initializer_list>

namespace
{
    bool StartsWith(char const* text, char const* prefix)
    {
        return std::strncmp(text, prefix, std::strlen(prefix)) == 0;
    }

    bool IsSqlMethod(char const* name)
    {
        return StartsWith(name, "WorldDB") || StartsWith(name, "CharDB") || StartsWith(name, "AuthDB");
    }

    int DeniedMethod(lua_State* L)
    {
        return luaL_error(L, "%s", lua_tostring(L, lua_upvalueindex(1)));
    }

    // load(ld [, source [, mode [, env]]]) with mode forced to "t": Lua code
    // cannot load precompiled bytecode.
    int TextOnlyLoad(lua_State* L)
    {
        int const args = lua_gettop(L);
        lua_pushvalue(L, lua_upvalueindex(1));
        lua_pushvalue(L, 1);
        if (args >= 2)
            lua_pushvalue(L, 2);
        else
            lua_pushnil(L);
        lua_pushliteral(L, "t");
        if (args >= 4)
        {
            lua_pushvalue(L, 4);
            lua_call(L, 4, LUA_MULTRET);
        }
        else
            lua_call(L, 3, LUA_MULTRET);
        return lua_gettop(L) - args;
    }

    void RemoveGlobal(lua_State* L, char const* name)
    {
        lua_pushnil(L);
        lua_setglobal(L, name);
    }

    void WrapLoad(lua_State* L, char const* name)
    {
        lua_getglobal(L, name);
        if (lua_isfunction(L, -1))
        {
            lua_pushcclosure(L, TextOnlyLoad, 1);
            lua_setglobal(L, name);
        }
        else
            lua_pop(L, 1);
    }

    // Keeps only the preload and Lua-file searchers (entries 1 and 2 in Lua 5.2);
    // the C and C-root searchers (DLL/.so loading) are dropped.
    void KeepLuaSearchers(lua_State* L, int package, char const* field)
    {
        lua_getfield(L, package, field);
        if (!lua_istable(L, -1))
        {
            lua_pop(L, 1);
            return;
        }
        int const original = lua_gettop(L);
        lua_createtable(L, 2, 0);
        for (int i = 1; i <= 2; ++i)
        {
            lua_rawgeti(L, original, i);
            lua_rawseti(L, -2, i);
        }
        lua_setfield(L, package, field);
        lua_pop(L, 1);
    }
}

bool ALESandbox::IsEnabled()
{
    return ALEConfig::GetInstance().IsSandboxEnabled();
}

char const* ALESandbox::GetRequiredOption(char const* methodName)
{
    if (!methodName || !IsEnabled())
        return nullptr;

    ALEConfig const& config = ALEConfig::GetInstance();

    if (std::strcmp(methodName, "HttpRequest") == 0)
        return config.IsSandboxHttpAllowed() ? nullptr : "ALE.Sandbox.AllowHttp";

    if (IsSqlMethod(methodName))
        return config.IsSandboxSqlAllowed() ? nullptr : "ALE.Sandbox.AllowSql";

    if (std::strcmp(methodName, "RunCommand") == 0)
        return config.IsSandboxCommandsAllowed() ? nullptr : "ALE.Sandbox.AllowCommands";

    return nullptr;
}

void ALESandbox::PushDeniedMethod(lua_State* L, char const* methodName, char const* option)
{
    lua_pushfstring(L, "ALE sandbox: %s is disabled (set %s = 1 in mod_ale.conf to allow it)", methodName, option);
    lua_pushcclosure(L, DeniedMethod, 1);
}

void ALESandbox::Apply(lua_State* L)
{
    if (!IsEnabled())
        return;

    int const top = lua_gettop(L);

    RemoveGlobal(L, "io");
    RemoveGlobal(L, "os");
    RemoveGlobal(L, "debug");
    RemoveGlobal(L, "dofile");
    RemoveGlobal(L, "loadfile");

    WrapLoad(L, "load");
    WrapLoad(L, "loadstring");

    lua_getglobal(L, "package");
    if (lua_istable(L, -1))
    {
        int const package = lua_gettop(L);

        lua_pushnil(L);
        lua_setfield(L, package, "loadlib");
        lua_pushnil(L);
        lua_setfield(L, package, "cpath");

        KeepLuaSearchers(L, package, "searchers");
        KeepLuaSearchers(L, package, "loaders");

        // require("io") and friends would otherwise return the cached library tables
        lua_getfield(L, package, "loaded");
        if (lua_istable(L, -1))
        {
            for (char const* name : { "io", "os", "debug" })
            {
                lua_pushnil(L);
                lua_setfield(L, -2, name);
            }
        }
    }

    lua_settop(L, top);
}
