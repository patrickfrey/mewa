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
#include "strings.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <limits>

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
#define MEWA_OBJTABLE_FMT		"mewa.obj.%d"

struct TableName
{
	char buf[ 20];

	void init( const char* instname, const char* fmtstr, int counter)
	{
		if ((int)sizeof(buf) <= std::snprintf( buf, sizeof(buf), fmtstr, counter))
		{
			throw mewa::Error( mewa::Error::TooManyInstancesCreated, instname);
		}
	}
};

struct CallTableName :public TableName
{
	static int tableIdx;
	void init() {TableName::init( MEWA_COMPILER_METATABLE_NAME, MEWA_CALLTABLE_FMT, ++tableIdx);}
};

int CallTableName::tableIdx = 0;

struct ObjectTableName :public TableName
{
	static int tableIdx;
	void init() {TableName::init( MEWA_TYPEDB_METATABLE_NAME, MEWA_OBJTABLE_FMT, ++tableIdx);}
};

int ObjectTableName::tableIdx = 0;

struct mewa_compiler_userdata_t
{
	mewa::Automaton automaton;
	CallTableName callTableName;
	FILE* outputFileHandle;

	void init()
	{
		new (&automaton) mewa::Automaton();
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
	ObjectTableName objTableName;
	int objCount;

	void init()
	{
		impl = nullptr;
		objCount = 0;
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
		lua_pushliteral( ls, "out of memory");\
		success = false;\
	}\
	catch ( ... )\
	{\
		lua_pushliteral( ls, "unknown error");\
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

static int checkNofArguments( const char* functionName, lua_State* ls, int minNofArgs, int maxNofArgs)
{
	int nn = lua_gettop( ls);
	if (nn > maxNofArgs)
	{
		if (maxNofArgs == 0)
		{
			throw std::runtime_error( mewa::string_format( "no arguments expected calling function '%s'", functionName));
		}
		else
		{
			throw std::runtime_error( mewa::string_format( "too many arguments (%d, maximum %d) calling function '%s'", nn, maxNofArgs, functionName));
		}
	}
	if (nn < minNofArgs)
	{
		throw std::runtime_error( mewa::string_format( "too few arguments (%d, minimum %d) calling '%s'", nn, minNofArgs, functionName));
	}
	return nn;
}

static void checkArgument( const char* functionName, lua_State* ls, int li, int luaTypeMask, const char* expectedTypeName)
{
	if (((1U << lua_type( ls, li)) & luaTypeMask) == 0)
	{
		char numbuf[ 32];
		std::snprintf( numbuf, sizeof(numbuf), "[%d] ", li);
		throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s", li, functionName, expectedTypeName));
	}
}

static std::string_view getArgumentAsString( const char* functionName, lua_State* ls, int li)
{
	checkArgument( functionName, ls, li, (1 << LUA_TSTRING), "string");
	std::size_t len;
	const char* str = lua_tolstring( ls, li, &len);
	return std::string_view( str, len);
}

static int getArgumentAsInteger( const char* functionName, lua_State* ls, int li, const char* expectedTypeName="integer")
{
	checkArgument( functionName, ls, li, (1 << LUA_TNUMBER), "integer");
	double val = lua_tonumber( ls, li);
	if (val - std::floor( val) < std::numeric_limits<double>::epsilon()*4)
	{
		return (int)val;
	}
	else
	{
		throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s", li, functionName, "integer"));
	}
}

static int getArgumentAsCardinal( const char* functionName, lua_State* ls, int li)
{
	int rt = getArgumentAsInteger( functionName, ls, li, "positive integer");
	if (rt <= 0) throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s", li, functionName, "positive integer"));
	return rt;
}

static int getArgumentAsNonNegativeInteger( const char* functionName, lua_State* ls, int li)
{
	int rt = getArgumentAsInteger( functionName, ls, li, "positive integer");
	if (rt < 0) throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s", li, functionName, "non negative integer"));
	return rt;
}

static void checkArgumentAsTable( const char* functionName, lua_State* ls, int li)
{
	checkArgument( functionName, ls, li, (1 << LUA_TTABLE), "table");
}

static mewa::Scope getArgumentAsScope( const char* functionName, lua_State* ls, int li)
{
	checkArgumentAsTable( functionName, ls, li);
	mewa::Scope::Step start = 0;
	mewa::Scope::Step end = std::numeric_limits<mewa::Scope::Step>::max();

	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	while (lua_next( ls, -2))
	{
		if (lua_type( ls, -2) == LUA_TNUMBER && lua_type( ls, -1) == LUA_TNUMBER)
		{
			double kk = lua_tonumber( ls, -2);
			if (kk - std::floor( kk) < std::numeric_limits<double>::epsilon()*4)
			{
				if ((int)kk == 1) start = lua_tointeger( ls, -1);
				else if ((int)kk == 2) end = lua_tointeger( ls, -1);
				else
				{
					throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s",
									li, functionName, "scope structure (pair of non negative integers, unsigned integer range)"));
				}
			}
		}
		else
		{
			throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s",
							li, functionName, "scope structure (pair of non negative integers, unsigned integer range)"));
		}
		lua_pop( ls, 1);
	}
	if (start < 0 || end < 0 || start > end)
	{
		throw std::runtime_error( mewa::string_format( "argument [%d] of function '%s' expected to be a %s",
							li, functionName, "scope structure (pair of non negative integers, unsigned integer range)"));
	}
	return mewa::Scope( start, end);
}

static void checkArgumentAsUserData( const char* functionName, lua_State* ls, int li)
{
	checkArgument( functionName, ls, li, (1 << LUA_TTABLE), "table");
}

