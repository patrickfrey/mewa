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
#include "lua_userdata.hpp"
#include "lua_parameter.hpp"
#include "lua_5_1.hpp"
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
#include <inttypes.h>

static int g_called_deprecated_get_type = 0;	//< Counter for calls of deprecated function typedb:get_type
static int g_called_deprecated_get_types = 0;	//< Counter for calls of deprecated function typedb:get_types

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

extern "C" int luaopen_mewa( lua_State* ls);

static mewa::Error::Location getLuaScriptErrorLocation( lua_State* ls)
{
	lua_Debug ar;
	lua_getstack( ls, 1, &ar);
	lua_getinfo( ls, "S", &ar);
	return mewa::Error::Location( ar.short_src, ar.linedefined);
}

/// \note Got this idea from Jason Turner C++ Weekly Ep. 91 "Using Lippincott Functions"
///		See also The C++ Standard Library2ndEd. by Nicolai M. Josuttis page 50
static void lippincottFunction( lua_State* ls)
{
	try
	{
		throw;
	}
	catch (const mewa::Error& err)
	{
		if (err.location().line())
		{
			lua_pushstring( ls, err.what());
		}
		else
		{
			mewa::Error errWithLocation( err, getLuaScriptErrorLocation( ls));
			lua_pushstring( ls, errWithLocation.what());
		}
		lua_error( ls);
	}
	catch (const std::runtime_error& err)
	{
		mewa::Error errWithLocation( mewa::Error::RuntimeException, err.what(), getLuaScriptErrorLocation( ls));
		lua_pushstring( ls, errWithLocation.what());
		lua_error( ls);
	}
	catch (const std::bad_alloc&)
	{
		mewa::Error errWithLocation( mewa::Error::MemoryAllocationError, getLuaScriptErrorLocation( ls));
		lua_pushstring( ls, errWithLocation.what());
		lua_error( ls);
	}
	catch (...)
	{
		mewa::Error errWithLocation( mewa::Error::UnexpectedException, getLuaScriptErrorLocation( ls));
		lua_pushstring( ls, errWithLocation.what());
		lua_error( ls);
	}
}

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

static int lua_print_redirected_impl( lua_State* ls, FILE* fh)
{
	[[maybe_unused]] static const char* functionName = "mewa print redirected";
	int nargs = lua_gettop( ls);
	for (int li=1; li <= nargs; ++li)
	{
		std::string content;
		std::size_t len = 0;
		const char* str = "";
		if (lua_isnil( ls, li))
		{
			str = "<nil>";
			len = 5;
		}
		else if (lua_isstring( ls, li))
		{
			str = lua_tolstring( ls, li, &len);
		}
		else
		{
			try
			{
				content = mewa::luaToString( ls, li, false/*formatted*/, 0/*no maximum depth*/);
				len = content.size();
				str = content.c_str();
			}
			catch (...) {lippincottFunction( ls);}
		}
		if (len < std::fwrite( str, 1, len, fh))
		{
			int ec = std::ferror( fh);
			mewa::Error err( (mewa::Error::Code)ec, functionName);
			luaL_error( ls, "%s", err.what());
		}
	}
	// Print eoln:
	if (1 > std::fwrite( "\n", 1, 1, fh))
	{
		int ec = std::ferror( fh);
		mewa::Error err( (mewa::Error::Code)ec, functionName);
		luaL_error( ls, "%s", err.what());
	}
	std::fflush( fh);
	return 0;
}

static void append_printbuffer( lua_State* ls, int ai, int ae, std::string& buffer)
{
	for (int li = ai; li <= ae; ++li)
	{
		std::string content;
		std::size_t len = 0;
		const char* str = "";
		if (lua_isnil( ls, li))
		{
			str = "<nil>";
			len = 5;
		}
		else if (lua_isstring( ls, li))
		{
			str = lua_tolstring( ls, li, &len);
		}
		else
		{
			content = mewa::luaToString( ls, li, false/*formatted*/, 0/*no maximum depth*/);
			len = content.size();
			str = content.c_str();
		}
		buffer.append( str, len);
	}
}

static int lua_print_section_redirected( lua_State* ls)
{
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_touserdata(ls, lua_upvalueindex(1));
	[[maybe_unused]] static const char* functionName = "mewa print section redirected";
	if (!cp)
	{
		mewa::Error err( mewa::Error::LuaInvalidUserData, functionName);
		luaL_error( ls, "%s", err.what());
	}
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, std::numeric_limits<int>::max()/*maxNofArgs*/);
		auto section = (nargs >= 1 && !lua_isnil(ls,1)) ? std::string(mewa::lua::getArgumentAsString( functionName, ls, 1)) : std::string();
		append_printbuffer( ls, 2, nargs, cp->outputSectionMap[ section]);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

