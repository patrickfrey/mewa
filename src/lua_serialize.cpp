/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Serialization of lua data structures
/// \file "lua_serialize.hpp"
#include "lua_serialize.hpp"
#include <vector>
#include <iostream>
#include <sstream>
#include <variant>
#include <limits>
#include <cmath>
#include <algorithm>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

using namespace mewa;

static void serializeLuaValue( std::ostream& out, lua_State* ls, int li, const std::string& indent);

static void serializeLuaNumber( std::ostream& out, lua_State* ls, int li)
{
	double num;
	num = lua_tonumber( ls, li);
	if (num - std::floor( num) < std::numeric_limits<double>::epsilon()*4)
	{
		out << lua_tointeger( ls, li);
	}
	else
	{
		out << num;
	}
}

struct LuaTableKey
{
	enum Type {NumberType,StringType};
	Type type;
	std::variant<double,const char*> value;

	LuaTableKey( const char* value_)
		:type(StringType),value(value_){}
	LuaTableKey( const double value_)
		:type(NumberType),value(value_){}
	LuaTableKey( const LuaTableKey& o)
		:type(o.type),value(o.value){}

	bool operator < (const LuaTableKey& o) const
	{
		if (type == o.type)
		{
			switch (type)
			{
				case NumberType: return std::get<double>(value) < std::get<double>(o.value);
				case StringType: return std::strcmp( std::get<const char*>(value), std::get<const char*>(o.value)) < 0;
			}
		}
		return (int)type < (int)o.type;
	}

	void push_luastack( lua_State* ls) const noexcept
	{
		switch (type)
		{
			case NumberType:
				lua_pushnumber( ls, std::get<double>(value));
				break;
			case StringType:
				lua_pushstring( ls, std::get<const char*>(value));
				break;
		}
	}

	void serialize( std::ostream& out) const
	{
		switch (type)
		{
			case NumberType:
			{
				double num = std::get<double>(value);
				if (num - std::floor( num) < std::numeric_limits<double>::epsilon()*4)
				{
					out << "[" << (int)(num + std::numeric_limits<double>::epsilon()*4) << "]";
				}
				else
				{
					out << "[" << std::get<double>(value) << "]";
				}
				break;
			}
			case StringType:
				out << std::get<const char*>(value);
				break;
		}
	}
};

static void serializeLuaTable( std::ostream& out, lua_State* ls, int li, const std::string& indent)
{
	std::vector<LuaTableKey> keyvec;
	keyvec.reserve( 1024);
	bool hasNameKeys = false;
	int rowcnt = 0;

	// Get keys in ascending order:
	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	while (lua_next( ls, -2))
	{
		if (lua_type( ls, -2) == LUA_TSTRING)
		{
			const char* sptr = lua_tostring( ls, -2);
			keyvec.push_back( sptr);
			hasNameKeys = true;
		}
		else if (lua_type( ls, -2) == LUA_TNUMBER)
		{
			double kk = lua_tonumber( ls, -2);
			++rowcnt;
			if (!hasNameKeys && (kk - std::floor( kk) > std::numeric_limits<double>::epsilon()*4))
			{
				hasNameKeys = true;
			}
			keyvec.push_back( kk);
		}
		lua_pop( ls, 1);
	}
 	std::sort( keyvec.begin(), keyvec.end());
	if (!hasNameKeys)
	{
		if ((int)keyvec.size() != rowcnt)
		{
			hasNameKeys = true;
		}
		else if (rowcnt
			&& (int)(std::get<double>(keyvec[0].value) + std::numeric_limits<double>::epsilon()*4) != 1
			&& (int)(std::get<double>(keyvec.back().value) + std::numeric_limits<double>::epsilon()*4) != rowcnt)
		{
			hasNameKeys = true;
		}
	}
	out << "{";
	if (hasNameKeys)
	{
		int kidx = 0;
		for (auto const& key : keyvec)
		{
			key.push_luastack( ls);					// STK: [TABLE] [KEY]
			lua_gettable( ls, -2);					// STK: [TABLE] [VALUE]

			if (kidx++) out << ",";
			out << indent;
			key.serialize( out);
			out << " = ";
			serializeLuaValue( out, ls, -1, indent);

			lua_pop( ls, 1);					// STK: [TABLE] 
		}
	}
	else
	{
		int kidx = 0;
		for (auto const& key : keyvec)
		{
			key.push_luastack( ls);					// STK: [TABLE] [KEY]
			lua_gettable( ls, -2);					// STK: [TABLE] [VALUE]

			if (kidx++) out << ",";
			out << indent;
			serializeLuaValue( out, ls, -1, indent);

			lua_pop( ls, 1);					// STK: [TABLE] 
		}
	}
	out << "}";
	lua_pop( ls, 1); // ... pop table and keyvec
}

static void serializeLuaValue( std::ostream& out, lua_State* ls, int li, const std::string& indent)
{
	int tp = lua_type( ls, li);
	switch (tp)
	{
		case LUA_TNIL:
			out << "nil";
			break;
		case LUA_TNUMBER:
			serializeLuaNumber( out, ls, li);
			break;
		case LUA_TBOOLEAN:
			out << (lua_toboolean( ls, li) ? "true" : "false");
			break;
		case LUA_TSTRING:
			out << '"' << lua_tostring( ls, li) << '"';
			break;
		case LUA_TTABLE:
			serializeLuaTable( out, ls, li, indent + "\t");
			break;
		case LUA_TFUNCTION:
		case LUA_TUSERDATA:
		case LUA_TTHREAD:
		case LUA_TLIGHTUSERDATA:
			out << "<" << lua_typename( ls, tp) << ">";
			break;
	}
}

std::string mewa::luaToString( lua_State* ls, int li)
{
	std::string rt;
	std::ostringstream out;
	serializeLuaValue( out, ls, li, "\n");
	rt.append( out.str());
	return rt;
}

