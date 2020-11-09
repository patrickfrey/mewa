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
	int line;					//< Line number of last lexem pushed
	FILE* dbgout;					//< Debug output or NULL if undefined

	CompilerContext( std::pmr::memory_resource* memrsc, std::size_t buffersize, int calltable_, int calltablesize_, FILE* dbgout_)
		:stateStack(memrsc),calltable(calltable_),calltablesize(calltablesize_),scopestep(0),line(0),dbgout(dbgout_)
	{
		stateStack.reserve( (buffersize - sizeof stateStack) / sizeof(State));
		stateStack.push_back( State( 1/*index*/, 0/*luastki*/, 0/*luastkn*/, 0/*scopecnt*/) );
	}
};


#ifdef __GNUC__
static void printDebug( FILE* dbgout, const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));
#else
static void printDebug( FILE* dbgout, const char* fmt, ...);
#endif

static void printDebug( FILE* dbgout, const char* fmt, ...)
{
	va_list ap;
	va_start( ap, fmt);
	std::string content = mewa::string_format_va( fmt, ap);
	va_end( ap);

	if (!std::fwrite( content.data(), content.size(), 1, dbgout))
	{
		throw mewa::Error( (mewa::Error::Code) std::ferror( dbgout), "debug output");
	}
	std::fflush( dbgout);
}

