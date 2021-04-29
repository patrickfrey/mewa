/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Serialization of lua data structures
/// \file "lua_serialize.hpp"
#ifndef _MEWA_LUA_SERIALIZE_HPP_INCLUDED
#define _MEWA_LUA_SERIALIZE_HPP_INCLUDED
#include "error.hpp"
#include <string>

#if __cplusplus >= 201703L
extern "C" {
#include <lua.h>
}

namespace mewa {

/// \brief Serialize the contents of a lua value on the stack
/// \param[in] ls Lua state
/// \param[in] li lua value reference
/// \param[in] use_indent true if output with indent, false if to return everything on one line
/// \return the result string
std::string luaToString( lua_State* ls, int li, bool use_indent, int depth);

} //namespace
#else
#error Building mewa requires C++17
#endif
#endif

