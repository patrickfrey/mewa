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
#include "fileio.hpp"
#include <cstdlib>
#include <iostream>
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
		throw mewa::Error( mewa::Error::TooFewArguments, mewa::string_format( "%s [%d, minimum %d]", functionName, nn, minNofArgs));
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
	else if (functionName)
	{
		throw mewa::Error( ec, functionName);
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

const char* mewa::lua::getArgumentAsCString( const char* functionName, lua_State* ls, int li)
{
	if (!mewa::lua::isArgumentType( functionName, ls, li, (1 << LUA_TSTRING)))
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedStringArgument);
	}
	return lua_tostring( ls, li);
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
		return (long)(val + std::numeric_limits<double>::epsilon()*4);
	}
	else
	{
		mewa::lua::throwArgumentError( functionName, li, ec);
	}
}

long mewa::lua::getArgumentAsUnsignedInteger( const char* functionName, lua_State* ls, int li)
{
	long rt = mewa::lua::getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedUnsignedIntegerArgument);
	if (rt <= 0) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedUnsignedIntegerArgument);
	return rt;
}

long mewa::lua::getArgumentAsNonNegativeInteger( const char* functionName, lua_State* ls, int li)
{
	long rt = mewa::lua::getArgumentAsInteger( functionName, ls, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	if (rt < 0) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedNonNegativeIntegerArgument);
	return rt;
}

bool mewa::lua::getArgumentAsBoolean( const char* functionName, lua_State* ls, int li)
{
	if (!mewa::lua::isArgumentType( functionName, ls, li, (1 << LUA_TBOOLEAN)|(1 << LUA_TNIL)))
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedBooleanArgument);
	}
	return lua_toboolean( ls, li);
}

double mewa::lua::getArgumentAsFloatingPoint( const char* functionName, lua_State* ls, int li)
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

static int getTableIndex( lua_State* ls, int li)
{
	double kk = lua_tonumber( ls, li);
	if (kk - std::floor( kk) < std::numeric_limits<double>::epsilon()*4)
	{
		return (int)(kk + std::numeric_limits<double>::epsilon()*4);
	}
	else
	{
		return 0;
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
			int kidx = getTableIndex( ls, -2);
			if (kidx == 1)
			{
				start = lua_tointeger( ls, -1);
			}
			else if (kidx == 2)
			{
				end = lua_tointeger( ls, -1);
				if (end < 0)
				{
					end = std::numeric_limits<Scope::Step>::max() + end + 1;
				}
			}
			else
			{
				mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
			}
		}
		else
		{
			mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
		}
		lua_pop( ls, 1);
	}
	if (start < 0 || end < 0 || start > end)
	{
		mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentScopeStructure);
	}
	lua_pop( ls, 1);
	return mewa::Scope( start, end);
}

mewa::TagMask mewa::lua::getArgumentAsTagMask( const char* functionName, lua_State* ls, int li)
{
	mewa::TagMask::BitSet bs = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, li);
	return mewa::TagMask( bs);
}

int mewa::lua::getArgumentAsConstructor( const char* functionName, lua_State* ls, int li)
{
	if (lua_isnil( ls, li)) return 0;
	lua_pushvalue( ls, li);
	return luaL_ref( ls, LUA_REGISTRYINDEX);
}

static mewa::TypeDatabase::TypeConstructorPair getArgumentAsTypeConstructorPair( const char* functionName, lua_State* ls, int li)
{
	if (lua_type( ls, li) == LUA_TNUMBER)
	{
		int type = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, li);
		return mewa::TypeDatabase::TypeConstructorPair( type, 0/*constructor*/);
	}
	else
	{
		lua_pushvalue( ls, li);
		lua_pushnil( ls);							//STK: [PARAMTAB] [KEY]
		int type = -1;
		int constructor = 0;
		int rowcnt = 0;

		while (lua_next( ls, -2))						// STK: [PARAMTAB] [KEY] [VAL]
		{
			++rowcnt;
			if (lua_type( ls, -2) == LUA_TNUMBER)
			{
				int kidx = getTableIndex( ls, -2);
				if (kidx == 1)
				{
					type = lua_tointeger( ls, -1);
				}
				else if (kidx == 2)
				{
					lua_pushvalue( ls, -1);
					constructor = luaL_ref( ls, LUA_REGISTRYINDEX);
				}
				else
				{
					mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentTypeConstructorPairList);
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
					lua_pushvalue( ls, -1);
					constructor = luaL_ref( ls, LUA_REGISTRYINDEX);
				}
			}
			else
			{
				mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentTypeConstructorPairList);
			}
			lua_pop( ls, 1);						// STK: [PARAMTAB] [KEY]
		}
		lua_pop( ls, 1);							// STK:
		if (type < 0)
		{
			mewa::lua::throwArgumentError( functionName, -1, mewa::Error::ExpectedArgumentTypeConstructorPairList);
		}
		return mewa::TypeDatabase::TypeConstructorPair( type, constructor);
	}
}

