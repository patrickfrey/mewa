/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Program for creating the lua module for mewa
/// \file "mewa.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "automaton.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include "version.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>
#include <map>

using namespace mewa;

#define ERRCODE_MEMORY_ALLOCATION	3
#define ERRCODE_RUNTIME_ERROR 		2
#define ERRCODE_INVALID_ARGUMENTS 	1

static void printUsage()
{
	std::cerr << "Usage: mewa [-h][-v][-V][-g][-b LUABIN][-o OUTF][-d DBGOUTF][-t TEMPLAT] INPFILE" << std::endl;
	std::cerr << "Description: Build a lua module implementing a compiler described in\n";
	std::cerr << "             a Bison/Yacc-like BNF dialect with lua callbacks implementing the type\n";
	std::cerr << "             system and the code generation.\n";
	std::cerr << "Options:\n";
	std::cerr << " --help,\n";
	std::cerr << " -h           : Print this usage.\n";
	std::cerr << " --version,\n";
	std::cerr << " -v           : Print the current version of mewa.\n";
	std::cerr << " --verbose,\n";
	std::cerr << " -V           : Do verbose output to stderr.\n";
	std::cerr << " --generate-compiler,\n";
	std::cerr << " -g           : Do generate a compiler as a Lua module.\n";
	std::cerr << " --luabin <LUABIN>,\n";
	std::cerr << " -b <LUABIN>  : Specify the path of the Lua program in the header of generated scripts.\n";
	std::cerr << " --output <OUTF>,\n";
	std::cerr << " -o <OUTF>    : Write the output to file with path OUTF instead of stderr.\n";
	std::cerr << " --dbgout <DBGF>,\n";
	std::cerr << " -d <DBGF>    : Write the debug output to file with path DBGF instead of stdout.\n";
	std::cerr << " --template <TEMPLATE>,\n";
	std::cerr << " -t <TEMPLATE>: Use content of file TEMPLATE as template for generated lua module (-g).\n";
	std::cerr << "Arguments:\n";
	std::cerr << "INPFILE       : Contains the description of the grammar to process\n";
	std::cerr << "                attributed with the Lua hooks doing the job.\n";
}

static void printWarning( const std::string& filename, const Error& error)
{
	if (error.line())
	{
		std::cerr << "Warning on line " << error.line() << " of " << filename << ": ";
	}
	else
	{
		std::cerr << "Warning in " << filename << ": ";
	}
	std::cerr << error.what() << std::endl;
}

static const std::string g_defaultTemplate_no_cmdline{R"(#!%luabin%

typesystem = require( "%typesystem%")
mewa = require("mewa")

compilerdef = %automaton%

if #arg == 0 then error( "arguments missing") end
if #arg > 1 then error( "too many arguments") end
compiler = mewa.compiler( compilerdef)
compiler:run( arg[0])
)"};

static const std::string g_defaultTemplate_with_cmdline{R"(#!%luabin%

typesystem = require( "%typesystem%")
cmdline = require( "%cmdline%")
mewa = require("mewa")

compilerdef = %automaton%

inputfile,outputfile,dbgout = cmdline:parse( "%language%", typesystem, arg)

compiler = mewa.compiler( compilerdef)
compiler:run( inputfile, outputfile, dbgout)
)"};



static std::string mapTemplateKey( const std::string& content, const char* key, const std::string& value)
{
	std::string rt;
	std::string pattern = std::string("%") + key + "%";
	char const* pp = std::strstr( content.c_str(), pattern.c_str());
	if (pp)
	{
		rt.append( content.c_str(), pp-content.c_str());
		rt.append( value);
		rt.append( pp + pattern.size());
	}
	else
	{
		rt.append( content);
	}
	return rt;
}

static std::string mapTemplate( const std::string& templat, const Automaton& automaton, const std::string& luabin)
{
	std::string rt = templat;
	rt = mapTemplateKey( rt, "luabin", luabin);
	rt = mapTemplateKey( rt, "typesystem", automaton.typesystem());
	rt = mapTemplateKey( rt, "language", automaton.language());
	rt = mapTemplateKey( rt, "cmdline", automaton.cmdline());
	rt = mapTemplateKey( rt, "automaton", automaton.tostring());
	return rt;
}