static void luaReduceStruct( 
		lua_State* ls, CompilerContext& ctx, int reductionSize,
		int callidx, const mewa::Automaton::Action::ScopeFlag scopeflag, int scopeStart)
{
	int stk_start = 0;
	int stk_end = 0;

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
	int structsize = scopeflag == mewa::Automaton::Action::NoScope ? 3:4;
											// STK [ARG1]...[ARGN]
	lua_createtable( ls, 0/*size array*/, structsize);	 			// STK [ARG1]...[ARGN] [TABLE]

	lua_pushliteral( ls, "call");							// STK [ARG1]...[ARGN] [TABLE] "call"
	lua_rawgeti( ls, ctx.calltable, callidx);					// STK [ARG1]...[ARGN] [TABLE] [CALLSTRUCT]
	lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]

	lua_pushliteral( ls, "line");							// STK [ARG1]...[ARGN] [TABLE] "line"
	lua_pushinteger( ls, ctx.line);							// STK [ARG1]...[ARGN] [TABLE] "line" [LINE]
	lua_rawset( ls, -3);								// STK [ARG1]...[ARGN] [TABLE]

	switch (scopeflag)
	{
		case mewa::Automaton::Action::Step:
			ctx.scopestep += 1;
			/*no break here!*/
		case mewa::Automaton::Action::NoScope:
			lua_pushliteral( ls, "step");					// STK [ARG1]...[ARGN] [TABLE] "step"
			lua_pushinteger( ls, ctx.scopestep);				// STK [ARG1]...[ARGN] [TABLE] [COUNTER]
			lua_rawset( ls, -3);						// STK [ARG1]...[ARGN] [TABLE]
			break;
		case mewa::Automaton::Action::Scope:
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
	if (nofLuaStackElements)
	{
		lua_replace( ls, -1-nofLuaStackElements);				// STK [TABLE] [ARG2]...[ARGN]
		if (nofLuaStackElements > 1) lua_pop( ls, nofLuaStackElements-1);	// STK [TABLE]
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

static void luaCallNodeFunction( lua_State* ls, int li, int calltable, FILE* dbgout)
{
	if (!lua_istable( ls, li)) throw mewa::Error( mewa::Error::BadElementOnCompilerStack, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
	lua_pushvalue( ls, li);						// STK: [NODE]
	lua_pushliteral( ls, "call");					// STK: [NODE] "call"
	lua_gettable( ls, -2);						// STK: [NODE] [CALL]

	if (lua_isnil( ls, -1))
	{
		lua_pop( ls, 1);					// STK: [NODE]
		lua_pushliteral( ls, "name");				// STK: [NODE] "name"
		lua_rawget( ls, -2);					// STK: [NODE] [NAME]
		if (lua_type( ls, -1) != LUA_TSTRING)
		{
			throw mewa::Error( mewa::Error::BadElementOnCompilerStack, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		std::string namestr( lua_tostring( ls, -1));
		lua_pop( ls, 1);					// STK: [NODE]
		lua_pushliteral( ls, "value");				// STK: [NODE] "value"
		lua_rawget( ls, -2);					// STK: [NODE] [VALUE]
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
		if (dbgout)
		{
			lua_pushliteral( ls, "name");				// STK: [NODE] [CALL] "name"
			lua_rawget( ls, -2);					// STK: [NODE] [CALL] [NAME]
			const char* procname = lua_tostring( ls, -1);
			printDebug( dbgout, "Calling Lua function: %s\n", procname);
			lua_pop( ls, 1);
		}
		int nargs = 1;
		lua_pushliteral( ls, "proc");					// STK: [NODE] [CALL] "proc"
		lua_rawget( ls, -2);						// STK: [NODE] [CALL] [FUNC]
		lua_pushvalue( ls, -3);						// STK: [NODE] [CALL] [FUNC] [NODE]
		lua_pushliteral( ls, "obj");					// STK: [NODE] [CALL] [FUNC] [NODE] "obj"
		lua_rawget( ls, -4);						// STK: [NODE] [CALL] [FUNC] [NODE] [OBJ]
		if (lua_isnil( ls, -1))
		{
			lua_pop( ls, 1);					// STK: [NODE] [CALL] [FUNCNAME] [FUNC]
		}
		else
		{
			nargs += 1;
		}
		int rc = lua_pcall( ls, nargs, 1/*nresults*/, 0/*errfunc*/);	// STK: [NODE] [CALL] [FUNCNAME] [FUNCRESULT]
		if (rc)
		{
			const char* msg = lua_tostring( ls, -1);
			mewa::Error err = mewa::Error::parseError( msg);
			if (err.code() == mewa::Error::RuntimeException)
			{
				throw mewa::Error( luaErrorCode2ErrorCode( rc), msg ? msg : "");
			}
			else
			{
				throw err;
			}
		}
		else if (dbgout && !lua_isnil( ls, -1))
		{
			printDebug( dbgout, "Lua call result [%d]\n", li-calltable);
			std::string resstr( mewa::luaToString( ls, -1, true/*formatted*/));
			printDebug( dbgout, "%s\n", resstr.c_str());
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
				ctx.line = lexem.line();
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

static void printDebugAction( FILE* dbgout, CompilerContext& ctx, const mewa::Automaton& automaton, const mewa::Lexem& lexem)
{
	std::string nm  = getLexemName( automaton.lexer(), lexem.id());
	std::string val( lexem.value().data(), lexem.value().size());

	auto nexti = automaton.actions().find( {ctx.stateStack.back().index,lexem.id()/*terminal*/});
	if (nexti == automaton.actions().end())
	{		
		if (automaton.lexer().isKeyword( lexem.id()))
		{
			printDebug( dbgout, "Error token %s at line %d in state %d\n",
					nm.c_str(), lexem.line(), ctx.stateStack.back().index);
		}
		else
		{
			printDebug( dbgout, "Error token %s = \"%s\" at line %d in state %d\n",
					nm.c_str(), val.c_str(), lexem.line(), ctx.stateStack.back().index);
		}
	}
	else
	{
		switch (nexti->second.type())
		{
			case mewa::Automaton::Action::Shift:
				if (automaton.lexer().isKeyword( lexem.id()))
				{
					printDebug( dbgout, "Shift token %s at line %d in state %d, goto %d\n",
							nm.c_str(), lexem.line(), ctx.stateStack.back().index, nexti->second.state());
				}
				else
				{
					printDebug( dbgout, "Shift token %s = \"%s\" at line %d in state %d, goto %d\n",
							nm.c_str(), val.c_str(), lexem.line(), ctx.stateStack.back().index, nexti->second.state());
				}
				break;
			case mewa::Automaton::Action::Reduce:
			{
				if ((int)ctx.stateStack.size() <= nexti->second.count() || nexti->second.count() < 0)
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
				}
				std::string callstr;
				if (nexti->second.call())
				{
					callstr.append( ", call ");
					callstr.append( automaton.call( nexti->second.call()).tostring());
				}
				int gtostate = 0;
				int newstateidx = ctx.stateStack[ ctx.stateStack.size() - nexti->second.count() -1].index;
				auto gtoi = automaton.gotos().find( {newstateidx, nexti->second.nonterminal()});
				if (gtoi == automaton.gotos().end())
				{
					throw mewa::Error( mewa::Error::LanguageAutomatonCorrupted, lexem.line());
				}
				gtostate = gtoi->second.state();

				if (!lexem.id() || automaton.lexer().isKeyword( lexem.id()))
				{
					printDebug( dbgout, "Reduce #%d to %s by token %s at line %d in state %d%s, goto %d\n",
							nexti->second.count(), automaton.nonterminal( nexti->second.nonterminal()).c_str(),
							nm.c_str(), lexem.line(), ctx.stateStack.back().index,
							callstr.c_str(), gtostate);
				}
				else
				{
					printDebug( dbgout, "Reduce #%d to %s by token %s = \"%s\" at line %d in state %d%s, goto %d\n",
							nexti->second.count(), automaton.nonterminal( nexti->second.nonterminal()).c_str(),
							nm.c_str(), val.c_str(), lexem.line(), ctx.stateStack.back().index,
							callstr.c_str(), gtostate);
				}
				break;
			}
			case mewa::Automaton::Action::Accept:
				printDebug( dbgout, "Accept\n");
				break;
		}
	}
}

void mewa::luaRunCompiler( lua_State* ls, const mewa::Automaton& automaton, const std::string_view& source, const char* calltable, FILE* dbgout)
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
			if (dbgout) printDebugAction( dbgout, ctx, automaton, lexem);
		}
		while (!feedLexem( ls, ctx, automaton, lexem));

		// Runtime check of stack indices:
		int top = lua_gettop( ls);
		if (!ctx.stateStack.empty() && ctx.stateStack.back().luastkn
			&& (top - nofLuaStackElements) != (ctx.stateStack.back().luastki + ctx.stateStack.back().luastkn - 1))
		{
			if (dbgout)
			{
				fprintf( dbgout, "Stack top=%d, nof elements=%d\n", top, nofLuaStackElements);
				for (auto stke : ctx.stateStack)
				{
					fprintf( dbgout, "Stack element: i=%d, n=%d\n", stke.luastki, stke.luastkn);
				}
			}
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
	}
	// Feed EOF:
	while (!feedLexem( ls, ctx, automaton, lexem/* empty ~ end of input*/))
	{
		if (dbgout) printDebugAction( dbgout, ctx, automaton, lexem);
	}
	// Call Lua with built tree structures:
	if (ctx.calltablesize)
	{
		int lastElementOnStack = lua_gettop( ls);
		for (int li=calltableref+1; li<=lastElementOnStack; ++li)
		{
			luaCallNodeFunction( ls, li, ctx.calltable, ctx.dbgout);
		}
	}
	lua_pop( ls, 1 + lua_gettop( ls) - nofLuaStackElements);
}