std::pmr::vector<mewa::TypeDatabase::TypeConstructorPair>
	mewa::lua::getArgumentAsTypeConstructorPairList( const char* functionName, lua_State* ls, int li, std::pmr::memory_resource* memrsc)
{
	std::pmr::vector<mewa::TypeDatabase::TypeConstructorPair> rt( memrsc);
	if (lua_isnil( ls, li)) return rt;
	checkArgumentAsTable( functionName, ls, li);

	lua_pushvalue( ls, li);
	lua_pushnil( ls);								//STK: [PARAMTAB] NIL
	while (lua_next( ls, -2))							//STK: [PARAMTAB] [KEY] [VAL]
	{
		if (lua_type( ls, -2) == LUA_TNUMBER)
		{
			int kidx = getTableIndex( ls, -2);
			if (!kidx) mewa::lua::throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentTypeConstructorPairList);

			std::size_t index = kidx-1;
			if (index == rt.size())
			{
				rt.emplace_back( getArgumentAsTypeConstructorPair( functionName, ls, -1));	//STK: [PARAMTAB] [KEY] [VAL]
			}
			else
			{
				if (index > rt.size())
				{
					rt.resize( index+1, mewa::TypeDatabase::TypeConstructorPair( -1, -1));
				}
			}
				rt[ index] = getArgumentAsTypeConstructorPair( functionName, ls, -1);		//STK: [PARAMTAB] [KEY] [VAL]
		}
		else
		{
			throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentTypeConstructorPairList);
		}
		lua_pop( ls, 1);											//STK: [PARAMTAB] [KEY]
	}
	lua_pop( ls, 1);												//STK:
	return rt;
}

std::vector<std::string> mewa::lua::getArgumentAsStringList( const char* functionName, lua_State* ls, int li)
{
	std::vector<std::string> rt;
	if (lua_isnil( ls, li)) return rt;
	checkArgumentAsTable( functionName, ls, li);

	lua_pushvalue( ls, li);
	lua_pushnil( ls);		//STK: [STRLIST] NIL
	while (lua_next( ls, -2))	//STK: [STRLIST] [KEY] [VAL]
	{
		if (lua_type( ls, -2) == LUA_TNUMBER && lua_type( ls, -1) == LUA_TSTRING)
		{
			int kidx = getTableIndex( ls, -2);
			if (!kidx) throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentStringList);
			while (kidx > (int)rt.size())
			{
				rt.push_back( std::string());
			}
			rt[ kidx-1] = lua_tostring( ls, -1);
		}
		else
		{
			throwArgumentError( functionName, li, mewa::Error::ExpectedArgumentStringList);
		}
		lua_pop( ls, 1);	//STK: [STRLIST] [KEY]
	}
	lua_pop( ls, 1);		//STK:
	return rt;
}