static int lua_print_redirected( lua_State* ls) {
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_touserdata(ls, lua_upvalueindex(1));
	[[maybe_unused]] static const char* functionName = "mewa print redirected";
	if (!cp)
	{
		mewa::Error err( mewa::Error::LuaInvalidUserData, functionName);
		luaL_error( ls, "%s", err.what());
	}
	try
	{
		int nargs = lua_gettop( ls);
		append_printbuffer( ls, 1, nargs, cp->outputBuffer);
	}
	catch (...) {lippincottFunction( ls);}
	return 0;
}

static int lua_debug_redirected( lua_State* ls) {
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_touserdata(ls, lua_upvalueindex(1));
	[[maybe_unused]] static const char* functionName = "mewa print redirected";
	if (!cp)
	{
		mewa::Error err( mewa::Error::LuaInvalidUserData, functionName);
		luaL_error( ls, "%s", err.what());
	}
	return lua_print_redirected_impl( ls, cp->debugFileHandle ? cp->debugFileHandle : ::stderr);
}

static const struct luaL_Reg g_printlib [] = {
	{"print_section", lua_print_section_redirected},
	{"print", lua_print_redirected},
	{"debug", lua_debug_redirected},
	{nullptr, nullptr} /* end of array */
};

static int mewa_tostring( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.tostring";
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 0/*minNofArgs*/, 3/*maxNofArgs*/);
		int maxdepth = (nargs >= 2 && !lua_isnil(ls,2)) ? mewa::lua::getArgumentAsInteger( functionName, ls, 2) : 8;
		bool use_indent = (nargs >= 3 && !lua_isnil(ls,3)) ? mewa::lua::getArgumentAsBoolean( functionName, ls, 3) : false;
		std::string rt = (nargs >= 1 && !lua_isnil(ls,1)) ? mewa::luaToString( ls, 1, use_indent, maxdepth) : std::string();
		lua_pushlstring( ls, rt.c_str(), rt.size());
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_version( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.version";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 0/*minNofArgs*/, 0/*maxNofArgs*/);
		lua_pushinteger( ls, MEWA_MAJOR_VERSION);
		lua_pushinteger( ls, MEWA_MINOR_VERSION);
		lua_pushinteger( ls, MEWA_PATCH_VERSION);
	}
	catch (...) { lippincottFunction( ls); }
	return 3;
}

static int mewa_llvm_float_tohex( lua_State* ls)
{
	/// PF:HACK This function should not be here but in an external library
	[[maybe_unused]] static const char* functionName = "mewa.llvm_float_tohex";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		double num = mewa::lua::getArgumentAsFloatingPoint( functionName, ls, 1);
		char buf[ 64];
		int64_t encnum;
		std::memmove( &encnum, &num, sizeof( encnum));
		encnum >>= 28;
		bool doinc = (encnum & 1);
		if (doinc) ++encnum;
		encnum = (encnum >> 1);
		encnum = (encnum << 29);
		const char* fmt64 = sizeof(long) == 8 ? "%lx" : "%llx";
		std::snprintf( buf, sizeof(buf), fmt64, encnum);
		for (char* bi = buf; *bi; ++bi) if (*bi >= 'a' && *bi <= 'z') {*bi -= 32;}
		lua_pushstring( ls, buf);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_llvm_double_tohex( lua_State* ls)
{
	/// PF:HACK This function should not be here but in an external library
	[[maybe_unused]] static const char* functionName = "mewa.llvm_double_tohex";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		double num = mewa::lua::getArgumentAsFloatingPoint( functionName, ls, 1);
		char buf[ 64];
		int64_t encnum;
		std::memmove( &encnum, &num, sizeof( encnum));
		const char* fmt64 = sizeof(long) == 8 ? "%lx" : "%llx";
		std::snprintf( buf, sizeof(buf), fmt64, encnum);
		for (char* bi = buf; *bi; ++bi) if (*bi >= 'a' && *bi <= 'z') {*bi -= 32;}
		lua_pushstring( ls, buf);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_stacktrace( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.stacktrace";
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 0/*minNofArgs*/, 2/*maxNofArgs*/);
		int nn = (nargs >= 1 && !lua_isnil(ls,1)) ? (int)mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 1) : std::numeric_limits<int>::max();
		std::vector<std::string> ignore = (nargs >= 2 && !lua_isnil(ls,2)) ? mewa::lua::getArgumentAsStringList( functionName, ls, 2) : std::vector<std::string>();
		mewa::lua::pushStackTrace( ls, functionName, nn, ignore);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_new_compiler( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.compiler";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		mewa::lua::checkArgumentAsTable( functionName, ls, 1);
		mewa::lua::checkStack( functionName, ls, 6);
	}
	catch (...) { lippincottFunction( ls); }

	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)lua_newuserdata( ls, sizeof(mewa_compiler_userdata_t));
	try
	{
		cp->init();
	}
	catch (...) { lippincottFunction( ls); }

	lua_pushliteral( ls, "call");
	lua_gettable( ls, 1);
	lua_setglobal( ls, cp->callTableName.buf);
	try
	{
		new (&cp->automaton) mewa::Automaton( mewa::luaLoadAutomaton( ls, 1));
	}
	catch (...) { lippincottFunction( ls); }

	luaL_getmetatable( ls, mewa_compiler_userdata_t::metatableName());
	lua_setmetatable( ls, -2);
	return 1;
}

