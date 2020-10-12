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
#include "lua_serialize.hpp"
#include "automaton.hpp"
#include "typedb.hpp"
#include "lexer.hpp"
#include "scope.hpp"
#include "error.hpp"
#include "version.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include <memory>
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
#define MEWA_OBJTREE_METATABLE_NAME 	"mewa.objtree"
#define MEWA_TYPETREE_METATABLE_NAME 	"mewa.typetree"
#define MEWA_REDUTREE_METATABLE_NAME 	"mewa.redutree"
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
		outputFileHandle = nullptr;
		new (&automaton) mewa::Automaton();
		callTableName.init();
	}
	void closeOutput() noexcept
	{
		if (outputFileHandle && outputFileHandle != ::stdout && outputFileHandle != ::stderr) std::fclose(outputFileHandle);
		outputFileHandle = nullptr;
	}
	void destroy( lua_State* ls) noexcept
	{
		lua_pushnil( ls);
		lua_setglobal( ls, callTableName.buf);

		closeOutput();
		automaton.~Automaton();
	}
	static const char* metatableName() noexcept {return MEWA_COMPILER_METATABLE_NAME;}
};

struct mewa_typedb_userdata_t
{
public:
	mewa::TypeDatabase* impl;
	ObjectTableName objTableName;
private:
	int objCount;

public:
	void init() noexcept
	{
		impl = nullptr;
		objTableName.init();
		objCount = 0;
	}
	void create()
	{
		if (impl) delete impl;
		impl = new mewa::TypeDatabase();
	}
	void destroy( lua_State* ls) noexcept
	{
		lua_pushnil( ls);
		lua_setglobal( ls, objTableName.buf);

		if (impl) delete impl;
		impl = nullptr;
	}
	int allocObjectHandle() noexcept
	{
		return ++objCount;
	}

	static const char* metatableName() noexcept {return MEWA_TYPEDB_METATABLE_NAME;}
};

template <class TreeType>
struct mewa_treetemplate_userdata_t
{
	std::shared_ptr<TreeType> tree;
	std::size_t nodeidx;
public:
	void init() noexcept
	{
		tree.reset();
		nodeidx = 0;
	}
	void create( TreeType&& o)
	{
		tree.reset( new TreeType( std::move( o)));
	}
	void destroy( lua_State* ls) noexcept
	{
		init();
	}
};

struct mewa_objtree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::NamedObjectTree>
{
	static const char* metatableName() noexcept {return MEWA_OBJTREE_METATABLE_NAME;}
};

struct mewa_typetree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::TypeDefinitionTree>
{
	static const char* metatableName() noexcept {return MEWA_TYPETREE_METATABLE_NAME;}
};

struct mewa_redutree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::ReductionDefinitionTree>
{
	static const char* metatableName() noexcept {return MEWA_REDUTREE_METATABLE_NAME;}
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
	mb->destroy( ls);
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
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_touserdata(ls, lua_upvalueindex(1));
	static const char* functionName = "mewa print redirected";
	if (!cp || !cp->outputFileHandle)
	{
		luaL_error( ls, "%s: %s", functionName, mewa::Error::code2String( mewa::Error::LuaInvalidUserData));
	}
	int nargs = lua_gettop( ls);
	for (int li=1; li <= nargs; ++li)
	{
		std::size_t len;
		const char* str = lua_tolstring( ls, li, &len);
		std::fwrite( str, 1, len, cp->outputFileHandle);
		int ec = std::ferror( cp->outputFileHandle);
		luaL_error( ls, "%s: %s", functionName, std::strerror( ec));
		std::fflush( cp->outputFileHandle);
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

static int getArgumentAsConstructor( const char* functionName, lua_State* ls, int li, int objtable, mewa_typedb_userdata_t* td)
{
	int handle = td->allocObjectHandle();
	if (lua_isnil( ls, li)) throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentNotNil);
	lua_pushvalue( ls, li);
	lua_rawseti( ls, objtable < 0 ? (objtable-1):objtable, handle);
	return handle;
}

static mewa::TypeDatabase::Parameter getArgumentAsParameter( const char* functionName, lua_State* ls, int li, int objtable, mewa_typedb_userdata_t* td)
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
					constructor = td->allocObjectHandle();
					lua_pushvalue( ls, -1);			// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL] [VAL]
					lua_rawseti( ls, -5, constructor);	// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
				else
				{
					throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
				}
			}
		}
		else if (lua_type( ls, -2) == LUA_TSTRING)
		{
			std::size_t len;
			char const* str = lua_tolstring( ls, -2, &len);
			if (len == 4 && 0==std::memcmp( str, "type", len))
			{
				type = lua_tointeger( ls, -1);
			}
			else if (len == 11 && 0==std::memcmp( str, "constructor", len))
			{
				constructor = td->allocObjectHandle();
				lua_pushvalue( ls, -1);			// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL] [VAL]
				lua_rawseti( ls, -5, constructor);	// STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
			}
			else
			{
				throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
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
	getArgumentAsParameterList( 
			const char* functionName, lua_State* ls, int li, int objtable,
			mewa_typedb_userdata_t* td, std::pmr::memory_resource* memrsc)
{
	std::pmr::vector<mewa::TypeDatabase::Parameter> rt( memrsc);
	if (lua_isnil( ls, li)) return rt;
	checkArgumentAsTable( functionName, ls, li);

	lua_pushvalue( ls, objtable);
	lua_pushvalue( ls, li < 0 ? (li-1) : li);
	lua_pushnil( ls);												//STK: [OBJTAB] [PARAMTAB] NIL
	while (lua_next( ls, -2))											//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
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
					rt.emplace_back( getArgumentAsParameter( functionName, ls, -1, -4, td));	//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
				else
				{
					if (index > rt.size())
					{
						rt.resize( index+1, mewa::TypeDatabase::Parameter( -1, -1));
					}
					rt[ index] = getArgumentAsParameter( functionName, ls, -1, -4, td);		//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
				}
			}
		}
		else
		{
			throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
		}
		lua_pop( ls, 1);											//STK: [OBJTAB] [PARAMTAB] [KEY]
	}
	lua_pop( ls, 2);												//STK:

	return rt;
}

