/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Function to load an automaton from a definition as Lua structure printed by the mewa program
/// \file "lua_run_compiler.cpp"

#include "lua_run_compiler.hpp"
#include "lua_serialize.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include "strings.hpp"
#include "memory_resource.hpp"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include <utility>
#include <typeinfo>
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
	int index;			//< Compiler automaton state index
	int luastki;			//< Index of first element on the Lua stack
	int luastkn;			//< Number of elements on the Lua stack
	int scopecnt;			//< Scope counter on creation

	State()
		:index(0),luastki(0),luastkn(0),scopecnt(0){}
	State( int index_, int luastki_, int luastkn_, int scopecnt_)
		:index(index_),luastki(luastki_),luastkn(luastkn_),scopecnt(scopecnt_){}
	State( const State& o)
		:index(o.index),luastki(o.luastki),luastkn(o.luastkn),scopecnt(o.scopecnt){}
};

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

#undef MEWA_LOWLEVEL_DEBUG
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


static void luaPushLexem( lua_State* ls, const mewa::Lexem& lexem)
{
	lua_createtable( ls, 0/*size array*/, 3/*size struct*/);				// STK [TABLE]
	lua_pushliteral( ls, "name");								// STK [TABLE] "name"
	lua_pushlstring( ls, lexem.name().data(), lexem.name().size());				// STK [TABLE] "arg" [NAME]
	lua_rawset( ls, -3);									// STK [TABLE]
	lua_pushliteral( ls, "value");								// STK [TABLE] "value"
	lua_pushlstring( ls, lexem.value().data(), lexem.value().size());			// STK [TABLE] "value" [VALUE]
	lua_rawset( ls, -3);									// STK [TABLE]
	lua_pushliteral( ls, "line");								// STK [TABLE] "line"
	lua_pushinteger( ls, lexem.line());							// STK [TABLE] "line" [LINE]
	lua_rawset( ls, -3);									// STK [TABLE]
}

struct CompilerContext
{
	std::pmr::vector<State> stateStack;		//< Compiler automaton state stack
	int calltable;					//< Lua stack address of call table
	int calltablesize;				//< Number of elements in the call table
	int scopestep;					//< Counter for step and scope structure
	std::ostream* dbgout;				//< Debug output or NULL if undefined

	CompilerContext( std::pmr::memory_resource* memrsc, std::size_t buffersize, int calltable_, int calltablesize_, std::ostream* dbgout_)
		:stateStack(memrsc),calltable(calltable_),calltablesize(calltablesize_),scopestep(0),dbgout(dbgout_)
	{
		stateStack.reserve( (buffersize - sizeof stateStack) / sizeof(State));
		stateStack.push_back( State( 1/*index*/, 0/*luastki*/, 0/*luastkn*/, 0/*scopecnt*/) );
	}
};