static double getArgumentAsFloatingPoint( const char* functionName, lua_State* ls, int li)
{
	checkArgument( functionName, ls, li, (1 << LUA_TNUMBER), "floating point number");
	return lua_tonumber( ls, li);
}

static void checkStack( const char* functionName, lua_State* ls, int sz)
{
	if (!lua_checkstack( ls, sz)) throw std::runtime_error( mewa::string_format( "no Lua stack left when calling '%s'", functionName));
}

static int mewa_new_compiler( lua_State* ls)
{
	static const char* functionName = "mewa.compiler";
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		checkArgumentAsTable( functionName, ls, 1);
	}
	CATCH_EXCEPTION( success)

	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	try
	{
		mw->init();
	}
	CATCH_EXCEPTION( success)
	lua_pushliteral( ls, "call");
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
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);

	lua_pushnil( ls);
	lua_setglobal( ls, mw->callTableName.buf);

	mw->destroy();
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	static const char* functionName = "compiler:__tostring";
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		checkStack( functionName, ls, 4);
	}
	CATCH_EXCEPTION( success)

	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
	std::string_view resultptr;
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
		int nn = checkNofArguments( functionName, ls, 2/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 10);
		std::string_view filename = getArgumentAsString( functionName, ls, 2);

		mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);
		std::string source = mewa::readFile( std::string(filename));
		std::string_view sourceptr = move_string_on_lua_stack( ls, std::move( source));		// STK: [COMPILER] [INPUTFILE] [SOURCE]

		if (nn >= 3)
		{
			bool hasRedirectedOutput = !lua_isnil( ls, 3);
			if (hasRedirectedOutput)
			{
				std::string_view outputFileName = getArgumentAsString( functionName, ls, 3);
				mw->closeOutput();
				if (outputFileName == "stderr")
				{
					mw->outputFileHandle = ::stderr;
				}
				else if (outputFileName == "stdout")
				{
					mw->outputFileHandle = ::stdout;
				}
				else
				{
					mw->outputFileHandle = std::fopen( outputFileName.data(), "w");
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
				std::string_view debugFilename = getArgumentAsString( functionName, ls, 4);
				if (debugFilename == "stderr")
				{
					mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, &std::cerr);
				}
				else if (debugFilename == "stdout")
				{
					mewa::luaRunCompiler( ls, mw->automaton, sourceptr, mw->callTableName.buf, &std::cout);
				}
				else
				{
					std::ofstream debugStream( debugFilename.data(), std::ios_base::out | std::ios_base::trunc);
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
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 0/*minNofArgs*/, 0/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)lua_newuserdata( ls, sizeof(mewa_typedb_userdata_t));
	mw->init();
	try
	{
		mw->create();
	}
	CATCH_EXCEPTION( success)

	luaL_getmetatable( ls, MEWA_TYPEDB_METATABLE_NAME);
	lua_setmetatable( ls, -2);

	lua_createtable( ls, 1<<16/*expected array elements*/, 0);
	lua_setglobal( ls, mw->objTableName.buf);

	return 1;
}

static int mewa_destroy_typedb( lua_State* ls)
{
	static const char* functionName = "typedb:__gc";
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	lua_pushnil( ls);
	lua_setglobal( ls, mw->objTableName.buf);

	mw->destroy();
	return 0;
}

static int mewa_typedb_get( lua_State* ls)
{
	static const char* functionName = "typedb:get";
	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
	}
	CATCH_EXCEPTION( success)

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);
	try
	{
		//getNamedObject( const Scope::Step step, const std::string_view& name) const;
	} CATCH_EXCEPTION(success)
	return 0;
}

static int mewa_typedb_set( lua_State* ls)
{
	static const char* functionName = "typedb:get";
	std::string_view name;
	mewa::Scope scope( 0, std::numeric_limits<mewa::Scope::Step>::max());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		scope = getArgumentAsScope( functionName, ls, 2);
		name = getArgumentAsString( functionName, ls, 3);
	}
	CATCH_EXCEPTION( success)

	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	int handle = ++mw->objCount;

	lua_getglobal( ls, mw->objTableName.buf);
	lua_pushvalue( ls, 4);
	lua_rawseti( ls, -2, handle);

	try
	{
		mw->impl->setNamedObject( scope, name, handle);
	} CATCH_EXCEPTION(success)
	return 0;
}

static int mewa_typedb_def_type( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_def_reduction( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_reduction( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_reductions( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_derive_type( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_resolve_type( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_typename( lua_State* ls)
{
	return 0;
}

static int mewa_typedb_parameters( lua_State* ls)
{
	return 0;
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
	{ "__gc",		mewa_destroy_compiler },
	{ "__tostring",		mewa_compiler_tostring },
	{ "run",		mewa_compiler_run },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_typedb_methods[] = {
	{ "__gc",		mewa_destroy_typedb },
	{ "get",		mewa_typedb_get },
	{ "set",		mewa_typedb_set },
	{ "def_type",		mewa_typedb_def_type },
	{ "def_reduction",	mewa_typedb_def_reduction },
	{ "reduction",		mewa_typedb_reduction },
	{ "reductions",		mewa_typedb_reductions },
	{ "derive_type",	mewa_typedb_derive_type },
	{ "resolve_type",	mewa_typedb_resolve_type },
	{ "typename",		mewa_typedb_typename },
	{ "parameters",		mewa_typedb_parameters },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",		mewa_new_compiler },
	{ "typedb",		mewa_new_typedb },
	{ nullptr,  		nullptr }
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



