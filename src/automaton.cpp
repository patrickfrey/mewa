/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator implementation
/// \file "automaton.cpp"
#include "automaton.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include <map>
#include <set>
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

class ProductionNode
{
public:
	enum Type
	{
		Unresolved,
		NonTerminal,
		Terminal
	};

public:
	ProductionNode()
		:m_type(Unresolved),m_index(0){}
	ProductionNode( Type type_, int index_)
		:m_type(type_),m_index(index_){}
	ProductionNode( const ProductionNode& o)
		:m_type(o.m_type),m_index(o.m_index){}
	ProductionNode& operator=( const ProductionNode& o)
		{m_type=o.m_type; m_index=o.m_index; return *this;}

	Type type() const					{return m_type;}
	int index() const					{return m_index;}

	void defineAsTerminal( int index_)			{m_type = Terminal; m_index = index_;}
	void defineAsNonTerminal( int index_)			{m_type = NonTerminal; m_index = index_;}

	bool operator == (const ProductionNode& o) const	{return m_type == o.m_type && m_index == o.m_index;}
	bool operator != (const ProductionNode& o) const	{return m_type != o.m_type || m_index != o.m_index;}

	bool operator < (const ProductionNode& o) const
	{
		return m_type == o.m_type
			? m_index < o.m_index
			: m_type < o.m_type;
	}

private:
	Type m_type;
	int m_index;
};


class ProductionNodeDef
	:public ProductionNode
{
public:
	ProductionNodeDef( const std::string_view& name_)
		:ProductionNode(),m_name(name_){}
	ProductionNodeDef( const ProductionNodeDef& o)
		:ProductionNode(o),m_name(o.m_name){}
	ProductionNodeDef& operator=( const ProductionNodeDef& o)
		{ProductionNode::operator=(o); m_name=o.m_name; return *this;}

	const std::string_view& name() const		{return m_name;}

private:
	std::string_view m_name;
};


typedef std::vector<ProductionNodeDef> ProductionNodeDefList;

struct ProductionDef
{
	ProductionNodeDef left;
	ProductionNodeDefList right;
	int priority;

	ProductionDef( ProductionNodeDef left_, ProductionNodeDefList right_, int priority_)
		:left(left_),right(right_),priority(priority_){}
	ProductionDef( const ProductionDef& o)
		:left(o.left),right(o.right),priority(o.priority){}
	ProductionDef& operator=( const ProductionDef& o)
		{left=o.left; right=o.right; priority=o.priority; return *this;}
	ProductionDef( ProductionDef&& o)
		:left(std::move(o.left)),right(std::move(o.right)),priority(o.priority){}
	ProductionDef& operator=( ProductionDef&& o)
		{left=std::move(o.left); right=std::move(o.right); priority=o.priority; return *this;}
};


struct TransitionItem
{
	int prodindex;
	int prodpos;
	int follow;
	int priority;

	TransitionItem( int prodindex_, int prodpos_, int follow_, int priority_)
		:prodindex(prodindex_),prodpos(prodpos_),follow(follow_),priority(priority_){}
	TransitionItem( const TransitionItem& o)
		:prodindex(o.prodindex),prodpos(o.prodpos),follow(o.follow),priority(o.priority){}

	bool operator < (const TransitionItem& o) const
	{
		return prodindex == o.prodindex
			? prodpos == o.prodpos
				? follow == o.follow
					? priority < o.priority
					: follow < o.follow
				: prodpos < o.prodpos
			: prodindex < o.prodindex;
	}
};

typedef std::set<TransitionItem> TransitionState;

