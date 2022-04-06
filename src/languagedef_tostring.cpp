/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/// \brief Function to print Lexem definitions as Lua table
/// \file "languagedef_tostring.cpp"
#include "languagedef_tostring.hpp"

using namespace mewa;

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

void mewa::printString( std::ostream& outstream, const std::string& str)
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

static void printArgument( std::ostream& outstream, const char* type, const std::vector<std::string>& args)
{
	outstream << type << "= {";
	int ai = 0;
	for (const auto& arg : args)
	{
		if (ai++) outstream << ",";
		printString( outstream, arg);
	}
	outstream << "}";
}

static void printArgument( std::ostream& outstream, const char* type, const std::string& arg)
{
	outstream << type << "=";
	printString( outstream, arg);
}

static void printArgument( std::ostream& outstream, const char* type, int arg)
{
	outstream << type << "=";
	outstream << arg;
}

static void printDecorators( std::ostream& outstream, const std::vector<LanguageDecorator>& decorators)
{
	for (auto const& dc : decorators)
	{
		outstream << ",";
		printArgument( outstream, dc.name.c_str(), dc.content);
	}
}

static void printLexerDefinitions( std::ostream& outstream, const char* indentstr, const Lexer& lexer, const std::vector<Lexer::Definition>& defs, const LanguageDecoratorMap& decorators, int& cnt)
{
	for (auto const& def : defs)
	{
		switch (def.type())
		{
			case Lexer::Definition::BadLexem:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "BAD");
				outstream << ",";
				printArgument( outstream, "name", def.bad());
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;

			case Lexer::Definition::NamedPatternLexem:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "TOKEN");
				outstream << ",";
				printArgument( outstream, "name", def.name());
				outstream << ",";
				printArgument( outstream, "pattern", def.pattern());
				if (def.select() != 0)
				{
					outstream << ",";
					printArgument( outstream, "select", def.select());
				}
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;

			case Lexer::Definition::KeywordLexem:
				break;

			case Lexer::Definition::IgnoreLexem:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "IGNORE");
				outstream << ",";
				printArgument( outstream, "pattern", def.ignore());
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;

			case Lexer::Definition::EolnComment:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "COMMENT");
				outstream << ",";
				printArgument( outstream, "open", def.start());
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;

			case Lexer::Definition::BracketComment:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "COMMENT");
				outstream << ",";
				printArgument( outstream, "open", def.start());
				outstream << ",";
				printArgument( outstream, "close", def.end());
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;

			case Lexer::Definition::IndentLexems:
				outstream << (cnt++ ? indentstr : (indentstr+1));
				outstream << "{";
				printArgument( outstream, "op", "INDENTL");
				outstream << ",";
				printArgument( outstream, "open", lexer.lexemName( def.openind()));
				outstream << ",";
				printArgument( outstream, "close", lexer.lexemName( def.closeind()));
				outstream << ",";
				printArgument( outstream, "nl", lexer.lexemName( def.newline()));
				outstream << ",";
				printArgument( outstream, "tabsize", def.tabsize());
				printDecorators( outstream, decorators.get( def.line()));
				outstream << ",";
				printArgument( outstream, "line", def.line());
				outstream << "}";
				break;
		}
	}
}

static void printProductions( std::ostream& outstream, const char* indentstr, const LanguageDef& langdef, const LanguageDecoratorMap& decorators, int& cnt)
{
	for (auto const& prod : langdef.prodlist)
	{
		outstream << (cnt++ ? indentstr : (indentstr+1));
		outstream << "{";
		printArgument( outstream, "op", "PROD");
		outstream << ",";
		printArgument( outstream, "left", std::string( prod.left.name()));
		outstream << ",right={";
		int ei = 0;
		for (auto const& ee : prod.right)
		{
			if (ei++) outstream << ",";
			outstream << "{";
			const char* type = ee.symbol() ? "name" : "symbol";
			printArgument( outstream, "type", type);
			outstream << ",";
			printArgument( outstream, "value", std::string( ee.name()));
			outstream << "}";
		}
		outstream << "}";
		if (prod.priority.defined())
		{
			outstream << ",";
			printArgument( outstream, "priority", prod.priority.tostring());
		}
		switch (prod.scope)
		{
			case Automaton::Action::NoScope:
			break;
			case Automaton::Action::Scope:
				outstream << ",";
				printArgument( outstream, "scope", "{}");
			break;
			case Automaton::Action::Step:
				outstream << ",";
				printArgument( outstream, "scope", ">>");
			break;
		}
		if (prod.callidx)
		{
			outstream << ",";
			printArgument( outstream, "call", langdef.calls[ prod.callidx-1].tostring());
		}
		printDecorators( outstream, decorators.get( prod.line));
		outstream << ",";
		printArgument( outstream, "line", prod.line);
		outstream << "}";
	}
}

std::string mewa::printLuaLanguageDefinition( const LanguageDef& langdef, const LanguageDecoratorMap& decoratormap)
{
	const char* indentstr1 = ",\n\t";
	const char* indentstr2 = ",\n\t\t";
	std::ostringstream out;
	out << "local languagedef = {\n";
	out << "\tVERSION = \"" << MEWA_VERSION_STRING << "\"";
	out << indentstr1;
	printArgument( out, "LANGUAGE", langdef.language);
	out << indentstr1;
	printArgument( out, "TYPESYSTEM", langdef.typesystem);
	out << indentstr1;
	printArgument( out, "CMDLINE", langdef.cmdline);
	out << indentstr1 << "RULES = {";
	auto const& defs = langdef.lexer.getDefinitions();
	int cnt = 0;
	printLexerDefinitions( out, indentstr2, langdef.lexer, defs, decoratormap, cnt);
	printProductions( out, indentstr2, langdef, decoratormap, cnt);
	out << "\n\t}\n}\nreturn languagedef\n\n";
	std::string rt = out.str();
	return rt;
}

