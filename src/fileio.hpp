/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Functions for reading and writing files
/// \file "fileio.hpp"
#ifndef _MEWA_FILEIO_HPP_INCLUDED
#define _MEWA_FILEIO_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <vector>
#include <string>
#include <string_view>

namespace mewa {

void writeFile( const std::string& filename, const std::string& content);
std::string readFile( const std::string& filename);
std::vector<std::string> readFileLines( const std::string& filename);
void removeFile( const std::string& filename);
std::string fileBaseName( const std::string_view& fnam);

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

