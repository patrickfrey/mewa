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
	static const char* functionName = "mewa print redirected";
	if (!mw || !mw->outputFileHandle)
	{
		luaL_error( ls, "%s: %s", functionName, mewa::Error::code2String( mewa::Error::LuaInvalidUserData));
	}
	int nargs = lua_gettop( ls);
	for (int li=1; li <= nargs; ++li)
	{
		std::size_t len;
		const char* str = lua_tolstring( ls, li, &len);
		std::fwrite( str, 1, len, mw->outputFileHandle);
		int ec = std::ferror( mw->outputFileHandle);
		luaL_error( ls, "%s: %s", functionName, std::strerror( ec));
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
		throw mewa::Error( mewa::Error::TooManyArguments, mewa::string_format( "%s [%d, maximum %d]", functionName, nn, maxNofArgs));
	}
	if (nn < minNofArgs)
	{
		throw mewa::Error( mewa::Error::TooFewArguments, mewa::string_format( "%s [%d, maximum %d]", functionName, nn, maxNofArgs));
	}
	return nn;
}

[[noreturn]] static void throwArgumentError( const char* functionName, int li, mewa::Error::Code ec)
{
	if (li >= 1)
	{
		char idbuf[ 128];
		std::snprintf( idbuf, sizeof(idbuf), "%s [%d] ", functionName, li);
		throw mewa::Error( ec, idbuf);
	}
	else
	{
		throw mewa::Error( ec);
	}
}

static bool isArgumentType( const char* functionName, lua_State* ls, int li, int luaTypeMask)
{
	return (((1U << lua_type( ls, li)) & luaTypeMask) != 0);
}

static std::string_view getArgumentAsString( const char* functionName, lua_State* ls, int li)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TSTRING)))
	{
		throwArgumentError( functionName, li, mewa::Error::ExpectedStringArgument);
	}
	std::size_t len;
	const char* str = lua_tolstring( ls, li, &len);
	return std::string_view( str, len);
}

static int getArgumentAsInteger( const char* functionName, lua_State* ls, int li, mewa::Error::Code ec = mewa::Error::ExpectedIntegerArgument)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TNUMBER)))
	{
		throwArgumentError( functionName, li, ec);
	}
	double val = lua_tonumber( ls, li);
	if (val - std::floor( val) < std::numeric_limits<double>::epsilon()*4)
	{
		return (int)val;
	}
	else
	{
		throwArgumentError( functionName, li, ec);
	}
}

static int getArgumentAsCardinal( const char* functionName, lua_State* ls, int li)
{
	int rt = getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedCardinalArgument);
	if (rt <= 0) throwArgumentError( functionName, li, mewa::Error::ExpectedCardinalArgument);
	return rt;
}

static int getArgumentAsNonNegativeInteger( const char* functionName, lua_State* ls, int li)
{
	int rt = getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	if (rt < 0) throwArgumentError( functionName, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	return rt;
}

static float getArgumentAsFloatingPoint( const char* functionName, lua_State* ls, int li)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TNUMBER)))
	{
		throwArgumentError( functionName, li, mewa::Error::ExpectedFloatingPointArgument);
	}
	return lua_tonumber( ls, li);
}

static void checkArgumentAsTable( const char* functionName, lua_State* ls, int li)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TTABLE)))
	{
		throwArgumentError( functionName, li, mewa::Error::ExpectedTableArgument);
	}
}

static mewa::Scope getArgumentAsScope( const char* functionName, lua_State* ls, int li)
{
	checkArgumentAsTable( functionName, ls, li);
	mewa::Scope::Step start = 0;
	mewa::Scope::Step end = std::numeric_limits<mewa::Scope::Step>::max();
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (lua_type( ls, -2) == LUA_TNUMBER && lua_type( ls, -1) == LUA_TNUMBER)
		{
			double kk = lua_tonumber( ls, -2);
			if (kk - std::floor( kk) < std::numeric_limits<double>::epsilon()*4)
			{
				if ((int)kk == 1)
				{
					start = lua_tointeger( ls, -1);
				}
				else if ((int)kk == 2)
				{
					end = lua_tointeger( ls, -1);
				}
				else
				{
					throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
				}
			}
		}
		else
		{
			throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
		}
		lua_pop( ls, 1);
	}
	if (rowcnt != 2 || start < 0 || end < 0 || start > end)
	{
		throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
	}
	lua_pop( ls, 1);
	return mewa::Scope( start, end);
}