static std::set<int> getLr1FirstSet(
				const ProductionDef& prod, int prodpos, int follow,
				const std::map<int, std::set<int> >& nonTerminalFirstSetMap,
				const std::set<int>& nullableNonterminalSet)
{
	std::set<int> rt;
	for (; prodpos < (int)prod.right.size(); ++prodpos)
	{
		const ProductionNodeDef& nd = prod.right[ prodpos];
		if (nd.type() == ProductionNodeDef::Terminal)
		{
			rt.insert( nd.index());
			break;
		}
		else if (nd.type() == ProductionNodeDef::NonTerminal)
		{
			auto fi = nonTerminalFirstSetMap.find( nd.index());
			if (fi != nonTerminalFirstSetMap.end())
			{
				rt.insert( fi->second.begin(), fi->second.end());

				if (nullableNonterminalSet.find( nd.index()) == nullableNonterminalSet.end())
				{
					break;
				}
			}
		}
	}
	if (prodpos == (int)prod.right.size())
	{
		rt.insert( follow);
	}
	return rt;
}

static TransitionState getLr0TransitionStateClosure( const TransitionState& ts, const std::vector<ProductionDef>& prodlist)
{
	TransitionState rt;
	for (auto item : ts)
	{
		rt.insert( item);

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				for (auto prodidx = 0; prodidx != prodlist.size(); ++prodidx)
				{
					if (prodlist[ prodidx].left.index() == nd.index())
					{
						rt.insert( TransitionItem( prodidx, 0, 0/*follow*/, 0/*priority*/));
					}
				}
			}
		}
	}
	return rt;
}

static TransitionState getLr1TransitionStateClosure(
				const TransitionState& ts,
				const std::vector<ProductionDef>& prodlist,
				const std::map<int, std::set<int> >& nonTerminalFirstSetMap,
				const std::set<int>& nullableNonterminalSet)
{
	TransitionState rt;
	for (auto item : ts)
	{
		rt.insert( item);

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				for (auto prodidx = 0; prodidx != prodlist.size(); ++prodidx)
				{
					const ProductionDef& trigger_prod = prodlist[ prodidx];
					if (trigger_prod.left.index() == nd.index())
					{
						std::set<int> firstOfRest = getLr1FirstSet( 
										prod, item.prodpos, item.follow,
										nonTerminalFirstSetMap, nullableNonterminalSet);
						for (auto follow :firstOfRest)
						{
							rt.insert( TransitionItem( prodidx, 0, follow, trigger_prod.priority));
						}
					}
				}
			}
		}
	}
	return rt;
}

static TransitionState getLr0TransitionStateFromLr1State( const TransitionState& ts)
{
	TransitionState rt;
	for (auto item : ts)
	{
		rt.insert( TransitionItem( item.prodindex, item.prodpos, 0/*follow*/, 0/*priority*/));
	}
	return rt;
}

class CalculateClosureLr0
{
public:
	explicit CalculateClosureLr0( const std::vector<ProductionDef>& prodlist_)
		:m_prodlist(prodlist_){}

	TransitionState operator()( const TransitionState& state) const
	{
		return getLr0TransitionStateClosure( state, m_prodlist);
	}

private:
	const std::vector<ProductionDef>& m_prodlist;
};

class CalculateClosureLr1
{
public:
	CalculateClosureLr1(
			const std::vector<ProductionDef>& prodlist_,
			const std::map<int, std::set<int> >& nonTerminalFirstSetMap_,
			const std::set<int>& nullableNonterminalSet_)
		:m_prodlist(prodlist_),m_nonTerminalFirstSetMap(nonTerminalFirstSetMap_),m_nullableNonterminalSet(nullableNonterminalSet_){}

	TransitionState operator()( const TransitionState& state) const
	{
		return getLr1TransitionStateClosure( state, m_prodlist, m_nonTerminalFirstSetMap, m_nullableNonterminalSet);
	}

private:
	const std::vector<ProductionDef>& m_prodlist;
	const std::map<int, std::set<int> >& m_nonTerminalFirstSetMap;
	const std::set<int>& m_nullableNonterminalSet;
};


static std::set<ProductionNode> getGotoNodes( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	std::set<ProductionNode> rt;
	for (auto item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			rt.insert( prod.right[ item.prodpos]);
		}
	}
	return rt;
}

