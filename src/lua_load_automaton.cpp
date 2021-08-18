/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Function to load an automaton from a definition as Lua structure printed by the mewa program
/// \file "lua_load_automaton.cpp"

#include "lua_load_automaton.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include "strings.hpp"
#include "version.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <typeinfo>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

static bool lua_isinteger( lua_State *ls, int li)
{
	int tp = lua_type( ls, li);
	if (tp != LUA_TNUMBER) return false;
	double val = lua_tonumber( ls, li);
	double fl = val - floor( val);
	return (fl < 10*std::numeric_limits<double>::epsilon());
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
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName, rowcnt));
		}
		int packedval = lua_tointeger( ls, -1);
		if (packedval <= 0)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName, rowcnt));
		}
		if (rt.insert( {KEY::unpack(packedkey), VAL::unpack(packedval)}).second == false/*element key already exists*/)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName, rowcnt));
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
		if (!lua_isinteger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		std::size_t vallen;
		const char* valstr = lua_tolstring( ls, -1, &vallen);
		if (!valstr)
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		rt.push_back( std::string( valstr, vallen));
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	return rt;
}

static std::vector<int> parseIntegerArray( lua_State *ls, int li, const std::string& tableName)
{
	std::vector<int> rt;
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (!lua_isinteger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		if (!lua_isinteger( ls, -1))
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, mewa::string_format( "table '%s', row %d", tableName.c_str(), rowcnt));
		}
		int val = lua_tointeger( ls, -1);
		rt.push_back( val);
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
	std::vector<std::string> keywords;
	// ... because keywords need to be loaded after the named token lexems, we need to store them first
	//     and apply the definitions at the end of parsing, because in a lua map they can appear in any order.
	int rowcnt = 0;

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (lua_type( ls, -2) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
						mewa::string_format( "table '%s', row %d", tableName, rowcnt));
		}
		const char* keystr = lua_tostring( ls, -2);
		if (!lua_istable( ls, -1))
		{
			throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
						mewa::string_format( "table '%s/%s', row %d", tableName, keystr, rowcnt));
		}
		if (0==std::strcmp( keystr, "comment"))
		{
			lua_pushvalue( ls, -1);
			lua_pushnil( ls);
			int colcnt = 0;
			while (lua_next( ls, -2))
			{
				++colcnt;
				if (!lua_isnumber( ls, -2) || colcnt != lua_tointeger( ls, -2))
				{
					throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
								mewa::string_format( "table '%s/%s', row %d col %d", tableName, keystr, rowcnt, colcnt));
				}
				if (lua_istable( ls, -1))
				{
					auto arg = parseStringArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
					if (arg.size() > 2 || arg.empty())
					{
						throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
									mewa::string_format( "table '%s/%s', row %d col %d", tableName, keystr, rowcnt, colcnt));
					}
					else if (arg.size() == 1)
					{
						rt.defineEolnComment( arg[ 0]);
					}
					else
					{
						rt.defineBracketComment( arg[0], arg[1]);
					}
				}
				else if (lua_type( ls, -1) == LUA_TSTRING)
				{
					auto arg = lua_tostring( ls, -1);
					rt.defineEolnComment( arg);
				}
				else
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
								mewa::string_format( "table '%s/%s', row %d col %d", tableName, keystr, rowcnt, colcnt));
				}
				lua_pop( ls, 1);
			}
			lua_pop( ls, 1);
		}
		else if (0==std::strcmp( keystr, "keyword"))
		{
			keywords = parseStringArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
		}
		else if (0==std::strcmp( keystr, "token"))
		{
			lua_pushvalue( ls, -1);
			lua_pushnil( ls);
			int colcnt = 0;
			while (lua_next( ls, -2))
			{
				++colcnt;
				if (!lua_isnumber( ls, -2) || colcnt != lua_tointeger( ls, -2))
				{
					throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
								mewa::string_format( "table '%s/%s', row %d col %d", tableName, keystr, rowcnt, colcnt));
				}
				auto arg = parseStringArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
				if (arg.size() > 3 || arg.empty())
				{
					throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
								mewa::string_format( "table '%s/%s', row %d", tableName, keystr, rowcnt));
				}
				else if (arg.size() > 2)
				{
					rt.defineLexem( arg[0], arg[1], string2int( arg[2]));
				}
				else
				{
					rt.defineLexem( arg[0], arg[1]);
				}
				lua_pop( ls, 1);
			}
			lua_pop( ls, 1);
		}
		else if (0==std::strcmp( keystr, "bad"))
		{
			auto badlexems = parseStringArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
			for (auto badlexem : badlexems)
			{
				rt.defineBadLexem( badlexem);
			}
		}
		else if (0==std::strcmp( keystr, "ignore"))
		{
			auto ignores = parseStringArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
			for (auto ignore : ignores)
			{
				rt.defineIgnore( ignore);
			}
		}
		else if (0==std::strcmp( keystr, "indentl"))
		{
			auto indentLexems = parseIntegerArray( ls, -1, mewa::string_format( "%s/%s", tableName, keystr));
			if (indentLexems.size() != 2)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "table '%s/%s'", tableName, keystr));
			}
			rt.setIndentLexems( {indentLexems[0]+0,indentLexems[0]+1,indentLexems[0]+2,indentLexems[1]} );
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	for (auto keyword : keywords)
	{
		rt.defineLexem( keyword);
	}
	return rt;
}