static int mewa_destroy_compiler( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "compiler:__gc";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());

	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	catch (...) { lippincottFunction( ls); }

	cp->destroy( ls);
	return 0;
}

static int mewa_compiler_tostring( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "compiler:__tostring";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
	}
	catch (...) { lippincottFunction( ls); }

	std::string_view resultptr;
	try
	{
		std::string result = cp->automaton.tostring();
		resultptr = move_string_on_lua_stack( ls, std::move( result));
	}
	catch (...) { lippincottFunction( ls); }

	lua_pushlstring( ls, resultptr.data(), resultptr.size());
	lua_replace( ls, -2); // ... dispose the element created with move_string_on_lua_stack
	return 1;
}

static void copyFileNameToBuffer( char* buf, std::size_t bufsize, std::string_view str)
{
	if (str.size() >= bufsize) throw mewa::Error( mewa::Error::InternalBufferOverflow, str);
	std::memcpy( buf, str.data(), str.size());
	buf[ str.size()] = 0;
}
static int mewa_compiler_run( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "compiler:run( target, options, inputfile, [,outputfile [,dbgoutput]])";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 4/*minNofArgs*/, 6/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 10);

		char targetfn[ 256]; 	//... target filename
		if (!lua_isnil( ls, 2))
		{
			copyFileNameToBuffer( targetfn, sizeof(targetfn), mewa::lua::getArgumentAsString( functionName, ls, 2));
		}
		else
		{
			targetfn[ 0] = 0;
		}
		int options_index = 3;	//... options
		char filename[ 256]; 	//... source filename
		copyFileNameToBuffer( filename, sizeof(filename), mewa::lua::getArgumentAsString( functionName, ls, 4));
		std::string_view outputfn = "stdout";

		std::string sourcestr_ = mewa::readFile( std::string(filename));
		std::string_view sourceptr = move_string_on_lua_stack( ls, std::move( sourcestr_));	// STK: [COMPILER] [INPUTFILE] [SOURCE]
		std::string targetstr_;
		if (targetfn[0]) targetstr_ = mewa::readFile( std::string(targetfn));
		std::string_view targetptr = move_string_on_lua_stack( ls, std::move( targetstr_));	// STK: [COMPILER] [INPUTFILE] [SOURCE] [TARGET]

		if (nargs >= 5)
		{
			if (!lua_isnil( ls, 5))
			{
				outputfn = mewa::lua::getArgumentAsString( functionName, ls, 5);
			}
			if (nargs >= 6 && !lua_isnil( ls, 6))
			{
				std::string_view debugFileName = mewa::lua::getArgumentAsString( functionName, ls, 6);
				cp->debugFileHandle = mewa_compiler_userdata_t::openFile( debugFileName.data());
				if (!cp->debugFileHandle) throw mewa::Error( (mewa::Error::Code) errno, debugFileName);
			}
		}
		lua_getglobal( ls, "_G");		// STK: table=_G
		lua_pushliteral( ls, "print");		// STK: table=_G, key=print
		lua_pushliteral( ls, "print");		// STK: table=_G, key=print, key=print
		lua_gettable( ls, -3);			// STK: table=_G, key=print, value=function
		lua_pushliteral( ls, "debug");		// STK: table=_G, key=print, value=function key=debug
		lua_pushliteral( ls, "debug");		// STK: table=_G, key=print, value=function key=debug key=debug
		lua_gettable( ls, -5);			// STK: table=_G, key=print, value=function key=debug value=function

		lua_getglobal( ls, "_G");
		lua_pushlightuserdata( ls, cp);
		luaL_setfuncs( ls, g_printlib, 1/*number of closure elements*/);
		lua_pop( ls, 1);

		mewa::luaRunCompiler( ls, cp->automaton, options_index, sourceptr, cp->callTableName.buf, cp->debugFileHandle);
		{
			std::string triple = mewa::fileBaseName( targetfn);

			// Map output with target template:
			cp->outputSectionMap[ "Source"] = filename;
			cp->outputSectionMap[ "Triple"] = triple;
			cp->outputSectionMap[ "Code"].append( cp->outputBuffer);
			std::string output = mewa::template_format( targetptr, '{', '}', cp->outputSectionMap);
			if (outputfn == "stderr")
			{
				std::fputs( output.c_str(), ::stderr);
			}
			else if (outputfn == "stdout")
			{
				std::puts( output.c_str());
			}
			else
			{
				mewa::writeFile( std::string(outputfn), output);
			}
		}

		// Restore old print function that has been left on the stack:
					//STK: table=_G, key=print, value=function key=debug, value=function
		lua_settable( ls, -5);	//STK: table=_G, key=print, value=function
		lua_settable( ls, -3);	//STK: table=_G
		lua_pop( ls, 1);	//STK:
		cp->closeOutput();
		lua_pop( ls, 3);	// ... destroy strings on lua stack created with move_string_on_lua_stack and options

		if (g_called_deprecated_get_types)
		{
			std::cerr << "Warning: Called deprecated function typedb:get_types. This function has been renamed to typedb:this_types (issue #4)" << std::endl;
		}
		if (g_called_deprecated_get_type)
		{
			std::cerr << "Warning: Called deprecated function typedb:get_type. This function has been renamed to typedb:this_type (issue #4)" << std::endl;
		}
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int mewa_new_typedb( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.typedb";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 0/*minNofArgs*/, 0/*maxNofArgs*/);
	}
	catch (...) { lippincottFunction( ls); }

	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)lua_newuserdata( ls, sizeof(mewa_typedb_userdata_t));
	td->init();
	try
	{
		td->create();
	}
	catch (...) { lippincottFunction( ls); }

	luaL_getmetatable( ls, mewa_typedb_userdata_t::metatableName());
	lua_setmetatable( ls, -2);

	return 1;
}

