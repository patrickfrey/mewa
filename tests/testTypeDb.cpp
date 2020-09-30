/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Typedb test program
/// \file "testTypeDb.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include "typedb.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include <cstring>

using namespace mewa;

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}

		std::string expected{R"(
)"};
		std::ostringstream outputbuf;
		outputbuf << "\n";

		std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testTypeDb.out", output);
			writeFile( "build/testTypeDb.exp", expected);
			std::cerr << "ERR test output (build/testTypeDb.out) differs expected build/testTypeDb.exp" << std::endl;
			return 3;
		}
		else
		{
			removeFile( "build/testTypeDb.out");
			removeFile( "build/testTypeDb.exp");
		}
		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << err.what() << std::endl;
		return (int)err.code();
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERR runtime " << err.what() << std::endl;
		return 1;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "ERR out of memory" << std::endl;
		return 2;
	}
	return 0;
}

