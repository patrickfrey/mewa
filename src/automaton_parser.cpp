/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Parser of the grammar EBNF for the LALR(1) automaton to build
/// \file "automaton_parser.cpp"
#include "automaton.hpp"
#include "automaton_structs.hpp"
#include "automaton_parser.hpp"
#include <map>
#include <algorithm>
#include <type_traits>

using namespace mewa;

class GrammarLexer
	:public Lexer
{
public:
	enum LexemType
	{
		ERROR=-1,
		NONE=0,
		IDENT,
		NUMBER,
		PRIORITY,
		DQSTRING,
		SQSTRING,
		PERCENT,
		SLASH,
		EQUAL,
		ARROW,
		DDOT,
		SEMICOLON,
		EPSILON,
		CALL,
		OPENBRK,
		CLOSEBRK,
		OR
	};

	GrammarLexer()
	{
		defineEolnComment( "//");
		defineBracketComment( "/*", "*/");
		defineLexem( "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		defineLexem( "NUMBER", "[0-9]+");
		defineLexem( "PRIORITY", "[0-9]+[LR]");
		defineLexem( "DQSTRING", "[\"]((([^\\\\\"\\n])|([\\\\][^\"\\n]))*)[\"]", 1);
		defineLexem( "SQSTRING", "[\']((([^\\\\\'\\n])|([\\\\][^\'\\n]))*)[\']", 1);
		defineLexem( "%");
		defineLexem( "/");
		defineLexem( "=");
		defineLexem( "→");
		defineLexem( ":");
		defineLexem( ";");
		defineLexem( "ε");
		defineLexem( "CALL", "[-]{0,1}[a-zA-Z_][:.a-zA-Z_0-9]*");
		defineLexem( "(");
		defineLexem( ")");
		defineLexem( "|");
	}
};

static int convertStringToInt( const std::string_view& str)
{
	int si = 0, se = str.size();
	int rt = 0;
	if (si == se)
	{
		throw Error( Error::UnexpectedTokenInGrammarDef, str);
	}
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
			throw Error( Error::UnexpectedTokenInGrammarDef, str);
		}
	}
	return rt;
}

static Priority convertStringToPriority( const std::string_view& str)
{
	Priority rt;
	if (str.size() == 0)
	{
		throw Error( Error::UnexpectedTokenInGrammarDef, str);
	}
	if (str[0] == 'L' || str[0] == 'R')
	{
		Assoziativity assoz = str[0] == 'L' ? Assoziativity::Left : Assoziativity::Right;
		rt = (str.size() == 1) ? Priority( 0, assoz) : Priority( convertStringToInt( str.substr( 1, str.size()-1)), assoz);
	}
	else if (str[str.size()-1] == 'L' || str[str.size()-1] == 'R')
	{
		Assoziativity assoz = str[str.size()-1] == 'L' ? Assoziativity::Left : Assoziativity::Right;
		rt = Priority( convertStringToInt( str.substr( 0, str.size()-1)), assoz);
	}
	else
	{
		rt = Priority( convertStringToInt( str));
	}
	if (rt.value >= Automaton::MaxPriority)
	{
		throw Error( Error::ComplexityMaxProductionPriorityInGrammarDef, str);
	}
	return rt;
}

static bool caseInsensitiveEqual( const std::string_view& a, const std::string_view& b)
{
	auto ai = a.begin(), ae = a.end(), bi = b.begin(), be = b.end();
	for (; ai != ae && bi != be; ++ai,++bi)
	{
		if (std::tolower(*ai) != std::tolower(*bi)) return false;
	}
	return ai == ae && bi == be;
}