static int mewa_destroy_typedb( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:__gc";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
	}
	catch (...) { lippincottFunction( ls); }

	td->destroy( ls);
	return 0;
}

static int mewa_typedb_scope( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:scope";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 3/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
		mewa::Scope rt_scope = td->curScope;
		mewa::Scope::Step rt_step = td->curStep;
		if (nargs >= 2)
		{
			td->curScope = mewa::lua::getArgumentAsScope( functionName, ls, 2);
			td->curStep = (nargs >= 3) ? mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 3) : (td->curScope.end()-1);
		}
		mewa::lua::pushScope( ls, functionName, rt_scope);
		lua_pushinteger( ls, rt_step);
	}
	catch (...) { lippincottFunction( ls); }
	return 2;
}

static int mewa_typedb_step( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:step";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		mewa::Scope::Step rt = td->curStep;
		if (nargs >= 2)
		{
			td->curStep = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		}
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_this_instance( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:this_instance";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	int handle = -1;
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 2);
		handle = td->impl->getObjectInstanceOfScope( name, td->curScope);
	}
	catch (...) { lippincottFunction( ls); }

	if (handle > 0)
	{
		// Get the item referenced by 'handle' from the object registry:
		lua_rawgeti( ls, LUA_REGISTRYINDEX, handle);
		return 1;
	}
	else
	{
		return 0;
	}
}

static int mewa_typedb_get_instance( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:get_instance";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	int handle = -1;
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 2);
		handle = td->impl->getObjectInstance( name, td->curStep);
	}
	catch (...) { lippincottFunction( ls); }

	if (handle > 0)
	{
		// Get the item referenced by 'handle' from the object registry:
		lua_rawgeti( ls, LUA_REGISTRYINDEX, handle);
		return 1;
	}
	else
	{
		return 0;
	}
}

