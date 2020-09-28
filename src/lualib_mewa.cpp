/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "export.hpp"
#include "lua_load_automaton.hpp"
#include "lua_run_compiler.hpp"
#include "lua_object_reference.hpp"
#include "automaton.hpp"
#include "typedb.hpp"
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
#include <iomanip>
#include <iostream>
#include <fstream>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

extern "C" int luaopen_mewa( lua_State* ls);

#define MEWA_COMPILER_METATABLE_NAME 	"mewa.compiler"
#define MEWA_TYPEDB_METATABLE_NAME 	"mewa.typedb"
#define MEWA_CALLTABLE_FMT	 	"mewa.calls.%d"

struct CallTableName
{
	char buf[ 20];

	void init()
	{
		static int callTableIdx = 0;
		if ((int)sizeof(buf) <= std::snprintf( buf, sizeof(buf), MEWA_CALLTABLE_FMT, ++callTableIdx))
		{
			throw mewa::Error( mewa::Error::TooManyInstancesCreated, MEWA_COMPILER_METATABLE_NAME);
		}
	}
};

struct mewa_compiler_userdata_t
{
	mewa::Automaton automaton;
	CallTableName callTableName;
	FILE* outputFileHandle;

	void init()
	{
		std::memset( &automaton, 0, sizeof( automaton));
		outputFileHandle = nullptr;
		callTableName.init();
	}
	void closeOutput()
	{
		if (outputFileHandle && outputFileHandle != ::stdout && outputFileHandle != ::stderr) std::fclose(outputFileHandle);
		outputFileHandle = nullptr;
	}
	void destroy()
	{
		closeOutput();
		automaton.~Automaton();
	}
};

struct mewa_typedb_userdata_t
{
	mewa::TypeDatabase* impl;

	void init()
	{
		impl = nullptr;
	}
	void create()
	{
		destroy();
		impl = new mewa::TypeDatabase();
	}
	void destroy()
	{
		if (impl) delete impl;
		impl = nullptr;
	}
};

#define CATCH_EXCEPTION( success) \
	catch (const std::runtime_error& err)\
	{\
		lua_pushstring( ls, err.what());\
		success = false;\
	}\
	catch (const std::bad_alloc& )\
	{\
		lua_pushstring( ls, "out of memory");\
		success = false;\
	}\
	catch ( ... )\
	{\
		lua_pushstring( ls, "unknown error");\
		success = false;\
	}\
	if (!success) lua_error( ls);

static int destroy_memblock( lua_State* ls)
{
	memblock_userdata_t* mb = (memblock_userdata_t*)luaL_checkudata( ls, 1, mewa::MemoryBlock::metatablename());
	mb->destroy();
	return 0;
}

static std::string_view move_string_on_lua_stack( lua_State* ls, std::string&& str)
{
	memblock_userdata_t* mb = memblock_userdata_t::create( ls);
	mewa::ObjectReference<std::string>* obj = new mewa::ObjectReference<std::string>( std::move(str));
	mb->memoryBlock = obj;
	return obj->obj();
}

static int lua_print_redirected( lua_State* ls) {
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)lua_touserdata(ls, lua_upvalueindex(1));
	if (!mw) luaL_error( ls, "calling print redirected without attached userdata");
	if (!mw->outputFileHandle) luaL_error( ls, "no valid filehandle in attached userdata of print redirected");

	int nargs = lua_gettop( ls);
	for (int li=1; li <= nargs; ++li)
	{
		std::size_t len;
		const char* str = lua_tolstring( ls, li, &len);
		std::fwrite( str, 1, len, mw->outputFileHandle);
		int ec = std::ferror( mw->outputFileHandle);
		if (ec) luaL_error( ls, "error in print redirected: %s", std::strerror( ec));
		std::fflush( mw->outputFileHandle);
	}
	return 0;
}

static const struct luaL_Reg g_printlib [] = {
	{"print", lua_print_redirected},
	{nullptr, nullptr} /* end of array */
};