LanguageDef mewa::parseLanguageDef( const std::string& source)
{
	LanguageDef rt;
	enum State {
		Init,
		ParseProductionAttributes,
		ParsePriority,
		ParseAssign,
		ParseProductionElement,
		ParseCall,
		ParseCallName,
		ParseCallArg,
		ParseCallClose,
		ParseEndOfProduction,
		ParsePattern,
		ParsePatternSelect,
		ParseEndOfLexemDef,
		ParseLexerCommand,
		ParseLexerCommandArg
	};
	struct StateContext
	{
		static const char* stateName( State state)
		{
			static const char* ar[] = {
					"Init","ParseProductionAttributes","ParsePriority",
					"ParseAssign","ParseProductionElement",
					"ParseCall", "ParseCallName","ParseCallArg","ParseCallClose",
					"ParseEndOfProduction",
					"ParsePattern","ParsePatternSelect","ParseEndOfLexemDef",
					"ParseLexerCommand","ParseLexerCommandArg"};
			return ar[ state];
		}
	};
	State state = Init;
	GrammarLexer grammarLexer;
	Scanner scanner( source);
	Lexem lexem( 1/*line*/);

	try
	{
		// [1] Parse grammar:
		std::string_view rulename;
		std::string_view patternstr;
		std::string_view cmdname;
		std::vector<std::string_view> cmdargs;
		std::multimap<std::string_view, std::size_t> prodmap;
		std::map<std::string_view, int> nonTerminalIdMap;
		std::set<std::string_view> usedSet;
		std::map<Automaton::Call, int> callmap;
		Priority priority;
		Automaton::Call::ArgumentType call_argtype;
		std::string call_function;
		std::string call_arg;
		int selectidx = 0;

		lexem = grammarLexer.next( scanner);
		for (; !lexem.empty(); lexem = grammarLexer.next( scanner))
		{
			switch ((GrammarLexer::LexemType)lexem.id())
			{
				case GrammarLexer::ERROR:
					throw Error( Error::BadCharacterInGrammarDef, lexem.value());
				case GrammarLexer::NONE:
					throw Error( Error::LogicError, StateContext::stateName( state)); //... loop exit condition (lexem.empty()) met
				case GrammarLexer::IDENT:
					if (state == Init)
					{
						rulename = lexem.value();
						patternstr.remove_suffix( patternstr.size());
						selectidx = 0;
						priority = Priority();
						call_argtype = Automaton::Call::NoArg;
						call_function.clear();
						call_arg.clear();
						state = ParseProductionAttributes;
					}
					else if (state == ParseProductionElement)
					{
						rt.prodlist.back().right.push_back( ProductionNodeDef( lexem.value(), true/*symbol*/));
					}
					else if (state == ParseCallName)
					{
						call_function = std::string( lexem.value());
						state = ParseCallArg;
					}
					else if (state == ParseCallArg)
					{
						call_arg = std::string( lexem.value());
						call_argtype = Automaton::Call::ReferenceArg;
						state = ParseCallClose;
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
					else if (state == ParsePriority)
					{
						priority = convertStringToPriority( lexem.value());
						state = ParseAssign;
					}
					else if (state == ParsePattern)
					{
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}	
					break;
				case GrammarLexer::CALL:
					if (state == ParseCallName)
					{
						call_function = std::string( lexem.value());
						state = ParseCallArg;
					}
					else if (state == ParseCallArg)
					{
						call_arg = std::string( lexem.value());
						call_argtype = Automaton::Call::ReferenceArg;
						state = ParseCallClose;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::OPENBRK:
					if (state == ParseProductionElement || state == ParseCall)
					{
						state = ParseCallName;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::CLOSEBRK:
					if (state == ParseCallArg || state == ParseCallClose)
					{
						auto cint = callmap.insert( {Automaton::Call( call_function, call_arg, call_argtype), callmap.size()+1});
						rt.prodlist.back().callidx = cint.first->second;
						if (cint.second/*insert took place*/)
						{
							rt.calls.push_back( cint.first->first);
						}
						state = ParseEndOfProduction;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::PRIORITY:
					if (state == ParsePriority)
					{
						priority = convertStringToPriority( lexem.value());
						state = ParseAssign;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::NUMBER:
					if (state == ParsePriority)
					{
						priority = convertStringToPriority( lexem.value());
						state = ParseAssign;
					}
					else if (state == ParsePatternSelect)
					{
						selectidx = convertStringToInt( lexem.value());
						state = ParseEndOfLexemDef;
					}
					else if (state == ParseCallArg)
					{
						call_arg = std::string( lexem.value());
						call_argtype = Automaton::Call::ReferenceArg;
						state = ParseCallClose;
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
						state = ParsePatternSelect;
					}
					else if (state == ParseLexerCommandArg)
					{
						cmdargs.push_back( lexem.value());
					}
					else if (state == ParseProductionElement)
					{
						if (!rt.lexer.lexemId( lexem.value())) rt.lexer.defineLexem( lexem.value());
						rt.prodlist.back().right.push_back( ProductionNodeDef( lexem.value(), false/*not a symbol, raw string implicitely defined*/));
					}
					else if (state == ParseCallArg)
					{
						call_arg = std::string( lexem.value());
						call_argtype = Automaton::Call::StringArg;
						state = ParseCallClose;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::EPSILON:
					if (state == ParseProductionElement && rt.prodlist.back().right.empty())
					{
						state = ParseCall;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
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
					if (state == ParseProductionAttributes)
					{
						state = ParsePriority;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::EQUAL:
				case GrammarLexer::ARROW:
					if (state == ParseProductionAttributes || state == ParseAssign)
					{
						prodmap.insert( std::pair<std::string_view, std::size_t>( rulename, rt.prodlist.size()));
						rt.prodlist.push_back( ProductionDef( rulename, ProductionNodeDefList(), priority));
						if (nonTerminalIdMap.insert( std::pair<std::string_view, int>( rulename, nonTerminalIdMap.size()+1)).second/*insert took place*/)
						{
							rt.nonterminals.push_back( std::string( rulename));
						}
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
					else if (state == ParseProductionAttributes)
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
						if (caseInsensitiveEqual( cmdname, "LANGUAGE"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.language = std::string( cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "TYPESYSTEM"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.typesystem = std::string( cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "IGNORE"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.lexer.defineIgnore( cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "BAD"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.lexer.defineBadLexem( cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "COMMENT"))
						{
							if (cmdargs.size() == 1)
							{
								rt.lexer.defineEolnComment( cmdargs[ 0]);
							}
							else if (cmdargs.size() == 2)
							{
								rt.lexer.defineBracketComment( cmdargs[ 0], cmdargs[ 1]);
							}
							else
							{
								throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							}
						}
						else
						{
							throw Error( Error::CommandNameUnknownInGrammarDef, cmdname);
						}
					}
					else if (state == ParsePatternSelect)
					{
						rt.lexer.defineLexem( rulename, patternstr);
					}
					else if (state == ParseEndOfLexemDef)
					{
						rt.lexer.defineLexem( rulename, patternstr, selectidx);
					}
					else if (state == ParseProductionElement || state == ParseEndOfProduction || state == ParseCall)
					{
						// ... everything already done
					}
					else
					{
						throw Error( Error::UnexpectedEndOfRuleInGrammarDef, StateContext::stateName( state));
					}
					state = Init;
					break;
				case GrammarLexer::OR:
					if (state == ParseProductionElement || state == ParseEndOfProduction || state == ParseCall)
					{
						prodmap.insert( std::pair<std::string_view, std::size_t>( rulename, rt.prodlist.size()));
						rt.prodlist.push_back( ProductionDef( rulename, ProductionNodeDefList(), priority));
						state = ParseProductionElement;
					}
					break;
			}
		}
		if (state != Init) throw Error( Error::UnexpectedEofInGrammarDef);

		// [2] Label grammar production elements:
		for (auto& prod : rt.prodlist)
		{
			if (rt.lexer.lexemId( prod.left.name()))
			{
				throw Error( Error::DefinedAsTerminalAndNonterminalInGrammarDef, prod.left.name());
			}
			prod.left.defineAsNonTerminal( nonTerminalIdMap.at( prod.left.name()));

			for (auto& element : prod.right)
			{
				if (element.type() == ProductionNodeDef::Unresolved)
				{
					int lxid = rt.lexer.lexemId( element.name());
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
						usedSet.insert( element.name());
					}
				}
			}
		}

		// [3] Test if all rules are reachable and the first rule defines a valid start symbol:
		for (auto const& pr : prodmap)
		{
			auto nt = nonTerminalIdMap.at( pr.first);
			if (usedSet.find( pr.first) == usedSet.end())
			{
				if (nt != 1/*isn't the start symbol*/)
				{
					throw Error( Error::UnreachableNonTerminalInGrammarDef, pr.first);
				}
			}
			else
			{
				if (nt == 1/*is the start symbol*/)
				{
					throw Error( Error::StartSymbolReferencedInGrammarDef, pr.first);
				}
			}
		}

		// [4] Test if start symbol (head of first production) appears only once on the left side and never on the right side:
		int startSymbolLeftCount = 0;
		for (auto const& prod: rt.prodlist)
		{
			if (prod.left.index() == 1) ++startSymbolLeftCount;
		}
		if (startSymbolLeftCount == 0)
		{
			throw Error( Error::EmptyGrammarDef);
		}
		else if (startSymbolLeftCount > 1)
		{
			throw Error( Error::StartSymbolDefinedTwiceInGrammarDef, rt.prodlist[0].left.name());
		}

		// [5] Test complexity:
		if (nonTerminalIdMap.size() >= Automaton::MaxNonterminal)
		{
			throw Error( Error::ComplexityMaxNonterminalInGrammarDef);
		}
	}
	catch (const Error& err)
	{
		throw Error( err.code(), err.arg(), err.line() ? err.line() : lexem.line());
	}
	return rt;
}


