/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Userdata structures for Lua
/// \file "lua_userdata.hpp"
#ifndef _MEWA_LUA_USERDATA_HPP_INCLUDED
#define _MEWA_LUA_USERDATA_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "typedb.hpp"
#include "automaton.hpp"
#include <string>
#include <string_view>
#include <cstdio>
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mewa {
namespace lua {

#define MEWA_COMPILER_METATABLE_NAME 	"mewa.compiler"
#define MEWA_TYPEDB_METATABLE_NAME 	"mewa.typedb"
#define MEWA_OBJTREE_METATABLE_NAME 	"mewa.objtree"
#define MEWA_TYPETREE_METATABLE_NAME 	"mewa.typetree"
#define MEWA_REDUTREE_METATABLE_NAME 	"mewa.redutree"
#define MEWA_CALLTABLE_FMT	 	"mewa.calls.%d"
#define MEWA_OBJTABLE_FMT		"mewa.obj.%d"

struct TableName
{
	char buf[ 20];

	void init( const char* instname, const char* fmtstr, int counter)
	{
		if ((int)sizeof(buf) <= std::snprintf( buf, sizeof(buf), fmtstr, counter))
		{
			throw mewa::Error( mewa::Error::TooManyInstancesCreated, instname);
		}
	}
	void initCopy( const TableName& o)
	{
		std::memcpy( buf, o.buf, sizeof(buf));
	}
};

struct CallTableName :public TableName
{
	void init()
	{
		static int tableIdx = 0;
		TableName::init( MEWA_COMPILER_METATABLE_NAME, MEWA_CALLTABLE_FMT, ++tableIdx);
	}
};

struct ObjectTableName :public TableName
{
	void init()
	{
		static int tableIdx = 0;
		TableName::init( MEWA_TYPEDB_METATABLE_NAME, MEWA_OBJTABLE_FMT, ++tableIdx);
	}
};
}} //namespace

struct mewa_compiler_userdata_t
{
	mewa::Automaton automaton;
	mewa::lua::CallTableName callTableName;
	FILE* outputFileHandle;

	void init()
	{
		outputFileHandle = nullptr;
		new (&automaton) mewa::Automaton();
		callTableName.init();
	}
	void closeOutput() noexcept
	{
		if (outputFileHandle && outputFileHandle != ::stdout && outputFileHandle != ::stderr) std::fclose(outputFileHandle);
		outputFileHandle = nullptr;
	}
	void destroy( lua_State* ls) noexcept
	{
		lua_pushnil( ls);
		lua_setglobal( ls, callTableName.buf);

		closeOutput();
		automaton.~Automaton();
	}
	static const char* metatableName() noexcept {return MEWA_COMPILER_METATABLE_NAME;}
};

struct mewa_typedb_userdata_t
{
public:
	mewa::TypeDatabase* impl;
	mewa::lua::ObjectTableName objTableName;
private:
	int objCount;

public:
	void init() noexcept
	{
		impl = nullptr;
		objTableName.init();
		objCount = 0;
	}
	void create()
	{
		if (impl) delete impl;
		impl = new mewa::TypeDatabase();
	}
	void destroy( lua_State* ls) noexcept
	{
		lua_pushnil( ls);
		lua_setglobal( ls, objTableName.buf);

		if (impl) delete impl;
		impl = nullptr;
	}
	int allocObjectHandle() noexcept
	{
		return ++objCount;
	}

	static const char* metatableName() noexcept {return MEWA_TYPEDB_METATABLE_NAME;}
};

template <class TT>
struct mewa_treetemplate_userdata_t
{
	typedef TT TreeType;

	mewa::lua::ObjectTableName objTableName;
	std::shared_ptr<TreeType> tree;
	std::size_t nodeidx;

public:
	void init( const mewa::lua::ObjectTableName& objTableName_) noexcept
	{
		objTableName.initCopy( objTableName_);
		tree.reset();
		nodeidx = 0;
	}
	void create( TreeType&& o)
	{
		tree.reset( new TreeType( std::move( o)));
	}
	void destroy( lua_State* ls) noexcept
	{
		tree.reset();
		nodeidx = 0;
	}
	void createCopy( const mewa_treetemplate_userdata_t& o, std::size_t nodeidx_)
	{
		tree = o.tree;
		nodeidx = nodeidx_;
	}
};


struct mewa_objtree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::ObjectInstanceTree>
{
	static const char* metatableName() noexcept {return MEWA_OBJTREE_METATABLE_NAME;}
};

struct mewa_typetree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::TypeDefinitionTree>
{
	static const char* metatableName() noexcept {return MEWA_TYPETREE_METATABLE_NAME;}
};

struct mewa_redutree_userdata_t	:public mewa_treetemplate_userdata_t<mewa::TypeDatabase::ReductionDefinitionTree>
{
	static const char* metatableName() noexcept {return MEWA_REDUTREE_METATABLE_NAME;}
};

#else
#error Building mewa requires C++17
#endif
#endif

