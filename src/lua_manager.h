#ifndef LUA_MANAGER_H
#define LUA_MANAGER_H 1

// The kinds of events that can cause a Lua call.
// Frame: Call program on frame end
// TODO: Add more
enum LuaEvent {
	Frame,
};

void se_lua_init();
#endif