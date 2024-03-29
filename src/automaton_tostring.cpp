/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief LALR(1) Parser generator tostring (Lua syntax) implementation
/// \file "automaton_tostring.cpp"
#include "automaton.hpp"
#include "languagedef_tostring.hpp"
#include "version.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>

using namespace mewa;

static const char* g_typeSystemModulePrefix = "typesystem.";

template <typename TABLETYPE>
static void printTable( std::ostream& outstream, const char* tablename, const TABLETYPE& table, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto keyval : table)
	{
		const char* sepc = tidx ? ",":"";
		const char* shft = ((tidx & 3) == 0) ? "\n\t\t" : "\t";
		++tidx;
		outstream << sepc << shft << "[" << keyval.first.packed() << "] = " << keyval.second.packed();
	}
	outstream << (sep ? "},\n" : "}\n");
}

static void printStringArray( std::ostream& outstream, const char* tablename, const std::vector<std::string>& list, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto element : list)
	{
		const char* sepc = tidx ? ",":"";
		const char* shft = ((tidx & 3) == 0) ? "\n\t\t" : " ";
		++tidx;

		outstream << sepc << shft << "\"" << element << "\"";
	}
	outstream << (sep ? "},\n" : "}\n");
}

static bool isConstantArgument( const std::string& val)
{
	if (val.empty()) return false;
	char ch = val[0];
	if (ch >= '0' && ch <= '9') return true;
	if (ch == '{' || ch == '-') return true;
	return false;
}

static void printLexer( std::ostream& outstream, const char* tablename, const Lexer& lexer, bool sep)
{
	auto definitions = lexer.getDefinitions();
	if (!definitions.empty())
	{
		std::map<std::string, std::vector<int> > defclassmap;
		int di = 0;
		for (auto def : definitions)
		{
			defclassmap[ def.typeName()].push_back( di++);
		}
		outstream << "\t" << tablename << " = {";

		int defclassidx = 0;
		for (auto defclass : defclassmap)
		{
			outstream << (defclassidx ? ",\n\t\t" : "\n\t\t") << defclass.first << " = {";
			++defclassidx;

			int defcnt = 0;
			for (auto defidx : defclass.second)
			{
				auto const& def = definitions[ defidx];
				const char* sepc = defcnt ? ",":"";
				const char* shft = "\n\t\t\t";

				switch (def.type())
				{
					case Lexer::Definition::BadLexem:
						outstream << sepc;
						printString( outstream, def.bad());
						break;
					case Lexer::Definition::NamedPatternLexem:
						outstream << sepc << shft << "{ ";
						printString( outstream, def.name());
						outstream << ", ";
						printString( outstream, def.pattern());
						if (def.select() != 0)
						{
							outstream << ", " << def.select();
						}
						outstream << " }";
						break;
					case Lexer::Definition::KeywordLexem:
						outstream << sepc << ((defcnt & 7) == 0 ? shft : " ");
						printString( outstream, def.name());
						break;
					case Lexer::Definition::IgnoreLexem:
						outstream << sepc;
						printString( outstream, def.ignore());
						break;
					case Lexer::Definition::EolnComment:
						outstream << sepc;
						printString( outstream, def.start());
						break;
					case Lexer::Definition::BracketComment:
						outstream << sepc << "{ ";
						printString( outstream, def.start());
						outstream << ", ";
						printString( outstream, def.end());
						outstream << " }";
						break;
					case Lexer::Definition::IndentLexems:
						outstream << " " << def.openind() << ", " << def.tabsize();
						break;
				}
				++defcnt;
			}
			outstream << " }";
		}
		outstream << (sep ? "},\n" : "}\n");
	}
}

static void printCallTable( std::ostream& outstream, const char* tablename, const std::vector<Automaton::Call>& table, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto call : table)
	{
		outstream << ((tidx++) ? ",\n\t\t{ " : "\n\t\t{ ");

		switch (call.argtype())
		{
			case Automaton::Call::NoArg:
				outstream << "name=";
				printString( outstream, call.function());
				outstream << ", proc=" << g_typeSystemModulePrefix << call.function();
				break;
			case Automaton::Call::StringArg:
				outstream << "name=";
				printString( outstream, call.function() + " " + call.arg());
				outstream << ", proc=" << g_typeSystemModulePrefix << call.function();
				outstream << ", obj=\"" << call.arg() << "\"";
				break;
			case Automaton::Call::ReferenceArg:
				outstream << "name=";
				printString( outstream, call.function() + " " + call.arg());
				if (isConstantArgument( call.arg()))
				{
					outstream << ", proc=" << g_typeSystemModulePrefix << call.function();
					outstream << ", obj=" << call.arg();
				}
				else
				{
					outstream << ", proc=" << g_typeSystemModulePrefix << call.function();
					outstream << ", obj=" << g_typeSystemModulePrefix << call.arg();
				}
				break;
		}
		outstream << "}";
	}
	outstream << (sep ? "},\n" : "}\n");
}

std::string Automaton::tostring() const
{
	std::ostringstream outstream;
	outstream << "{\n";
	outstream << "\tmewa = \"" << MEWA_VERSION_STRING << "\",\n";
	if (!language().empty())
	{
		outstream << "\tlanguage = \"" << language() << "\",\n";
	}
	if (!typesystem().empty())
	{
		outstream << "\ttypesystem = \"" << typesystem() << "\",\n";
	}
	if (!cmdline().empty())
	{
		outstream << "\tcmdline = \"" << cmdline() << "\",\n";
	}
	printLexer( outstream, "lexer", lexer(), true/*sep*/);
	printStringArray( outstream, "nonterminal", m_nonterminals, true/*sep*/);
	printTable( outstream, "action", actions(), true/*sep*/);
	printTable( outstream, "gto", gotos(), true/*sep*/);
	std::string callprefix = "typesystem.";
	printCallTable( outstream, "call", calls(), false/*sep*/);
	outstream << "}\n";
	return outstream.str();
}

