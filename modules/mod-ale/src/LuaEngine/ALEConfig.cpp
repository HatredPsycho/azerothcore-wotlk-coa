#include "ALEConfig.h"

ALEConfig& ALEConfig::GetInstance()
{
    static ALEConfig instance;
    return instance;
}

ALEConfig::ALEConfig() : ConfigValueCache<ALEConfigValues>(ALEConfigValues::CONFIG_VALUE_COUNT)
{
}

void ALEConfig::Initialize(bool reload)
{
    ConfigValueCache<ALEConfigValues>::Initialize(reload);
}

void ALEConfig::BuildConfigCache()
{
    SetConfigValue<bool>(ALEConfigValues::ENABLED,                    "ALE.Enabled",            "false");
    SetConfigValue<bool>(ALEConfigValues::TRACEBACK_ENABLED,          "ALE.TraceBack",          "false");
    SetConfigValue<bool>(ALEConfigValues::AUTORELOAD_ENABLED,         "ALE.AutoReload",         "false");
    SetConfigValue<bool>(ALEConfigValues::BYTECODE_CACHE_ENABLED,     "ALE.BytecodeCache",      "false");
    // CoA: sandbox (on by default, every Allow* off by default)
    SetConfigValue<bool>(ALEConfigValues::SANDBOX_ENABLED,            "ALE.Sandbox.Enable",        "true");
    SetConfigValue<bool>(ALEConfigValues::SANDBOX_ALLOW_HTTP,         "ALE.Sandbox.AllowHttp",     "false");
    SetConfigValue<bool>(ALEConfigValues::SANDBOX_ALLOW_SQL,          "ALE.Sandbox.AllowSql",      "false");
    SetConfigValue<bool>(ALEConfigValues::SANDBOX_ALLOW_COMMANDS,     "ALE.Sandbox.AllowCommands", "false");

    SetConfigValue<std::string>(ALEConfigValues::SCRIPT_PATH,         "ALE.ScriptPath",         "lua_scripts");
    SetConfigValue<std::string>(ALEConfigValues::REQUIRE_PATH,        "ALE.RequirePaths",       "");
    SetConfigValue<std::string>(ALEConfigValues::REQUIRE_CPATH,       "ALE.RequireCPaths",      "");

    SetConfigValue<uint32>(ALEConfigValues::AUTORELOAD_INTERVAL,      "ALE.AutoReloadInterval", 1);
}
