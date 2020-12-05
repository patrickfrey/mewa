/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Functions for building and manipulating strings
/// \file "strings.hpp"
#ifndef _MEWA_STRINGS_HPP_INCLUDED
#define _MEWA_STRINGS_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <string>
#include <map>

namespace mewa {

std::string string_format_va( const char* fmt, va_list ap);

#ifdef __GNUC__
std::string string_format( const char* fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
std::string string_format( const char* fmt, ...);
#endif

/// \brief Substitute Patterns [sb][key][eb] for keys that are defined in a map by their value
/// \param[in] templatstr string to substitute variables in
/// \param[in] sb start bracket character
/// \param[in] eb end bracket character
/// \param[in] substmap variables to substitute
std::string template_format( const std::string& templatstr, char sb, char eb, const std::map<std::string,std::string>& substmap);

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif
