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

static int lua_print_redirected_impl( lua_State* ls, FILE* fh) {
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
				content = mewa::luaToString( ls, li, false/*formatted*/);
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
					content = mewa::luaToString( ls, li, false/*formatted*/);
					len = content.size();
					str = content.c_str();
				}
				catch (...)
				{
					lippincottFunction( ls);
				}
			}
			cp->outputBuffer.append( str, len);
		}
		cp->outputBuffer.push_back( '\n');
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
	{"print", lua_print_redirected},
	{"debug", lua_debug_redirected},
	{nullptr, nullptr} /* end of array */
};

static int mewa_tostring( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "mewa.tostring";
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 2/*maxNofArgs*/);
		bool use_indent = false;
		if (nargs == 2 && lua_isboolean( ls, 2))
		{
			use_indent = lua_toboolean( ls, 2);
		}
		std::string rt = mewa::luaToString( ls, 1, use_indent);
		lua_pushlstring( ls, rt.c_str(), rt.size());
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
		int nn = (nargs >= 1) ? (int)mewa::lua::getArgumentAsCardinal( functionName, ls, 1) : std::numeric_limits<int>::max();
		std::vector<std::string> ignore = (nargs >= 2) ? mewa::lua::getArgumentAsStringList( functionName, ls, 2) : std::vector<std::string>();
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