static void checkConflictingPrioritiesForShift( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist, const Lexer& lexer)
{
	int priority = -1;
	for (auto item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNode::Terminal && nd.index() == terminal)
			{
				if (priority == -1)
				{
					priority = item.priority;
				}
				else if (priority != item.priority)
				{
					throw Error( Error::PriorityConflictInGrammarDef, lexer.lexemName( terminal));
				}
			}
		}
	}
}

template <class CalculateClosureFunctor>
static TransitionState getGotoState(
				const TransitionState& state, const ProductionNode& gto,
				const std::vector<ProductionDef>& prodlist, const CalculateClosureFunctor& calcClosure)
{
	TransitionState gtoState;
	for (auto item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (gto == nd)
			{
				gtoState.insert( TransitionItem( item.prodindex, item.prodpos+1, item.follow, item.priority));
			}
		}
	}
	return calcClosure( gtoState);
}

template <class CalculateClosureFunctor>
static std::map<TransitionState,int> getAutomatonStateAssignments( const std::vector<ProductionDef>& prodlist, const CalculateClosureFunctor& calcClosure)
{
	std::map<TransitionState,int> rt;

	rt.insert( {calcClosure( {{0,0,0,0}}), 1});
	std::vector<TransitionState> statestk( {rt.begin()->first});

	std::size_t stkidx = 0;
	while (stkidx < statestk.size())
	{
		auto const& state = statestk[ stkidx];

		auto gtos = getGotoNodes( state, prodlist);
		for (auto gto : gtos)
		{
			auto newState = getGotoState( state, gto, prodlist, calcClosure);
			if (rt.insert( {newState,rt.size()+1}).second == true/*insert took place*/)
			{
				statestk.push_back( newState);
			}
		}
	}
	return rt;
}

static std::set<int> getNullableNonterminalSet( const std::vector<ProductionDef>& prodlist)
{
	std::set<int> rt;
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto prod : prodlist)
		{
			auto ei = prod.right.begin(), ee = prod.right.end();
			for (; ei != ee && ei->type() == ProductionNodeDef::NonTerminal
				&& rt.find(ei->index()) != rt.end(); ++ei){}
			if (ei == ee)
			{
				rt.insert( prod.left.index());
				changed = true;
			}
		}
	}
	return rt;
}

static std::map<int, std::set<int> >
	getNonTerminalFirstSetMap( const std::vector<ProductionDef>& prodlist, const std::set<int>& nullableNonterminalSet)
{
	std::map<int, std::set<int> > rt;
	std::set<int> nullableNonterminalSetVerify;
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto prod : prodlist)
		{
			auto ei = prod.right.begin(), ee = prod.right.end();
			for (; ei != ee; ++ei)
			{
				if (ei->type() == ProductionNodeDef::Terminal)
				{
					changed |= (rt[ prod.left.index()].insert( ei->index()).second == true/*insert took place*/);
				}
				else if (ei->type() == ProductionNodeDef::NonTerminal)
				{
					auto ri = rt.find( ei->index());
					if (ri != rt.end())
					{
						//... found the FIRST set of the right hand non terminal,
						// so we insert its content into the FIRST set of the left hand nonterminal.
						std::size_t sz = rt[ prod.left.index()].size();
						rt[ prod.left.index()].insert( ri->second.begin(), ri->second.end());
						changed |= sz < rt[ prod.left.index()].size();
					}
					else
					{
						//... found a right hand nonterminal with no FIRST set defined yet,
						// so we force another iteration.
						changed = true;
					}
					if (nullableNonterminalSet.find( ei->index()) == nullableNonterminalSet.end()) break;
				}
			}
			if (ei == ee)
			{
				changed |= (rt[ prod.left.index()].insert( 0/*'$'*/).second == true/*insert took place*/);
				nullableNonterminalSetVerify.insert( prod.left.index());
			}
		}
	}
	if (nullableNonterminalSetVerify != nullableNonterminalSet)
	{
		throw Error( Error::LogicError);
	}
	return rt;
}


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

