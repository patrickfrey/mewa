/*
 * Some functions that ensure compatibility with Lua 5.1, 5.2
*/
#include <cmath>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if LUA_VERSION_NUM <= 502
static bool lua_isinteger( lua_State *ls, int li)
{
	int tp = lua_type( ls, li);
	if (tp != LUA_TNUMBER) return false;
	double val = lua_tonumber( ls, li);
	double fl = val - floor( val);
	return (fl < 10*std::numeric_limits<double>::epsilon());
}
#endif