static void printAutomaton( const std::string& filename, const std::string& templat, const Automaton& automaton, const std::string& luabin)
{
	if (filename.empty())
	{
		std::cout << mapTemplate( templat, automaton, luabin) << std::endl;
	}
	else
	{
		std::string content = mapTemplate( templat, automaton, luabin);
		content.push_back( '\n');
		writeFile( filename, content);
	}
}

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		enum Command {
			NoCommand,
			GenerateCompilerForLua,
		};
		Command cmd = NoCommand;
		std::string inputFilename;
		std::string outputFilename;
		std::string debugFilename;
		std::string templat;
		std::string luabin = "/usr/bin/lua";

		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V") || 0==std::strcmp( argv[argi], "--verbose"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-v") || 0==std::strcmp( argv[argi], "--version"))
			{
				std::cout << "mewa version " << MEWA_VERSION_STRING << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "-h") || 0==std::strcmp( argv[argi], "--help"))
			{
				printUsage();
			}
			else if (0==std::strcmp( argv[argi], "-g") || 0==std::strcmp( argv[argi], "--generate-compiler"))
			{
				if (cmd != NoCommand)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				cmd = GenerateCompilerForLua;
			}
			else if (0==std::memcmp( argv[argi], "-b", 2) || 0==std::memcmp( argv[argi], "--luabin=", 9))
			{
				int optofs = (argv[argi][1] == '-') ? 9:2;
				if (argv[argi][ optofs])
				{
					luabin = argv[argi]+optofs;
				}
				else
				{
					++argi;
					if (argi == argc || argv[argi][0] == '-') 
					{
						std::cerr << "Option -b,--luabin requires a path (lua program) as argument" << std::endl << std::endl;
						printUsage();
						return ERRCODE_INVALID_ARGUMENTS;
					}
					luabin = argv[argi];
				}
			}
			else if (0==std::memcmp( argv[argi], "-o", 2) || 0==std::memcmp( argv[argi], "--output=", 9))
			{
				int optofs = (argv[argi][1] == '-') ? 9:2;
				if (argv[argi][ optofs])
				{
					outputFilename = argv[argi]+optofs;
				}
				else
				{
					++argi;
					if (argi == argc || argv[argi][0] == '-') 
					{
						std::cerr << "Option -o,--output requires a file path as argument" << std::endl << std::endl;
						printUsage();
						return ERRCODE_INVALID_ARGUMENTS;
					}
					outputFilename = argv[argi];
				}
			}
			else if (0==std::memcmp( argv[argi], "-d", 2) || 0==std::memcmp( argv[argi], "--dbgout=", 9))
			{
				int optofs = (argv[argi][1] == '-') ? 9:2;
				if (argv[argi][ optofs])
				{
					debugFilename = argv[argi]+optofs;
					verbose = true;
				}
				else
				{
					++argi;
					if (argi == argc || argv[argi][0] == '-') 
					{
						std::cerr << "Option -d,--dbgout requires a file path as argument" << std::endl << std::endl;
						printUsage();
						return ERRCODE_INVALID_ARGUMENTS;
					}
					debugFilename = argv[argi];
					verbose = true;
				}
			}
			else if (0==std::memcmp( argv[argi], "-t", 2) || 0==std::memcmp( argv[argi], "--template=", 11))
			{
				int optofs = (argv[argi][1] == '-') ? 11:2;
				if (argv[argi][ optofs])
				{
					templat = argv[argi]+optofs;
				}
				else
				{
					++argi;
					if (argi == argc || argv[argi][0] == '-') 
					{
						std::cerr << "Option -t requires a file path as argument" << std::endl << std::endl;
						printUsage();
						return ERRCODE_INVALID_ARGUMENTS;
					}
					templat = readFile( argv[argi]);
				}
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Unknown program option " << argv[argi] << std::endl << std::endl;
				printUsage();
				return ERRCODE_INVALID_ARGUMENTS;
			}
			else
			{
				break;
			}
		}
		if (argi == argc)
		{
			std::cerr << "Too few arguments, input file expected" << std::endl << std::endl;
			printUsage();
			return ERRCODE_INVALID_ARGUMENTS;
		}
		if (argi + 1 < argc)
		{
			std::cerr << "Too many arguments, only input file expected" << std::endl << std::endl;
			printUsage();
			return ERRCODE_INVALID_ARGUMENTS;
		}
		if (!outputFilename.empty() && cmd == NoCommand)
		{
			std::cerr << "Output file but no action defined, nothing is written to " << outputFilename << std::endl;
			printUsage();
			return ERRCODE_INVALID_ARGUMENTS;
		}
		inputFilename = argv[ argi];
		std::string source = readFile( inputFilename);

		std::vector<Error> warnings;
		Automaton automaton;
		if (debugFilename.empty())
		{
			automaton.build( source, warnings, Automaton::DebugOutput().enable( verbose ? Automaton::DebugOutput::All : Automaton::DebugOutput::None));
		}
		else
		{
			std::stringstream dbgoutstream;
			automaton.build( source, warnings, Automaton::DebugOutput( dbgoutstream).enable( Automaton::DebugOutput::All));
			std::string dbgoutput = dbgoutstream.str();
			writeFile( debugFilename, dbgoutput);
		}
		if (!warnings.empty())
		{
			for (auto warning : warnings)
			{
				printWarning( inputFilename, warning);
			}
			throw mewa::Error( mewa::Error::ConflictsInGrammarDef, mewa::string_format( "%zu", warnings.size()));
		}
		if (templat.empty())
		{
			if (automaton.cmdline().empty())
			{
				templat = g_defaultTemplate_no_cmdline;
			}
			else
			{
				templat = g_defaultTemplate_with_cmdline;
			}
		}
		switch (cmd)
		{
			case NoCommand:
				break;
			case GenerateCompilerForLua:
				printAutomaton( outputFilename, templat, automaton, luabin);
				break;
		}
		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << err.what() << std::endl;
		return (int)err.code() < 128 ? (int)err.code() /*errno*/ : ERRCODE_RUNTIME_ERROR;
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERR runtime " << err.what() << std::endl;
		return ERRCODE_RUNTIME_ERROR;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "ERR out of memory" << std::endl;
		return ERRCODE_MEMORY_ALLOCATION;
	}
	return 0;
}
