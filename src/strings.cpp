/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Functions for building and manipulating strings
/// \file "strings.cpp"
#include "strings.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <stdexcept>
#include <new>

using namespace mewa;

struct AllocString
{
	char* ptr;

	AllocString( std::size_t len)
	{
		ptr = (char*)std::malloc( len+1);
		if (!ptr) throw std::bad_alloc();
	}
	~AllocString()
	{
 		std::free( ptr);
	}
};

std::string mewa::string_format_va( const char* fmt, va_list ap)
{
	std::string rt;
	va_list ap_copy;
	va_copy( ap_copy, ap);
	char msgbuf[ 4096];
	int len = ::vsnprintf( msgbuf, sizeof(msgbuf), fmt, ap_copy);
	if (len < (int)sizeof( msgbuf))
	{
		if (len < 0) return std::string();
		rt.append( msgbuf, len);
	}
	else
	{
		AllocString msg( len);
		::vsnprintf( msg.ptr, len+1, fmt, ap);
		rt.append( msg.ptr, len);
	}
	return rt;
}

std::string mewa::string_format( const char* fmt, ...)
{
	std::string rt;
	va_list ap;
	va_start( ap, fmt);
	rt = string_format_va( fmt, ap);
	va_end( ap);
	return rt;
}

std::string mewa::template_format( const std::string& templatstr, char sb, char eb, const std::map<std::string,std::string>& substmap)
{
	std::string rt;
	char const* ti = templatstr.c_str();
	char const* ts = std::strchr( ti, sb);
	while (*ts)
	{
		char const* keystart = ts+1;
		char const* te = std::strchr( keystart, eb);
		if (te)
		{
			auto si = substmap.find( std::string( keystart, te-keystart));
			if (si != substmap.end())
			{
				rt.append( ti, ts-ti);
				rt.append( si->second);
				ti = te + 1;
			}
			ts = std::strchr( te+1, sb);
			if (!ts) break;
		}
		else break;
	}
	rt.append( ti);
	return rt;
}

