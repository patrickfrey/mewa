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
#include "scope.hpp"
#include "error.hpp"
#include "strings.hpp"
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
	FILE* debugFileHandle;
	typedef std::string OutputBuffer;
	std::string outputBuffer;

	void init()
	{
		debugFileHandle = nullptr;
		new (&automaton) mewa::Automaton();
		new (&outputBuffer) OutputBuffer();
		callTableName.init();
	}
	void closeOutput() noexcept
	{
		closeFile( debugFileHandle);
	}
	void destroy( lua_State* ls) noexcept
	{
		lua_pushnil( ls);
		lua_setglobal( ls, callTableName.buf);

		closeOutput();
		automaton.~Automaton();
		outputBuffer.~OutputBuffer();
	}
	static const char* metatableName() noexcept {return MEWA_COMPILER_METATABLE_NAME;}

	static FILE* openFile( const char* path)
	{
		if (0==std::strcmp( path, "stderr"))
		{
			return ::stderr;
		}
		else if (0==std::strcmp( path, "stdout"))
		{
			return ::stdout;
		}
		else
		{
			return std::fopen( path, "w");
		}
	}

private:
	void closeFile( FILE*& fileHandle)
	{
		if (fileHandle && fileHandle != ::stdout && fileHandle != ::stderr) std::fclose( fileHandle);
		fileHandle = nullptr;
	}
};

struct mewa_typedb_userdata_t
{
public:
	mewa::TypeDatabase* impl;
	mewa::lua::ObjectTableName objTableName;
	mewa::Scope curScope;
	mewa::Scope::Step curStep;

private:
	int objCount;

public:
	void init() noexcept
	{
		impl = nullptr;
		objTableName.init();
		objCount = 0;
		curScope = mewa::Scope( 0, std::numeric_limits<mewa::Scope::Step>::max());
		curStep = 0;
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

template <class T>
class shared_ptr
{
public:
	shared_ptr()
	{
		m_refcnt=0;
	}
	shared_ptr( const shared_ptr<T>& o) = delete;
	shared_ptr( shared_ptr<T>&& o) = delete;
	~shared_ptr()
	{
		reset();
	}
	void init()
	{
		m_refcnt=0;
	}
	void reset()
	{
		if (m_refcnt)
		{
			if (*m_refcnt == 1)
			{
				((T*)(m_refcnt+1))->~T();
				std::free( m_refcnt);
			}
			else
			{
				*m_refcnt -= 1;
			}
		}
	}
	void reset( T&& obj)
	{
		reset();
		m_refcnt = (int*)std::malloc( sizeof(int)+sizeof(T));
		if (m_refcnt == nullptr) throw std::bad_alloc();
		*m_refcnt = 1;
		T* ptr = (T*)(m_refcnt+1);
		new (ptr) T( std::move( obj));
	}
	T* get() noexcept
	{
		return (T*)(m_refcnt+1);
	}
	T const* get() const noexcept
	{
		return (T const*)(m_refcnt+1);
	}
	T& operator*() noexcept
	{
		return *get();
	}
	T const& operator*() const noexcept
	{
		return *get();
	}
	shared_ptr<T>& operator=( const shared_ptr<T>& o)
	{
		if (o.m_refcnt)
		{
			if (o.m_refcnt != m_refcnt)
			{
				reset();
				m_refcnt = o.m_refcnt;
				*m_refcnt += 1;
			}
		}
		else
		{
			reset();
		}
		return *this;
	}

private:
	int* m_refcnt;
};

template <class TT>
struct mewa_treetemplate_userdata_t
{
	typedef TT TreeType;

	mewa::lua::ObjectTableName objTableName;
	shared_ptr<TreeType> tree;
	std::size_t nodeidx;

public:
	void init( const mewa::lua::ObjectTableName& objTableName_) noexcept
	{
		objTableName.initCopy( objTableName_);
		tree.init();
		nodeidx = 0;
	}
	void create( TreeType&& o)
	{
		tree.reset( TreeType( std::move( o)));
		nodeidx = 1;
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

