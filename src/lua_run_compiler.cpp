/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Function to load an automaton from a definition as Lua structure printed by the mewa program ("mewa -g EBNF")
	/// \file "lua_run_compiler.cpp"

#include "lua_run_compiler.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include "strings.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <typeinfo>
#include <limits>
#include <memory_resource>
#include <lua.h>
#include <lauxlib.h>

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

static std::string getLexemName( const mewa::Lexer& lexer, int terminal)
{
        return terminal ? lexer.lexemName( terminal) : std::string("$");
}

static std::string expectedTerminalList( const std::map<mewa::Automaton::ActionKey,mewa::Automaton::Action>& actions, int state, const mewa::Lexer& lexer)
{
	std::string rt;
	auto ai = actions.lower_bound({ state, 0 });
	int aidx = 0;
	for (; ai != actions.end() && ai->first.state() == state; ++ai,++aidx)
	{
		if (aidx) rt.append(", ");
		rt.append( getLexemName( lexer, ai->first.terminal()));
	}
	return rt;
}

static std::string stateTransitionInfo( int state, int terminal, const mewa::Lexer& lexer)
{
	char buf[ 64];
	std::snprintf( buf, sizeof(buf), "state %d, token ", state);
	std::string rt( buf);
	rt.append( getLexemName( lexer, terminal));
	return rt;
}

static std::string tokenString( const mewa::Lexem& lexem, const mewa::Lexer& lexer)
{
	std::string rt;
	if (lexem.id() == 0)
	{
		rt.append( "$");
	}
	else
	{
		rt.append( lexer.lexemName( lexem.id()));
		if (!lexer.isKeyword( lexem.id()))
		{
			rt.append( " = \"");
			rt.append( lexem.value());
			rt.append( "\"");
		}
	}
	return rt;
}

struct State
{
	int index;
	int luastki;
	int luastkn;

	State()
		:index(0),luastki(0),luastkn(0){}
	State( int index_)
		:index(index_),luastki(0),luastkn(0){}
	State( int index_, int luastki_, int luastkn_)
		:index(index_),luastki(luastki_),luastkn(luastkn_){}
	State( const State& o)
		:index(o.index),luastki(o.luastki),luastkn(o.luastkn){}
};

static void luaPushLexem( lua_State* ls, const mewa::Lexem& lexem)
{
	lua_createtable( ls, 0/*size array*/, 2/*size struct*/);				// STK [TABLE]
	lua_pushlstring( ls, lexem.name().data(), lexem.name().size());				// STK [TABLE] "arg" [NAME]
	lua_pushstring( ls, "name");								// STK [TABLE] "name"
	lua_rawset( ls, -3);									// STK [TABLE]
	lua_pushstring( ls, "value");								// STK [TABLE] "value"
	lua_pushlstring( ls, lexem.value().data(), lexem.value().size());			// STK [TABLE] "value" [VALUE]
	lua_rawset( ls, -3);									// STK [TABLE]
}

static void luaReduceStruct( lua_State* ls, std::pmr::vector<State>& stateStack, int nn, const std::string_view& name, int callidx)
{
	int stk_start = 0;
	int stk_end = 0;
	if (nn > 0)
	{
		std::size_t si = stateStack.size(), se = stateStack.size() - nn;
		for (; si > se; --si)
		{
			const State& st = stateStack[ si-1];
			if (st.luastki)
			{
				stk_end = st.luastki + st.luastkn;
				break;
			}
		}
		for (; si > se; --si)
		{
			const State& st = stateStack[ si-1];
			if (st.luastki)
			{
				stk_start = st.luastki;
			}
		}
												// STK [ARG1]...[ARGN]
		lua_createtable( ls, 0/*size array*/, 2/*size struct*/); 			// STK [ARG1]...[ARGN] [TABLE]
		lua_pushstring( ls, "name");							// STK [ARG1]...[ARGN] [TABLE] "name"
		lua_pushlstring( ls, name.data(), name.size());					// STK [ARG1]...[ARGN] [TABLE] [NAME]
		lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]
		lua_pushstring( ls, "arg");							// STK [ARG1]...[ARGN] [TABLE] "arg"
		lua_createtable( ls, stk_end-stk_start/*size array*/, 0/*size struct*/);	// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB]
		int li = -(stk_end-stk_start)-3, le = -3;
		for (int tidx=0; li != le; ++li,++tidx)
		{
			lua_pushvalue( ls, li);							// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB] [ARGi]
			lua_rawseti( ls, -4, tidx);						// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB]
		}
		lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]
		lua_replace( ls, -1-nn);							// STK [TABLE] [ARG2]...[ARGN]
		if (nn > 1) lua_pop( ls, nn-1);							// STK [TABLE]
	}
}

