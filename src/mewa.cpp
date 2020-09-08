/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Program for creating the lua module for mewa
/// \file "mewa.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "automaton.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <string>
#include <vector>

using namespace mewa;

#define ERRCODE_MEMORY_ALLOCATION	12 /*ENOMEM*/
#define ERRCODE_RUNTIME_ERROR 		-2
#define ERRCODE_INVALID_ARGUMENTS 	-1

static const char* g_typeSystemModulePrefix = "typesystem.";

static void printUsage()
{
	std::cerr << "Usage: mewa [-h][-v][-g][-o OUTF][-d DBGOUTF] INPUTFILE" << std::endl;
	std::cerr << "Description: Build a lua module implementing a compiler described as\n";
	std::cerr << "             an EBNF and some lua hooks implementing the type system\n";
	std::cerr << "             and the code generation.\n";
	std::cerr << "Options:\n";
	std::cerr << " -h         : Print this usage\n";
	std::cerr << " -v         : Verbose output to stderr\n";
	std::cerr << " -g         : Action do generate parsing tables for import by lua module for 'mewa'\n";
	std::cerr << " -o <OUTF>  : Write the output to file with path OUTF instead of stderr.\n";
	std::cerr << " -d <DBGF>  : Write the debug output to file with path DBGF instead of stdout.\n";
	std::cerr << "Arguments:\n";
	std::cerr << "INPUTFILE   : Contains the EBNF of the grammar to process with the Lua hooks embedded.\n";
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

template <typename TABLETYPE>
static void printTable( std::ostream& outstream, const char* tablename, const TABLETYPE& table, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto keyval : table)
	{
		outstream << ((tidx++) ? ",\n\t\t" : "\n\t\t");
		outstream << "[" << keyval.first.packed() << "] = " << keyval.second.packed();
	}
	outstream << (sep ? "},\n" : "}\n");
}

static bool isConstantArgument( const std::string& val)
{
	if (val.empty()) return false;
	char ch = val[0];
	if (ch >= '0' || ch <= '9') return true;
	if (ch == '-') return true;
	return false;
}

static void printCallTable( std::ostream& outstream, const char* tablename, const std::vector<Automaton::Call>& table, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto call : table)
	{
		outstream << ((tidx++) ? ",\n\t\t" : "\n\t\t");
		switch (call.argtype())
		{
			case Automaton::Call::NoArg:
				outstream << g_typeSystemModulePrefix << call.function();
				break;
			case Automaton::Call::StringArg:
				outstream << "{" << g_typeSystemModulePrefix << call.function() << ", \"" << call.arg() << "\"}";
				break;
			case Automaton::Call::ReferenceArg:
				if (isConstantArgument( call.arg()))
				{
					outstream << "{" << g_typeSystemModulePrefix << call.function() << ", " << call.arg() << "}";
				}
				else
				{
					outstream << "{" << g_typeSystemModulePrefix << call.function() << ", " << g_typeSystemModulePrefix << call.arg() << "}";
				}
				break;
		}
	}
	outstream << (sep ? "},\n" : "}\n");
}

static void printAutomaton( const std::string& filename, const Automaton& automaton)
{
	std::ostringstream outstream;
	outstream << "typesystem = require(\"typesystem\")\n";
	outstream << "compiler = {\n";
	if (!automaton.language().empty())
	{
		outstream << "\tlanguage = \"" << automaton.language() << "\"";
	}
	printTable( outstream, "action", automaton.actions(), true/*sep*/);
	printTable( outstream, "gto", automaton.gotos(), true/*sep*/);
	std::string callprefix = "typesystem.";
	printCallTable( outstream, "call", automaton.calls(), false/*sep*/);
	outstream << "}\n\n";

	if (filename.empty())
	{
		std::cout << outstream.str();
	}
	else
	{
		writeFile( filename, outstream.str());
	}
}

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		enum Command {
			NoCommand,
			GenerateTables
		};
		Command cmd = NoCommand;
		std::string inputFilename;
		std::string outputFilename;
		std::string debugFilename;
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-v"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				printUsage();
			}
			else if (0==std::strcmp( argv[argi], "-o"))
			{
				++argi;
				if (argi == argc || argv[argi][0] == '-') 
				{
					std::cerr << "Option -o requires a file path as argument" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				outputFilename = argv[argi];
			}
			else if (0==std::strcmp( argv[argi], "-d"))
			{
				++argi;
				if (argi == argc || argv[argi][0] == '-') 
				{
					std::cerr << "Option -d requires a file path as argument" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				debugFilename = argv[argi];
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-g"))
			{
				if (cmd != NoCommand)
				{
					std::cerr << "Conflicting options" << std::endl << std::endl;
					printUsage();
					return ERRCODE_INVALID_ARGUMENTS;
				}
				cmd = GenerateTables;
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
		for (auto warning : warnings) printWarning( inputFilename, warning);

		switch (cmd)
		{
			case NoCommand:
				break;
			case GenerateTables:
				printAutomaton( outputFilename, automaton);
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
