#include <lua.h>
#include <lauxlib.h>

#include <soup/ffi.hpp>
#include <soup/joaat.hpp>
#include <soup/SharedLibrary.hpp>

[[nodiscard]] static uintptr_t checkregister(lua_State* L, int i)
{
	if (lua_type(L, i) == LUA_TSTRING)
	{
		return reinterpret_cast<uintptr_t>(luaL_checkstring(L, i));
	}
	if (lua_type(L, i) == LUA_TUSERDATA)
	{
		return (uintptr_t)lua_touserdata(L, i);
	}
	if (lua_type(L, i) == LUA_TBOOLEAN)
	{
		return lua_istrue(L, i) ? 1 : 0;
	}
	return (uintptr_t)luaL_checkinteger(L, i);
}

static uintptr_t ffi_call(lua_State* L, void* addr, int i, int num_args)
{
	std::vector<uintptr_t> args{};
	for (; i != num_args; ++i)
	{
		args.emplace_back(checkregister(L, i));
	}
	return soup::ffi::fastcall(addr, args);
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