static void checkStack( const char* functionName, lua_State* ls, int sz)
{
	if (!lua_checkstack( ls, sz)) throw mewa::Error( mewa::Error::LuaStackOutOfMemory, functionName);
}

/// \note Assume STK: [OBJTAB] [REDUTAB]
static inline void luaPushTypeConstructorPair( lua_State* ls, int type, int constructor)
{
	lua_createtable( ls, 0/*narr*/, 2/*nrec*/);	// STK: [OBJTAB] [ELEMTAB] [ELEM]
	lua_pushliteral( ls, "type");			// STK: [OBJTAB] [ELEMTAB] [ELEM] "type"
	lua_pushinteger( ls, type);			// STK: [OBJTAB] [ELEMTAB] [ELEM] "type" [TYPE]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]

	lua_pushliteral( ls, "constructor");		// STK: [OBJTAB] [ELEMTAB] [ELEM] "constructor"
	lua_rawgeti( ls, -4, constructor);		// STK: [OBJTAB] [ELEMTAB] [ELEM] "constructor" [CONSTRUCTOR]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]
}

static void luaPushTypeConstructor(
		lua_State* ls, const char* functionName, const char* objTableName, int constructor)
{
	checkStack( functionName, ls, 6);

	lua_getglobal( ls, objTableName);		// STK: [OBJTAB] 
	lua_rawgeti( ls, -2, constructor);		// STK: [OBJTAB] [CONSTRUCTOR]
	lua_replace( ls, -2);				// STK: [CONSTRUCTOR]
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
		luaPushTypeConstructorPair( ls, reduction.type, reduction.constructor);	// STK: [OBJTAB] [REDUTAB] [REDU]
		lua_rawseti( ls, -2, ridx);						// STK: [OBJTAB] [REDUTAB]
	}
	lua_replace( ls, -2);								// STK: [REDUTAB]
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
		luaPushTypeConstructorPair( ls, item.type, item.constructor);	// STK: [OBJTAB] [ITEMTAB] [ITEM]
		lua_rawseti( ls, -2, ridx);					// STK: [OBJTAB] [ITEMTAB]
	}
	lua_replace( ls, -2);							// STK: [ITEMTAB]
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
		luaPushTypeConstructorPair( ls, parameter.type, parameter.constructor);	// STK: [OBJTAB] PARAMTAB] [PARAM]
		lua_rawseti( ls, -2, ridx);						// STK: [OBJTAB] PARAMTAB]
	}
	lua_replace( ls, -2);								// STK: [PARAMTAB]
}


