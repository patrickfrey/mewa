/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Functions for reading and writing files
/// \file "fileio.cpp"
#include "fileio.hpp"
#include "strings.hpp"
#include "error.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <libgen.h>

using namespace mewa;

void mewa::writeFile( const std::string& filename, const std::string& content)
{
	FILE* ff = std::fopen( filename.c_str(), "w");
	if (!ff) throw Error( (Error::Code)errno, filename);
	if (content.size() != std::fwrite( content.c_str(), 1, content.size(), ff))
	{
		std::fclose( ff);
		throw Error( (Error::Code)std::ferror( ff), filename);
	}
	std::fclose( ff);
}

std::string mewa::readFile( const std::string& filename)
{
	std::string rt;
	FILE* ff = std::fopen( filename.c_str(), "r");
	if (!ff) throw Error( (Error::Code)errno, filename);

	enum {RBufsize = 1<<12};
	char rbuf[ RBufsize];
	std::size_t nn = std::fread( rbuf, 1, RBufsize, ff);
	for (; nn == RBufsize; nn = std::fread( rbuf, 1, RBufsize, ff))
	{
		rt.append( rbuf, RBufsize);
	}
	rt.append( rbuf, nn);

	int eno = std::ferror( ff);
	bool eof = std::feof( ff);
	std::fclose( ff);

	if (eno)
	{
		throw Error( (Error::Code)eno, filename);
	}
	else if (!eof)
	{
		throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
	}
	return rt;
}

std::vector<std::string> mewa::readFileLines( const std::string& filename)
{
	std::vector<std::string> rt;
	std::string content = mewa::readFile( filename);
	char const* si = content.c_str();
	char const* sn = std::strchr( si, '\n');
	for (; sn; si=sn+1,sn = std::strchr( si, '\n'))
	{
		rt.emplace_back( std::string( si, sn));
	}
	rt.emplace_back( std::string( si));
	return rt;
}

void mewa::removeFile( const std::string& filename)
{
	while (0>::remove( filename.c_str()))
	{
		int ec = errno;
		if (ec == EINTR) continue;
		if (ec != ENOENT)
		{
			throw Error( (Error::Code)ec, filename);
		}
		break;
	}
}

std::string mewa::fileBaseName( const std::string_view& fnam)
{
	char buf[ 1024];
	std::size_t nn = fnam.size() >= sizeof(buf) ? sizeof(buf)-1 : fnam.size();
	std::memcpy( buf, fnam.data(), nn);
	char* bn = ::basename( buf);
	std::size_t ee = std::strchr( bn, '\0')-bn;
	for (; ee > 0 && bn[ ee] != '.'; --ee){}
	if (ee != 0) nn = ee;
	return std::string( bn, nn);
}