static mewa::Automaton::Call parseCall( lua_State *ls, int li, const std::string& tableName)
{
	std::string function;
	std::string arg;
	mewa::Automaton::Call::ArgumentType argtype = mewa::Automaton::Call::NoArg;
	bool defined[ 3] = {false, false, false};

	lua_pushvalue( ls, li);
	lua_pushnil( ls);

	while (lua_next( ls, -2))
	{
		if (lua_type( ls, -2) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable, mewa::string_format( "table '%s'", tableName.c_str()));
		}
		const char* key = lua_tostring( ls, -2); 

		if (key[0] == 'n' && 0==std::strcmp( key, "name"))
		{
			defined[ 0] = true;
			if (lua_type( ls, -1) != LUA_TSTRING)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "table '%s', key '%s'", tableName.c_str(), key));
			}
			std::size_t vallen;
			const char* valstr = lua_tolstring( ls, -1, &vallen);
			const char* argstr = std::strchr( valstr, ' ');
			if (argstr)
			{

				function.append( valstr, argstr-valstr);
				arg.append( argstr+1);
			}
			else
			{
				function.append( valstr, vallen);
			}
		}
		else if (key[0] == 'p' && 0==std::strcmp( key, "proc"))
		{
			defined[ 1] = true;
			if (lua_type( ls, -1) != LUA_TFUNCTION)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable, mewa::string_format( "table '%s', key '%s'", tableName.c_str(), key));
			}
		}
		else if (key[0] == 'o' && 0==std::strcmp( key, "obj"))
		{
			defined[ 2] = true;
			argtype = (lua_type( ls, -1) == LUA_TSTRING) ? mewa::Automaton::Call::StringArg : mewa::Automaton::Call::ReferenceArg;
		}
		else
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
						mewa::string_format( "table '%s', key '%s'", tableName.c_str(), key));
		}
		lua_pop( ls, 1);
	}
	if (!defined[0] || !defined[1])
	{
		throw mewa::Error( mewa::Error::UnresolvableFunctionInLuaCallTable, function);
	}
	else if (!arg.empty() && !defined[2])
	{
		throw mewa::Error( mewa::Error::UnresolvableFunctionArgInLuaCallTable, arg);
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
		if (!lua_isnumber( ls, -2) || rowcnt != lua_tointeger( ls, -2))
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
						mewa::string_format( "table '%s', row %d", tableName.c_str(), rowcnt));
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

