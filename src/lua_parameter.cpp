/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Parsing of the arguments and creating the return values for lua functions
/// \file "lua_parameter.cpp"
#include "lua_parameter.hpp"
#include "lua_userdata.hpp"
#include "error.hpp"
#include "scope.hpp"
#include "typedb.hpp"
#include "strings.hpp"
#include <cstdlib>
#include <cmath>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

int mewa::lua::checkNofArguments( const char* functionName, lua_State* ls, int minNofArgs, int maxNofArgs)
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

void mewa::lua::throwArgumentError( const char* functionName, int li, mewa::Error::Code ec)
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

std::string_view mewa::lua::getArgumentAsString( const char* functionName, lua_State* ls, int li)
{
	if (!mewa::lua::isArgumentType( functionName, ls, li, (1 << LUA_TSTRING)))
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedStringArgument);
	}
	std::size_t len;
	const char* str = lua_tolstring( ls, li, &len);
	return std::string_view( str, len);
}

long mewa::lua::getArgumentAsInteger( const char* functionName, lua_State* ls, int li, mewa::Error::Code ec)
{
	if (!mewa::lua::isArgumentType( functionName, ls, li, (1 << LUA_TNUMBER)))
	{
		mewa::lua::throwArgumentError( functionName, li, ec);
	}
	double val = lua_tonumber( ls, li);
	if (val - std::floor( val) < std::numeric_limits<double>::epsilon()*4)
	{
		return (long)val;
	}
	else
	{
		mewa::lua::throwArgumentError( functionName, li, ec);
	}
}

long mewa::lua::getArgumentAsCardinal( const char* functionName, lua_State* ls, int li)
{
	long rt = mewa::lua::getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedCardinalArgument);
	if (rt <= 0) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedCardinalArgument);
	return rt;
}

long mewa::lua::getArgumentAsNonNegativeInteger( const char* functionName, lua_State* ls, int li)
{
	long rt = mewa::lua::getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	if (rt < 0) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	return rt;
}

float mewa::lua::getArgumentAsFloatingPoint( const char* functionName, lua_State* ls, int li)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TNUMBER)))
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedFloatingPointArgument);
	}
	return lua_tonumber( ls, li);
}

void mewa::lua::checkArgumentAsTable( const char* functionName, lua_State* ls, int li)
{
	if (!isArgumentType( functionName, ls, li, (1 << LUA_TTABLE)))
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedTableArgument);
	}
}

mewa::Scope mewa::lua::getArgumentAsScope( const char* functionName, lua_State* ls, int li)
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
					mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
				}
			}
		}
		else
		{
			mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
		}
		lua_pop( ls, 1);
	}
	if (rowcnt != 2 || start < 0 || end < 0 || start > end)
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
	}
	lua_pop( ls, 1);
	return mewa::Scope( start, end);
}

mewa::TagMask mewa::lua::getArgumentAsTagMask( const char* functionName, lua_State* ls, int li)
{
	mewa::TagMask::BitSet bs = mewa::lua::getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	return mewa::TagMask( bs);
}

int mewa::lua::getArgumentAsConstructor( const char* functionName, lua_State* ls, int li, int objtable, mewa_typedb_userdata_t* td)
{
	int handle = td->allocObjectHandle();
	if (lua_isnil( ls, li)) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentNotNil);
	lua_pushvalue( ls, li);
	lua_rawseti( ls, objtable < 0 ? (objtable-1):objtable, handle);
	return handle;
}

mewa::TypeDatabase::Parameter mewa::lua::getArgumentAsParameter( const char* functionName, lua_State* ls, int li, int objtable, mewa_typedb_userdata_t* td)
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
				mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
			}
		}
		else
		{
			mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
		}
		lua_pop( ls, 1);						// STK: [OBJTAB] [PARAMTAB] [KEY]
	}
	lua_pop( ls, 2);							// STK:
	if (rowcnt != 2 || type < 0 || constructor <= 0)
	{
		mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentParameterStructure);
	}
	return mewa::TypeDatabase::Parameter( type, constructor);
}

std::pmr::vector<mewa::TypeDatabase::Parameter>
	mewa::lua::getArgumentAsParameterList( 
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
					mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentParameterStructure);
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
					rt[ index] = mewa::lua::getArgumentAsParameter( functionName, ls, -1, -4, td);	//STK: [OBJTAB] [PARAMTAB] [KEY] [VAL]
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

