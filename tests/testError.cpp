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
	buf << "Code=" << (int)error.code() 
			<< ", Line=" << error.location().line()
			<< ", File=\"" << error.location().filename() << "\""
			<< ", Arg=[" << arg << "]"
			<< ", What=[" << error.what() << "]";
	return buf.str();
}

static void testError( std::ostream& out, const char* title, const Error& error, bool verbose)
{
	Error parsedError = Error::parseError( error.what());
	if (verbose) std::cerr << "Test " << title << ":\n\tIN  " << errorToString( error) << "\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test " << title << ": " << errorToString( parsedError) << std::endl;
}

static void testRuntimeException( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "Message from outer space!");
	if (verbose) std::cerr << "Test runtime error:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test runtime error: " << errorToString( parsedError) << std::endl;
}

static void testRuntimeExceptionWithLineInfo( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "at line 91: Message from outer space with line info!");
	if (verbose) std::cerr << "Test runtime error with line info:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test runtime error with line info: " << errorToString( parsedError) << std::endl;
}

static void testRuntimeExceptionWithLocationInfo( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "at line 91 in file \"program.txt\": Message from outer space with location info!");
	if (verbose) std::cerr << "Test runtime error with location info:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test runtime error with location info: " << errorToString( parsedError) << std::endl;
}

static void testCompileErrorWithLineInfo( std::ostream& out, bool verbose)
{
	Error parsedError = Error::parseError( "Error at line 612: This is a compilation error!");
	if (verbose) std::cerr << "Test compile error with line info:\n\tOUT " << errorToString( parsedError) << std::endl;
	out << "Test compile error with line info: " << errorToString( parsedError) << std::endl;
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
Test Error(): Code=0, Line=0, File="", Arg=[], What=[]
Test Error( errno): Code=12, Line=0, File="", Arg=[], What=[#12 "Cannot allocate memory"]
Test Error( code): Code=431, Line=0, File="", Arg=[], What=[#431 "Bad character in a regular expression passed to the lexer"]
Test Error( code, line): Code=449, Line=345, File="", Arg=[], What=[#449 "Incompatible mewa major version. You need a higher version of the mewa Lua module" at line 345]
Test Error( code, Location( filename, line)): Code=411, Line=345, File="file.txt", Arg=[], What=[#411 "Expected table as argument" at line 345 in file "file.txt"]
Test Error( code, param): Code=447, Line=0, File="", Arg=[13.45], What=[#447 "Bad mewa version": 13.45]
Test Error( code, param, line): Code=431, Line=123, File="", Arg=[int, float], What=[#431 "Bad character in a regular expression passed to the lexer" at line 123: int, float]
Test Error( code, param, Location( filename, line)): Code=418, Line=123, File="source.txt", Arg=[int, float], What=[#418 "Too few arguments" at line 123 in file "source.txt": int, float]
Test Error( RuntimeException): Code=402, Line=0, File="", Arg=[], What=[#402 "Runtime error exception"]
Test Error( RuntimeException, param): Code=402, Line=0, File="", Arg=[a runtime error with msg], What=[#402 "Runtime error exception": a runtime error with msg]
Test Error( RuntimeException, param, line): Code=402, Line=234, File="", Arg=[a runtime error with msg and line info], What=[#402 "Runtime error exception" at line 234: a runtime error with msg and line info]
Test Error( RuntimeException, param, line): Code=402, Line=234, File="input.txt", Arg=[a runtime error with msg and location info], What=[#402 "Runtime error exception" at line 234 in file "input.txt": a runtime error with msg and location info]
Test runtime error: Code=402, Line=0, File="", Arg=[Message from outer space!], What=[#402 "Runtime error exception": Message from outer space!]
Test runtime error with line info: Code=402, Line=91, File="", Arg=[Message from outer space with line info!], What=[#402 "Runtime error exception" at line 91: Message from outer space with line info!]
Test runtime error with location info: Code=402, Line=91, File="program.txt", Arg=[Message from outer space with location info!], What=[#402 "Runtime error exception" at line 91 in file "program.txt": Message from outer space with location info!]
Test compile error with line info: Code=420, Line=612, File="", Arg=[This is a compilation error!], What=[Error at line 612: This is a compilation error!]

)"};
		std::ostringstream outputbuf;
		outputbuf << std::endl;

		testError( outputbuf, "Error()",
					Error(), verbose);
		testError( outputbuf, "Error( errno)",
					Error( (Error::Code)12/*ENOMEM*/), verbose);
		testError( outputbuf, "Error( code)",
					Error( Error::IllegalFirstCharacterInLexer), verbose);
		testError( outputbuf, "Error( code, line)",
					Error( Error::IncompatibleMewaMajorVersion, 345), verbose);
		testError( outputbuf, "Error( code, Location( filename, line))",
					Error( Error::ExpectedTableArgument, Error::Location( "file.txt", 345)), verbose);
		testError( outputbuf, "Error( code, param)",
					Error( Error::BadMewaVersion, "13.45"), verbose);
		testError( outputbuf, "Error( code, param, line)",
					Error( Error::IllegalFirstCharacterInLexer, "int, float", 123), verbose);
		testError( outputbuf, "Error( code, param, Location( filename, line))",
					Error( Error::TooFewArguments, "int, float", Error::Location( "source.txt", 123)), verbose);

		testError( outputbuf, "Error( RuntimeException)",
					Error( Error::RuntimeException), verbose);
		testError( outputbuf, "Error( RuntimeException, param)",
					Error( Error::RuntimeException, "a runtime error with msg"), verbose);
		testError( outputbuf, "Error( RuntimeException, param, line)",
					Error( Error::RuntimeException, "a runtime error with msg and line info", 234), verbose);
		testError( outputbuf, "Error( RuntimeException, param, line)",
					Error( Error::RuntimeException, "a runtime error with msg and location info", Error::Location( "input.txt", 234)), verbose);

		testRuntimeException( outputbuf, verbose);
		testRuntimeExceptionWithLineInfo( outputbuf, verbose);
		testRuntimeExceptionWithLocationInfo( outputbuf, verbose);
		testCompileErrorWithLineInfo( outputbuf, verbose);

		outputbuf << "\n";

		std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testError.out", output);
			writeFile( "build/testError.exp", expected);
			std::cerr << "ERR test output differs expected (diff build/testError.out build/testError.exp)" << std::endl;
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