static int mewa_tostring( lua_State* ls)
{
	static const char* functionName = "mewa.tostring";

	bool success = true;
	try
	{
		int nn = checkNofArguments( functionName, ls, 1/*minNofArgs*/, 2/*maxNofArgs*/);
		bool formatted = true;
		if (nn == 2 && lua_isboolean( ls, 2))
		{
			formatted = lua_toboolean( ls, 2);
		}
		std::string rt = mewa::luaToString( ls, 1, formatted);
		lua_pushlstring( ls, rt.c_str(), rt.size());
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_new_compiler( lua_State* ls)
{
	static const char* functionName = "mewa.compiler";

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		checkArgumentAsTable( functionName, ls, 1);
		checkStack( functionName, ls, 6);
	}
	CATCH_EXCEPTION( success)

	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	try
	{
		cp->init();
	}
	CATCH_EXCEPTION( success)
	lua_pushliteral( ls, "call");
	lua_gettable( ls, 1);
	lua_setglobal( ls, cp->callTableName.buf);

	try
	{
		new (&cp->automaton) mewa::Automaton( mewa::luaLoadAutomaton( ls, 1));
	}
	CATCH_EXCEPTION( success)
	luaL_getmetatable( ls, mewa_compiler_userdata_t::metatableName());
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_compiler( lua_State* ls)
{
	static const char* functionName = "compiler:__gc";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	cp->destroy( ls);
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	static const char* functionName = "compiler:__tostring";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());

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
		std::string result = cp->automaton.tostring();
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
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());

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
				cp->closeOutput();
				if (outputFileName == "stderr")
				{
					cp->outputFileHandle = ::stderr;
				}
				else if (outputFileName == "stdout")
				{
					cp->outputFileHandle = ::stdout;
				}
				else
				{
					cp->outputFileHandle = std::fopen( outputFileName.data(), "w");
				}
				lua_getglobal( ls, "_G");		// STK: table=_G
				lua_pushliteral( ls, "print");		// STK: table=_G, key=print
				lua_pushliteral( ls, "print");		// STK: table=_G, key=print, key=print
				lua_gettable( ls, -3);			// STK: table=_G, key=print, value=function

				lua_getglobal( ls, "_G");
				lua_pushlightuserdata( ls, cp);
				luaL_setfuncs( ls, g_printlib, 1/*number of closure elements*/);
				lua_pop( ls, 1);
			}
			if (nn >= 4 && !lua_isnil( ls, 4))
			{
				std::string_view debugFilename = getArgumentAsString( functionName, ls, 4);
				if (debugFilename == "stderr")
				{
					mewa::luaRunCompiler( ls, cp->automaton, sourceptr, cp->callTableName.buf, &std::cerr);
				}
				else if (debugFilename == "stdout")
				{
					mewa::luaRunCompiler( ls, cp->automaton, sourceptr, cp->callTableName.buf, &std::cout);
				}
				else
				{
					std::ofstream debugStream( debugFilename.data(), std::ios_base::out | std::ios_base::trunc);
					mewa::luaRunCompiler( ls, cp->automaton, sourceptr, cp->callTableName.buf, &debugStream);
				}
			}
			else
			{
				mewa::luaRunCompiler( ls, cp->automaton, sourceptr, cp->callTableName.buf, nullptr/*no debug output*/);
			}
			if (hasRedirectedOutput)
			{
				// Restore old print function that has been left on the stack:
							//STK: table=_G, key=print, value=function
				lua_settable( ls, -3);	//STK: table=_G
				lua_pop( ls, 1);	//STK:
				cp->closeOutput();
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

	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)lua_newuserdata( ls, sizeof(mewa_typedb_userdata_t));
	td->init();
	try
	{
		td->create();
	}
	CATCH_EXCEPTION( success)

	luaL_getmetatable( ls, mewa_typedb_userdata_t::metatableName());
	lua_setmetatable( ls, -2);

	lua_createtable( ls, 1<<16/*expected array elements*/, 0);
	lua_setglobal( ls, td->objTableName.buf);

	return 1;
}

static int mewa_destroy_typedb( lua_State* ls)
{
	static const char* functionName = "typedb:__gc";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	CATCH_EXCEPTION( success)

	td->destroy( ls);
	return 0;
}

static int mewa_typedb_get( lua_State* ls)
{
	static const char* functionName = "typedb:get";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	int handle = -1;

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		std::string_view name = getArgumentAsString( functionName, ls, 2);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 3);
		handle = td->impl->getNamedObject( name, step);
	}
	CATCH_EXCEPTION( success)

	// Get the item addressed with handle from the object table on the top of the stack and return it:
	lua_getglobal( ls, td->objTableName.buf);
	lua_rawgeti( ls, -1, handle);
	lua_replace( ls, -2);

	return 1;
}