static int mewa_new_compiler( lua_State* ls)
{
	static const char* functionName = "mewa.compiler";
	int nn = lua_gettop(ls);
	if (nn > 1) return luaL_error( ls, "too many arguments calling '%s'", functionName);
	if (nn < 1) return luaL_error( ls, "too few arguments calling '%s'", functionName);

	luaL_checktype( ls, 1, LUA_TTABLE);
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	bool success = true;
	try
	{
		mw->init();
	}
	CATCH_EXCEPTION( success)
	lua_pushstring( ls, "call");
	lua_gettable( ls, 1);
	lua_setglobal( ls, mw->callTableName.buf);

	try
	{
		new (&mw->automaton) mewa::Automaton( mewa::luaLoadAutomaton( ls, 1));
	}
	CATCH_EXCEPTION( success)
	luaL_getmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_compiler( lua_State* ls)
{
	static const char* functionName = "compiler:__gc";
	int nn = lua_gettop(ls);
	if (nn > 1) luaL_error( ls, "too many arguments calling '%s'", functionName);
	if (nn == 0) luaL_error( ls, "no object specified ('.' instead of ':') calling '%s'", functionName);

	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	mw->destroy();
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	static const char* functionName = "compiler:__tostring";
	int nn = lua_gettop(ls);
	if (nn > 1) return luaL_error( ls, "too many arguments calling '%s'", functionName);
	if (nn == 0) return luaL_error( ls, "no object specified ('.' instead of ':') calling '%s'", functionName);
	if (!lua_checkstack( ls, 4)) return luaL_error( ls, "no Lua stack left in '%s'", functionName);

	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	std::string_view resultptr;
	bool success = true;
	try
	{
		std::string result = mw->automaton.tostring();
		resultptr = move_string_on_lua_stack( ls, std::move( result));
	}
	CATCH_EXCEPTION( success)

	lua_pushlstring( ls, resultptr.data(), resultptr.size());
	lua_replace( ls, -2); // ... dispose the element created with move_string_on_lua_stack
	return 1;
}

static int mewa_compiler_run( lua_State* ls)
{
	static const char* functionName = "compiler:run( inputfile[ , outputfile[ , dbgoutput]])";
	bool success = true;
	try
	{
		int nn = lua_gettop(ls);
		if (nn > 4) return luaL_error( ls, "too many arguments calling '%s'", functionName);
		if (nn < 2) return luaL_error( ls, "too few arguments calling '%s'", functionName);
		if (!lua_checkstack( ls, 10)) return luaL_error( ls, "no Lua stack left in '%s'", functionName);

		mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
		if (lua_type( ls, 2) != LUA_TSTRING) throw mewa::Error( mewa::Error::ExpectedFilenameAsArgument,
								std::string(functionName) + "[1] " + lua_typename( ls, lua_type( ls, 2)));
		const char* filename = lua_tostring( ls, 2);
		std::string source = mewa::readFile( filename);
		std::string_view sourceptr = move_string_on_lua_stack( ls, std::move( source));		// STK: [COMPILER] [INPUTFILE] [SOURCE]

		if (nn >= 3)
		{
			bool hasRedirectedOutput = !lua_isnil( ls, 3);
			if (hasRedirectedOutput)
			{
				if (lua_type( ls, 3) != LUA_TSTRING)
				{
					throw mewa::Error( mewa::Error::ExpectedFilenameAsArgument,
								std::string(functionName) + "[2] " + lua_typename( ls, lua_type( ls, 3)));
				}
				const char* outputFileName = lua_tostring( ls, 3);
				mw->closeOutput();
				if (0==std::strcmp( outputFileName, "stderr"))
				{
					mw->outputFileHandle = ::stderr;
				}
				else if (0==std::strcmp( outputFileName, "stdout"))
				{
					mw->outputFileHandle = ::stdout;
				}
				else
				{
					mw->outputFileHandle = std::fopen( outputFileName, "w");
				}
				lua_getglobal( ls, "_G");		// STK: table=_G
				lua_pushliteral( ls, "print");		// STK: table=_G, key=print
				lua_pushliteral( ls, "print");		// STK: table=_G, key=print, key=print
				lua_gettable( ls, -3);			// STK: table=_G, key=print, value=function

				lua_getglobal( ls, "_G");
				lua_pushlightuserdata( ls, mw);
				luaL_setfuncs( ls, g_printlib, 1/*number of closure elements*/);
				lua_pop( ls, 1);
			}
			if (nn >= 4 && !lua_isnil( ls, 4))
			{
				if (lua_type( ls, 4) != LUA_TSTRING)
				{
					throw mewa::Error( mewa::Error::ExpectedFilenameAsArgument,
								std::string(functionName) + "[3] " + lua_typename( ls, lua_type( ls, 4)));
				}
				const char* debugFilename = lua_tostring( ls, 4);
				if (0==std::strcmp( debugFilename, "stderr"))
				{
					mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, &std::cerr);
				}
				else if (0==std::strcmp( debugFilename, "stdout"))
				{
					mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, &std::cout);
				}
				else
				{
					std::ofstream debugStream( debugFilename, std::ios_base::out | std::ios_base::trunc);
					mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, &debugStream);
				}
			}
			else
			{
				mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, nullptr/*no debug output*/);
			}
			if (hasRedirectedOutput)
			{
				// Restore old print function that has been left on the stack:
							//STK: table=_G, key=print, value=function
				lua_settable( ls, -3);	//STK: table=_G
				lua_pop( ls, 1);	//STK:
				mw->closeOutput();
			}
		}
		lua_pop( ls, 1); // ... destroy string on lua stack created with move_string_on_lua_stack
	}
	CATCH_EXCEPTION( success)
	return 0;
}