static void luaReduceStruct( 
		lua_State* ls, CompilerContext& ctx, int reductionSize,
		int callidx, const mewa::Automaton::Action::ScopeFlag scopeflag, int scopeStart)
{
	int stk_start = 0;
	int stk_end = 0;
	if (reductionSize > 0)
	{
		std::size_t si = ctx.stateStack.size(), se = ctx.stateStack.size() - reductionSize;
		for (; si > se; --si)
		{
			const State& st = ctx.stateStack[ si-1];
			if (st.luastki)
			{
				stk_end = st.luastki + st.luastkn;
				break;
			}
		}
		for (; si > se; --si)
		{
			const State& st = ctx.stateStack[ si-1];
			if (st.luastki)
			{
				stk_start = st.luastki;
			}
		}
		int structsize = scopeflag == mewa::Automaton::Action::NoScope ? 2:3;
												// STK [ARG1]...[ARGN]
		lua_createtable( ls, 0/*size array*/, structsize);	 			// STK [ARG1]...[ARGN] [TABLE]

		lua_pushliteral( ls, "call");							// STK [ARG1]...[ARGN] [TABLE] "call"
 		lua_rawgeti( ls, ctx.calltable, callidx);					// STK [ARG1]...[ARGN] [TABLE] [CALLSTRUCT]
		lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]

		switch (scopeflag)
		{
			case mewa::Automaton::Action::NoScope:
				break;
			case mewa::Automaton::Action::Step:
				ctx.scopestep += 1;
				lua_pushliteral( ls, "step");					// STK [ARG1]...[ARGN] [TABLE] "step"
				lua_pushinteger( ls, ctx.scopestep);				// STK [ARG1]...[ARGN] [TABLE] [COUNTER]
				lua_rawset( ls, -3);						// STK [ARG1]...[ARGN] [TABLE]
				break;
			case mewa::Automaton::Action::Scope:
			{
				ctx.scopestep += 1;
				lua_pushliteral( ls, "scope");					// STK [ARG1]...[ARGN] [TABLE] "scope"
				lua_createtable( ls, 0/*size array*/, structsize);	 	// STK [ARG1]...[ARGN] [TABLE] "scope" [TABLE]
				lua_pushinteger( ls, scopeStart);				// STK [ARG1]...[ARGN] [TABLE] "scope" [TABLE] [SCOPESTART]
				lua_rawseti( ls, -2, 1);					// STK [ARG1]...[ARGN] [TABLE] "scope" [TABLE]
				lua_pushinteger( ls, ctx.scopestep /*scope end*/);		// STK [ARG1]...[ARGN] [TABLE] "scope" [TABLE] [SCOPEEND]
				lua_rawseti( ls, -2, 2);					// STK [ARG1]...[ARGN] [TABLE] "scope" [TABLE]
				lua_rawset( ls, -3);						// STK [ARG1]...[ARGN] [TABLE]
				break;
			}
		}
		int nofLuaStackElements = stk_end-stk_start;
		lua_pushliteral( ls, "arg");							// STK [ARG1]...[ARGN] [TABLE] "arg"
		lua_createtable( ls, nofLuaStackElements/*size array*/, 0/*size struct*/);	// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB]
		int li = -nofLuaStackElements-3, le = -3;
		for (int tidx=1; li != le; ++li,++tidx)
		{
			lua_pushvalue( ls, li);							// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB] [ARGi]
			lua_rawseti( ls, -2, tidx);						// STK [ARG1]...[ARGN] [TABLE] "arg" [ARGTAB]
		}
		lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]
		lua_replace( ls, -1-nofLuaStackElements);					// STK [TABLE] [ARG2]...[ARGN]
		if (nofLuaStackElements > 1) lua_pop( ls, nofLuaStackElements-1);		// STK [TABLE]
	}
}

static mewa::Error::Code luaErrorCode2ErrorCode( int rc)
{
	switch (rc)
	{
		case LUA_ERRRUN: return mewa::Error::LuaCallErrorERRRUN;
		case LUA_ERRMEM: return mewa::Error::LuaCallErrorERRMEM;
		case LUA_ERRERR: return mewa::Error::LuaCallErrorERRERR;
	}
	return rc ? mewa::Error::LuaCallErrorUNKNOWN : mewa::Error::Ok;
}

