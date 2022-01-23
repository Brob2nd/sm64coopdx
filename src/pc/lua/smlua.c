#include "smlua.h"
#include "pc/mod_list.h"

lua_State* gLuaState = NULL;

static void smlua_exec_file(char* path) {
    lua_State* L = gLuaState;
    if (luaL_dofile(L, path) != LUA_OK) {
        LOG_LUA("LUA: Failed to load lua file '%s'.", path);
        puts(lua_tostring(L, lua_gettop(L)));
    }
    lua_pop(L, lua_gettop(L));
}

static void smlua_exec_str(char* str) {
    lua_State* L = gLuaState;
    if (luaL_dostring(L, str) != LUA_OK) {
        LOG_LUA("LUA: Failed to load lua string.");
        puts(lua_tostring(L, lua_gettop(L)));
    }
    lua_pop(L, lua_gettop(L));
}

static void smlua_load_script(char* path) {
    lua_State* L = gLuaState;
    if (luaL_loadfile(L, path) != LUA_OK) {
        LOG_LUA("LUA: Failed to load lua script '%s'.", path);
        puts(lua_tostring(L, lua_gettop(L)));
        return;
    }

    lua_newtable(L); // create _ENV tables
    lua_newtable(L); // create metatable
    lua_getglobal(L, "_G"); // get global table

    // set global as the metatable
    lua_setfield(L, -2, "__index");
    lua_setmetatable(L, -2);

    // push to registry with path as name (must be unique)
    lua_setfield(L, LUA_REGISTRYINDEX, path);
    lua_getfield(L, LUA_REGISTRYINDEX, path);
    lua_setupvalue(L, 1, 1); // set upvalue (_ENV)

    // run chunks
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        LOG_LUA("LUA: Failed to execute lua script '%s'.", path);
        puts(lua_tostring(L, lua_gettop(L)));
        smlua_dump_stack();
        return;
    }
}

static void smlua_init_mario_states(void) {
    lua_State* L = gLuaState;
    lua_newtable(L);
    int t = lua_gettop(gLuaState);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        lua_pushinteger(L, i);
        smlua_push_object(L, LOT_MARIO_STATE, &gMarioStates[i]);
        lua_settable(L, t);
    }
    lua_setglobal(L, "gMarioStates");
}

void smlua_init(void) {
    smlua_shutdown();
    gLuaState = luaL_newstate();
    lua_State* L = gLuaState;

    // load libraries
    luaopen_base(L);
    //luaopen_coroutine(L);
    //luaopen_debug(L);
    //luaopen_io(L);
    luaL_requiref(L, "math", luaopen_math, 1);
    //luaopen_os(L);
    //luaopen_package(L);
    luaL_requiref(L, "string", luaopen_string, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    //luaopen_utf8(L);

    smlua_bind_hooks();
    smlua_bind_cobject();
    smlua_bind_functions();
    smlua_bind_functions_autogen();

    extern char gSmluaConstants[];
    smlua_exec_str(gSmluaConstants);

    smlua_init_mario_states();

    // load scripts
    LOG_INFO("Loading scripts:");
    struct ModTable* table = (gNetworkType == NT_SERVER) ? &gModTableLocal : &gModTableRemote;
    for (int i = 0; i < table->entryCount; i++) {
        struct ModListEntry* entry = &table->entries[i];
        if (!entry->enabled) { continue; }
        LOG_INFO("    %s", entry->path);
        smlua_load_script(entry->path);
    }
}

void smlua_update(void) {
    lua_State* L = gLuaState;
    if (L == NULL) { return; }
    smlua_call_event_hooks(HOOK_UPDATE);
}

void smlua_shutdown(void) {
    lua_State* L = gLuaState;
    if (L != NULL) {
        lua_close(L);
        gLuaState = NULL;
    }
}