static void adjustStateCountersOnStack( std::pmr::vector<State>& stateStack)
{
	int ofs = 0;
	for (auto elem : stateStack)
	{
		if (elem.luastki) {ofs = elem.luastki-1; break;}
	}
	if (!ofs) throw mewa::Error( mewa::Error::CompiledSourceTooComplex);
	for (auto elem : stateStack)
	{
		if (elem.luastki) {elem.luastki -= ofs;}
	}
}

static int getNextLuaStackIndex( std::pmr::vector<State>& stateStack)
{
	int rt = 1;
	for (auto si = stateStack.rbegin(), se = stateStack.rend(); si != se; ++si)
	{
		if (si->luastkn)
		{
			rt = si->luastki + si->luastkn;
			if (rt >= std::numeric_limits<int>::max())
			{
				adjustStateCountersOnStack( stateStack);
				rt = getNextLuaStackIndex( stateStack);
			}
			break;
		}
	}
	return rt;
}

static int getLuaStackReductionSize( const std::pmr::vector<State>& stateStack, int nn) noexcept
{
	int start = 0;
	int end = 0;
	for (auto si = stateStack.end() - nn; si != stateStack.end(); ++si)
	{
		if (si->luastki) {start = si->luastki; break;}
	}
	for (auto si = stateStack.rbegin(), se = stateStack.rbegin() + nn; si != se; ++si)
	{
		if (si->luastkn) {end = si->luastki + si->luastkn; break;}
	}
	return end-start;
}

#ifdef MEWA_LOWLEVEL_DEBUG
static int countLuaStackElements( const std::pmr::vector<State>& stateStack)
{
	return getLuaStackReductionSize( stateStack, stateStack.size());
}

static void dumpStateStack( std::ostream& out, const std::pmr::vector<State>& stateStack, const char* title)
{
	std::cerr << "Stack " << title << ":" << std::endl;
	for (auto elem : stateStack)
	{
		std::cerr << "\tState " << elem.index << "[" << elem.luastki << " :" << elem.luastkn << "]" << std::endl;
	}
}
#endif

/// \brief Feed lexem to the automaton state
/// \return true, if the lexem has been consumed, false if we have to feed the same lexem again
static bool feedLexem( lua_State* ls, std::pmr::vector<State>& stateStack, const mewa::Automaton& automaton, const mewa::Lexem& lexem)
{
	auto nexti = automaton.actions().find( {stateStack.back().index, lexem.id()/*terminal*/});
	if (nexti == automaton.actions().end())
	{
		throw mewa::Error( mewa::Error::UnexpectedTokenNotOneOf, tokenString( lexem, automaton.lexer()) + " { "
				   + expectedTerminalList( automaton.actions(), stateStack.back().index, automaton.lexer()) + " }", lexem.line());
	}
	switch (nexti->second.type())
	{
		case mewa::Automaton::Action::Shift:
			if (!automaton.lexer().isKeyword( lexem.id()))
			{
				int next_luastki = getNextLuaStackIndex( stateStack);
				stateStack.push_back( State( nexti->second.state(), next_luastki, 1));
				luaPushLexem( ls, lexem);
			}
			else
			{
				stateStack.push_back( State( nexti->second.state()));
			}
			return true;

		case mewa::Automaton::Action::Reduce:
		{
			if ((int)stateStack.size() <= nexti->second.count() || nexti->second.count() < 0)
			{
				throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
			}
			int luaStackNofElements = getLuaStackReductionSize( stateStack, nexti->second.count());
			if (nexti->second.call())
			{
				int callidx = nexti->second.call();
				auto const& call = automaton.call( callidx);
				luaReduceStruct( ls, stateStack, luaStackNofElements, call.arg().size() ? call.arg() : call.function(), callidx);
				luaStackNofElements = 1;
			}
			stateStack.resize( stateStack.size() - nexti->second.count());

			auto gtoi = automaton.gotos().find( {stateStack.back().index, nexti->second.nonterminal()});
			if (gtoi == automaton.gotos().end())
			{
				throw mewa::Error( mewa::Error::LanguageAutomatonMissingGoto,
							stateTransitionInfo( stateStack.back().index, lexem.id()/*terminal*/, automaton.lexer()), lexem.line());
			}
			else if (luaStackNofElements)
			{
				int next_luastki = getNextLuaStackIndex( stateStack);
				stateStack.push_back( State( gtoi->second.state(), next_luastki, luaStackNofElements));
			}
			else
			{
				stateStack.push_back( State( gtoi->second.state()));
			}
			return false;
		}
		case mewa::Automaton::Action::Accept:
			if (lexem.id()/*terminal*/ != 0)
			{
				throw mewa::Error( mewa::Error::LanguageAutomatonUnexpectedAccept, lexem.line());
			}
			return true;
	}
	return false;
}