static void callLuaNodeFunction( lua_State* ls, CompilerContext& ctx, int li)
{
	if (!lua_istable( ls, li)) throw mewa::Error( mewa::Error::BadElementOnCompilerStack, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
	lua_pushvalue( ls, li);						// STK: [NODE]
	lua_pushliteral( ls, "call");					// STK: [NODE] "call"
	lua_gettable( ls, -2);						// STK: [NODE] [CALL]

	if (lua_isnil( ls, -1))
	{
		lua_pop( ls, 1);					// STK: [NODE]
		lua_pushliteral( ls, "name");				// STK: [NODE] "name"
		lua_gettable( ls, -2);					// STK: [NODE] [NAME]
		if (lua_type( ls, -1) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadElementOnCompilerStack, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		std::string namestr( lua_tostring( ls, -1));
		lua_pop( ls, 1);					// STK: [NODE]
		lua_pushliteral( ls, "value");				// STK: [NODE] "value"
		lua_gettable( ls, -2);					// STK: [NODE] [VALUE]
		if (lua_type( ls, -1) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadElementOnCompilerStack, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		std::string valuestr( lua_tostring( ls, -1));
		lua_pop( ls, 2);					// STK:
		throw mewa::Error( mewa::Error::NoLuaFunctionDefinedForItem, namestr + " = " + valuestr);
	}
	else
	{
		int nargs = 1;
		lua_rawgeti( ls, -1, 1);				// STK: [NODE] [CALL] [FUNCNAME]
		lua_rawgeti( ls, -2, 2);				// STK: [NODE] [CALL] [FUNCNAME] [FUNC]
		lua_rawgeti( ls, -3, 3);				// STK: [NODE] [CALL] [FUNCNAME] [FUNC] [CTXARG]
		if (lua_isnil( ls, -1))
		{
			lua_pop( ls, 1);				// STK: [NODE] [CALL] [FUNCNAME] [FUNC]
			lua_pushliteral( ls, "arg");			// STK: [NODE] [CALL] [FUNCNAME] [FUNC] "arg"
			lua_rawget( ls, -5/*[NODE]*/);			// STK: [NODE] [CALL] [FUNCNAME] [FUNC] [ARG]
		}
		else
		{
			nargs += 1;
			lua_pushliteral( ls, "arg");			// STK: [NODE] [CALL] [FUNCNAME] [FUNC] [CTXARG] "arg"
			lua_rawget( ls, -6/*[NODE]*/);			// STK: [NODE] [CALL] [FUNCNAME] [FUNC] [CTXARG] [ARG]
		}
		int rc = lua_pcall( ls, nargs, 1/*nresults*/, 0/*errfunc*/);
		if (rc)
		{
			const char* msg = lua_tostring( ls, -1);
			throw mewa::Error( luaErrorCode2ErrorCode( rc), msg ? msg : "");
		}
		else if (ctx.dbgout && !lua_isnil( ls, -1))
		{
			*ctx.dbgout << "Lua call result [" << li-ctx.calltable << "]" << std::endl;
			*ctx.dbgout << mewa::luaToString( ls, -1, true/*formatted*/) << std::endl;
		}
	}
}


/// \brief Feed next lexem to the automaton state
/// \return true, if the lexem has been consumed, false if we have to feed the same lexem again
static bool feedLexem( lua_State* ls, CompilerContext& ctx, const mewa::Automaton& automaton, const mewa::Lexem& lexem)
{
	auto nexti = automaton.actions().find( {ctx.stateStack.back().index, lexem.id()/*terminal*/});
	if (nexti == automaton.actions().end())
	{
		throw mewa::Error( mewa::Error::UnexpectedTokenNotOneOf, tokenString( lexem, automaton.lexer()) + " { "
				   + expectedTerminalList( automaton.actions(), ctx.stateStack.back().index, automaton.lexer()) + " }", lexem.line());
	}
	switch (nexti->second.type())
	{
		case mewa::Automaton::Action::Shift:
			if (!automaton.lexer().isKeyword( lexem.id()))
			{
				int next_luastki = getNextLuaStackIndex( ctx.stateStack);
				ctx.stateStack.push_back( State( nexti->second.state(), next_luastki, 1, ctx.scopestep));
				luaPushLexem( ls, lexem);
			}
			else
			{
				ctx.stateStack.push_back( State( nexti->second.state(), 0/*luastki*/, 0/*luastkn*/, ctx.scopestep));
			}
			return true;

		case mewa::Automaton::Action::Accept:
		case mewa::Automaton::Action::Reduce:
		{
			int reductionSize = nexti->second.count();
			if ((int)ctx.stateStack.size() <= reductionSize || reductionSize < 0)
			{
				throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
			}
			int luaStackNofElements;
			int scopeStart = reductionSize == 0 ? ctx.scopestep : ctx.stateStack[ ctx.stateStack.size() - reductionSize].scopecnt;

			if (nexti->second.call())
			{
				int callidx = nexti->second.call();
				luaReduceStruct( ls, ctx, reductionSize, callidx, nexti->second.scopeflag(), scopeStart);
				luaStackNofElements = 1;
			}
			else
			{
				luaStackNofElements = getLuaStackReductionSize( ctx.stateStack, reductionSize);
			}
			ctx.stateStack.resize( ctx.stateStack.size() - reductionSize);

			if (nexti->second.type() == mewa::Automaton::Action::Accept)
			{
				if (lexem.id()/*terminal*/ != 0)
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonUnexpectedAccept, lexem.line());
				}
				if (ctx.stateStack.size() != 1 || ctx.stateStack.back().luastki || ctx.stateStack.back().luastkn)
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
				}
				ctx.stateStack.back().luastki = 1;
				ctx.stateStack.back().luastkn = luaStackNofElements+1;
				return true;
			}
			else
			{
				auto gtoi = automaton.gotos().find( {ctx.stateStack.back().index, nexti->second.nonterminal()});
				if (gtoi == automaton.gotos().end())
				{
					auto info = stateTransitionInfo( ctx.stateStack.back().index, lexem.id()/*terminal*/, automaton.lexer());
					throw mewa::Error( mewa::Error::LanguageAutomatonMissingGoto, info, lexem.line());
				}
				else if (luaStackNofElements)
				{
					int next_luastki = getNextLuaStackIndex( ctx.stateStack);
					ctx.stateStack.push_back( State( gtoi->second.state(), next_luastki, luaStackNofElements, scopeStart));
				}
				else
				{
					ctx.stateStack.push_back( State( gtoi->second.state(), 0/*luastki*/, 0/*luastkn*/, scopeStart));
				}
				return false;
			}
		}
	}
	return false;
}