static int mewa_new_typedb( lua_State* ls)
{
	static const char* functionName = "mewa.typedb";
	int nn = lua_gettop(ls);
	if (nn > 0) return luaL_error( ls, "too many arguments calling '%s'", functionName);

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)lua_newuserdata( ls, sizeof(mewa_typedb_userdata_t));
	mw->init();
	bool success = true;
	try
	{
		mw->create();
	}
	CATCH_EXCEPTION( success)
	luaL_getmetatable( ls, MEWA_TYPEDB_METATABLE_NAME);
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_typedb( lua_State* ls)
{
	static const char* functionName = "typedb:__gc";
	int nn = lua_gettop(ls);
	if (nn > 1) luaL_error( ls, "too many arguments calling '%s'", functionName);
	if (nn == 0) luaL_error( ls, "no object specified ('.' instead of ':') calling '%s'", functionName);

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);
	mw->destroy();
	return 0;
}

static int mewa_typedb_tostring( lua_State* ls)
{
	static const char* functionName = "typedb:__tostring";
	int nn = lua_gettop(ls);
	if (nn > 1) return luaL_error( ls, "too many arguments calling '%s'", functionName);
	if (nn == 0) return luaL_error( ls, "no object specified ('.' instead of ':') calling '%s'", functionName);
	if (!lua_checkstack( ls, 4)) return luaL_error( ls, "no Lua stack left in '%s'", functionName);

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);
	if (!mw->impl) return 0;

	std::string_view resultptr;
	bool success = true;
	try
	{
		std::string result = mw->impl->tostring();
		resultptr = move_string_on_lua_stack( ls, std::move( result));
	}
	CATCH_EXCEPTION( success)

	lua_pushlstring( ls, resultptr.data(), resultptr.size());
	lua_replace( ls, -2); // ... dispose the element created with move_string_on_lua_stack
	return 1;
}

static const struct luaL_Reg memblock_control_methods[] = {
	{ "__gc",	destroy_memblock },
	{ nullptr,	nullptr }
};

static void create_memblock_control_class( lua_State* ls)
{
	luaL_newmetatable( ls, mewa::MemoryBlock::metatablename());
	lua_pushvalue( ls, -1);

	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, memblock_control_methods, 0);
	lua_pop( ls, 1);
}

static const struct luaL_Reg mewa_compiler_methods[] = {
	{ "__gc",	mewa_destroy_compiler },
	{ "__tostring",	mewa_compiler_tostring },
	{ "run",	mewa_compiler_run },
	{ nullptr,	nullptr }
};

static const struct luaL_Reg mewa_typedb_methods[] = {
	{ "__gc",	mewa_destroy_typedb },
	{ "__tostring",	mewa_typedb_tostring },
	{ nullptr,	nullptr }
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",	mewa_new_compiler },
	{ "typedb",	mewa_new_typedb },
	{ nullptr,  	nullptr }
};

DLL_PUBLIC int luaopen_mewa( lua_State* ls)
{
	create_memblock_control_class( ls);

	luaL_newmetatable( ls, MEWA_COMPILER_METATABLE_NAME);
	lua_pushvalue( ls, -1);
	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, mewa_compiler_methods, 0);

	luaL_newmetatable( ls, MEWA_TYPEDB_METATABLE_NAME);
	lua_pushvalue( ls, -1);
	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, mewa_typedb_methods, 0);

	luaL_newlib( ls, mewa_functions);
	return 1;
}