static int getArgumentAsConstructor( const char* functionName, lua_State* ls, int li, int objtable, int& objcnt)
{
	int handle = ++objcnt;
	lua_pushvalue( ls, li);
	lua_rawseti( ls, objtable < 0 ? (objtable-1):objtable, handle);
	return handle;
}

static mewa::TypeDatabase::Parameter getArgumentAsParameter( const char* functionName, lua_State* ls, int li, int objtable, int& objcnt)
{
	lua_pushvalue( ls, objtable);
	lua_pushvalue( ls, li < 0 ? (li-1) : li );
	lua_pushnil( ls);							//STK: [OBJTAB] [PARAMTAB] [KEY] NIL
	int type = -1;
	int constructor = -1;
	int rowcnt = 0;

	while (lua_next( ls, -2))						// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
	{
		++rowcnt;
		if (lua_type( ls, -2) == LUA_TNUMBER)
		{
			double kk = lua_tonumber( ls, -2);
			if (kk - std::floor( kk) < std::numeric_limits<double>::epsilon()*4)
			{
				if ((int)kk == 1)
				{
					type = lua_tointeger( ls, -1);
				}
				else if ((int)kk == 2)
				{
					constructor = ++objcnt;
					lua_pushvalue( ls, -1);			// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL] [VAL]
					lua_rawseti( ls, -5, constructor);	// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
				else
				{
					throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
				}
			}
		}
		else
		{
			throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
		}
		lua_pop( ls, 1);						// STK: [OBJTAB] [PARAMTAB] [KEY]
	}
	lua_pop( ls, 2);							// STK:
	if (rowcnt != 2 || type < 0 || constructor <= 0)
	{
		throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
	}
	return mewa::TypeDatabase::Parameter( type, constructor);
}

static std::pmr::vector<mewa::TypeDatabase::Parameter>
	getArgumentAsParameterList( const char* functionName, lua_State* ls, int li, int objtable, int& objcnt,std::pmr::memory_resource* memrsc)
{
	std::pmr::vector<mewa::TypeDatabase::Parameter> rt( memrsc);
	if (lua_isnil( ls, li)) return rt;
	checkArgumentAsTable( functionName, ls, li);

	lua_pushvalue( ls, objtable);
	lua_pushvalue( ls, li < 0 ? (li-1) : li);
	lua_pushnil( ls);										//STK: [OBJTAB] [PARAMTAB] NIL
	while (lua_next( ls, -2))									// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
	{
		if (lua_type( ls, -2) == LUA_TNUMBER)
		{
			double kk = lua_tonumber( ls, -2);
			if (kk - std::floor( kk) < std::numeric_limits<double>::epsilon()*4)
			{
				if ((int)kk < 1)
				{
					throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentParameterStructure);
				}
				std::size_t index = (int)kk-1;
				if (index == rt.size())
				{
					rt.push_back( getArgumentAsParameter( functionName, ls, -1, -4, objcnt));	//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
				else
				{
					if (index > rt.size())
					{
						rt.resize( index+1, mewa::TypeDatabase::Parameter( -1, -1));
					}
					rt[ index] = getArgumentAsParameter( functionName, ls, -1, -4, objcnt);	//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
			}
		}
		else
		{
			throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
		}
		lua_pop( ls, 1);									//STK: [OBJTAB] [PARAMTAB] [KEY]
	}
	lua_pop( ls, 2);										//STK:

	return rt;
}

static void checkStack( const char* functionName, lua_State* ls, int sz)
{
	if (!lua_checkstack( ls, sz)) throw mewa::Error( mewa::Error::LuaStackOutOfMemory, functionName);
}

static void luaPushReductionResults(
		lua_State* ls, const char* functionName, const char* objTableName,
		const std::pmr::vector<mewa::TypeDatabase::ReductionResult>& reductions)
{
	checkStack( functionName, ls, reductions.size()+6);

	lua_getglobal( ls, objTableName);
	lua_createtable( ls, reductions.size()/*narr*/, 0/*nrec*/);
	int ridx = 0;
	for (auto const& reduction : reductions)
	{
		++ridx;
		lua_createtable( ls, 2/*narr*/, 0/*nrec*/);	// STK: [OBJTAB] [REDUTAB] [REDU] 
		lua_pushinteger( ls, reduction.type);		// STK: [OBJTAB] [REDUTAB] [REDU] [TYPE]
		lua_rawseti( ls, -2, 1);			// STK: [OBJTAB] [REDUTAB] [REDU]

		lua_rawgeti( ls, -3, reduction.constructor);	// STK: [OBJTAB] [REDUTAB] [REDU] [CONSTRUCTOR]
		lua_rawseti( ls, -2, 2);			// STK: [OBJTAB] [REDUTAB] [REDU]

		lua_rawseti( ls, -2, ridx);			// STK: [OBJTAB] [REDUTAB]
	}
	lua_replace( ls, -2);					// STK: [REDUTAB]
}

static void luaPushResolveResultItems(
		lua_State* ls, const char* functionName, const char* objTableName,
		const std::pmr::vector<mewa::TypeDatabase::ResolveResultItem>& items)
{
	checkStack( functionName, ls, items.size()+6);

	lua_getglobal( ls, objTableName);
	lua_createtable( ls, items.size()/*narr*/, 0/*nrec*/);
	int ridx = 0;
	for (auto const& item : items)
	{
		++ridx;
		lua_createtable( ls, 2/*narr*/, 0/*nrec*/);	// STK: [OBJTAB] [ITEMTAB] [ITEM] 
		lua_pushinteger( ls, item.type);		// STK: [OBJTAB] [ITEMTAB] [ITEM] [TYPE]
		lua_rawseti( ls, -2, 1);			// STK: [OBJTAB] [ITEMTAB] [ITEM]

		lua_rawgeti( ls, -3, item.constructor);		// STK: [OBJTAB] [ITEMTAB] [ITEM] [CONSTRUCTOR]
		lua_rawseti( ls, -2, 2);			// STK: [OBJTAB] [ITEMTAB] [ITEM]

		lua_rawseti( ls, -2, ridx);			// STK: [OBJTAB] [ITEMTAB]
	}
	lua_replace( ls, -2);					// STK: [ITEMTAB]
}

static void luaPushParameters(
		lua_State* ls, const char* functionName, const char* objTableName,
		const mewa::TypeDatabase::ParameterList& parameters)
{
	checkStack( functionName, ls, parameters.size()+6);

	lua_getglobal( ls, objTableName);
	lua_createtable( ls, parameters.size()/*narr*/, 0/*nrec*/);
	int ridx = 0;
	for (auto const& parameter : parameters)
	{
		++ridx;
		lua_createtable( ls, 2/*narr*/, 0/*nrec*/);	// STK: [OBJTAB] [ITEMTAB] [ITEM] 
		lua_pushinteger( ls, parameter.type);		// STK: [OBJTAB] [ITEMTAB] [ITEM] [TYPE]
		lua_rawseti( ls, -2, 1);			// STK: [OBJTAB] [ITEMTAB] [ITEM]

		lua_rawgeti( ls, -3, parameter.constructor);	// STK: [OBJTAB] [ITEMTAB] [ITEM] [CONSTRUCTOR]
		lua_rawseti( ls, -2, 2);			// STK: [OBJTAB] [ITEMTAB] [ITEM]

		lua_rawseti( ls, -2, ridx);			// STK: [OBJTAB] [ITEMTAB]
	}
	lua_replace( ls, -2);					// STK: [ITEMTAB]
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
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	lua_pushnil( ls);
	lua_setglobal( ls, mw->callTableName.buf);

	mw->destroy();
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	static const char* functionName = "compiler:__tostring";
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		checkStack( functionName, ls, 4);
	}
	CATCH_EXCEPTION( success)

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
	mewa_compiler_userdata_t* mw = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, MEWA_COMPILER_METATABLE_NAME);

	bool success = true;
	try
	{
		int nn = checkNofArguments( functionName, ls, 2/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 10);
		std::string_view filename = getArgumentAsString( functionName, ls, 2);

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
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	lua_pushnil( ls);
	lua_setglobal( ls, mw->objTableName.buf);

	mw->destroy();
	return 0;
}

static int mewa_typedb_get( lua_State* ls)
{
	static const char* functionName = "typedb:get";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);
	int handle = -1;

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		std::string_view name = getArgumentAsString( functionName, ls, 2);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 3);
		handle = mw->impl->getNamedObject( name, step);
	}
	CATCH_EXCEPTION( success)

	// Get the item addressed with handle from the object table on the top of the stack and return it:
	lua_getglobal( ls, mw->objTableName.buf);
	lua_rawgeti( ls, -1, handle);
	lua_replace( ls, -2);

	return 1;
}

