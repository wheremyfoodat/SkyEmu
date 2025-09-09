extern "C" {
#include "lua_manager.h"
}

#include <string>

#ifdef SE_ENABLE_LUA
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

class LuaManager {
	lua_State* L = nullptr;
	bool initialized = false;
	bool haveScript = false;

	void signalEventInternal(LuaEvent e);

  public:
	LuaManager() {}

	void close();
	void initialize();
	void initializeThunks();
	void loadFile(const char* path);
	void loadString(const std::string& code);

	void reset();
	void signalEvent(LuaEvent e) {
		if (haveScript) [[unlikely]] {
			signalEventInternal(e);
		}
	}
};

#else  // Lua not enabled, Lua manager does nothing
class LuaManager {
  public:
	LuaManager() {}

	void close() {}
	void initialize() {}
	void loadFile(const char* path) {}
	void loadString(const std::string& code) {}
	void reset() {}
	void signalEvent(LuaEvent e) {}
};
#endif

LuaManager luaManager;

#ifdef SE_ENABLE_LUA
void LoadImguiBindings(lua_State* lState);

void LuaManager::initialize() {
	L = luaL_newstate();  // Open Lua

	if (!L) {
		printf("Lua initialization failed, continuing without Lua");
		initialized = false;
		return;
	}
	luaL_openlibs(L);

	initializeThunks();
	LoadImguiBindings(L);

	initialized = true;
	haveScript = false;
}

void LuaManager::close() {
	if (initialized) {
		lua_close(L);
		initialized = false;
		haveScript = false;
		L = nullptr;
	}
}

void LuaManager::loadFile(const char* path) {
	// Initialize Lua if it has not been initialized
	if (!initialized) {
		initialize();
	}

	// If init failed, don't execute
	if (!initialized) {
		printf("Lua initialization failed, file won't run\n");
		haveScript = false;

		return;
	}

	int status = luaL_loadfile(L, path);  // load Lua script
	int ret = lua_pcall(L, 0, 0, 0);      // tell Lua to run the script

	if (ret != 0) {
		haveScript = false;
		// tell us what mistake we made
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
	} else {
		haveScript = true;
	}
}

