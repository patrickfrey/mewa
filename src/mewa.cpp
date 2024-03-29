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
#include "automaton_parser.hpp"
#include "languagedef_tostring.hpp"
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
	std::cerr << "Usage: mewa [-h][-v][-V][-s][-g][-b LUABIN][-o OUTF][-d DBGOUTF][-t TEMPLAT] INPFILE" << std::endl;
	std::cerr << "Description: Build a lua module implementing a compiler described in\n";
	std::cerr << "             a Bison/Yacc-like BNF dialect with lua node function calls implementing\n";
	std::cerr << "             the type system and the code generation.\n";
	std::cerr << "Options:\n";
	std::cerr << " --help,\n";
	std::cerr << " -h           : Print this usage.\n";
	std::cerr << " --version,\n";
	std::cerr << " -v           : Print the current version of mewa.\n";
	std::cerr << " --verbose,\n";
	std::cerr << " -V           : Verbose output to stderr.\n";
	std::cerr << " --generate-compiler,\n";
	std::cerr << " -g           : Generate a compiler as a Lua module.\n";
	std::cerr << " --generate-template,\n";
	std::cerr << " -s           : Generate a template for your Lua module implementing the typesystem.\n";
	std::cerr << "                Extracts all Lua function calls from the grammar and prints their\n";
	std::cerr << "                empty implementation stubs to the output. No debug output is provided.\n";
	std::cerr << " --generate-language,\n";
	std::cerr << " -l           : Generate a Lua table with the language description parsed for Lua\n";
	std::cerr << "                scripts generating descriptions for interfacing with other tools.\n";
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
	if (error.location().line())
	{
		std::cerr << "Warning on line " << error.location().line() << " of " << filename << ": ";
	}
	else
	{
		std::cerr << "Warning in " << filename << ": ";
	}
	std::cerr << error.what() << std::endl;
}

static const std::string g_defaultTemplate{R"(#!%luabin%

typesystem = require( "%typesystem%")
cmdline = require( "%cmdline%")
mewa = require("mewa")

compilerdef = %automaton%

ccmd = cmdline.parse( "%language%", arg)

compiler = mewa.compiler( compilerdef)
compiler:run( ccmd.target, ccmd.options, ccmd.input, ccmd.output, ccmd.debug)
)"};

static const std::string g_defaultTemplate_no_cmdline{R"(#!%luabin%

typesystem = require( "%typesystem%")
mewa = require("mewa")

compilerdef = %automaton%

if not arg[1] then error( "Missign argument (program to compile)\n") end

compiler = mewa.compiler( compilerdef)
compiler:run( nil, nil, arg[1])
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
			GenerateTypesystemTemplateForLua,
			GenerateLanguageDescriptionForLua
		};
		Command cmd = NoCommand;
		std::string inputFilename;
		std::string outputFilename;
		std::string debugFilename;
		std::string templat;
		std::string luabin;

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
				return 0;
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
			else if (0==std::strcmp( argv[argi], "-s") || 0==std::strcmp( argv[argi], "--generate-template"))
			{
				if (cmd != NoCommand || !luabin.empty() || !templat.empty() || !debugFilename.empty())
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				cmd = GenerateTypesystemTemplateForLua;
			}
			else if (0==std::strcmp( argv[argi], "-l") || 0==std::strcmp( argv[argi], "--generate-language"))
			{
				if (cmd != NoCommand)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				cmd = GenerateLanguageDescriptionForLua;
			}
			else if (0==std::memcmp( argv[argi], "-b", 2) || 0==std::memcmp( argv[argi], "--luabin=", 9))
			{
				if (cmd == GenerateTypesystemTemplateForLua)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
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
				if (cmd == GenerateTypesystemTemplateForLua)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
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
				if (cmd == GenerateTypesystemTemplateForLua)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
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
		if (luabin.empty())
		{
			luabin = "/usr/bin/lua";
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
		std::string output;
		if (cmd == GenerateTypesystemTemplateForLua)
		{
			LanguageDef langdef( parseLanguageDef( source));
			output = printLuaTypeSystemStub( langdef);
		}
		else if (cmd == GenerateLanguageDescriptionForLua)
		{
			std::set<int> lines;
			LanguageDef langdef( parseLanguageDef( source));
			for (auto const& prod: langdef.prodlist)
			{
				if (prod.line)
				{
					lines.insert( prod.line);
				}
			}
			std::vector<Lexer::Definition> ldar = langdef.lexer.getDefinitions();
			bool predLine = false;
			for (auto const& lxdef: ldar)
			{
				if (lxdef.line())
				{
					if (predLine)
					{
						lines.insert( lxdef.line()-1);
						predLine = false;
					}
					lines.insert( lxdef.line());
				}
				else
				{
					predLine = true;
				}
			}
			LanguageDecoratorMap decoratormap( parseLanguageDecoratorMap( source));
			output = printLuaLanguageDefinition( langdef, decoratormap);
		}
		else
		{
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
					templat = g_defaultTemplate;
				}
			}
		}
		switch (cmd)
		{
			case NoCommand:
				break;
			case GenerateCompilerForLua:
				printAutomaton( outputFilename, templat, automaton, luabin);
				break;
			case GenerateTypesystemTemplateForLua:
				if (outputFilename.empty())
				{
					std::cout << output << std::endl;
				}
				else
				{
					writeFile( outputFilename, output);
				}
				break;
			case GenerateLanguageDescriptionForLua:
				if (outputFilename.empty())
				{
					std::cout << output << std::endl;
				}
				else
				{
					writeFile( outputFilename, output);
				}
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