static int mewa_typedb_set( lua_State* ls)
{
	static const char* functionName = "typedb:set";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	mewa::Scope scope( 0, std::numeric_limits<mewa::Scope::Step>::max());
	std::string_view name;

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		name = getArgumentAsString( functionName, ls, 2);
		scope = getArgumentAsScope( functionName, ls, 3);
	}
	CATCH_EXCEPTION( success)

	// Put the 4th argument into the object table of this typedb with an index stored as handle in the typedb:
	int handle = ++mw->objCount;
	lua_getglobal( ls, mw->objTableName.buf);
	lua_pushvalue( ls, 4);
	lua_rawseti( ls, -2, handle);

	try
	{
		mw->impl->setNamedObject( name, scope, handle);
	} CATCH_EXCEPTION(success)
	return 0;
}

static int mewa_typedb_def_type( lua_State* ls)
{
	static const char* functionName = "typedb:def_type";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	int buffer_parameter[ 1024];
	mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);

	bool success = true;
	try
	{
		int nn = checkNofArguments( functionName, ls, 5/*minNofArgs*/, 7/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope scope = getArgumentAsScope( functionName, ls, 2);
		int contextType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		std::string_view name = getArgumentAsString( functionName, ls, 4);
		lua_getglobal( ls, mw->objTableName.buf);
		int constructor = getArgumentAsConstructor( functionName, ls, 5, -1/*objtable*/, mw->objCount);
		std::pmr::vector<mewa::TypeDatabase::Parameter> parameter;
		if (nn >= 6) parameter = getArgumentAsParameterList( functionName, ls, 6, -1/*objtable*/, mw->objCount, &memrsc_parameter);
		int priority = (nn >= 7) ? getArgumentAsInteger( functionName, ls, 7) : 0;
		lua_pop( ls, 1); // ... obj table
		int rt = mw->impl->defineType( scope, contextType, name, constructor, parameter, priority);
		lua_pushinteger( ls, rt);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_def_reduction( lua_State* ls)
{
	static const char* functionName = "typedb:def_reduction";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		int nn = checkNofArguments( functionName, ls, 5/*minNofArgs*/, 6/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope scope = getArgumentAsScope( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		lua_getglobal( ls, mw->objTableName.buf);
		int constructor = getArgumentAsConstructor( functionName, ls, 5, -1/*objtable*/, mw->objCount);
		float weight = (nn >= 6) ? getArgumentAsFloatingPoint( functionName, ls, 6) : 1.0;
		lua_pop( ls, 1); // ... obj table
		mw->impl->defineReduction( scope, toType, fromType, constructor, weight);
	}
	CATCH_EXCEPTION( success)
	return 0;
}

static int mewa_typedb_reduction( lua_State* ls)
{
	static const char* functionName = "typedb:reduction";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		int constructor = mw->impl->reduction( step, toType, fromType);
		if (constructor <= 0)
		{
			lua_pushnil( ls);
		}
		else
		{
			lua_getglobal( ls, mw->objTableName.buf);
			lua_rawgeti( ls, -1, constructor);
			lua_replace( ls, -2);
		}
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_reductions( lua_State* ls)
{
	static const char* functionName = "typedb:reductions";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int type = getArgumentAsCardinal( functionName, ls, 3);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto reductions = mw->impl->reductions( step, type, resbuf);
		luaPushReductionResults( ls, functionName, mw->objTableName.buf, reductions);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_derive_type( lua_State* ls)
{
	static const char* functionName = "typedb:derive_type";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto deriveres = mw->impl->deriveType( step, toType, fromType, resbuf);
		luaPushReductionResults( ls, functionName, mw->objTableName.buf, deriveres.reductions);
		lua_pushnumber( ls, deriveres.weightsum);
	}
	CATCH_EXCEPTION( success)
	return 2;
}

static int mewa_typedb_resolve_type( lua_State* ls)
{
	static const char* functionName = "typedb:resolve_type";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int contextType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		std::string_view name = getArgumentAsString( functionName, ls, 4);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto resolveres = mw->impl->resolveType( step, contextType, name, resbuf);
		luaPushReductionResults( ls, functionName, mw->objTableName.buf, resolveres.reductions);
		luaPushResolveResultItems( ls, functionName, mw->objTableName.buf, resolveres.items);
	}
	CATCH_EXCEPTION( success)
	return 2;
}

static int mewa_typedb_typename( lua_State* ls)
{
	static const char* functionName = "typedb:typename";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsNonNegativeInteger( functionName, ls, 2);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto rt = mw->impl->typeToString( type, resbuf);
		lua_pushlstring( ls, rt.c_str(), rt.size());
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_parameters( lua_State* ls)
{
	static const char* functionName = "typedb:parameters";
	mewa_typedb_userdata_t* mw = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, MEWA_TYPEDB_METATABLE_NAME);

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsCardinal( functionName, ls, 2);
		auto rt = mw->impl->parameters( type);
		luaPushParameters( ls, functionName, mw->objTableName.buf, rt);
	}
	CATCH_EXCEPTION( success)
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



