/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief LALR(1) Parser generator tostring (Lua syntax) implementation
/// \file "automaton_tostring.cpp"
#include "automaton.hpp"
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

static bool isConstantArgument( const std::string& val)
{
	if (val.empty()) return false;
	char ch = val[0];
	if (ch >= '0' && ch <= '9') return true;
	if (ch == '-') return true;
	return false;
}

static std::string escapeString( const std::string& str)
{
	std::string rt;
	char const* si = str.c_str();
	for (; *si; ++si)
	{
		if (*si == '\\')
		{
			rt.append( "\\\\");
		}
		else
		{
			rt.push_back( *si);
		}
	}
	return rt;
}

static void printString( std::ostream& outstream, const std::string& str)
{
	char const* sqpos = std::strchr( str.c_str(), '\'');
	char const* dqpos = std::strchr( str.c_str(), '\"');
	if (sqpos && dqpos)
	{
		sqpos = 0;
		dqpos = 0;
		char const* si = str.c_str();
		for (; si; ++si)
		{
			if (*si == '\\')
			{
				++si;
			}
			else if (*si == '\'')
			{
				sqpos = si;
			}
			else if (*si == '\"')
			{
				dqpos = si;
			}
		}
	}
	if (dqpos)
	{
		if (sqpos) throw Error( Error::EscapeQuoteErrorInString, str);
		outstream << "\'" << escapeString( str) << "\'";
	}
	else
	{
		outstream << "\"" << escapeString( str) << "\"";
	}
}

static void printCallTable( std::ostream& outstream, const char* tablename, const std::vector<Automaton::Call>& table, bool sep)
{
	outstream << "\t" << tablename << " = {";
	int tidx = 0;
	for (auto call : table)
	{
		outstream << ((tidx++) ? ",\n\t\t{ " : "\n\t\t{ ");
		printString( outstream, call.function() + " " + call.arg());

		switch (call.argtype())
		{
			case Automaton::Call::NoArg:
				outstream << ", " << g_typeSystemModulePrefix << call.function();
				break;
			case Automaton::Call::StringArg:
				outstream << ", " << g_typeSystemModulePrefix << call.function() << ", \"" << call.arg() << "\"}";
				break;
			case Automaton::Call::ReferenceArg:
				if (isConstantArgument( call.arg()))
				{
					outstream << ", " << g_typeSystemModulePrefix << call.function() << ", " << call.arg();
				}
				else
				{
					outstream << ", " << g_typeSystemModulePrefix << call.function() << ", " << g_typeSystemModulePrefix << call.arg();
				}
				break;
		}
		outstream << "}";
	}
	outstream << (sep ? "},\n" : "}\n");
}

static void printLexems( std::ostream& outstream, const char* tablename, const Lexer& lexer, bool sep)
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
				}
				++defcnt;
			}
			outstream << " }";
		}
		outstream << (sep ? "},\n" : "}\n");
	}
}

std::string Automaton::tostring() const
{
	std::ostringstream outstream;
	outstream << "{\n";
	if (!language().empty())
	{
		outstream << "\tlanguage = \"" << language() << "\",\n";
	}
	if (!typesystem().empty())
	{
		outstream << "\ttypesystem = \"" << typesystem() << "\",\n";
	}
	printLexems( outstream, "lexer", lexer(), true/*sep*/);
	printTable( outstream, "action", actions(), true/*sep*/);
	printTable( outstream, "gto", gotos(), true/*sep*/);
	std::string callprefix = "typesystem.";
	printCallTable( outstream, "call", calls(), false/*sep*/);
	outstream << "}\n";
	return outstream.str();
}

