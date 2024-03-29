/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Parser of the grammar for the LALR(1) automaton to build
/// \file "automaton_parser.cpp"
#include "automaton.hpp"
#include "automaton_structs.hpp"
#include "automaton_parser.hpp"
#include "strings.hpp"
#include "utf8.hpp"
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
		OPENCURLYBRK,
		CLOSECURLYBRK,
		COMMA,
		OPENBRK,
		CLOSEBRK,
		OR,
		STEP,
		SCOPE
	};

	GrammarLexer()
	{
		defineEolnComment( 0/*no line*/, "//");
		defineEolnComment( 0/*no line*/, "#");
		defineBracketComment( 0/*no line*/, "/*", "*/");
		defineLexem( 0/*no line*/, "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		defineLexem( 0/*no line*/, "NUMBER", "[0-9]+");
		defineLexem( 0/*no line*/, "PRIORITY", "[0-9]+[LR]");
		defineLexem( 0/*no line*/, "DQSTRING", "[\"]((([^\\\\\"\\n])|([\\\\][^\"\\n]))*)[\"]", 1);
		defineLexem( 0/*no line*/, "SQSTRING", "[\']((([^\\\\\'\\n])|([\\\\][^\'\\n]))*)[\']", 1);
		defineLexem( "%");
		defineLexem( "/");
		defineLexem( "=");
		defineLexem( "→");
		defineLexem( ":");
		defineLexem( ";");
		defineLexem( "ε");
		defineLexem( 0/*no line*/, "CALL", "[-]{0,1}[a-zA-Z_][.a-zA-Z_0-9]*");
		defineLexem( "{");
		defineLexem( "}");
		defineLexem( ",");
		defineLexem( "(");
		defineLexem( ")");
		defineLexem( "|");
		defineLexem( ">>");
		defineLexem( "{}");
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
		ParseCallArgMemberName,
		ParseCallArgMemberEquals,
		ParseCallArgMemberValue,
		ParseCallArgMemberDelim,
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
					"ParseCall", "ParseCallName","ParseCallArg",
					"ParseCallArgMemberName","ParseCallArgMemberEquals","ParseCallArgMemberValue","ParseCallArgMemberDelim",
					"ParseCallClose",
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
		Automaton::Call::ArgumentType call_argtype = Automaton::Call::NoArg;
		std::string call_function;
		std::string call_arg;
		int selectidx = 0;

		lexem = grammarLexer.next( scanner);
		for (; !lexem.empty(); lexem = grammarLexer.next( scanner))
		{
			GrammarLexer::LexemType lexemtype = (GrammarLexer::LexemType)lexem.id();
			switch (lexemtype)
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
					else if (state == ParseCallArgMemberValue)
					{
						call_arg.append( lexem.value());
						state = ParseCallArgMemberDelim;
					}
					else if (state == ParseCallArgMemberName)
					{
						call_arg.append( lexem.value());
						state = ParseCallArgMemberEquals;
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
					else if (state == ParseCallName)
					{
						state = ParseEndOfProduction;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::STEP:
				case GrammarLexer::SCOPE:
					if (state == ParseCallName)
					{
						if (rt.prodlist.back().scope != Automaton::Action::NoScope)
						{
							throw Error( Error::DuplicateScopeInGrammarDef);
						}
						rt.prodlist.back().scope = lexemtype == GrammarLexer::STEP ? Automaton::Action::Step : Automaton::Action::Scope;
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
					else if (state == ParseCallArgMemberValue)
					{
						call_arg.append( lexem.value());
						state = ParseCallArgMemberDelim;
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
				case GrammarLexer::OPENCURLYBRK:
					if (state == ParseCallArg)
					{
						call_argtype = Automaton::Call::ReferenceArg;
						call_arg = "{";
						state = ParseCallArgMemberName;
					}
					else if (state == ParseCallArgMemberValue)
					{
						throw Error( Error::NestedCallArgumentStructureInGrammarDef);
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::CLOSECURLYBRK:
					if (state == ParseCallArgMemberDelim)
					{
						call_arg.push_back( '}');
						state = ParseCallClose;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
					}
					break;
				case GrammarLexer::COMMA:
					if (state == ParseCallArgMemberDelim)
					{
						call_arg.push_back( ',');
						state = ParseCallArgMemberName;
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
						rt.prodlist.back().right.push_back( ProductionNodeDef( lexem.value(), false/*not a symbol, raw string implicitly defined*/));
					}
					else if (state == ParseCallArg)
					{
						call_arg = std::string( lexem.value());
						call_argtype = Automaton::Call::StringArg;
						state = ParseCallClose;
					}
					else if (state == ParseCallArgMemberValue)
					{
						if (lexemtype == GrammarLexer::DQSTRING)
						{
							call_arg.push_back( '\"');
							call_arg.append( lexem.value());
							call_arg.push_back( '\"');
						}
						else
						{
							call_arg.push_back( '\'');
							call_arg.append( lexem.value());
							call_arg.push_back( '\'');
						}
						state = ParseCallArgMemberDelim;
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
					if (state == ParseCallArgMemberEquals && lexemtype == GrammarLexer::EQUAL)
					{
						call_arg.append( lexem.value());
						state = ParseCallArgMemberValue;
					}
					else if (state == ParseProductionAttributes || state == ParseAssign)
					{
						prodmap.insert( std::pair<std::string_view, std::size_t>( rulename, rt.prodlist.size()));
						rt.prodlist.push_back( ProductionDef( lexem.line(), rulename, ProductionNodeDefList(), priority));
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
						else if (caseInsensitiveEqual( cmdname, "CMDLINE"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.cmdline = std::string( cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "IGNORE"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.lexer.defineIgnore( lexem.line(), cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "BAD"))
						{
							if (cmdargs.size() != 1) throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							rt.lexer.defineBadLexem( lexem.line(), cmdargs[ 0]);
						}
						else if (caseInsensitiveEqual( cmdname, "COMMENT"))
						{
							if (cmdargs.size() == 1)
							{
								rt.lexer.defineEolnComment( lexem.line(), cmdargs[ 0]);
							}
							else if (cmdargs.size() == 2)
							{
								rt.lexer.defineBracketComment( lexem.line(), cmdargs[ 0], cmdargs[ 1]);
							}
							else
							{
								throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							}
						}
						else if (caseInsensitiveEqual( cmdname, "INDENTL"))
						{
							if (cmdargs.size() != 4)
							{
								throw Error( Error::CommandNumberOfArgumentsInGrammarDef, cmdname);
							}
							int indent = convertStringToInt( cmdargs[3]);
							rt.lexer.defineIndentLexems( lexem.line(), cmdargs[0], cmdargs[1], cmdargs[2], indent);
						}
						else
						{
							throw Error( Error::CommandNameUnknownInGrammarDef, cmdname);
						}
					}
					else if (state == ParsePatternSelect)
					{
						rt.lexer.defineLexem( lexem.line(), rulename, patternstr);
					}
					else if (state == ParseEndOfLexemDef)
					{
						rt.lexer.defineLexem( lexem.line(), rulename, patternstr, selectidx);
					}
					else if (state == ParseProductionElement || state == ParseEndOfProduction || state == ParseCall)
					{
						/* nothing to do anymore here ... */
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
						rt.prodlist.push_back( ProductionDef( lexem.line(), rulename, ProductionNodeDefList(), priority));
						call_argtype = Automaton::Call::NoArg;
						call_function.clear();
						call_arg.clear();
						state = ParseProductionElement;
					}
					else
					{
						throw Error( Error::UnexpectedTokenInGrammarDef, lexem.value());
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
		std::string unreachableListString;
		for (auto const& pr : prodmap)
		{
			auto nt = nonTerminalIdMap.at( pr.first);
			if (usedSet.find( pr.first) == usedSet.end())
			{
				if (nt != 1/*isn't the start symbol*/)
				{
					if (!unreachableListString.empty()) unreachableListString.append( ", ");
					unreachableListString.append( pr.first);
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
		if (!unreachableListString.empty())
		{
			throw Error( Error::UnreachableNonTerminalInGrammarDef, unreachableListString);
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

		// [6] Test for duplicate productions:
		std::set<std::string> prodset;
		for (auto const& prod: rt.prodlist)
		{
			if (prodset.insert( prod.prodstring()).second == false /*not new*/)
			{
				throw Error( Error::DuplicateProductionInGrammarDef, prod.prodstring());
			}
		}
	}
	catch (const Error& err)
	{
		throw Error( err.code(), err.arg(), err.location().line() ? err.location().line() : lexem.line());
	}
	return rt;
}

std::string mewa::printLuaTypeSystemStub( const LanguageDef& langdef)
{
	std::string rt = mewa::string_format( "local mewa = require \"mewa\"\nlocal typedb = mewa.typedb()\nlocal typesystem = {}\n\n");
	std::set<std::string> fset;
	for (auto call : langdef.calls)
	{
		auto ins = fset.insert( call.function());
		if (ins.second == true/*insert took place*/)
		{
			std::string argstr;
			if (call.argtype() != Automaton::Call::NoArg)
			{
				argstr.append(", decl");
			}
			rt.append( mewa::string_format( "function typesystem.%s( node%s)\nend\n", call.function().c_str(), argstr.c_str()));
		}
	}
	rt.append( "\nreturn typesystem\n\n");
	return rt;
}

static char const* skipSpaces( char const* str, const char* end=nullptr)
{
	for (; (unsigned char)*str <= 32 && *str != '\n' && str != end; ++str){}
	return str;
}

static char const* skipEoln( char const* str, const char* end=nullptr)
{
	for (; *str != '\n' && str != end; ++str){}
	return str;
}

static std::vector<std::string> splitLines( char const* str, std::size_t sz)
{
	std::vector<std::string> rt;
	const char* end = str + sz;
	while (str < end)
	{
		const char* start = skipSpaces( str, end); //skip leading spaces
		str = skipEoln( start, end);
		std::size_t nn = str-start;
		while (nn > 0 && (unsigned char)start[ nn-1] <= 32)
		{
			--nn;
		}
		rt.emplace_back( start, nn);
	}
	while (!rt.empty() && rt.back().empty())
	{
		rt.pop_back();
	}
	return rt;
}

static std::pair<std::string,std::string> splitDecoratorDef( const std::string& ln)
{
	char const* lp = skipSpaces( ln.c_str(), ln.c_str() + ln.size());
	if (lp[0] == '@')
	{
		char const* start = skipSpaces( lp+1, ln.c_str() + ln.size());
		char const* cc = start;
		while (((unsigned char)(*cc|32) >= 'a' && (unsigned char)(*cc|32) <= 'z') || (*cc >= '0' && *cc <= '9') || (*cc == '_'))
		{
			++cc;
		}
		std::string id( start,cc-start);
		cc = skipSpaces( cc, ln.c_str() + ln.size());
		return {id, std::string(cc)};
	}
	else
	{
		return {"",lp};
	}
}

static void extractDecorators( std::vector<LanguageDecorator>& res, const std::vector<std::string>& comment)
{
	for (auto const& cm : comment)
	{
		auto const& dd = splitDecoratorDef( cm);
		if (dd.first.empty())
		{
			if (!res.empty())
			{
				res.back().content.push_back( dd.second);
			}
		}
		else
		{
			std::vector<std::string> content;
			content.push_back( dd.second);
			res.push_back( {dd.first, content});
		}
	}
}

LanguageDecoratorMap mewa::parseLanguageDecoratorMap( const std::string& source)
{
	LanguageDecoratorMap rt;
	GrammarLexer grammarLexer;
	Scanner scanner( source);
	Lexem lexem( 1/*line*/);
	std::vector<std::string> comment;
	std::vector<LanguageDecorator> decorators;
	std::size_t symlen = 0;
	int line = 0;
	char const* start = scanner.next();
	for (; *start; start = scanner.next( symlen))
	{
		comment.clear();
		if (start[0] == '#' || (start[0] == '/' && start[1] == '/'))
		{
			line = scanner.line();
			start += start[0] == '#' ? 1:2;
			if (scanner.scan( "\n"))
			{
				comment.emplace_back( start, scanner.current() - start - 1);
			}
			else
			{
				comment.emplace_back( start);
			}
			symlen = 0;
		}
		else if (start[0] == '/' && start[1] == '*')
		{
			line = scanner.line();
			start += 2;
			if (scanner.scan( "*/"))
			{
				comment = splitLines( start, scanner.current() - start);
			}
			else
			{
				throw Error( Error::UnexpectedEofInGrammarDef);
			}
			symlen = 1;
		}
		else
		{
			line = scanner.line();
			grammarLexer.next( scanner);
			for (auto const& dc : decorators)
			{
				if (LanguageDecorator::isReservedAttribute( dc.name))
				{
					throw Error( Error::UsingReservedAttributeNameInGrammarDef);
				}
				rt.addDecorator( line, dc);
			}
			decorators.clear();
		}
		if (!comment.empty())
		{
			extractDecorators( decorators, comment);
		}
	}
	return rt;
}