static int mewa_compiler_run( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "compiler:run( target, inputfile [,outputfile [,dbgoutput]])";
	mewa_compiler_userdata_t* cp = (mewa_compiler_userdata_t*)luaL_checkudata( ls, 1, mewa_compiler_userdata_t::metatableName());
	try
	{
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 5/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 10);

		std::string_view targetfn = mewa::lua::getArgumentAsString( functionName, ls, 2); //... target filename
		std::string_view filename = mewa::lua::getArgumentAsString( functionName, ls, 3); //... source filename
		std::string_view outputfn = "stdout";

		std::string source = mewa::readFile( std::string(filename));
		std::string_view sourceptr = move_string_on_lua_stack( ls, std::move( source));	// STK: [COMPILER] [INPUTFILE] [SOURCE]

		if (nargs >= 4)
		{
			if (!lua_isnil( ls, 4))
			{
				outputfn = mewa::lua::getArgumentAsString( functionName, ls, 4);
			}
			if (nargs >= 5 && !lua_isnil( ls, 5))
			{
				std::string_view debugFileName = mewa::lua::getArgumentAsString( functionName, ls, 5);
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

		mewa::luaRunCompiler( ls, cp->automaton, sourceptr, cp->callTableName.buf, cp->debugFileHandle);
		{
			// Map output with target template:
			std::map<std::string,std::string> outputTemplateMap;
			outputTemplateMap[ "Source"] = filename;
			outputTemplateMap[ "Code"] = cp->outputBuffer;
			std::string targetstr = mewa::readFile( std::string(targetfn));
			std::string output = mewa::template_format( targetstr, '{', '}', outputTemplateMap);
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
		lua_settable( ls, -5);	//STK: table=_G key=print, value=function 
		lua_settable( ls, -3);	//STK: table=_G
		lua_pop( ls, 1);	//STK:
		cp->closeOutput();
		lua_pop( ls, 1);	// ... destroy string on lua stack created with move_string_on_lua_stack
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

	lua_createtable( ls, 1<<16/*expected array elements*/, 0);
	lua_setglobal( ls, td->objTableName.buf);

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
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 1/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 4);
		mewa::Scope rt = td->curScope;
		if (nargs >= 2)
		{
			td->curScope = mewa::lua::getArgumentAsScope( functionName, ls, 2);
			td->curStep = td->curScope.start();
		}
		mewa::lua::pushScope( ls, functionName, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
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
		if (nargs >= 2) td->curStep = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
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
		// Get the item addressed with handle from the object table on the top of the stack and return it:
		lua_getglobal( ls, td->objTableName.buf);
		lua_rawgeti( ls, -1, handle);
		lua_replace( ls, -2);
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

	// Put the 4th argument into the object table of this typedb with an index stored as handle in the typedb:
	int handle = td->allocObjectHandle();
	lua_getglobal( ls, td->objTableName.buf);
	lua_pushvalue( ls, 3);
	lua_rawseti( ls, -2, handle);
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
			tagSet |= mewa::lua::getArgumentAsCardinal( functionName, ls, li);
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
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 3/*minNofArgs*/, 6/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
		std::string_view name = mewa::lua::getArgumentAsString( functionName, ls, 3);
		lua_getglobal( ls, td->objTableName.buf);
		int constructor = (nargs >= 4) ? mewa::lua::getArgumentAsConstructor( functionName, ls, 4, -1/*objtable*/, td) : 0;
		std::pmr::vector<mewa::TypeDatabase::Parameter> parameter;
		if (nargs >= 5) parameter = mewa::lua::getArgumentAsParameterList( functionName, ls, 5, -1/*objtable*/, td, &memrsc_parameter);
		int priority = (nargs >= 6) ? mewa::lua::getArgumentAsInteger( functionName, ls, 6) : 0;
		lua_pop( ls, 1); // ... obj table
		int rt = td->impl->defineType( td->curScope, contextType, name, constructor, parameter, priority);
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_get_type( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:get_type";
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
		if (nargs >= 4) parameter = mewa::lua::getArgumentAsTypeList( functionName, ls, 4, &memrsc_parameter, true/*allow t/c pairs*/);
		int rt = td->impl->getType( td->curScope, contextType, name, parameter);
		if (!rt) return 0;
		lua_pushinteger( ls, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
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
		int fromType = mewa::lua::getArgumentAsCardinal( functionName, ls, 3);
		lua_getglobal( ls, td->objTableName.buf);
		int constructor = mewa::lua::getArgumentAsConstructor( functionName, ls, 4, -1/*objtable*/, td);
		int tag = mewa::lua::getArgumentAsCardinal( functionName, ls, 5);
		float weight = (nargs >= 6) ? mewa::lua::getArgumentAsFloatingPoint( functionName, ls, 6) : 0.0;
		lua_pop( ls, 1); // ... obj table
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
		int fromType = mewa::lua::getArgumentAsCardinal( functionName, ls, 3);
		mewa::TagMask selectTags( nargs >= 4 ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchAll());

		auto redu = td->impl->getReduction( td->curStep, toType, fromType, selectTags);
		if (!redu.defined()) return 0;
		lua_pushnumber( ls, redu.weight);
		if (redu.constructor == 0) return 1;

		lua_getglobal( ls, td->objTableName.buf);
		lua_rawgeti( ls, -1, redu.constructor);
		lua_replace( ls, -2);
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
		int nargs = mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 3/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 8);
		int type = mewa::lua::getArgumentAsCardinal( functionName, ls, 2);
		mewa::TagMask selectTags( nargs >= 3 ? mewa::lua::getArgumentAsTagMask( functionName, ls, 3) : mewa::TagMask::matchAll());

		mewa::TypeDatabase::ResultBuffer resbuf;
		auto res = td->impl->getReductions( td->curStep, type, selectTags, resbuf);
		mewa::lua::pushWeightedReductions( ls, functionName, td->objTableName.buf, res.reductions);
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
		int fromType = mewa::lua::getArgumentAsCardinal( functionName, ls, 3);
		mewa::TagMask selectTags( nargs >= 4 ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchAll());
		mewa::TagMask selectTagsPathLen( nargs >= 5 ? mewa::lua::getArgumentAsTagMask( functionName, ls, 5) : mewa::TagMask::matchNothing());
		int maxPathLen = nargs >= 6 ? mewa::lua::getArgumentAsInteger( functionName, ls, 6) : -1;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto deriveres = td->impl->deriveType( td->curStep, toType, fromType, selectTags, selectTagsPathLen, maxPathLen, resbuf);
		if (deriveres.defined)
		{
			mewa::lua::pushReductionResults( ls, functionName, td->objTableName.buf, deriveres.reductions);
			lua_pushnumber( ls, deriveres.weightsum);
			if (deriveres.conflictPath.empty())
			{
				return 2;
			}
			else
			{
				mewa::lua::pushTypePath( ls, functionName, deriveres.conflictPath);
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
		mewa::TagMask selectTags( nargs >= 4 ? mewa::lua::getArgumentAsTagMask( functionName, ls, 4) : mewa::TagMask::matchNothing());

		mewa::TypeDatabase::ResultBuffer resbuf;

		if (lua_type( ls, 2) == LUA_TNUMBER)
		{
			int contextType = mewa::lua::getArgumentAsNonNegativeInteger( functionName, ls, 2);
			auto resolveres = td->impl->resolveType( td->curStep, contextType, name, selectTags, resbuf);
			return mewa::lua::pushResolveResult( ls, functionName, td->objTableName.buf, resolveres);
		}
		else if (lua_type( ls, 2) == LUA_TTABLE)
		{
			int buffer_parameter[ 256];
			mewa::monotonic_buffer_resource memrsc_parameter( buffer_parameter, sizeof buffer_parameter);

			std::pmr::vector<int> contextTypes = mewa::lua::getArgumentAsTypeList( functionName, ls, 2, &memrsc_parameter, false/*allow t/c pairs*/);
			auto resolveres = td->impl->resolveType( td->curStep, contextTypes, name, selectTags, resbuf);
			return mewa::lua::pushResolveResult( ls, functionName, td->objTableName.buf, resolveres);
		}
		else
		{
			mewa::lua::throwArgumentError( functionName, 2, mewa::Error::ExpectedArgumentTypeList);
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
		const char* sep = (nargs >= 3) ? mewa::lua::getArgumentAsCString( functionName, ls, 3) : " "/*default separator*/;
		mewa::TypeDatabase::ResultBuffer resbuf;

		auto rt = td->impl->typeToString( type, sep, resbuf);
		lua_pushlstring( ls, rt.c_str(), rt.size());
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
		int type = mewa::lua::getArgumentAsCardinal( functionName, ls, 2);
		auto rt = td->impl->typeParameters( type);
		mewa::lua::pushParameters( ls, functionName, td->objTableName.buf, rt);
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
		int type = mewa::lua::getArgumentAsCardinal( functionName, ls, 2);
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
		int type = mewa::lua::getArgumentAsCardinal( functionName, ls, 2);
		auto rt = td->impl->typeConstructor( type);
		mewa::lua::pushTypeConstructor( ls, functionName, td->objTableName.buf, rt);
	}
	catch (...) { lippincottFunction( ls); }
	return 1;
}

static int mewa_typedb_type_scope_start( lua_State* ls)
{
	[[maybe_unused]] static const char* functionName = "typedb:type_scope_start";
	mewa_typedb_userdata_t* td = (mewa_typedb_userdata_t*)luaL_checkudata( ls, 1, mewa_typedb_userdata_t::metatableName());
	try
	{
		mewa::lua::checkNofArguments( functionName, ls, 2/*minNofArgs*/, 2/*maxNofArgs*/);
		mewa::lua::checkStack( functionName, ls, 2);
		int type = mewa::lua::getArgumentAsCardinal( functionName, ls, 2);
		auto rt = td->impl->typeScopeStart( type);
		lua_pushinteger( ls, rt);
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
		mewa::lua::pushReductionDefinition( ls, functionName, ud->objTableName.buf, redu);
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
			ud->init( ud->objTableName);
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
			cur_ud->init( ud->objTableName);
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
				chld_ud->init( ud->objTableName);
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
		mewa::lua::pushTypeConstructor( ls, functionName, ud->objTableName.buf, (*ud->tree)[ ud->nodeidx].item().value);
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
	{ "get_instance",	mewa_typedb_get_instance },
	{ "set_instance",	mewa_typedb_set_instance },
	{ "def_type",		mewa_typedb_def_type },
	{ "get_type",		mewa_typedb_get_type },
	{ "def_reduction",	mewa_typedb_def_reduction },
	{ "reduction_tagmask",	mewa_typedb_reduction_tagmask },
	{ "derive_type",	mewa_typedb_derive_type },
	{ "resolve_type",	mewa_typedb_resolve_type },
	{ "type_name",		mewa_typedb_type_name },
	{ "type_string",	mewa_typedb_type_string },
	{ "type_parameters",	mewa_typedb_type_parameters },
	{ "type_nof_parameters",mewa_typedb_type_nof_parameters },
	{ "type_constructor",	mewa_typedb_type_constructor },
	{ "type_scope_start",	mewa_typedb_type_scope_start },
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



