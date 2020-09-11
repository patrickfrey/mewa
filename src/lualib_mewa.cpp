/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "export.hpp"
#include "automaton.hpp"
#include "lexer.hpp"
#include "scope.hpp"
#include "error.hpp"
#include "version.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <utility>
#include <map>
#include <lua.h>
#include <lauxlib.h>

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

extern "C" int luaopen_mewa( lua_State *ls);

struct mewa_compiler_userdata_t
{
	mewa::Automaton automaton;

	~mewa_compiler_userdata_t(){}
};
#define MEWA_COMPILER_METATABLE_NAME 	"mewa.compiler"
#define MEWA_TYPESYSTEM_METATABLE_NAME 	"mewa.typesystem"


#ifdef __GNUC__
static std::string stringf( const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
static std::string stringf( const char* fmt, ...);
#endif
static std::string stringf( const char* fmt, ...)
{
	std::string rt;
	va_list ap;
	va_start( ap, fmt);
	char msgbuf[ 4096];
	int len = ::vsnprintf( msgbuf, sizeof(msgbuf), fmt, ap);
	if (len > (int)sizeof( msgbuf)) len = sizeof( msgbuf);
	va_end( ap);
	return std::string( msgbuf, len);
}

template <typename KEY, typename VAL>
static std::map<KEY,VAL> parsePackedTable( lua_State *ls, int li, const char* tableName)
{
	std::map<KEY,VAL> rt;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		int packedkey = lua_tointeger( ls, -2);
		if (packedkey <= 0)
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName, rowcnt));
		}
		int packedval = lua_tointeger( ls, -1);
		if (packedval <= 0)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s', row %d", tableName, rowcnt));
		}
		if (rt.insert( {KEY::unpack(packedkey), VAL::unpack(packedval)}).second == false/*element key already exists*/)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s', row %d", tableName, rowcnt));
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return rt;
}

static std::vector<std::string> parseStringArray( lua_State *ls, int li, const std::string& tableName)
{
	std::vector<std::string> rt;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (rowcnt != lua_tointeger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		std::size_t vallen;
		const char* valstr = lua_tolstring( ls, -1, &vallen);
		if (!valstr)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		rt.push_back( std::string( valstr, vallen));
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return rt;
}

static int string2int( const std::string& value)
{
	int rt = 0;
	char const* si = value.c_str();
	for (; *si >= '0' && *si <= '9'; ++si)
	{
		rt = rt * 10 + (*si - '0');
	}
	if (*si) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, value);
	return rt;
}

static mewa::Lexer parseLexerDefinitions( lua_State *ls, int li, const char* tableName)
{
	mewa::Lexer rt;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (!lua_isstring( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName, rowcnt));
		}
		const char* keystr = lua_tostring( ls, -2);
		if (!lua_istable( ls, -1))
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
		}
		auto arg = parseStringArray( ls, -1, stringf( "%s/%s", tableName, keystr));
		if (0==std::strcmp( keystr, "comment"))
		{
			if (arg.size() > 2) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
			if (arg.size() == 1)
			{
				rt.defineEolnComment( arg[ 0]);
			}
			else
			{
				rt.defineBracketComment( arg[0], arg[1]);
			}
		}
		else if (0==std::strcmp( keystr, "keyword"))
		{
			if (arg.size() > 1) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
			rt.defineLexem( arg[0]);
		}
		else if (0==std::strcmp( keystr, "token"))
		{
			if (arg.size() > 3) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
			if (arg.size() > 2)
			{
				rt.defineLexem( arg[0], arg[1], string2int( arg[2]));
			}
			else
			{
				rt.defineLexem( arg[0], arg[1]);
			}
		}
		else if (0==std::strcmp( keystr, "bad"))
		{
			if (arg.size() > 1) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
			rt.defineBadLexem( arg[0]);
		}
		else if (0==std::strcmp( keystr, "ignore"))
		{
			if (arg.size() > 1) throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "table '%s/%s', row %d", tableName, keystr, rowcnt));
			rt.defineIgnore( arg[0]);
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return rt;
}

