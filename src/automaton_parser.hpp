/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Parser of the grammar for the LALR(1) automaton to build
/// \file "automaton_parser.hpp"
#ifndef _MEWA_AUTOMATON_PARSER_HPP_INCLUDED
#define _MEWA_AUTOMATON_PARSER_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "automaton_structs.hpp"
#include "languagedef.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include <utility>
#include <string>
#include <string_view>
#include <set>
#include <map>
#include <vector>

namespace mewa {

LanguageDef parseLanguageDef( const std::string& source);
LanguageDecoratorMap parseLanguageDecoratorMap( const std::string& source);

std::string printLuaTypeSystemStub( const LanguageDef& langdef);

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