std::pmr::vector<int> mewa::lua::getArgumentAsTypeList( 
	const char* functionName, lua_State* ls, int li, std::pmr::memory_resource* memrsc, bool allowTypeConstructorPairs)
{
	std::pmr::vector<int> rt( memrsc);
	if (lua_isnil( ls, li)) return rt;
	checkArgumentAsTable( functionName, ls, li);

	lua_pushvalue( ls, li);
	lua_pushnil( ls);		//STK: [PARAMTAB] NIL
	while (lua_next( ls, -2))	//STK: [PARAMTAB] [KEY] [VAL]
	{
		if (lua_type( ls, -2) == LUA_TNUMBER)
		{
			int kidx = getTableIndex( ls, -2);
			if (!kidx) mewa::lua::throwArgumentError(
					functionName, li, allowTypeConstructorPairs
								? mewa::Error::ExpectedArgumentTypeList
								: mewa::Error::ExpectedArgumentTypeOrTypeConstructorPairList);

			int type = -1;
			if (lua_type( ls, -1) == LUA_TTABLE)
			{
				if (allowTypeConstructorPairs)
				{
					lua_pushliteral( ls, "type");					// STK: [KEY] [VAL] "type"
					lua_rawget( ls, -2);						// STK: [KEY] [VAL] [TYPE]
					if (lua_isnumber( ls, -1)) 
					{
						type = lua_tointeger( ls, -1);
					}
					else
					{
						mewa::lua::throwArgumentError(
								functionName, li, allowTypeConstructorPairs
											? mewa::Error::ExpectedArgumentTypeList
											: mewa::Error::ExpectedArgumentTypeOrTypeConstructorPairList);
					}
					lua_pop( ls, 1);						// STK: [KEY] [VAL]
				}
				else
				{
					mewa::lua::throwArgumentError(
						functionName, li, allowTypeConstructorPairs
									? mewa::Error::ExpectedArgumentTypeList
									: mewa::Error::ExpectedArgumentTypeOrTypeConstructorPairList);
				}
			}
			else
			{
				type = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, -1);
			}
			std::size_t index = kidx-1;
			if (index == rt.size())
			{
				rt.push_back( type);
			}
			else
			{
				if (index > rt.size())
				{
					rt.resize( index+1, 0);
				}
				rt[ index] = type;
			}
		}
		else
		{
			mewa::lua::throwArgumentError(
				functionName, li, allowTypeConstructorPairs
							? mewa::Error::ExpectedArgumentTypeList
							: mewa::Error::ExpectedArgumentTypeOrTypeConstructorPairList);
		}
		lua_pop( ls, 1);	//STK: [PARAMTAB] [KEY]
	}
	lua_pop( ls, 1);		//STK:
	return rt;
}

static inline void pushTypeConstructorPair( lua_State* ls, int type, int constructor)
{
	lua_createtable( ls, 0/*narr*/, 2/*nrec*/);	// STK: [TAB]
	lua_pushliteral( ls, "type");			// STK: [TAB] "type"
	lua_pushinteger( ls, type);			// STK: [TAB] "type" [TYPE]
	lua_rawset( ls, -3);				// STK: [TAB] 

	if (constructor > 0)
	{
		lua_pushliteral( ls, "constructor");			// STK: [TAB] "constructor"
		lua_rawgeti( ls, LUA_REGISTRYINDEX, constructor);	// STK: [TAB] "constructor" [CONSTRUCTOR]
		lua_rawset( ls, -3);					// STK: [TAB]
	}
}

/// \note Assume STK: [OBJTAB] [REDUTAB]
static inline void pushTypeConstructorWeightTuple( lua_State* ls, int type, int constructor, float weight, bool countFlag)
{
	pushTypeConstructorPair( ls, type, constructor);
	lua_pushliteral( ls, "weight");			// STK: [OBJTAB] [ELEMTAB] [ELEM] "weight"
	lua_pushnumber( ls, weight);			// STK: [OBJTAB] [ELEMTAB] [ELEM] "weight" [WEIGHT]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]
	lua_pushliteral( ls, "count");			// STK: [OBJTAB] [ELEMTAB] [ELEM] "count"
	lua_pushboolean( ls, countFlag);			// STK: [OBJTAB] [ELEMTAB] [ELEM] "count" [COUNT]
	lua_rawset( ls, -3);				// STK: [OBJTAB] [ELEMTAB] [ELEM]
}

void mewa::lua::pushTypeConstructor(
		lua_State* ls, const char* functionName, int constructor)
{
	checkStack( functionName, ls, 6);
	if (constructor > 0)
	{
		lua_rawgeti( ls, LUA_REGISTRYINDEX, constructor);
	}
	else
	{
		lua_pushnil( ls);
	}
}

void mewa::lua::pushWeightedReductions(
		lua_State* ls, const char* functionName,
		const std::pmr::vector<mewa::TypeDatabase::WeightedReduction>& reductions)
{
	checkStack( functionName, ls, 6);
	lua_createtable( ls, reductions.size()/*narr*/, 0/*nrec*/);			// STK: [REDUTAB]
	int ridx = 0;
	for (auto const& reduction : reductions)
	{
		++ridx;
		pushTypeConstructorWeightTuple( ls, reduction.type, reduction.constructor, reduction.weight, reduction.count);	// STK: [OBJTAB] [REDUTAB] [REDU]
		lua_rawseti( ls, -2, ridx);											// STK: [OBJTAB] [REDUTAB]
	}
}

void mewa::lua::pushTypeList(
		lua_State* ls, const char* functionName, const std::pmr::vector<int>& typelist)
{
	checkStack( functionName, ls, 6);

	lua_createtable( ls, typelist.size()/*narr*/, 0/*nrec*/);		// STK: [TYPETAB] 
	int tidx = 0;
	for (int tp : typelist)
	{
		++tidx;
		lua_pushinteger( ls, tp);					// STK: [TYPETAB] [TYPE]
		lua_rawseti( ls, -2, tidx);					// STK: [TYPETAB]
	}
}

