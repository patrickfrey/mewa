/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Object with ownership passed to Lua context for avoiding memory leaks on lua_error
/// \file "lua_run_compiler.hpp"
#ifndef _MEWA_LUA_OBJECT_REFERENCE_HPP_INCLUDED
#define _MEWA_LUA_OBJECT_REFERENCE_HPP_INCLUDED
#if __cplusplus >= 201703L
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace mewa {

class MemoryBlock
{
public:
	static const char* metatablename() noexcept
	{
		return "mewa.local.memblock";
	}
	virtual ~MemoryBlock(){};
};

template <class OBJECT>
class ObjectReference :public MemoryBlock
{
public:
	ObjectReference( OBJECT&& obj_)
		:m_obj(std::move(obj_)){}
	virtual ~ObjectReference(){}

	const OBJECT& obj() const noexcept		{return m_obj;}

private:
	OBJECT m_obj;
};

} //namespace


// \brief Structure to give control of memory to lua to avoid memory leaks in case of Lua exceptions
struct memblock_userdata_t
{
	mewa::MemoryBlock* memoryBlock;

	void destroy( lua_State* ls)
	{
		if (memoryBlock) delete memoryBlock;
	}
	static memblock_userdata_t* create( lua_State* ls) noexcept
	{
		memblock_userdata_t* mb = (memblock_userdata_t*)lua_newuserdata( ls, sizeof(memblock_userdata_t));
		mb->memoryBlock = nullptr;
		luaL_getmetatable( ls, mewa::MemoryBlock::metatablename());
		lua_setmetatable( ls, -2);
		return mb;
	}
};

#else
#error Building mewa requires C++17
#endif
#endif