static mewa::Automaton::Call parseCall( lua_State *ls, int li, const std::string& tableName)
{
	std::string function;
	std::string arg;
	mewa::Automaton::Call::ArgumentType argtype = mewa::Automaton::Call::NoArg;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (rowcnt != lua_tointeger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		std::size_t vallen;
		const char* valstr;
		const char* argstr;
		
		switch (rowcnt)
		{
			case 1: 
				if (!lua_isstring( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
				}
				valstr = lua_tolstring( ls, -1, &vallen); function.append( valstr, vallen);
				argstr = std::strchr( valstr, ' ');
				if (argstr)
				{
					function.append( valstr, argstr-valstr);
					arg.append( argstr+1);
				}
				else
				{
					function.append( valstr);
				}
				break;
			case 2: 
				if (lua_type( ls, -1) != LUA_TFUNCTION)
				{
					throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
				}
				break;
			case 3:
				argtype = lua_isstring( ls, -1) ? mewa::Automaton::Call::StringArg : mewa::Automaton::Call::ReferenceArg;
				break;
		}
		lua_pop( ls, 1);
	}
	if (rowcnt < 2)
	{
		throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s'", tableName.c_str()));
	}
	lua_pop( ls, 1);
	return mewa::Automaton::Call( function, arg, argtype);
}

static std::vector<mewa::Automaton::Call> parseCallList( lua_State *ls, int li, const std::string& tableName)
{
	std::vector<mewa::Automaton::Call> rt;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (rowcnt != lua_tointeger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		if (lua_istable( ls, -1))
		{
			rt.push_back( parseCall( ls, -1, tableName + "[]"));
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return rt;
}

static mewa::Automaton parseAutomaton( lua_State *ls, int li)
{
	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	int rowcnt = 0;

	std::string language;
	std::string typesystem;
	mewa::Lexer lexer;
	std::map<mewa::Automaton::ActionKey,mewa::Automaton::Action> actions;
	std::map<mewa::Automaton::GotoKey,mewa::Automaton::Goto> gotos;
	std::vector<mewa::Automaton::Call> calls;

	while (lua_next( ls, -2))
	{
		try
		{
			++rowcnt;
			if (!lua_isstring( ls, -2))
			{
				throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
			}
			const char* keystr = lua_tostring( ls, -2);
			if (0==std::strcmp( keystr, "language"))
			{
				if (!lua_isstring( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				language = lua_tostring( ls, -1);
			}
			else if (0==std::strcmp( keystr, "typesystem"))
			{
				if (!lua_isstring( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				typesystem = lua_tostring( ls, -1);
			}
			else if (0==std::strcmp( keystr, "lexer"))
			{
				if (!lua_istable( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				lexer = parseLexerDefinitions( ls, -1, keystr);
			}
			else if (0==std::strcmp( keystr, "action"))
			{
				if (!lua_istable( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				actions = parsePackedTable<mewa::Automaton::ActionKey,mewa::Automaton::Action>( ls, -1, keystr);
			}
			else if (0==std::strcmp( keystr, "gto"))
			{
				if (!lua_istable( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				gotos = parsePackedTable<mewa::Automaton::GotoKey,mewa::Automaton::Goto>( ls, -1, keystr);
			}
			else if (0==std::strcmp( keystr, "call"))
			{
				if (!lua_istable( ls, -1))
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, stringf( "automaton definition, row %d", rowcnt));
				}
				calls = parseCallList( ls, -1, keystr);
			}
		}
		catch (const mewa::Error& err)
		{
			throw mewa::Error( err.code(), stringf( "%s, row %d", err.arg().c_str(), rowcnt));
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return mewa::Automaton( language, typesystem, lexer, actions, gotos, calls);
}

#define LUA_MAP_EXCEPTION( CALL) \
	try\
	{\
		CALL;\
	}\
	catch (const std::runtime_error& err)\
	{\
		lua_pushstring( ls, err.what());\
		lua_error( ls);\
	}\
	catch (const std::bad_alloc& )\
	{\
		lua_pushstring( ls, "out of memory");\
		lua_error( ls);\
	}\
	catch ( ... )\
	{\
		lua_pushstring( ls, "unknown error");\
		lua_error( ls);\
	}


static int mewa_new_compiler( lua_State *ls)
{
	luaL_checktype( ls, 1, LUA_TTABLE);
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	LUA_MAP_EXCEPTION( new (&mw->automaton) mewa::Automaton( parseAutomaton( ls, 1)))
	luaL_getmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_compiler( lua_State *ls)
{
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	mw->automaton.~Automaton();
	return 0;
}

static int mewa_compiler_tostring( lua_State *ls)
{
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	std::string result;
	LUA_MAP_EXCEPTION( result = mw->automaton.tostring())
	lua_pushlstring( ls, result.c_str(), result.size());
	return 1;
}

static const struct luaL_Reg mewa_compiler_methods[] = {
	{ "__gc",	mewa_destroy_compiler },
	{ "__tostring",	mewa_compiler_tostring },
	{ NULL,		NULL },
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",	mewa_new_compiler },
	{ NULL,  	NULL }
};

DLL_PUBLIC int luaopen_mewa( lua_State *ls)
{
	/* Create the metatable and put it on the stack. */
	luaL_newmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_pushvalue( ls, -1);

	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, mewa_compiler_methods, 0);

	luaL_newlib( ls, mewa_functions);
	return 1;
}