void mewa::lua::pushTypeConstructorPairs(
		lua_State* ls, const char* functionName,
		const mewa::TypeDatabase::TypeConstructorPairList& parameters)
{
	checkStack( functionName, ls, 8);

	lua_createtable( ls, parameters.size()/*narr*/, 0/*nrec*/);			// STK: [PARAMTAB]
	int ridx = 0;
	for (auto const& parameter : parameters)
	{
		++ridx;
		pushTypeConstructorPair( ls, parameter.type, parameter.constructor);	// STK: [PARAMTAB] [PARAM]
		lua_rawseti( ls, -2, ridx);						// STK: [PARAMTAB]
	}
}

static void pushTypeConstructorPairsWithReductionRoot(
		lua_State* ls, const char* functionName,
		int li_reduroot, const mewa::TypeDatabase::TypeConstructorPairList& parameters)
{
	mewa::lua::checkStack( functionName, ls, 8);

	lua_createtable( ls, parameters.size()/*narr*/, 0/*nrec*/);			// STK: [PARAMTAB]
	int ridx = 1;
	lua_pushvalue( ls, li_reduroot < 0 ? (li_reduroot-1) : li_reduroot);		// STK: [PARAMTAB] [REDUROOT]
	lua_rawseti( ls, -2, ridx);							// STK: [PARAMTAB]
	for (auto const& parameter : parameters)
	{
		++ridx;
		pushTypeConstructorPair( ls, parameter.type, parameter.constructor);	// STK: [PARAMTAB] [PARAM]
		lua_rawseti( ls, -2, ridx);						// STK: [PARAMTAB]
	}
}

void mewa::lua::pushReductionDefinition(
		lua_State* ls, const char* functionName,
		const mewa::TypeDatabase::ReductionDefinition& redu)
{
	checkStack( functionName, ls, 6);

	lua_createtable( ls, 0/*narr*/, 5/*nrec*/);					// STK: [REDUTAB] 

	lua_pushliteral( ls, "to");							// STK: [REDUTAB] "to"
	lua_pushinteger( ls, redu.toType);						// STK: [REDUTAB] "to" [TYPE]
	lua_rawset( ls, -3);								// STK: [REDUTAB]

	lua_pushliteral( ls, "from");							// STK: [REDUTAB] "from"
	lua_pushinteger( ls, redu.fromType);						// STK: [REDUTAB] "from" [TYPE]
	lua_rawset( ls, -3);								// STK: [REDUTAB]

	if (redu.constructor > 0)
	{
		lua_pushliteral( ls, "constructor");					// STK: [REDUTAB] "constructor"
		lua_rawgeti( ls, LUA_REGISTRYINDEX, redu.constructor);			// STK: [REDUTAB] "constructor" [CONSTRUCTOR]
		lua_rawset( ls, -3);							// STK: [REDUTAB]
	}

	lua_pushliteral( ls, "tag");							// STK: [REDUTAB] "tag"
	lua_pushinteger( ls, redu.tag);							// STK: [REDUTAB] "tag" [TAG]
	lua_rawset( ls, -3);								// STK: [REDUTAB]

	lua_pushliteral( ls, "weight");							// STK: [REDUTAB] "weight"
	lua_pushnumber( ls, redu.weight);						// STK: [REDUTAB] "weight" [WEIGHT]
	lua_rawset( ls, -3);								// STK: [REDUTAB]
}

void mewa::lua::pushScope( lua_State* ls, const char* functionName, const mewa::Scope& scope)
{
	lua_createtable( ls, 2/*narr*/, 0/*nrec*/);					// STK: [SCOPETAB]
	lua_pushinteger( ls, scope.start());						// STK: [SCOPETAB] [START]
	lua_rawseti( ls, -2, 1);							// STK: [SCOPETAB]
	lua_pushinteger( ls, scope.end());						// STK: [SCOPETAB] [END]
	lua_rawseti( ls, -2, 2);							// STK: [SCOPETAB]
}