static int parseVersion( char const* vstr)
{
	char const* vi = vstr;
	int rt = 0;
	for (; *vi >= '0' && *vi <= '9'; ++vi)
	{
		rt = rt * 10 + (*vi - '0');
		if (rt > 100) throw mewa::Error( mewa::Error::BadMewaVersion, vstr);
	}
	rt = rt * 100;
	if (*vi != '.') throw mewa::Error( mewa::Error::BadMewaVersion, vstr);
	int minor = 0;
	for (++vi; *vi >= '0' && *vi <= '9'; ++vi)
	{
		minor = minor * 10 + (*vi - '0');
		if (minor > 100) throw mewa::Error( mewa::Error::BadMewaVersion, vstr);
	}
	if (*vi != '.' && *vi != '\0') throw mewa::Error( mewa::Error::BadMewaVersion, vstr);
	rt += minor;
	if (rt == 0) throw mewa::Error( mewa::Error::BadMewaVersion, vstr);
	return rt;
}

mewa::Automaton mewa::luaLoadAutomaton( lua_State *ls, int li)
{
	lua_pushvalue( ls, li);
	lua_pushnil( ls);
	int rowcnt = 0;

	int version = 0;
	std::string language;
	std::string typesystem;
	std::string cmdline;
	mewa::Lexer lexer;
	std::map<mewa::Automaton::ActionKey,mewa::Automaton::Action> actions;
	std::map<mewa::Automaton::GotoKey,mewa::Automaton::Goto> gotos;
	std::vector<mewa::Automaton::Call> calls;
	std::vector<std::string> nonterminals;

	while (lua_next( ls, -2))
	{
		++rowcnt;
		if (lua_type( ls, -2) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadKeyInGeneratedLuaTable,
						mewa::string_format( "automaton definition, row %d", rowcnt));
		}
		const char* keystr = lua_tostring( ls, -2);
		if (0==std::strcmp( keystr, "mewa"))
		{
			if (lua_type( ls, -1) != LUA_TSTRING) throw Error( Error::BadMewaVersion, "not a string");
			version = parseVersion( lua_tostring( ls, -1));
			if (version >= ((MEWA_MAJOR_VERSION+1) * 100))
			{
				throw mewa::Error( mewa::Error::IncompatibleMewaMajorVersion, parseVersion( lua_tostring( ls, -1)));
			}
		}
		else if (0==std::strcmp( keystr, "language"))
		{
			if (lua_type( ls, -1) != LUA_TSTRING)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			language = lua_tostring( ls, -1);
		}
		else if (0==std::strcmp( keystr, "typesystem"))
		{
			if (lua_type( ls, -1) != LUA_TSTRING)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			typesystem = lua_tostring( ls, -1);
		}
		else if (0==std::strcmp( keystr, "cmdline"))
		{
			if (lua_type( ls, -1) != LUA_TSTRING)
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			cmdline = lua_tostring( ls, -1);
		}
		else if (0==std::strcmp( keystr, "lexer"))
		{
			if (!lua_istable( ls, -1))
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			lexer = parseLexerDefinitions( ls, -1, keystr);
		}
		else if (0==std::strcmp( keystr, "nonterminal"))
		{
			if (!lua_istable( ls, -1))
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			nonterminals = parseStringArray( ls, -1, keystr);
		}
		else if (0==std::strcmp( keystr, "action"))
		{
			if (!lua_istable( ls, -1))
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			actions = parsePackedTable<mewa::Automaton::ActionKey,mewa::Automaton::Action>( ls, -1, keystr);
		}
		else if (0==std::strcmp( keystr, "gto"))
		{
			if (!lua_istable( ls, -1))
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			gotos = parsePackedTable<mewa::Automaton::GotoKey,mewa::Automaton::Goto>( ls, -1, keystr);
		}
		else if (0==std::strcmp( keystr, "call"))
		{
			if (!lua_istable( ls, -1))
			{
				throw mewa::Error( mewa::Error::BadValueInGeneratedLuaTable,
							mewa::string_format( "automaton definition '%s', row %d", keystr, rowcnt));
			}
			calls = parseCallList( ls, -1, keystr);
		}
		lua_pop( ls, 1);
	}
	lua_pop( ls, 1);
	if (version == 0)
	{
		throw mewa::Error( mewa::Error::MissingMewaVersion);
	}
	return mewa::Automaton( version, language, typesystem, cmdline, lexer, actions, gotos, calls, nonterminals);
}