static int mewa_typedb_set_instance( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:set_instance";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	std::string_view name;
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
		name = mewa::lua::getArgumentAsString( functionName, ls, 2);
		if (lua_isnil( ls, 3)) mewa::lua::throwArgumentError( functionName, 3, mewa::Error::ExpectedArgumentNotNil);
	}
	catch (...) { lippincottFunction( ls); }

	// Get a reference handle of the 3rd argument in the registry:
	lua_pushvalue( ls, 3);
	int handle = luaL_ref( ls, LUA_REGISTRYINDEX);

	try
	{
		td->impl->setObjectInstance( name, td->curScope, handle);
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int mewa_typedb_reduction_tagmask( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb.reduction_tagmask";
	try
	{
		int firstArg = 1;
		if (mewa::lua::isArgumentType( functionName, ls, firstArg, (1 << LUA_TUSERDATA)))
		{
			luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
			firstArg = 2;
		}
		mewa::TagMask tagSet;
		int nargs = lua_gettop( ls);
		for (int li=firstArg; li <= nargs; ++li)
		{
			tagSet |= mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, li);
		}
		lua_pushinteger( ls, tagSet.mask());
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_def_type( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:def_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	int buffer_parameter[ 1024];
	mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 5/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		int constructor = (nargs >= 4) ? mewa::lua::getArgumentAsConstructor( functionName, ls, 4) : 0;
		std::pmr::vector<mewa::TypeDatabase::TypeConstructorPair> parameter;
		if (nargs >= 5 && !lua_isnil(ls,5)) parameter = mewa::lua::getArgumentAsTypeConstructorPairList( functionName, ls, 5, &memrsc_parameter);
		lua_pop( ls, 1); // ... obj table
		int rt = td->impl->defineType( td->curScope, contextType, name, constructor, parameter);
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_def_type_as( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:def_type_as";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	int buffer_parameter[ 1024];
	mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 4/*minNofArgs*/, 4/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		int asType = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 4);
		int rt = td->impl->defineTypeAs( td->curScope, contextType, name, asType);
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_this_type( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:this_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	int buffer_parameter[ 1024];
	mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 4/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		std::pmr::vector<int> parameter;
		if (nargs >= 4 && !lua_isnil(ls,4)) parameter = mewa::lua::getArgumentAsTypeList( functionName, ls, 4, &memrsc_parameter, true/*allow t/c pairs*/);
		int rt = td->impl->getType( td->curScope, contextType, name, parameter);
		if (!rt) return 0;
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_this_types( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:this_types";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());

	int buffer_parameter[ 1024];
	mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 3/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto rt = td->impl->getTypes( td->curScope, contextType, name, resbuf);
		mewa::lua::pushTypeList( ls, functionName, rt.types);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_get_type( lua_State* ls)
{
	++g_called_deprecated_get_type;
	return mewa_typedb_this_type( ls);
}
static int mewa_typedb_get_types( lua_State* ls)
{
	++g_called_deprecated_get_types;
	return mewa_typedb_this_types( ls);
}

static int mewa_typedb_def_reduction( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:def_reduction";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 5/*minNofArgs*/, 6/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int toType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		int fromType = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 3);
		int constructor = mewa::lua::getArgumentAsConstructor( functionName, ls, 4);
		int tag = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 5);
		float weight = (nargs >= 6 && !lua_isnil(ls,6)) ? mewa::lua::getArgumentAsFloatingPoint( functionName, ls, 6) : 0.0;
		td->impl->defineReduction( td->curScope, toType, fromType, constructor, tag, weight);
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int mewa_typedb_get_reduction( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:get_reduction";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 4/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int toType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		int fromType = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 3);
		mewa::TagMask selectTags( nargs >= 4 && !lua_isnil(ls,4) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchAll());

		auto redu = td->impl->getReduction( td->curStep, toType, fromType, selectTags);
		if (!redu.defined()) return 0;
		lua_pushnumber( ls, redu.weight);
		if (redu.constructor == 0) return 1;

		lua_rawgeti( ls, LUA_REGISTRYINDEX, redu.constructor);
	}
	catch (...) { lippincottFunction( ls); }
	return 2;
}

static int mewa_typedb_get_reductions( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:get_reductions";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 4/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int type = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 2);
		mewa::TagMask selectTags( nargs >= 3 && !lua_isnil(ls,3) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 3) : mewa::TagMask::matchAll());
		mewa::TagMask selectTagsCount( nargs >= 4 && !lua_isnil(ls,4) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchNothing());

		mewa::TypeDatabase::ResultBuffer resbuf;
		auto res = td->impl->getReductions( td->curStep, type, selectTags, selectTagsCount, resbuf);
		mewa::lua::pushWeightedReductions( ls, functionName, res.reductions);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_derive_type( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:derive_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 6/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int toType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		int fromType = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 3);
		mewa::TagMask selectTags( nargs >= 4 && !lua_isnil(ls,4) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchAll());
		mewa::TagMask selectTagsCount( nargs >= 5 && !lua_isnil(ls,5) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 5) : mewa::TagMask::matchNothing());
		int maxPathLen = nargs >= 6 && !lua_isnil(ls,6) ? mewa::lua::getArgumentAsInteger( functionName, ls, 6) : 1;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto deriveres = td->impl->deriveType( td->curStep, toType, fromType, selectTags, selectTagsCount, maxPathLen, resbuf);
		if (deriveres.defined)
		{
			mewa::lua::pushTypeConstructorPairs( ls, functionName, deriveres.reductions);
			lua_pushnumber( ls, deriveres.weightsum);
			if (deriveres.conflictPath.empty())
			{
				return 2;
			}
			else
			{
				mewa::lua::pushTypeList( ls, functionName, deriveres.conflictPath);
				return 3;
			}
		}
		else
		{
			return 0;
		}
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int mewa_typedb_resolve_type( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:resolve_type";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 4/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		mewa::TagMask selectTags( nargs >= 4 && !lua_isnil(ls,4) ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchAll());

		mewa::TypeDatabase::ResultBuffer resbuf;

		if (mewa::lua::argumentIsArray( ls, 2))
		{
			int buffer_parameter[ 1024];
			mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);

			std::pmr::vector<int> contextTypes = mewa::lua::getArgumentAsTypeList( functionName, ls, 2, &memrsc_parameter, true/*allow t/c pairs*/);
			auto resolveres = td->impl->resolveType( td->curStep, contextTypes, name, selectTags, resbuf);
			return mewa::lua::pushResolveResult( ls, functionName, 2/*root context argument lua index*/, resolveres);
		}
		else
		{
			int contextType = mewa::lua::getArgumentAsType( functionName, ls, 2);
			auto resolveres = td->impl->resolveType( td->curStep, contextType, name, selectTags, resbuf);
			return mewa::lua::pushResolveResult( ls, functionName, 0/*root context argument lua index*/, resolveres);
		}
	}
	catch (...) { lippincottFunction( ls); }
	return 0;
}

static int mewa_typedb_type_name( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_name";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);

		auto rt = td->impl->typeName( type);
		lua_pushlstring( ls, rt.data(), rt.size());
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_string( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_string";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 3/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		const char* sep = (nargs >= 3 && !lua_isnil(ls,3)) ? mewa::lua::getArgumentAsCString( functionName, ls, 3) : " "/*default separator*/;
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto rt = td->impl->typeToString( type, sep, resbuf);
		lua_pushlstring( ls, rt.c_str(), rt.size());
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_context( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_context";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);

		int rt = td->impl->typeContext( type);
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_parameters( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_parameters";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 6);
		int type = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 2);
		auto rt = td->impl->typeParameters( type);
		mewa::lua::pushTypeConstructorPairs( ls, functionName, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_nof_parameters( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_nof_parameters";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 2);
		auto rt = td->impl->typeParameters( type);
		lua_pushinteger( ls, rt.size());
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_constructor( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_constructor";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 2);
		auto rt = td->impl->typeConstructor( type);
		mewa::lua::pushTypeConstructor( ls, functionName, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_scope( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_scope";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsUnsignedInteger( functionName, ls, 2);
		auto rt = td->impl->typeScope( type);
		if (!rt.defined()) return 0;
		mewa::lua::pushScope( ls, functionName, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

namespace {

/// Tree constructors:
template <class UD> constexpr int tree_nof_args() noexcept				{return 0;}
template <> constexpr int tree_nof_args<mewa_objtree_userdata_t>() noexcept		{return 2;}
template <> constexpr int tree_nof_args<mewa_typetree_userdata_t>() noexcept		{return 1;}
template <> constexpr int tree_nof_args<mewa_redutree_userdata_t>() noexcept		{return 1;}

template <class UD>
void create_tree_impl( const char* functionName, lua_State* ls, UD& ud, mewa_typedb_userdata_t* td)
{throw mewa::Error( mewa::Error::LogicError, mewa::string_format( "%s line %d", __FILE__, (int)__LINE__));}

template <>
void create_tree_impl<mewa_objtree_userdata_t>( const char* functionName, lua_State* ls, mewa_objtree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 2);
	ud.create( td->impl->getObjectInstanceTree( name));
}

template <>
void create_tree_impl<mewa_typetree_userdata_t>( const char* functionName, lua_State* ls, mewa_typetree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	ud.create( td->impl->getTypeDefinitionTree());
}

template <>
void create_tree_impl<mewa_redutree_userdata_t>( const char* functionName, lua_State* ls, mewa_redutree_userdata_t& ud, mewa_typedb_userdata_t* td)
{
	ud.create( td->impl->getReductionDefinitionTree());
}

/// Tree iterators:
template <class TREETYPE>
int tree_iter( lua_State* ls)
{
	return luaL_error( ls, "not implemented");
}
template <>
int tree_iter<mewa::TypeDatabase::TypeDefinitionTree>( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "type_tree:list iterator";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa_typetree_userdata_t* ud = (mewa_typetree_userdata_t*)luaL_checkudata( ls, lua_upvalueindex(1), mewa_typetree_userdata_t::metatableName());
		std::size_t iter = lua_tointeger( ls, lua_upvalueindex(2));
		if (!ud->tree.get() || ud->nodeidx == 0 || iter >= (*ud->tree)[ ud->nodeidx].item().value.size()) return 0;
		lua_pushinteger( ls, (*ud->tree)[ ud->nodeidx].item().value[ iter]);
		iter += 1;
		lua_pushinteger( ls, iter);
		lua_replace( ls, lua_upvalueindex(2));
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}
template <>
int tree_iter<mewa::TypeDatabase::ReductionDefinitionTree>( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "reduction_tree:list iterator";
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa_redutree_userdata_t* ud = (mewa_redutree_userdata_t*)luaL_checkudata( ls, lua_upvalueindex(1), mewa_redutree_userdata_t::metatableName());
		std::size_t iter = lua_tointeger( ls, lua_upvalueindex(2));
		if (!ud->tree.get() || ud->nodeidx == 0 || iter >= (*ud->tree)[ ud->nodeidx].item().value.size()) return 0;
		const mewa::TypeDatabase::ReductionDefinition& redu = (*ud->tree)[ ud->nodeidx].item().value[ iter];
		mewa::lua::pushReductionDefinition( ls, functionName, redu);
		iter += 1;
		lua_pushinteger( ls, iter);
		lua_replace( ls, lua_upvalueindex(2));
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}
}//anonymous namespace


template <class UD>
struct LuaTreeMetaMethods
{
	static int create( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "typedb:*_tree";
		mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
		try
		{
			constexpr int nargs = tree_nof_args<UD>();
			mewa::lua::checkNofArguments( functionName, ls, nargs/*minNofArgs*/, nargs/*maxNofArgs*/);

			UD* ud = (UD*)lua_newuserdata( ls, sizeof(UD));
			ud->init();
			create_tree_impl( functionName, ls, *ud, td);
		}
		catch (...) { lippincottFunction( ls); }

		luaL_getmetatable( ls, UD::metatableName());
		lua_setmetatable( ls, -2);
		return 1;
	}

	static int gc( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "*tree:__gc";
		UD* ud = (UD*)luaL_checkudata( ls, 1, UD::metatableName());
		try
		{
			mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
		}
		catch (...) { lippincottFunction( ls); }

		ud->destroy( ls);
		return 0;
	}

	static int chld_iter( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "*tree:chld iterator";
		try
		{
			mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
			UD* ud = (UD*)luaL_checkudata( ls, lua_upvalueindex(1), UD::metatableName());
			std::size_t iter = lua_tointeger( ls, lua_upvalueindex(2));
			if (!ud->tree.get() || !iter) return 0;

			UD* cur_ud = (UD*)lua_newuserdata( ls, sizeof(UD));
			cur_ud->init();
			cur_ud->createCopy( *ud, iter);
			luaL_getmetatable( ls, UD::metatableName());
			lua_setmetatable( ls, -2);

			iter = (*ud->tree)[ ud->nodeidx].next();
			lua_pushinteger( ls, iter);
			lua_replace( ls, lua_upvalueindex(2));
		}
		catch (...) { lippincottFunction( ls); }
		return 1;
	}

	static int chld( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "*tree:chld";
		UD* ud = (UD*)luaL_checkudata( ls, 1, UD::metatableName());
		try
		{
			mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
			if (!ud->tree.get() || ud->nodeidx == 0) return 0;
			std::size_t chld = (*ud->tree)[ ud->nodeidx].chld();
			if (!chld)
			{
				// ... No children, return empty list
				lua_pushvalue( ls, 1);
				lua_pushinteger( ls, 0);
				lua_pushcclosure( ls, &chld_iter, 2);
			}
			else
			{
				UD* chld_ud = (UD*)lua_newuserdata( ls, sizeof(UD));
				chld_ud->init();
				chld_ud->createCopy( *ud, chld);
				luaL_getmetatable( ls, UD::metatableName());
				lua_setmetatable( ls, -2);

				lua_pushinteger( ls, chld);
				lua_pushcclosure( ls, &chld_iter, 2);
			}
		}
		catch (...) { lippincottFunction( ls); }
		return 1;
	}

	static int scope( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "*tree:scope";
		UD* ud = (UD*)luaL_checkudata( ls, 1, UD::metatableName());
		try
		{
			mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
			if (!ud->tree.get() || ud->nodeidx == 0) return 0;

			mewa::lua::pushScope( ls, functionName, (*ud->tree)[ ud->nodeidx].item().scope);
		}
		catch (...) { lippincottFunction( ls); }
		return 1;
	}

	static int list( lua_State* ls)
	{
		[[maybe_unused]] static const char* functionName = "*tree:list";
		luaL_checkudata( ls, 1, UD::metatableName());
		try
		{
			mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 1/*maxNofArgs*/);
			mewa::lua::checkStack( functionName, ls, 4);

			lua_pushvalue( ls, 1);		//... userdata
			lua_pushnumber( ls, 0);		//... list iterator index on first element of array
			lua_pushcclosure( ls, &tree_iter<typename UD::TreeType>, 2);
		}
		catch (...) { lippincottFunction( ls); }
		return 1;
	}
};

static int mewa_objtree_instance( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "*tree:instance";
	try
	{
		mewa_objtree_userdata_t* ud = (mewa_objtree_userdata_t*)luaL_checkudata( ls, 1, mewa_objtree_userdata_t::metatableName());
		if (!ud->tree.get() || ud->nodeidx == 0) return 0;
		mewa::lua::pushTypeConstructor( ls, functionName, (*ud->tree)[ ud->nodeidx].item().value);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}


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
	{ "scope",		mewa_typedb_scope },
	{ "step",		mewa_typedb_step },
	{ "this_instance",	mewa_typedb_this_instance },
	{ "get_instance",	mewa_typedb_get_instance },
	{ "set_instance",	mewa_typedb_set_instance },
	{ "def_type",		mewa_typedb_def_type },
	{ "def_type_as",	mewa_typedb_def_type_as },
	{ "this_type",		mewa_typedb_this_type },
	{ "this_types",		mewa_typedb_this_types },
	{ "get_type",		mewa_typedb_get_type },
	{ "get_types",		mewa_typedb_get_types },
	{ "def_reduction",	mewa_typedb_def_reduction },
	{ "reduction_tagmask",	mewa_typedb_reduction_tagmask },
	{ "derive_type",	mewa_typedb_derive_type },
	{ "resolve_type",	mewa_typedb_resolve_type },
	{ "type_name",		mewa_typedb_type_name },
	{ "type_string",	mewa_typedb_type_string },
	{ "type_context",	mewa_typedb_type_context },
	{ "type_parameters",	mewa_typedb_type_parameters },
	{ "type_nof_parameters",mewa_typedb_type_nof_parameters },
	{ "type_constructor",	mewa_typedb_type_constructor },
	{ "type_scope",		mewa_typedb_type_scope },
	{ "get_reduction",	mewa_typedb_get_reduction },
	{ "get_reductions",	mewa_typedb_get_reductions },
	{ "instance_tree",	LuaTreeMetaMethods<mewa_objtree_userdata_t>::create },
	{ "type_tree",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::create },
	{ "reduction_tree",	LuaTreeMetaMethods<mewa_redutree_userdata_t>::create },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_objtree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_objtree_userdata_t>::scope },
	{ "instance",		mewa_objtree_instance },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_typetree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::scope },
	{ "list",		LuaTreeMetaMethods<mewa_typetree_userdata_t>::list },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_redutree_methods[] = {
	{ "__gc",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::gc },
	{ "chld",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::chld },
	{ "scope",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::scope },
	{ "list",		LuaTreeMetaMethods<mewa_redutree_userdata_t>::list },
	{ nullptr,		nullptr }
};

static const struct luaL_Reg mewa_functions[] = {
	{ "compiler",		mewa_new_compiler },
	{ "typedb",		mewa_new_typedb },
	{ "tostring",		mewa_tostring },
	{ "version",		mewa_version },
	{ "llvm_float_tohex",	mewa_llvm_float_tohex },
	{ "llvm_double_tohex",	mewa_llvm_double_tohex },
	{ "stacktrace",		mewa_stacktrace },
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