static int mewa_typedb_set( lua_State* ls)
{
	static const char* functionName = "typedb:set";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	mewa::Scope scope( 0, std::numeric_limits<mewa::Scope::Step>::max());
	std::string_view name;

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		name = getArgumentAsString( functionName, ls, 2);
		scope = getArgumentAsScope( functionName, ls, 3);
		if (lua_isnil( ls, 4)) throwArgumentError( functionName, 4, mewa::Error::ExpectedArgumentNotNil);
	}
	CATCH_EXCEPTION( success)

	// Put the 4th argument into the object table of this typedb with an index stored as handle in the typedb:
	int handle = td->allocObjectHandle();
	lua_getglobal( ls, td->objTableName.buf);
	lua_pushvalue( ls, 4);
	lua_rawseti( ls, -2, handle);

	try
	{
		td->impl->setNamedObject( name, scope, handle);
	} CATCH_EXCEPTION(success)
	return 0;
}

static int mewa_typedb_def_type( lua_State* ls)
{
	static const char* functionName = "typedb:def_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

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
		lua_getglobal( ls, td->objTableName.buf);
		int constructor = getArgumentAsConstructor( functionName, ls, 5, -1/*objtable*/, td);
		std::pmr::vector<mewa::TypeDatabase::Parameter> parameter;
		if (nn >= 6) parameter = getArgumentAsParameterList( functionName, ls, 6, -1/*objtable*/, td, &memrsc_parameter);
		int priority = (nn >= 7) ? getArgumentAsInteger( functionName, ls, 7) : 0;
		lua_pop( ls, 1); // ... obj table
		int rt = td->impl->defineType( scope, contextType, name, constructor, parameter, priority);
		lua_pushinteger( ls, rt);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_def_reduction( lua_State* ls)
{
	static const char* functionName = "typedb:def_reduction";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		int nn = checkNofArguments( functionName, ls, 5/*minNofArgs*/, 6/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope scope = getArgumentAsScope( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		lua_getglobal( ls, td->objTableName.buf);
		int constructor = getArgumentAsConstructor( functionName, ls, 5, -1/*objtable*/, td);
		float weight = (nn >= 6) ? getArgumentAsFloatingPoint( functionName, ls, 6) : 1.0;
		lua_pop( ls, 1); // ... obj table
		td->impl->defineReduction( scope, toType, fromType, constructor, weight);
	}
	CATCH_EXCEPTION( success)
	return 0;
}

static int mewa_typedb_reduction( lua_State* ls)
{
	static const char* functionName = "typedb:reduction";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		int constructor = td->impl->reduction( step, toType, fromType);
		if (constructor <= 0)
		{
			lua_pushnil( ls);
		}
		else
		{
			lua_getglobal( ls, td->objTableName.buf);
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
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int type = getArgumentAsCardinal( functionName, ls, 3);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto reductions = td->impl->reductions( step, type, resbuf);
		luaPushReductionResults( ls, functionName, td->objTableName.buf, reductions);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_derive_type( lua_State* ls)
{
	static const char* functionName = "typedb:derive_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int toType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		int fromType = getArgumentAsCardinal( functionName, ls, 4);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto deriveres = td->impl->deriveType( step, toType, fromType, resbuf);
		luaPushReductionResults( ls, functionName, td->objTableName.buf, deriveres.reductions);
		lua_pushnumber( ls, deriveres.weightsum);
	}
	CATCH_EXCEPTION( success)
	return 2;
}

static int mewa_typedb_resolve_type( lua_State* ls)
{
	static const char* functionName = "typedb:resolve_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		checkStack( functionName, ls, 8);
		mewa::Scope::Step step = getArgumentAsCardinal( functionName, ls, 2);
		int contextType = getArgumentAsNonNegativeInteger( functionName, ls, 3);
		std::string_view name = getArgumentAsString( functionName, ls, 4);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto resolveres = td->impl->resolveType( step, contextType, name, resbuf);
		luaPushReductionResults( ls, functionName, td->objTableName.buf, resolveres.reductions);
		luaPushResolveResultItems( ls, functionName, td->objTableName.buf, resolveres.items);
	}
	CATCH_EXCEPTION( success)
	return 2;
}

static int mewa_typedb_type_name( lua_State* ls)
{
	static const char* functionName = "typedb:type_name";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsNonNegativeInteger( functionName, ls, 2);

		auto rt = td->impl->typeName( type);
		lua_pushlstring( ls, rt.data(), rt.size());
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_type_string( lua_State* ls)
{
	static const char* functionName = "typedb:type_string";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsNonNegativeInteger( functionName, ls, 2);
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto rt = td->impl->typeToString( type, resbuf);
		lua_pushlstring( ls, rt.c_str(), rt.size());
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_type_parameters( lua_State* ls)
{
	static const char* functionName = "typedb:type_parameters";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsCardinal( functionName, ls, 2);
		auto rt = td->impl->typeParameters( type);
		luaPushParameters( ls, functionName, td->objTableName.buf, rt);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

static int mewa_typedb_type_constructor( lua_State* ls)
{
	static const char* functionName = "typedb:type_constructor";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	bool success = true;
	try
	{
		checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		checkStack( functionName, ls, 2);
		int type = getArgumentAsCardinal( functionName, ls, 2);
		auto rt = td->impl->typeConstructor( type);
		luaPushTypeConstructor( ls, functionName, td->objTableName.buf, rt);
	}
	CATCH_EXCEPTION( success)
	return 1;
}

namespace {
template <class UD>
void create_tree_impl( const char* functionName, lua_State* ls, UD& ud, mewa_typedb_userdata_t* td)
{throw mewa::Error( mewa::Error::LogicError, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));}

template <>
void create_tree_impl<mewa_objtree_userdata_t>( const char* functionName, lua_State* ls, mewa_objtree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
	std::string_view name = getArgumentAsString( functionName, ls, 2);
	ud.create( td->impl->getNamedObjectTree( name));
}

template <>
void create_tree_impl<mewa_typetree_userdata_t>( const char* functionName, lua_State* ls, mewa_typetree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	ud.create( td->impl->getTypeDefinitionTree());
}

template <>
void create_tree_impl<mewa_redutree_userdata_t>( const char* functionName, lua_State* ls, mewa_redutree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	ud.create( td->impl->getReductionDefinitionTree());
}
}//anonymous namespace

template <class UD>
struct LuaTreeMetaMethods
{
	static int create( lua_State* ls)
	{
		static const char* functionName = "typedb:*_tree";
		mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

		UD* ud = (UD*)lua_newuserdata( ls, sizeof(UD));
		bool success = true;
		try
		{
			ud->init();
			create_tree_impl( functionName, ls, *ud, td);
		}
		CATCH_EXCEPTION( success)

		luaL_getmetatable( ls, UD::metatableName());
		lua_setmetatable( ls, -2);

		return 1;
	}

	static int gc( lua_State* ls)
	{
		static const char* functionName = "*tree:__gc";
		UD* ud = (UD*)luaL_checkudata( ls, 1, UD::metatableName());
		bool success = true;
		try
		{
			checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		}
		CATCH_EXCEPTION( success)

		ud->destroy( ls);
		return 0;
	}

	static int chld( lua_State* ls)
	{
		static const char* functionName = "tree:chld";
		return 0;
	}

	static int scope( lua_State* ls)
	{
		static const char* functionName = "tree:scope";
		return 0;
	}

	static int index( lua_State* ls)
	{
		static const char* functionName = "tree:__index";
		return 0;
	}

	static int tostring( lua_State* ls)
	{
		static const char* functionName = "tree:__index";
		return 0;
	}

};


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
	{ "type_name",		mewa_typedb_type_name },
	{ "type_string",	mewa_typedb_type_string },
	{ "type_parameters",	mewa_typedb_type_parameters },
	{ "type_constructor",	mewa_typedb_type_constructor },
	{ "obj_tree",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::create },
	{ "type_tree",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::create },
	{ "reduction_tree",	LuaTreeMetaMethods<mewa_redutree_userdata_t>::create },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_objtree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::scope },
	{ "__index",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::index },
	{ "__tostring",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::tostring },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_typetree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::scope },
	{ "__index",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::index },
	{ "__tostring",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::tostring },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_redutree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::scope },
	{ "__index",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::index },
	{ "__tostring",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::tostring },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",		mewa_new_compiler },
	{ "typedb",		mewa_new_typedb },
	{ "tostring",		mewa_tostring },
	{ nullptr,  		nullptr }
};

static void createMetatable( lua_State* ls, const char* metatableName, const struct luaL_Reg* metatableMethods)
{
	luaL_newmetatable( ls, metatableName);
	lua_pushvalue( ls, -1);
	lua_setfield( ls, -2, "__index");
	luaL_setfuncs( ls, metatableMethods, 0);
}

DLL_PUBLIC int luaopen_mewa( lua_State* ls)
{
	create_memblock_control_class( ls);

	createMetatable( ls, mewa_compiler_userdata_t::metatableName(), mewa_compiler_methods);
	createMetatable( ls, mewa_typedb_userdata_t::metatableName(), mewa_typedb_methods);
	createMetatable( ls, mewa_objtree_userdata_t::metatableName(), mewa_objtree_methods);
	createMetatable( ls, mewa_typetree_userdata_t::metatableName(), mewa_typetree_methods);
	createMetatable( ls, mewa_redutree_userdata_t::metatableName(), mewa_redutree_methods);

	luaL_newlib( ls, mewa_functions);
	return 1;
}



