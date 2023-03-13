#include <lua.h>
#include <lauxlib.h>

#include <soup/ffi.hpp>
#include <soup/joaat.hpp>
#include <soup/SharedLibrary.hpp>

struct FfiCallArgs
{
	inline static FfiCallArgs* instance = nullptr;

	lua_State* const L;
	std::vector<uintptr_t> args{};
	bool has_func = false;

	FfiCallArgs(lua_State* L)
		: L(L)
	{
		instance = this;
	}

	~FfiCallArgs()
	{
		instance = nullptr;
		if (has_func)
		{
			lua_pushstring(L, "luaffi_func");
			lua_pushnil(L);
			lua_settable(L, LUA_REGISTRYINDEX);
		}
	}

	static void funcInvoker()
	{
		if (!instance)
		{
			throw std::exception("Function invoked too late.");
		}
		lua_pushstring(instance->L, "luaffi_func");
		lua_gettable(instance->L, LUA_REGISTRYINDEX);
		lua_call(instance->L, 0, 0);
	}

	void push(int i)
	{
		if (lua_type(L, i) == LUA_TSTRING)
		{
			args.emplace_back(reinterpret_cast<uintptr_t>(luaL_checkstring(L, i)));
		}
		else if (lua_type(L, i) == LUA_TUSERDATA)
		{
			args.emplace_back((uintptr_t)lua_touserdata(L, i));
		}
		else if (lua_type(L, i) == LUA_TBOOLEAN)
		{
			args.emplace_back(lua_istrue(L, i) ? 1 : 0);
		}
		else if (lua_type(L, i) == LUA_TFUNCTION)
		{
			if (has_func)
			{
				luaL_error(L, "Can only push a single function per ffi call");
			}
			has_func = true;
			lua_pushstring(L, "luaffi_func");
			lua_pushvalue(L, i);
			lua_settable(L, LUA_REGISTRYINDEX);
			args.emplace_back((uintptr_t)&funcInvoker);
		}
		else
		{
			args.emplace_back((uintptr_t)luaL_checkinteger(L, i));
		}
	}
};

static uintptr_t ffi_call(lua_State* L, void* addr, int i, int num_args)
{
	if (addr == nullptr)
	{
		luaL_error(L, "Attempt to call a nullptr");
	}
	FfiCallArgs args{ L };
	for (; i != num_args; ++i)
	{
		args.push(i);
	}
	return soup::ffi::fastcall(addr, args.args);
}

static uintptr_t ffi_call(lua_State* L)
{
	const int num_args = (lua_gettop(L) + 1);
	return ffi_call(L, (void*)luaL_checkinteger(L, 1), 2, num_args);
}

static uintptr_t SharedLibrary_call(lua_State* L)
{
	const int num_args = (lua_gettop(L) + 1);
	auto lib = (soup::SharedLibrary*)lua_touserdata(L, 1);
	return ffi_call(L, lib->getAddress(luaL_checkstring(L, 2)), 3, num_args);
}

static int luaffi_open(lua_State* L)
{
	auto path = luaL_checkstring(L, 1);
	auto lib = std::construct_at((soup::SharedLibrary*)lua_newuserdata(L, sizeof(soup::SharedLibrary)), path);
	if (!lib->isLoaded())
	{
		luaL_error(L, "Failed to load lib");
	}

	lua_newtable(L);

	lua_pushstring(L, "__gc");
	lua_pushcfunction(L, [](lua_State* L) -> int
	{
		std::destroy_at((soup::SharedLibrary*)lua_touserdata(L, 1));
		return 0;
	});
	lua_settable(L, -3);

	lua_pushstring(L, "__index");
	lua_pushcfunction(L, [](lua_State* L) -> int
	{
		switch (soup::joaat::hash(luaL_checkstring(L, 2)))
		{
		case soup::joaat::hash("getAddress"):
			lua_pushcfunction(L, [](lua_State* L) -> int
			{
				auto lib = (soup::SharedLibrary*)lua_touserdata(L, 1);
				lua_pushinteger(L, (lua_Integer)lib->getAddress(luaL_checkstring(L, 2)));
				return 1;
			});
			return 1;

		case soup::joaat::hash("call"):
			lua_pushcfunction(L, [](lua_State* L) -> int
			{
				lua_pushinteger(L, SharedLibrary_call(L));
				return 1;
			});
			return 1;

		case soup::joaat::hash("callString"):
			lua_pushcfunction(L, [](lua_State* L) -> int
			{
				lua_pushstring(L, (const char*)SharedLibrary_call(L));
				return 1;
			});
			return 1;
		}
		return 0;
	});
	lua_settable(L, -3);

	lua_setmetatable(L, -2);

	return 1;
}

static int luaffi_call(lua_State* L)
{
	lua_pushinteger(L, ffi_call(L));
	return 1;
}

static int luaffi_call_string(lua_State* L)
{
	lua_pushstring(L, (const char*)ffi_call(L));
	return 1;
}

__declspec(dllexport) extern "C" int luaopen_luaffi(lua_State* L)
{
	lua_newtable(L);

	lua_pushstring(L, "open");
	lua_pushcfunction(L, &luaffi_open);
	lua_settable(L, -3);

	lua_pushstring(L, "call");
	lua_pushcfunction(L, &luaffi_call);
	lua_settable(L, -3);

	lua_pushstring(L, "call_string");
	lua_pushcfunction(L, &luaffi_call_string);
	lua_settable(L, -3);

	return 1;
}
