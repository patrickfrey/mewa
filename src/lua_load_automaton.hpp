/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Function to load an automaton from a definition as Lua structure printed by the mewa program ("mewa -g EBNF")
/// \file "lua_load_automaton.hpp"
#ifndef _MEWA_LUA_LOAD_AUTOMATON_HPP_INCLUDED
#define _MEWA_LUA_LOAD_AUTOMATON_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "automaton.hpp"
#include <lua.h>

namespace mewa {

Automaton luaLoadAutomaton( lua_State *ls, int li);

} //namespace
#else
#error Building mewa requires C++17
#endif
#endif

