#ifndef LUA_MANAGER_H
#define LUA_MANAGER_H 1

// The kinds of events that can cause a Lua call.
// Frame: Call program on frame end
// TODO: Add more
typedef enum {
	LUA_EVENT_FRAME,
} lua_event_t;

struct lua_State;

void se_lua_load_string(const char* script);
void se_lua_event(lua_event_t event);

// Lua function thunks
int se_lua_read32_thunk(struct lua_State* L);

#endif