/// \note Assume STK: [OBJTAB] [REDUTAB]
static inline void pushTypeConstructorPair( lua_State* ls, int type, int constructor)
{
	lua_createtable( ls, 0/*narr*/, 2/*nrec*/);	// STK: [OBJTAB] [ELEMTAB] [ELEM]
	lua_pushliteral( ls, "type");			// STK: [OBJTAB] [ELEMTAB] [ELEM] "type"
	lua_pushinteger( ls, type);			// STK: [OBJTAB] [ELEMTAB] [ELEM] "type" [TYPE]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]

	lua_pushliteral( ls, "constructor");		// STK: [OBJTAB] [ELEMTAB] [ELEM] "constructor"
	lua_rawgeti( ls, -4, constructor);		// STK: [OBJTAB] [ELEMTAB] [ELEM] "constructor" [CONSTRUCTOR]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]
}

void mewa::lua::pushTypeConstructor(
		lua_State* ls, const char* functionName, const char* objTableName, int constructor)
{
	checkStack( functionName, ls, 6);

	lua_getglobal( ls, objTableName);		// STK: [OBJTAB] 
	lua_rawgeti( ls, -1, constructor);		// STK: [OBJTAB] [CONSTRUCTOR]
	lua_replace( ls, -2);				// STK: [CONSTRUCTOR]
}

void mewa::lua::pushReductionResults(
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
		pushTypeConstructorPair( ls, reduction.type, reduction.constructor);	// STK: [OBJTAB] [REDUTAB] [REDU]
		lua_rawseti( ls, -2, ridx);						// STK: [OBJTAB] [REDUTAB]
	}
	lua_replace( ls, -2);								// STK: [REDUTAB]
}

void mewa::lua::pushResolveResultItems(
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
		pushTypeConstructorPair( ls, item.type, item.constructor);	// STK: [OBJTAB] [ITEMTAB] [ITEM]
		lua_rawseti( ls, -2, ridx);					// STK: [OBJTAB] [ITEMTAB]
	}
	lua_replace( ls, -2);							// STK: [ITEMTAB]
}

void mewa::lua::pushParameters(
		lua_State* ls, const char* functionName, const char* objTableName,
		const mewa::TypeDatabase::ParameterList& parameters)
{
	checkStack( functionName, ls, 8);

	lua_getglobal( ls, objTableName);
	lua_createtable( ls, parameters.size()/*narr*/, 0/*nrec*/);
	int ridx = 0;
	for (auto const& parameter : parameters)
	{
		++ridx;
		pushTypeConstructorPair( ls, parameter.type, parameter.constructor);	// STK: [OBJTAB] PARAMTAB] [PARAM]
		lua_rawseti( ls, -2, ridx);						// STK: [OBJTAB] PARAMTAB]
	}
	lua_replace( ls, -2);								// STK: [PARAMTAB]
}

void mewa::lua::pushReductionDefinition(
		lua_State* ls, const char* functionName, const char* objTableName,
		const mewa::TypeDatabase::ReductionDefinition& redu)
{
	checkStack( functionName, ls, 6);

	lua_getglobal( ls, objTableName);		// STK: [OBJTAB] 
	lua_createtable( ls, 0/*narr*/, 5/*nrec*/);	// STK: [OBJTAB] [REDUTAB] 

	lua_pushliteral( ls, "to");			// STK: [OBJTAB] [REDUTAB] "to"
	lua_pushinteger( ls, redu.toType);		// STK: [OBJTAB] [REDUTAB] "to" [TYPE]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [REDUTAB]

	lua_pushliteral( ls, "from");			// STK: [OBJTAB] [REDUTAB] "from"
	lua_pushinteger( ls, redu.fromType);		// STK: [OBJTAB] [REDUTAB] "from" [TYPE]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [REDUTAB]

	lua_pushliteral( ls, "constructor");		// STK: [OBJTAB] [REDUTAB] "constructor"
	lua_rawgeti( ls, -3, redu.constructor);		// STK: [OBJTAB] [REDUTAB] "constructor" [CONSTRUCTOR]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [REDUTAB]

	lua_pushliteral( ls, "tag");			// STK: [OBJTAB] [REDUTAB] "tag"
	lua_pushinteger( ls, redu.tag);			// STK: [OBJTAB] [REDUTAB] "tag" [TAG]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [REDUTAB]

	lua_pushliteral( ls, "weight");			// STK: [OBJTAB] [REDUTAB] "weight"
	lua_pushnumber( ls, redu.weight);		// STK: [OBJTAB] [REDUTAB] "weight" [WEIGHT]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [REDUTAB]

	lua_replace( ls, -2);				// STK: [OBJTAB]
}


