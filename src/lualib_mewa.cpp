/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "export.hpp"
#include "lua_load_automaton.hpp"
#include "lua_run_compiler.hpp"
#include "automaton.hpp"
#include "lexer.hpp"
#include "scope.hpp"
#include "error.hpp"
#include "version.hpp"
#include "fileio.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <typeinfo>
#include <lua.h>
#include <lauxlib.h>

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

extern "C" int luaopen_mewa( lua_State* ls);

#define MEWA_COMPILER_METATABLE_NAME 	"mewa.compiler"
#define MEWA_TYPESYSTEM_METATABLE_NAME 	"mewa.typesystem"
#define MEWA_CALLTABLE_FMT	 	"mewa.calls.%d"

struct CallTableName
{
	char buf[ 64];

	void init()
	{
		static int callTableIdx = 0;
		(void)std::snprintf( buf, sizeof(buf), MEWA_CALLTABLE_FMT, ++callTableIdx);
	}
};

struct mewa_compiler_userdata_t
{
	mewa::Automaton automaton;
	CallTableName callTableName;

	~mewa_compiler_userdata_t(){}
};

#define CATCH_EXCEPTION \
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

static int mewa_new_compiler( lua_State* ls)
{

	luaL_checktype( ls, 1, LUA_TTABLE);
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	mw->callTableName.init();
	lua_pushstring( ls, "call");
	lua_gettable( ls, 1);
	lua_setglobal( ls, mw->callTableName.buf);

	try
	{
		new (&mw->automaton) mewa::Automaton( mewa::luaLoadAutomaton( ls, 1));
	}
	CATCH_EXCEPTION
	luaL_getmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_compiler( lua_State* ls)
{
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	mw->automaton.~Automaton();
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	std::string result;
	try
	{
		result = mw->automaton.tostring();
	}
	CATCH_EXCEPTION

	lua_pushlstring( ls, result.c_str(), result.size());
	//... [PF:BUG] This call may fail with ENOMEM and cause an lua_error longjump, leaving a memory leak (result). Could not figure out a way to catch this.
	return 1;
}

static int mewa_compiler_run( lua_State* ls)
{
	std::string source;
	try
	{
		mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t *)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
		if (!lua_isstring( ls, 2)) throw mewa::Error( mewa::Error::ExpectedFilenameAsArgument);
		const char* filename = lua_tostring( ls, 1);
		source = mewa::readFile( filename);
		mewa::luaRunCompiler( ls, mw->automaton, source);
	}
	CATCH_EXCEPTION
	return 0;
}

static const struct luaL_Reg mewa_compiler_methods[] = {
	{ "__gc",	mewa_destroy_compiler },
	{ "__tostring",	mewa_compiler_tostring },
	{ "run",	mewa_compiler_run },
	{ NULL,		NULL },
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",	mewa_new_compiler },
	{ NULL,  	NULL }
};

DLL_PUBLIC int luaopen_mewa( lua_State* ls)
{
	/* Create the metatable and put it on the stack. */
	luaL_newmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_pushvalue( ls, -1);

	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, mewa_compiler_methods, 0);

	luaL_newlib( ls, mewa_functions);
	return 1;
}