void Automaton::build( const std::string& fileName, const std::string& source)
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
		std::multimap<std::string_view, std::size_t> prodmap;
		std::map<std::string_view, int> nonTerminalIdMap;
		std::set<std::string_view> usedSet;
		std::vector<ProductionDef> prodlist;
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
						patternstr.remove_suffix( patternstr.size());
						priority = 0;
						selectidx = 0;
						state = ParsePriority;
					}
					else if (state == ParseProductionElement)
					{
						prodlist.back().right.push_back( lexem.value());
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
						prodmap.insert( std::pair<std::string_view, std::size_t>( rulename, prodlist.size()));
						prodlist.push_back( ProductionDef( rulename, ProductionNodeDefList(), priority));
						nonTerminalIdMap.insert( std::pair<std::string_view, int>( rulename, nonTerminalIdMap.size()+1));
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
						// ... everything already done
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
		for (auto prod : prodlist)
		{
			if (lexer.lexemId( prod.left.name()))
			{
				throw Error( Error::DefinedAsTerminalAndNonterminalInGrammarDef, prod.left.name());
			}
			prod.left.defineAsNonTerminal( nonTerminalIdMap.at( prod.left.name()));

			for (auto element : prod.right)
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
						usedSet.insert( element.name());
					}
				}
			}
		}

		// [3] Test if all rules are reachable and the first rule defines a valid start symbol:
		for (auto pr : prodmap)
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
		for (auto prod : prodlist)
		{
			if (prod.left.index() == 1) ++startSymbolLeftCount;
		}
		if (startSymbolLeftCount == 0)
		{
			throw Error( Error::EmptyGrammarDef);
		}
		else if (startSymbolLeftCount > 1)
		{
			throw Error( Error::StartSymbolDefinedTwiceInGrammarDef, prodlist[0].left.name());
		}

		// [5] Build the LALR(1) automaton:
		std::set<int> nullableNonterminalSet = getNullableNonterminalSet( prodlist);
		std::map<int, std::set<int> > nonTerminalFirstSetMap = getNonTerminalFirstSetMap( prodlist, nullableNonterminalSet);

		CalculateClosureLr0 calcClosureLr0( prodlist);
		CalculateClosureLr1 calcClosureLr1( prodlist, nonTerminalFirstSetMap, nullableNonterminalSet);

		std::map<TransitionState,int> stateAssignmentsLr0 = getAutomatonStateAssignments( prodlist, calcClosureLr0);
		std::map<TransitionState,int> stateAssignmentsLr1 = getAutomatonStateAssignments( prodlist, calcClosureLr1);
		std::map<ActionKey,int> prioritymap;

		for (auto lr1StateAssign : stateAssignmentsLr1)
		{
			auto const& lr1State = lr1StateAssign.first;
			auto gtos = getGotoNodes( lr1State, prodlist);
			for (auto gto : gtos)
			{
				int from_stateidx = stateAssignmentsLr0.at( getLr0TransitionStateFromLr1State( lr1State));
				int to_stateidx = stateAssignmentsLr0.at( getLr0TransitionStateFromLr1State( getGotoState( lr1State, gto, prodlist, calcClosureLr1)));
				if (gto.type() == ProductionNode::Terminal)
				{
					checkConflictingPrioritiesForShift( lr1State, gto.index(), prodlist, lexer);

					ActionKey key( from_stateidx, gto.index());
					auto ains = m_actions.insert( {key, Action( Action::Shift, to_stateidx)} );
					if (ains.second/*is new*/)
					{
					}
				}
			}
		}
	}
	catch (const Error& err)
	{
		throw Error( err.code(), err.arg(), err.line() ? err.line() : lexem.line());
	}
}