static void printDebugAction( std::ostream& dbgout, CompilerContext& ctx, const mewa::Automaton& automaton, const mewa::Lexem& lexem)
{
	auto nexti = automaton.actions().find( {ctx.stateStack.back().index,lexem.id()/*terminal*/});
	if (nexti == automaton.actions().end())
	{
		dbgout << "Error token " << getLexemName( automaton.lexer(), lexem.id());
		if (!automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
		dbgout << " at line " << lexem.line() << " in state " << ctx.stateStack.back().index << std::endl;
	}
	else
	{
		switch (nexti->second.type())
		{
			case mewa::Automaton::Action::Shift:
				dbgout << "Shift token " << getLexemName( automaton.lexer(), lexem.id());
				if (!automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
				dbgout << " at line " << lexem.line() << " in state " << ctx.stateStack.back().index << ", goto " << nexti->second.state() << std::endl;
				break;
			case mewa::Automaton::Action::Reduce:
			{
				dbgout << "Reduce #" << nexti->second.count() << " to " << automaton.nonterminal( nexti->second.nonterminal())
					<< " by token " << getLexemName( automaton.lexer(), lexem.id());
				if (lexem.id() && !automaton.lexer().isKeyword( lexem.id())) dbgout << " = \"" << lexem.value() << "\"";
				dbgout << " at line " << lexem.line() << " in state " << ctx.stateStack.back().index;
				if (nexti->second.call())
				{
					dbgout << ", call " << automaton.call( nexti->second.call()).tostring();
				}
				if ((int)ctx.stateStack.size() <= nexti->second.count() || nexti->second.count() < 0)
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
				}
				int newstateidx = ctx.stateStack[ ctx.stateStack.size() - nexti->second.count() -1].index;
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

void mewa::luaRunCompiler( lua_State* ls, const mewa::Automaton& automaton, const std::string_view& source, const char* calltable, std::ostream* dbgout)
{
	int buffer[ 2048];
	mewa::monotonic_buffer_resource memrsc( buffer, sizeof buffer);

	lua_getglobal( ls, calltable);					// STK: [CALLTABLE]

	int calltableref = lua_gettop( ls);
	int calltablesize = lua_rawlen( ls, calltableref);;
	if (!lua_istable( ls, calltableref)) throw mewa::Error( mewa::Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));

	int nofLuaStackElements = lua_gettop( ls);

	CompilerContext ctx( &memrsc, sizeof buffer, calltableref, calltablesize, dbgout);
	Scanner scanner( source);

	// Feed source lexems:
	Lexem lexem = automaton.lexer().next( scanner);
	for (; !lexem.empty(); lexem = automaton.lexer().next( scanner))
	{
		if (lexem.id() <= 0) throw Error( Error::BadCharacterInGrammarDef, lexem.value());
		do
		{
			if (dbgout) printDebugAction( *dbgout, ctx, automaton, lexem);
		}
		while (!feedLexem( ls, ctx, automaton, lexem));

		// Runtime check of stack indices:
		if (!ctx.stateStack.empty() && ctx.stateStack.back().luastkn
			&& (lua_gettop( ls) - nofLuaStackElements + 1) != ctx.stateStack.back().luastki + ctx.stateStack.back().luastkn)
		{
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
	}
	// Feed EOF:
	while (!feedLexem( ls, ctx, automaton, lexem/* empty ~ end of input*/))
	{
		if (dbgout) printDebugAction( *dbgout, ctx, automaton, lexem);
	}
	// Call Lua with built tree structures:
	if (ctx.calltablesize)
	{
		int lastElementOnStack = lua_gettop( ls);
		for (int li=calltableref+1; li<=lastElementOnStack; ++li)
		{
			callLuaNodeFunction( ls, ctx, li);
		}
	}
	lua_pop( ls, 1 + lua_gettop( ls) - nofLuaStackElements);
}


