/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator and parser implementation
/// \file "grammar.cpp"
#include "grammar.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include <map>
#include <vector>
#include <string>
#include <string_view>

using namespace mewa;

class GrammarLexer
	:public Lexer
{
public:
	enum LexemType
	{
		ERROR=-1,
		NONE=0,
		IDENT=1,
		NUMBER,
		DQSTRING,
		SQSTRING,
		PERCENT,
		SLASH,
		EQUAL,
		DDOT,
		SEMICOLON
	};

	GrammarLexer()
	{
		defineLexem( "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		defineLexem( "NUMBER", "[0-9]+");
		defineLexem( "DQSTRING", "[\"]((([^\\\\\"\\n])|([\\\\][^\"\\n]))*)[\"]", 1);
		defineLexem( "SQSTRING", "[\']((([^\\\\\'\\n])|([\\\\][^\'\\n]))*)[\']", 1);
		defineLexem( "%");
		defineLexem( "/");
		defineLexem( "=");
		defineLexem( ":");
		defineLexem( ";");
	}
};

class ProductionNodeDef
{
public:
	enum Type
	{
		Unresolved,
		NonTerminal,
		Terminal
	};

public:
	ProductionNodeDef( const std::string_view& name_)
		:m_name(name_),m_type(Unresolved),m_index(0){}
	ProductionNodeDef( const ProductionNodeDef& o)
		:m_name(o.m_name),m_type(o.m_type),m_index(o.m_index){}
	ProductionNodeDef& operator=( const ProductionNodeDef& o)
		{m_name=o.m_name; m_type=o.m_type; m_index=o.m_index;
		 return *this;}
	ProductionNodeDef( ProductionNodeDef&& o)
		:m_name(std::move(o.m_name)),m_type(o.m_type),m_index(o.m_index){}
	ProductionNodeDef& operator=( ProductionNodeDef&& o)
		{m_name=std::move(o.m_name); m_type=o.m_type; m_index=o.m_index;
		 return *this;}

	const std::string_view& name() const		{return m_name;}
	Type type() const				{return m_type;}
	int index() const				{return m_index;}

	void defineAsTerminal( int index_)		{m_type = Terminal; m_index = index_;}
	void defineAsNonTerminal( int index_)		{m_type = NonTerminal; m_index = index_;}

private:
	std::string_view m_name;
	Type m_type;
	int m_index;
};

typedef std::vector<ProductionNodeDef> ProductionNodeDefList;

static int convertStringToInt( const std::string_view& str)
{
	int si = 0, se = str.size();
	int rt = 0;
	for (; si != se; ++si)
	{
		char ch = str[ si];
		if (ch >= '0' && ch <= '9')
		{
			rt = rt * 10 + (ch - '0');
			if (rt < 0) throw Error( Error::ValueOutOfRangeInGrammarDef, str);
		}
		else
		{
			throw Error( Error::ExpectedNumberInGrammarDef, str);
		}
	}
	return rt;
}

static bool caseInsensitiveCompare( const std::string_view& a, const std::string_view& b)
{
	auto ai = a.begin(), ae = a.end(), bi = b.begin(), be = b.end();
	for (; ai != ae && bi != be; ++ai,++bi)
	{
		if (std::tolower(*ai) != std::tolower(*bi)) return false;
	}
	return ai == ae && bi == be;
}

static Grammar loadGrammar( const std::string& fileName, const std::string& source)
{
	enum State {
		Init,
		ParsePriority,
		ParsePriorityNumber,
		ParseAssign,
		ParseProductionElement,
		ParsePattern,
		ParsePatternSelect,
		ParseEndOfLexemDef,
		ParseLexerCommand,
		ParseLexerCommandArg
	};
	State state = Init;
	GrammarLexer grammarLexer;
	Scanner scanner( source);
	Lexer lexer;
	Lexem lexem( 1/*line*/);

	try
	{
		// [1] Parse grammar:
		std::string_view rulename;
		std::string_view patternstr;
		std::string_view cmdname;
		std::vector<std::string_view> cmdargs;
		std::multimap<std::string_view, ProductionNodeDefList> prodmap;
		std::map<std::string_view, int> nonTerminalIdMap;
		ProductionNodeDefList prod;
		int priority = 0;
		int selectidx = 0;

		lexem = grammarLexer.next( scanner);
		for (; !lexem.empty(); lexem = grammarLexer.next( scanner))
		{
			switch ((GrammarLexer::LexemType)lexem.id())
			{
				case GrammarLexer::ERROR:
					throw Error( Error::BadCharacterInGrammarDef);
				case GrammarLexer::NONE:
					throw Error( Error::LogicError); //... loop exit condition (lexem.empty()) met
				case GrammarLexer::IDENT:
					if (state == Init)
					{
						rulename = lexem.value();
						prod.clear();
						patternstr.remove_suffix( patternstr.size());
						priority = 0;
						selectidx = 0;
						state = ParsePriority;
					}
					else if (state == ParseProductionElement)
					{
						prod.push_back( lexem.value());
					}
					else if (state == ParseLexerCommand)
					{
						cmdname = lexem.value();
						state = ParseLexerCommandArg;
					}
					else if (state == ParseLexerCommandArg)
					{
						cmdargs.push_back( lexem.value());
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}	
					break;
				case GrammarLexer::NUMBER:
					if (state == ParsePriorityNumber)
					{
						priority = convertStringToInt( lexem.value());
						state = ParseAssign;
					}
					else if (state == ParsePatternSelect)
					{
						selectidx = convertStringToInt( lexem.value());
						state = ParseEndOfLexemDef;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::DQSTRING:
				case GrammarLexer::SQSTRING:
					if (state == ParsePattern)
					{
						patternstr = lexem.value();
						state = ParsePriority;
					}
					else if (state == ParseLexerCommandArg)
					{
						cmdargs.push_back( lexem.value());
					}
					break;
				case GrammarLexer::PERCENT:
					if (state == Init)
					{
						cmdname.remove_suffix( cmdname.size());
						cmdargs.clear();
						state = ParseLexerCommand;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::SLASH:
					if (state == ParsePriority)
					{
						state = ParsePriorityNumber;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::EQUAL:
					if (state == ParsePriority || state == ParseAssign)
					{
						state = ParseProductionElement;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::DDOT:
					if (state == ParseAssign)
					{
						throw Error( Error::PriorityDefNotForLexemsInGrammarDef);
					}
					else if (state == ParsePriority)
					{
						state = ParsePattern;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}	
					break;
				case GrammarLexer::SEMICOLON:
					if (state == ParseLexerCommandArg)
					{
						if (caseInsensitiveCompare( cmdname, "IGNORE"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							lexer.defineIgnore( cmdargs[ 0]);
						}
						else if (caseInsensitiveCompare( cmdname, "BAD"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							lexer.defineBadLexem( cmdargs[ 0]);
						}
						else if (caseInsensitiveCompare( cmdname, "COMMENT"))
						{
							if (cmdargs.size() == 1)
							{
								lexer.defineEolnComment( cmdargs[ 0]);
							}
							else if (cmdargs.size() == 2)
							{
								lexer.defineBracketComment( cmdargs[ 0], cmdargs[ 1]);
							}
							else
							{
								throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							}
						}
					}
					else if (state == ParsePatternSelect)
					{
						lexer.defineLexem( rulename, patternstr);
					}
					else if (state == ParseEndOfLexemDef)
					{
						lexer.defineLexem( rulename, patternstr, selectidx);
					}
					else if (state == ParseProductionElement)
					{
						prodmap.insert( std::pair<std::string_view, ProductionNodeDefList>( rulename, prod));
						nonTerminalIdMap.insert( std::pair<std::string_view, int>( rulename, nonTerminalIdMap.size()+1));
					}
					else
					{
						throw Error( Error::UnexpectedEndOfRuleInGrammarDef, lexem.value());
					}
					state = Init;
					break;
			}
		}
		if (state != Init) throw Error( Error::UnexpectedEofInGrammarDef);

		// [2] Label grammar production elements:
		for (auto pr : prodmap)
		{
			for (auto element : pr.second)
			{
				if (element.type() == ProductionNodeDef::Unresolved)
				{
					int lxid = lexer.lexemId( element.name());
					auto nt = nonTerminalIdMap.find( element.name());
					if (nt == nonTerminalIdMap.end())
					{
						if (!lxid) throw Error( Error::UnresolvedIdentifierInGrammarDef, element.name());
						element.defineAsTerminal( lxid);
					}
					else
					{
						if (lxid) throw Error( Error::DefinedAsTerminalAndNonterminalInGrammarDef, element.name());
						element.defineAsNonTerminal( nt->second);
					}
				}
			}
		}

		return Grammar( lexer);
	}
	catch (const Error& err)
	{
		throw Error( err.code(), err.arg(), err.line() ? err.line() : lexem.line());
	}
}






