/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Parsing of the arguments and creating the return values for Lua functions
/// \file "lua_parameter.hpp"
#ifndef _MEWA_LUA_PARAMETER_HPP_INCLUDED
#define _MEWA_LUA_PARAMETER_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "lua_userdata.hpp"
#include "scope.hpp"
#include "typedb.hpp"
#include "error.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <memory_resource>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mewa {
namespace lua {

int checkNofArguments( const char* functionName, lua_State* ls, int minNofArgs, int maxNofArgs);

[[noreturn]] void throwArgumentError( const char* functionName, int li, mewa::Error::Code ec);

inline bool isArgumentType( const char* functionName, lua_State* ls, int li, int luaTypeMask)
{
	return (((1U << lua_type( ls, li)) & luaTypeMask) != 0);
}

std::string_view getArgumentAsString( const char* functionName, lua_State* ls, int li);
const char* getArgumentAsCString( const char* functionName, lua_State* ls, int li);

long getArgumentAsInteger( const char* functionName, lua_State* ls, int li, mewa::Error::Code ec = mewa::Error::ExpectedIntegerArgument);

long getArgumentAsCardinal( const char* functionName, lua_State* ls, int li);

long getArgumentAsNonNegativeInteger( const char* functionName, lua_State* ls, int li);

float getArgumentAsFloatingPoint( const char* functionName, lua_State* ls, int li);

void checkArgumentAsTable( const char* functionName, lua_State* ls, int li);

mewa::Scope getArgumentAsScope( const char* functionName, lua_State* ls, int li);

mewa::TagMask getArgumentAsTagMask( const char* functionName, lua_State* ls, int li);

int getArgumentAsConstructor( const char* functionName, lua_State* ls, int li, int objtable, mewa_typedb_userdata_t* td);

std::pmr::vector<mewa::TypeDatabase::TypeConstructorPair> getArgumentAsTypeConstructorPairList( 
	const char* functionName, lua_State* ls, int li, int objtable,
	mewa_typedb_userdata_t* td, std::pmr::memory_resource* memrsc);

std::pmr::vector<int> getArgumentAsTypeList( 
	const char* functionName, lua_State* ls, int li, std::pmr::memory_resource* memrsc, bool allowTypeConstructorPairs);

std::vector<std::string> getArgumentAsStringList( const char* functionName, lua_State* ls, int li);

inline void checkStack( const char* functionName, lua_State* ls, int sz)
{
	if (!lua_checkstack( ls, sz)) throw mewa::Error( mewa::Error::LuaStackOutOfMemory, functionName);
}

void pushTypeConstructor(
		lua_State* ls, const char* functionName, const char* objTableName, int constructor);
void pushWeightedReductions(
		lua_State* ls, const char* functionName, const char* objTableName,
		const std::pmr::vector<mewa::TypeDatabase::WeightedReduction>& reductions);
void pushTypePath(
		lua_State* ls, const char* functionName, const std::pmr::vector<int>& typepath);
void pushResolveResultItems(
		lua_State* ls, const char* functionName, const char* objTableName,
		const std::pmr::vector<mewa::TypeDatabase::ResolveResultItem>& items);
void pushTypeConstructorPairs(
		lua_State* ls, const char* functionName, const char* objTableName,
		const mewa::TypeDatabase::TypeConstructorPairList& list);
void pushReductionDefinition(
		lua_State* ls, const char* functionName, const char* objTableName,
		const mewa::TypeDatabase::ReductionDefinition& redu);
void pushScope( lua_State* ls, const char* functionName, const mewa::Scope& scope);
int pushResolveResult(
		lua_State* ls, const char* functionName, int rootContextArgumentLuaIndex, const char* objTableName,
		const mewa::TypeDatabase::ResolveResult& resolveres);

void pushStackTrace( lua_State* ls, const char* functionName, int nn, const std::vector<std::string>& ignoreList);

}} //namespace
#else
#error Building mewa requires C++17
#endif
#endif