int mewa::lua::pushResolveResult(
		lua_State* ls, const char* functionName, int contextTabLuaIndex,
		const mewa::TypeDatabase::ResolveResult& resolveres)
{
	if (resolveres.rootIndex >= 0)
	{
		if (resolveres.conflictType >= 0)
		{
			// Conflict, push alternatives as array:
			lua_createtable( ls, 2/*narr*/, 0/*nrec*/);			// STK: [CONFLICTAR]
			lua_pushinteger( ls, resolveres.contextType);			// STK: [CONFLICTAR] [1ST CTX]
			lua_rawseti( ls, -2, 1);					// STK: [CONFLICTAR]
			lua_pushinteger( ls, resolveres.conflictType);			// STK: [CONFLICTAR] [2ND CTX]
			lua_rawseti( ls, -2, 2);					// STK: [CONFLICTAR]
			return 1;
		}
		else if (contextTabLuaIndex)
		{
			lua_pushinteger( ls, resolveres.contextType);									// STK: [CTYPE]
			lua_rawgeti( ls, contextTabLuaIndex>0?contextTabLuaIndex:(contextTabLuaIndex-1), resolveres.rootIndex); 	// STK: [CTYPE] [ROOT]
			if (lua_istable( ls, -1))
			{
				pushTypeConstructorPairsWithReductionRoot( ls, functionName, -1, resolveres.reductions); 		// STK: [CTYPE] [REDUS]
				lua_replace( ls, -2);
			}
			else
			{
				lua_pop( ls, 1);											// STK: [CTYPE]
				mewa::lua::pushTypeConstructorPairs( ls, functionName, resolveres.reductions);		// STK: [CTYPE] [REDUS]
			}
			mewa::lua::pushTypeList( ls, functionName, resolveres.items);		 					// STK: [CTYPE] [REDUS] [ITEMS]
			return 3;
		}
		else
		{
			lua_pushinteger( ls, resolveres.contextType);
			mewa::lua::pushTypeConstructorPairs( ls, functionName, resolveres.reductions);
			mewa::lua::pushTypeList( ls, functionName, resolveres.items);
			return 3;
		}
	}
	return 0;
}

void mewa::lua::pushStackTrace( lua_State* ls, const char* functionName, int nn, const std::vector<std::string>& ignoreList)
{
	std::map<std::string,std::vector<std::string> > locationMap;
	lua_newtable( ls);								// STK: [STKTR]
	int ridx = 1;
	int resultIndex = 1;
	lua_Debug ar;
	while (resultIndex <= nn && lua_getstack( ls, ridx, &ar))
	{
		bool do_ignore = false;
		lua_getinfo( ls, "S", &ar);
		if (0 == std::strcmp( ar.short_src, "[C]")) break;

		lua_createtable( ls, 0/*narr*/, 3/*nrec*/);				// STK: [STKTR] [ELEM]
		if (ar.linedefined >= 1)
		{
			lua_pushliteral( ls, "line");					// STK: [STKTR] [ELEM] "line"
			lua_pushinteger( ls, ar.linedefined);				// STK: [STKTR] [ELEM] "line" [LINE]
			lua_rawset( ls, -3);						// STK: [STKTR] [ELEM]
		}
		lua_pushliteral( ls, "file");						// STK: [STKTR] [ELEM] "file"
		lua_pushstring( ls, ar.short_src);					// STK: [STKTR] [ELEM] "file" [FILENAME]
		lua_rawset( ls, -3);							// STK: [STKTR] [ELEM]
		auto fi = locationMap.find( ar.short_src);
		if (fi == locationMap.end())
		{
			auto ins = locationMap.insert( {ar.short_src, mewa::readFileLines( ar.short_src)} );
			fi = ins.first;
		}
		if ((int)fi->second.size() >= ar.linedefined)
		{
			char const* ln = fi->second[ ar.linedefined-1].c_str();
			for (; *ln && (unsigned char)*ln <= 32; ++ln){}
			if (0==std::memcmp( ln, "function ", 8)) ln += 8;
			for (; *ln && (unsigned char)*ln <= 32; ++ln){}
			char const* le = std::strchr( ln, '(');
			if (!le) le = std::strchr( ln, '\0');
			std::string funcstr( ln, le-ln);
			for (auto ignore : ignoreList)
			{
				const char* si = std::strstr( funcstr.c_str(), ignore.c_str());
				do_ignore |= (!!si);
			}
			lua_pushliteral( ls, "function");				// STK: [STKTR] [ELEM] "function"
			lua_pushlstring( ls, ln, le-ln);				// STK: [STKTR] [ELEM] "function" [FUNCNAME]
			lua_rawset( ls, -3);						// STK: [STKTR] [ELEM]
		}
		else
		{
			throw mewa::Error( mewa::Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		if (do_ignore)
		{
			lua_pop( ls, 1);
		}
		else
		{
			lua_rawseti( ls, -2, resultIndex);				// STK: [STKTR]
			++resultIndex;
		}
		++ridx;
	}
}