static void printDebugAction( std::ostream& dbgout, std::pmr::vector<State>& stateStack, const mewa::Automaton& automaton, const mewa::Lexem& lexem)
{
	auto nexti = automaton.actions().find( {stateStack.back().index,lexem.id()/*terminal*/});
	if (nexti == automaton.actions().end())
	{
		dbgout << "Error token " << getLexemName( automaton.lexer(), lexem.id());
		if (!automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
		dbgout << " at line " << lexem.line() << " in state " << stateStack.back().index << std::endl;
	}
	else
	{
		switch (nexti->second.type())
		{
			case mewa::Automaton::Action::Shift:
				dbgout << "Shift token " << getLexemName( automaton.lexer(), lexem.id());
				if (!automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
				dbgout << " at line " << lexem.line() << " in state " << stateStack.back().index << ", goto " << nexti->second.state() << std::endl;
				break;
			case mewa::Automaton::Action::Reduce:
			{
				dbgout << "Reduce #" << nexti->second.count() << " to " << automaton.nonterminal( nexti->second.nonterminal())
					<< " by token " << getLexemName( automaton.lexer(), lexem.id());
				if (lexem.id() && !automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
				dbgout << " at line " << lexem.line() << " in state " << stateStack.back().index;
				if (nexti->second.call())
				{
					dbgout << ", call " << automaton.call( nexti->second.call()).tostring();
				}
				if ((int)stateStack.size() <= nexti->second.count() || nexti->second.count() < 0)
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
				}
				int newstateidx = stateStack[ stateStack.size() - nexti->second.count() -1].index;
				auto gtoi = automaton.gotos().find( {newstateidx, nexti->second.nonterminal()});
				if (gtoi != automaton.gotos().end())
				{
					dbgout << ", goto " << gtoi->second.state();
				}
				dbgout << std::endl;
				break;
			}
			case mewa::Automaton::Action::Accept:
				dbgout << "Accept" << std::endl;
				break;
		}
	}
}

void mewa::luaRunCompiler( lua_State* ls, const mewa::Automaton& automaton, const std::string_view& source, std::ostream* dbgout)
{
	int buffer[ 2048];
	std::pmr::monotonic_buffer_resource memrsc( buffer, sizeof buffer);
	int nofLuaStackElements = lua_gettop( ls);

	Scanner scanner( source);
	std::pmr::vector<State> stateStack( &memrsc );	// ... use pmr to avoid memory leak on lua error exit
	stateStack.reserve( (sizeof buffer - sizeof stateStack) / sizeof(State));
	stateStack.push_back( {1} );

	Lexem lexem = automaton.lexer().next( scanner);
	for (; !lexem.empty(); lexem = automaton.lexer().next( scanner))
	{
		if (lexem.id() <= 0) throw Error( Error::BadCharacterInGrammarDef, lexem.value());
		do
		{
			if (dbgout) printDebugAction( *dbgout, stateStack, automaton, lexem);
		}
		while (!feedLexem( ls, stateStack, automaton, lexem));

		// Runtime check of stack indices:
		if (!stateStack.empty() && stateStack.back().luastkn
			&& (lua_gettop( ls) - nofLuaStackElements + 1) != stateStack.back().luastki + stateStack.back().luastkn)
		{
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
	}
	while (!feedLexem( ls, stateStack, automaton, lexem/* empty ~ end of input*/))
	{
		if (dbgout) printDebugAction( *dbgout, stateStack, automaton, lexem);
	}
}

