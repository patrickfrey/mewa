/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Error structure creation/parsing test program
/// \file "testError.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
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

static std::string errorToString( const Error& error)
{
	std::ostringstream buf;
	char const* arg = error.arg();
	if (!arg) arg = "";
	buf << "Code=" << (int)error.code() << ", Line=" << error.line() << ", Arg=[" << arg << "], What=[" << error.what() << "]";
	return buf.str();
}

static void testError( std::ostream& out, const char* title, const Error& error, bool verbose)
{
	Error parsedError = Error::parseError( error.what());
	if (verbose) std::cerr << "Test " << title << ":\n\tIN  " << errorToString( error) << "\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test " << title << ": " << errorToString( parsedError) << std::endl;
}

static void testUnspecifiedError( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "Error from outer space!");
	if (verbose) std::cerr << "Test unspecified error:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test unspecified error: " << errorToString( parsedError) << std::endl;
}

static void testUnspecifiedErrorWithLineInfo( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "at line 91: Error from outer space with line info!");
	if (verbose) std::cerr << "Test unspecified error with line info:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test unspecified error with line info: " << errorToString( parsedError) << std::endl;
}

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
				std::cerr << "Usage: testError [-h][-V]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testError [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testError [-h][-V]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}

		std::string expected{R"(
Test Error(): Code=0, Line=0, Arg=[], What=[]
Test Error( errno): Code=12, Line=0, Arg=[], What=[#12 "Cannot allocate memory"]
Test Error( code): Code=421, Line=0, Arg=[], What=[#421 "Bad character in a regular expression passed to the lexer"]
Test Error( code, line): Code=451, Line=345, Arg=[], What=[#451 "Failed to resolve type" at line 345]
Test Error( code, param): Code=437, Line=0, Arg=[13.45], What=[#437 "Bad mewa version": 13.45]
Test Error( code, param, line): Code=452, Line=123, Arg=[int, float], What=[#452 "Ambiguous type reference" at line 123: int, float]
Test Error( UnspecifiedError): Code=402, Line=0, Arg=[], What=[#402 "Unspecified error"]
Test Error( UnspecifiedError, param): Code=402, Line=0, Arg=[an unspecified error with msg], What=[#402 "Unspecified error": an unspecified error with msg]
Test Error( UnspecifiedError, param, line): Code=402, Line=234, Arg=[an unspecified error with msg and line info], What=[#402 "Unspecified error" at line 234: an unspecified error with msg and line info]
Test unspecified error: Code=402, Line=0, Arg=[Error from outer space!], What=[#402 "Unspecified error": Error from outer space!]
Test unspecified error with line info: Code=402, Line=91, Arg=[Error from outer space with line info!], What=[#402 "Unspecified error" at line 91: Error from outer space with line info!]

)"};
		std::ostringstream outputbuf;
		outputbuf << std::endl;

		testError( outputbuf, "Error()", Error(), verbose);
		testError( outputbuf, "Error( errno)", Error( (Error::Code)12/*ENOMEM*/), verbose);
		testError( outputbuf, "Error( code)", Error( Error::IllegalFirstCharacterInLexer), verbose);
		testError( outputbuf, "Error( code, line)", Error( Error::UnresolvableType, 345), verbose);
		testError( outputbuf, "Error( code, param)", Error( Error::BadMewaVersion, "13.45"), verbose);
		testError( outputbuf, "Error( code, param, line)", Error( Error::AmbiguousTypeReference, "int, float", 123), verbose);

		testError( outputbuf, "Error( UnspecifiedError)", Error( Error::UnspecifiedError), verbose);
		testError( outputbuf, "Error( UnspecifiedError, param)", Error( Error::UnspecifiedError, "an unspecified error with msg"), verbose);
		testError( outputbuf, "Error( UnspecifiedError, param, line)", Error( Error::UnspecifiedError, "an unspecified error with msg and line info", 234), verbose);
		testUnspecifiedError( outputbuf, verbose);
		testUnspecifiedErrorWithLineInfo( outputbuf, verbose);

		outputbuf << "\n";

		std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testError.out", output);
			writeFile( "build/testError.exp", expected);
			std::cerr << "ERR test output (build/testError.out) differs expected build/testError.exp" << std::endl;
			return 3;
		}
		else
		{
			removeFile( "build/testError.out");
			removeFile( "build/testError.exp");
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