void LuaManager::loadString(const std::string& code) {
	// Initialize Lua if it has not been initialized
	if (!initialized) {
		initialize();
	}

	// If init failed, don't execute
	if (!initialized) {
		printf("Lua initialization failed, file won't run\n");
		haveScript = false;

		return;
	}

	if (luaL_loadstring(L, code.c_str())) {
		fprintf(stderr, "luaL_loadstring failed: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		haveScript = false;
		return;
	}

	int ret = lua_pcall(L, 0, 0, 0);  // tell Lua to run the script

	if (ret != 0) {
		haveScript = false;
		// tell us what mistake we made
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
	} else {
		haveScript = true;
	}
}

void LuaManager::signalEventInternal(LuaEvent e) {
	lua_getglobal(L, "eventHandler");         // We want to call the event handler
	lua_pushinteger(L, static_cast<int>(e));  // Push event type

	// Call the function with 1 argument and 0 outputs, without an error handler
	lua_pcall(L, 1, 0, 0);
}

void LuaManager::reset() {
	// Reset scripts
	haveScript = false;
}

// Initialize C++ thunks for Lua code to call here
// All code beyond this point is terrible and full of global state, don't judge

/*
#define MAKE_MEMORY_FUNCTIONS(size)                                            \
  static int read##size##Thunk(lua_state* L) {                                 \
	const u32 vaddr = (u32)lua_tointeger(L, 1);                                \
	lua_pushinteger(L, LuaManager::g_emulator->getMemory().read##size(vaddr)); \
	return 1;                                                                  \
  }                                                                            \
  static int write##size##Thunk(lua_state* L) {                                \
	const u32 vaddr = (u32)lua_tointeger(L, 1);                                \
	const u##size value = (u##size)lua_tointeger(L, 2);                        \
	LuaManager::g_emulator->getMemory().write##size(vaddr, value);             \
	return 0;                                                                  \
  }

MAKE_MEMORY_FUNCTIONS(8)
MAKE_MEMORY_FUNCTIONS(16)
MAKE_MEMORY_FUNCTIONS(32)
MAKE_MEMORY_FUNCTIONS(64)
#undef MAKE_MEMORY_FUNCTIONS

static int readFloatThunk(lua_state* L) {
  const u32 vaddr = (u32)lua_tointeger(L, 1);
  lua_pushnumber(L, (lua_Number)Helpers::bit_cast<float, u32>(
						LuaManager::g_emulator->getMemory().read32(vaddr)));
  return 1;
}

static int writeFloatThunk(lua_state* L) {
  const u32 vaddr = (u32)lua_tointeger(L, 1);
  const float value = (float)lua_tonumber(L, 2);
  LuaManager::g_emulator->getMemory().write32(
	  vaddr, Helpers::bit_cast<u32, float>(value));
  return 0;
}

static int readDoubleThunk(lua_state* L) {
  const u32 vaddr = (u32)lua_tointeger(L, 1);
  lua_pushnumber(L, (lua_Number)Helpers::bit_cast<double, u64>(
						LuaManager::g_emulator->getMemory().read64(vaddr)));
  return 1;
}

static int writeDoubleThunk(lua_state* L) {
  const u32 vaddr = (u32)lua_tointeger(L, 1);
  const double value = (double)lua_tonumber(L, 2);
  LuaManager::g_emulator->getMemory().write64(
	  vaddr, Helpers::bit_cast<u64, double>(value));
  return 0;
}

static int getAppIDThunk(lua_state* L) {
  std::optional<u64> id = LuaManager::g_emulator->getMemory().getProgramID();

  // If the app has an ID, return true + its ID
  // Otherwise return false and 0 as the ID
  if (id.has_value()) {
	lua_pushboolean(L, 1);             // Return true
	lua_pushnumber(L, u32(*id));       // Return bottom 32 bits
	lua_pushnumber(L, u32(*id >> 32)); // Return top 32 bits
  } else {
	lua_pushboolean(L, 0); // Return false
	// Return no ID
	lua_pushnumber(L, 0);
	lua_pushnumber(L, 0);
  }

  return 3;
}

static int pauseThunk(lua_state* L) {
  LuaManager::g_emulator->pause();
  return 0;
}

static int resumeThunk(lua_state* L) {
  LuaManager::g_emulator->resume();
  return 0;
}

static int resetThunk(lua_state* L) {
  LuaManager::g_emulator->reset(Emulator::ReloadOption::Reload);
  return 0;
}

static int loadROMThunk(lua_state* L) {
  // Path argument is invalid, report that loading failed and exit
  if (lua_type(L, 1) != LUA_TSTRING) {
	lua_pushboolean(L, 0);
	lua_error(L);
	return 1;
  }

  usize pathLength;
  const char *const str = lua_tolstring(L, 1, &pathLength);

  const auto path = std::filesystem::path(std::string(str, pathLength));
  // Load ROM and reply if it succeeded or not
  lua_pushboolean(L, LuaManager::g_emulator->loadROM(path) ? 1 : 0);
  return 1;
}

static int getButtonsThunk(lua_state* L) {
  auto buttons =
	  LuaManager::g_emulator->getServiceManager().getHID().getOldButtons();
  lua_pushinteger(L, static_cast<lua_Integer>(buttons));

  return 1;
}

static int getCirclepadThunk(lua_state* L) {
  auto &hid = LuaManager::g_emulator->getServiceManager().getHID();
  s16 x = hid.getCirclepadX();
  s16 y = hid.getCirclepadY();

  lua_pushinteger(L, static_cast<lua_Number>(x));
  lua_pushinteger(L, static_cast<lua_Number>(y));
  return 2;
}

static int getButtonThunk(lua_state* L) {
  auto &hid = LuaManager::g_emulator->getServiceManager().getHID();
  // This function accepts a mask. You can use it to check if one or more
  // buttons are pressed at a time
  const u32 mask = (u32)lua_tonumber(L, 1);
  const bool result = (hid.getOldButtons() & mask) == mask;

  // Return whether the selected buttons are all pressed
  lua_pushboolean(L, result ? 1 : 0);
  return 1;
}
*/

// clang-format off
static constexpr luaL_Reg functions[] = {
    /*
	{ "__read8", read8Thunk },
	{ "__read16", read16Thunk },
	{ "__read32", read32Thunk },
	{ "__read64", read64Thunk },
	{ "__readFloat", readFloatThunk },
	{ "__readDouble", readDoubleThunk },
	{ "__write8", write8Thunk} ,
	{ "__write16", write16Thunk },
	{ "__write32", write32Thunk },
	{ "__write64", write64Thunk },
	{ "__writeFloat", writeFloatThunk },
	{ "__writeDouble", writeDoubleThunk },
	{ "__getAppID", getAppIDThunk },
	{ "__pause", pauseThunk }, 
	{ "__resume", resumeThunk },
	{ "__reset", resetThunk },
	{ "__loadROM", loadROMThunk },
	{ "__getButtons", getButtonsThunk },
	{ "__getCirclepad", getCirclepadThunk },
	{ "__getButton", getButtonThunk },
	{ "__disassembleARM", disassembleARMThunk },
	{ "__disassembleTeak", disassembleTeakThunk },
	{"__addServiceIntercept", addServiceInterceptThunk },
	{"__clearServiceIntercepts", clearServiceInterceptsThunk },
    */
    { nullptr, nullptr },
};
// clang-format on

void LuaManager::initializeThunks() {
	static const char* runtimeInit = R"(
)";

	auto addIntConstant = [&](int x, const char* name) {
		lua_pushinteger(L, x);
		lua_setglobal(L, name);
	};

	// luaL_register(L, "GLOBALS", functions);
	//  Add values for event enum
	addIntConstant(LuaEvent::Frame, "__Frame");

	/*
	// Add enums for 3DS keys
	addIntConstant(HID::Keys::A, "__ButtonA");
	addIntConstant(HID::Keys::B, "__ButtonB");
	addIntConstant(HID::Keys::X, "__ButtonX");
	addIntConstant(HID::Keys::Y, "__ButtonY");
	addIntConstant(HID::Keys::Up, "__ButtonUp");
	addIntConstant(HID::Keys::Down, "__ButtonDown");
	addIntConstant(HID::Keys::Left, "__ButtonLeft");
	addIntConstant(HID::Keys::Right, "__ButtonRight");
	addIntConstant(HID::Keys::L, "__ButtonL");
	addIntConstant(HID::Keys::R, "__ButtonR");
	addIntConstant(HID::Keys::ZL, "__ButtonZL");
	addIntConstant(HID::Keys::ZR, "__ButtonZR");
	*/

	// Call our Lua runtime initialization before any Lua script runs
	if (luaL_loadstring(L, runtimeInit)) {
		fprintf(stderr, "luaL_loadstring failed: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1);
		haveScript = false;
		return;
	}

	int ret = lua_pcall(L, 0, 0, 0);  // tell Lua to run the script

	if (ret != 0) {
		initialized = false;
		fprintf(stderr, "%s\n", lua_tostring(L, -1));  // Init should never fail!
	} else {
		initialized = true;
	}
}
#endif

void se_lua_init() {
	const char* lua_script = R"(
    function draw_ui()
        local shown = imgui.Begin("FE6 hax")

        if shown then
            if imgui.SmallButton("Game Over") then
                print("Button clicked")
            end
        end

        imgui.End()
    end

    draw_ui()
  )";

	luaManager.loadString(lua_script);
}
