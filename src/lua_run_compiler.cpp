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
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <typeinfo>
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

static void luaPushLexem( lua_State* ls, const mewa::Lexem& lexem)
{
	lua_createtable( ls, 0/*size array*/, 2/*size struct*/);
	lua_pushstring( ls, "name");
	lua_pushlstring( ls, lexem.name().data(), lexem.name().size());
	lua_settable( ls, -3);
	lua_pushstring( ls, "value");
	lua_pushlstring( ls, lexem.value().data(), lexem.value().size());
	lua_settable( ls, -3);
}

struct State
{
	int index;
	int luastki;

	State()
		:index(0),luastki(-1){}
	State( int index_, int luastki_=-1)
		:index(index_),luastki(luastki_){}
	State( const State& o)
		:index(o.index),luastki(o.luastki){}
};

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
				int last_luastki = stateStack.empty() ? 0 : stateStack.back().luastki;
				stateStack.push_back( State( nexti->second.state(), last_luastki+1));
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
			stateStack.resize( stateStack.size() - nexti->second.count());
			auto gtoi = automaton.gotos().find( {stateStack.back().index, nexti->second.nonterminal()});
			if (gtoi == automaton.gotos().end())
			{
				throw mewa::Error( mewa::Error::LanguageAutomatonMissingGoto,
							stateTransitionInfo( stateStack.back().index, lexem.id()/*terminal*/, automaton.lexer()), lexem.line());
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

	mewa::Scanner scanner( source);
	std::pmr::vector<State> stateStack( &memrsc );	// ... use pmr to avoid memory leak on lua error exit
	stateStack.reserve( (sizeof buffer - sizeof stateStack) / sizeof(State));
	stateStack.push_back( {1} );

	mewa::Lexem lexem = automaton.lexer().next( scanner);
	for (; !lexem.empty(); lexem = automaton.lexer().next( scanner))
	{
		if (lexem.id() <= 0) throw mewa::Error( mewa::Error::BadCharacterInGrammarDef, lexem.value());
		do
		{
			if (dbgout) printDebugAction( *dbgout, stateStack, automaton, lexem);
		}
		while (!feedLexem( ls, stateStack, automaton, lexem));
	}
	while (!feedLexem( ls, stateStack, automaton, lexem/* empty ~ end of input*/))
	{
		if (dbgout) printDebugAction( *dbgout, stateStack, automaton, lexem);
	}
}
