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
	ProductionNodeDef( const std::string_view& name_, bool symbol_)
		:ProductionNode(),m_name(name_),m_symbol(symbol_){}
	ProductionNodeDef( const ProductionNodeDef& o)
		:ProductionNode(o),m_name(o.m_name),m_symbol(o.m_symbol){}
	ProductionNodeDef& operator=( const ProductionNodeDef& o)
		{ProductionNode::operator=(o); m_name=o.m_name; m_symbol=o.m_symbol; return *this;}

	const std::string_view& name() const			{return m_name;}

	std::string tostring() const
	{
		std::string rt;
		if (m_symbol)
		{
			rt.append( name());
		}
		else
		{
			rt.push_back( '"');
			rt.append( name());
			rt.push_back( '"');
		}
		return rt;
	}

private:
	std::string_view m_name;
	bool m_symbol;
};


typedef std::vector<ProductionNodeDef> ProductionNodeDefList;


enum class Assoziativity :short {Undefined, Left, Right};
struct Priority {
	short value;
	Assoziativity assoziativity;

	Priority( short value_=0, Assoziativity assoziativity_ = Assoziativity::Undefined)
		:value(value_),assoziativity(assoziativity_){}
	Priority( const Priority& o)
		:value(o.value),assoziativity(o.assoziativity){}

	bool operator == (const Priority& o) const	{return value == o.value && assoziativity == o.assoziativity;}
	bool operator != (const Priority& o) const	{return value != o.value || assoziativity != o.assoziativity;}
	bool operator < (const Priority& o) const 	{return value == o.value ? assoziativity < o.assoziativity : value < o.value;}
	bool defined() const				{return value != 0 || assoziativity != Assoziativity::Undefined;}

	std::string tostring() const
	{
		std::string rt;
		char buf[ 32];
		std::snprintf( buf, sizeof(buf), 
				assoziativity == Assoziativity::Undefined
					? "%d" : (assoziativity == Assoziativity::Left ? "L%d" : "R%d"),
				(int)value);
		rt.append( buf);
		return rt;
	}
};


struct ProductionDef
{
	ProductionNodeDef left;
	ProductionNodeDefList right;
	Priority priority;
	int callidx;

	ProductionDef( const std::string_view& leftname_, const ProductionNodeDefList& right_, const Priority priority_, int callidx_ = 0)
		:left(leftname_,true/*symbol*/),right(right_),priority(priority_),callidx(callidx_){}
	ProductionDef( const ProductionDef& o)
		:left(o.left),right(o.right),priority(o.priority),callidx(o.callidx){}
	ProductionDef& operator=( const ProductionDef& o)
		{left=o.left; right=o.right; priority=o.priority; callidx=o.callidx; return *this;}

	std::string tostring() const
	{
		return head_tostring() + " = "
			+ element_range_tostring( right.begin(), right.end());
	}

	std::string tostring( int pos) const
	{
		char callstrbuf[ 64];
		callstrbuf[ 0] = 0;
		if (callidx)
		{
			std::snprintf( callstrbuf, sizeof(callstrbuf), " (%d)", callidx);
		}
		return head_tostring() + " = "
			+ element_range_tostring( right.begin(), right.begin() + pos) + " . "
			+ element_range_tostring( right.begin() + pos, right.end())
			+ callstrbuf;
	}

	std::string prefix_tostring( int pos) const
	{
		return head_tostring() + " = "
			+ element_range_tostring( right.begin(), right.begin() + pos) + " ...";
	}

private:	
	std::string head_tostring() const
	{
		return priority.defined() ? (std::string( left.name()) + "/" + priority.tostring()) : (std::string( left.name()));
	}

	static std::string element_range_tostring( ProductionNodeDefList::const_iterator ri, const ProductionNodeDefList::const_iterator& re)
	{
		std::string rt;
		for (int ridx=0; ri != re; ++ri,++ridx)
		{
			if (ridx) rt.push_back( ' ');
			rt.append( ri->tostring());
		}
		return rt;
	}
};


struct TransitionItem
{
	int prodindex;
	int prodpos;
	int follow;
	Priority priority;

	TransitionItem( int prodindex_, int prodpos_, int follow_, const Priority priority_)
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
	TransitionState rt( ts);
	std::vector<TransitionItem> orig( ts.begin(), ts.end());

	for (std::size_t oidx = 0; oidx < orig.size(); ++oidx)
	{
		auto item = orig[ oidx];

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				for (auto prodidx = 0U; prodidx != prodlist.size(); ++prodidx)
				{
					if (prodlist[ prodidx].left.index() == nd.index())
					{
						TransitionItem insitem( prodidx, 0, 0/*follow*/, 0/*priority*/);
						if (rt.insert( insitem).second == true/*insert took place*/)
						{
							orig.push_back( insitem);
						}
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
	TransitionState rt( ts);
	std::vector<TransitionItem> orig( ts.begin(), ts.end());

	for (std::size_t oidx = 0; oidx < orig.size(); ++oidx)
	{
		auto item = orig[ oidx];

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				for (auto prodidx = 0U; prodidx != prodlist.size(); ++prodidx)
				{
					const ProductionDef& trigger_prod = prodlist[ prodidx];
					if (trigger_prod.left.index() == nd.index())
					{
						std::set<int> firstOfRest = getLr1FirstSet( 
										prod, item.prodpos, item.follow,
										nonTerminalFirstSetMap, nullableNonterminalSet);
						for (auto follow :firstOfRest)
						{
							TransitionItem insitem( prodidx, 0, follow, trigger_prod.priority);
							if (rt.insert( insitem).second == true/*insert took place*/)
							{
								orig.push_back( insitem);
							}
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
	for (auto const& item : ts)
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
	for (auto const& item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			rt.insert( prod.right[ item.prodpos]);
		}
	}
	return rt;
}

static std::set<ProductionNode> getReduceNodes( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	std::set<ProductionNode> rt;
	for (auto const& item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos == (int)prod.right.size())
		{
			rt.insert( ProductionNode( ProductionNode::Terminal, item.follow));
		}
	}
	return rt;
}

static Priority getShiftPriority( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist)
{
	Priority priority;
	int last_prodidx = -1;
	for (auto const& item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNode::Terminal && nd.index() == terminal)
			{
				if (last_prodidx == -1)
				{
					priority = item.priority;
					last_prodidx = item.prodindex;
				}
				else if (priority != item.priority)
				{
					throw Error( Error::PriorityConflictInGrammarDef, prod.tostring() + ", " + prodlist[ last_prodidx].tostring());
				}
			}
		}
	}
	return priority;
}

struct ReductionDef
{
	Priority priority;
	int head;
	int count;
	std::string_view headname;

	ReductionDef()
		:priority(),head(-1),count(-1),headname("",0){}
	ReductionDef( const ReductionDef& o)
		:priority(o.priority),head(o.head),count(o.count),headname(o.headname){}
};

static ReductionDef getReductionDef( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist)
{
	ReductionDef rt;
	int last_prodidx = -1;
	for (auto const& item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.follow == terminal && item.prodpos == (int)prod.right.size())
		{
			if (last_prodidx == -1)
			{
				rt.priority = item.priority;
				rt.head = prod.left.index();
				rt.headname = prod.left.name();
				rt.count = prod.right.size();
				last_prodidx = item.prodindex;
			}
			else if (rt.priority != item.priority || rt.head != prod.left.index() || rt.count != (int)prod.right.size())
			{
				throw Error( Error::PriorityConflictInGrammarDef, prod.tostring() + ", " + prodlist[ last_prodidx].tostring());
			}
		}
	}
	return rt;
}

static std::string getStateTransitionString( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist)
{
	const char* redustr = nullptr;
	const char* shiftstr = nullptr;
	std::string prodstr;
	for (auto const& item : state)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNode::Terminal && nd.index() == terminal)
			{
				shiftstr = "->";
				prodstr = prod.prefix_tostring( item.prodpos);
			}
		}
		else if (terminal == item.follow)
		{
			redustr = "<-";
			if (prodstr.empty()) prodstr = prod.tostring( item.prodpos);
		}
	}
	if (redustr && shiftstr)
	{
		return prodstr + shiftstr + "|" + redustr;
	}
	else if (redustr)
	{
		return prodstr + redustr;
	}
	else if (shiftstr)
	{
		return prodstr + shiftstr;
	}
	else
	{
		return prodstr;
	}
}

template <class CalculateClosureFunctor>
static TransitionState getGotoState(
				const TransitionState& state, const ProductionNode& gto,
				const std::vector<ProductionDef>& prodlist, const CalculateClosureFunctor& calcClosure)
{
	TransitionState gtoState;
	for (auto const& item : state)
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

	for (std::size_t stkidx = 0; stkidx < statestk.size(); ++stkidx)
	{
		auto state = statestk[ stkidx];

		auto gtos = getGotoNodes( state, prodlist);
		for (auto const& gto : gtos)
		{
			auto newState = getGotoState( state, gto, prodlist, calcClosure);
			if (newState.empty()) throw Error( Error::LogicError);
			if (rt.insert( {newState,rt.size()+1}).second == true/*insert took place*/)
			{
				statestk.push_back( newState);
			}
		}
	}
	return rt;
}

static bool isAcceptState( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	for (auto item : state)
	{
		if (item.prodindex == 0 && item.prodpos == (int)prodlist[ 0].right.size() && item.follow == 0)
		{
			return true;
		}
	}
	return false;
}

static std::set<int> getNullableNonterminalSet( const std::vector<ProductionDef>& prodlist)
{
	std::set<int> rt;
	bool changed = true;
	while (changed)
	{
		changed = false;
		for (auto const& prod : prodlist)
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
		for (auto const& prod : prodlist)
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
					if (nullableNonterminalSet.find( ei->index()) != nullableNonterminalSet.end()) continue;
				}
				break;
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
	if (str.size() == 0)
	{
		throw Error( Error::UnexpectedTokenInGrammarDef, str);
	}
	if (str[0] == 'L' || str[0] == 'R')
	{
		Assoziativity assoz = str[0] == 'L' ? Assoziativity::Left : Assoziativity::Right;
		return (str.size() == 1) ? Priority( 0, assoz) : Priority( convertStringToInt( str.substr( 1, str.size()-1)), assoz);
	}
	else if (str[str.size()-1] == 'L' || str[str.size()-1] == 'R')
	{
		Assoziativity assoz = str[str.size()-1] == 'L' ? Assoziativity::Left : Assoziativity::Right;
		return Priority( convertStringToInt( str.substr( 0, str.size()-1)), assoz);
	}
	else
	{
		return Priority( convertStringToInt( str));
	}
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

static void insertAction( 
	std::map<Automaton::ActionKey,Priority>& priorityMap,
	std::map<Automaton::ActionKey,Automaton::Action>& actionMap,
	const std::vector<ProductionDef>& prodlist,
	const TransitionState& lr1State,
	int terminal,
	const Automaton::ActionKey& key,
	const Automaton::Action& action,
	const Priority priority, 
	std::vector<Error>& warnings)
{
	auto ains = actionMap.insert( {key, action} );
	Priority storedPriority = priorityMap[ key];

	if (ains.second/*is new*/)
	{
		priorityMap[ key] = priority;
	}
	else if (priority.value > storedPriority.value)
	{
		actionMap[ key] = action;
		priorityMap[ key] = priority;
	}
	else if (priority.value == storedPriority.value)
	{
		if (priority.assoziativity != storedPriority.assoziativity || priority.assoziativity == Assoziativity::Undefined)
		{
			warnings.push_back(
				Error( Error::ShiftReduceConflictInGrammarDef, 
				getStateTransitionString( lr1State, terminal, prodlist)));
		}
		else
		{
			const Automaton::Action& storedAction = ains.first->second;
			if (action.type() == storedAction.type())
			{
				warnings.push_back(
					Error( action.type() == Automaton::Action::Shift ? Error::ShiftShiftConflictInGrammarDef : Error::ReduceReduceConflictInGrammarDef,
						getStateTransitionString( lr1State, terminal, prodlist)));
			}
			else if (action.type() == Automaton::Action::Reduce)
			{
				if (priority.assoziativity == Assoziativity::Left)
				{
					actionMap[ key] = action;
					priorityMap[ key] = priority;
				}
			}
			else //if (action.type() == Automaton::Action::Shift)
			{
				if (priority.assoziativity == Assoziativity::Right)
				{
					actionMap[ key] = action;
					priorityMap[ key] = priority;
				}
			}
		}
	}
}

struct LanguageDef
{
	std::string language;
	Lexer lexer;
	std::vector<ProductionDef> prodlist;
	std::vector<Automaton::Call> calls;

	LanguageDef() 				:language(),lexer(),prodlist(),calls(){};
	LanguageDef( const LanguageDef& o) 	:language(o.language),lexer(o.lexer),prodlist(o.prodlist),calls(o.calls){}
	LanguageDef( LanguageDef&& o)  		:language(std::move(o.language)),lexer(std::move(o.lexer)),prodlist(std::move(o.prodlist)),calls(std::move(o.calls)){}
};

static LanguageDef parseLanguageDef( const std::string& source)
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

static void printLexems( const Lexer& lexer, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- Lexems:" << std::endl;
	std::vector<Lexer::Definition> definitions = lexer.getDefinitions();

	for (auto def : definitions)
	{
		switch (def.type())
		{
			case Lexer::Definition::BadLexem:
				dbgout.out() << "BAD " << def.bad() << std::endl;
				break;
			case Lexer::Definition::NamedPatternLexem:
				dbgout.out() << "DEF " << def.name() << " " << def.pattern() << " " << def.select() << std::endl;
				break;
			case Lexer::Definition::KeywordLexem:
				dbgout.out() << "KEYWORD " << def.name() << std::endl;
				break;
			case Lexer::Definition::IgnoreLexem:
				dbgout.out() << "IGNORE " << def.ignore() << std::endl;
				break;
			case Lexer::Definition::EolnComment:
				dbgout.out() << "COMMENT " << def.start() << std::endl;
				break;
			case Lexer::Definition::BracketComment:
				dbgout.out() << "COMMENT " << def.start() << " " << def.end() << std::endl;
				break;
		}
	}
	dbgout.out() << std::endl;
}

static void printLanguageName( const std::string& language, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "== " << language << " ==" << std::endl;
}

static void printProductions( const std::vector<ProductionDef>& prodlist, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- Productions:" << std::endl;
	for (auto const& prod : prodlist)
	{
		dbgout.out() << prod.tostring() << std::endl;
	}
	dbgout.out() << std::endl;
}

static void printLr0States( const std::map<TransitionState,int>& lr0statemap, const CalculateClosureLr1& calcClosureLr0,
			    const std::vector<ProductionDef>& prodlist, const Lexer& lexer, Automaton::DebugOutput dbgout)
{
	std::map<int,TransitionState> stateinvmap;
	for (auto const& st : lr0statemap)
	{
		stateinvmap[ st.second] = st.first;
	}
	dbgout.out() << "-- LR(0) States:" << std::endl;
	for (auto const& invst : stateinvmap)
	{
		dbgout.out() << "[" << invst.first << "]" << std::endl;
		for (auto const& item : invst.second)
		{
			dbgout.out() << "\t" << prodlist[ item.prodindex].tostring( item.prodpos) << std::endl;
		}
	}
	dbgout.out() << std::endl;
}

static void printLalr1States( const std::map<TransitionState,int>& lr0statemap, const std::map<TransitionState,int>& lr1statemap, const CalculateClosureLr1& calcClosureLr1,
			 const std::vector<ProductionDef>& prodlist, const Lexer& lexer, Automaton::DebugOutput dbgout)
{
	std::map<int,TransitionState> stateinvmap;
	for (auto const& st : lr1statemap)
	{
		int stateidx = lr0statemap.at( getLr0TransitionStateFromLr1State( st.first));
		stateinvmap[ stateidx].insert( st.first.begin(), st.first.end());
	}
	dbgout.out() << "-- LALR(1) States (Merged LR(1) elements assigned to LR(0) states):" << std::endl;
	for (auto const& invst : stateinvmap)
	{
		dbgout.out() << "[" << invst.first << "]" << std::endl;
		for (auto const& item : invst.second)
		{
			dbgout.out() << "\t" << prodlist[ item.prodindex].tostring( item.prodpos)
					<< ", " << (item.follow ? lexer.lexemName( item.follow) : std::string_view("$"));
			if (item.prodpos == (int)prodlist[ item.prodindex].right.size())
			{
				if (item.prodindex == 0 && item.follow == 0)
				{
					dbgout.out() << " -> ACCEPT";
				}
				else
				{
					ReductionDef rd = getReductionDef( invst.second, item.follow, prodlist);
					dbgout.out() << " -> REDUCE " << rd.headname << " #" << rd.count;
				}
			}
			else
			{
				auto gto = prodlist[ item.prodindex].right[ item.prodpos];
				auto gtoState = getGotoState( invst.second, gto, prodlist, calcClosureLr1);
				if (gtoState.empty()) throw Error( Error::LogicError);
				int to_stateidx = lr0statemap.at( getLr0TransitionStateFromLr1State( gtoState));

				if (gto.type() == ProductionNodeDef::Terminal)
				{
					dbgout.out() << " -> SHIFT " << gto.tostring() << " GOTO " << to_stateidx;
				}
				else
				{
					dbgout.out() << " -> GOTO " << to_stateidx;
				}
			}
			dbgout.out() << std::endl;
		}
	}
	dbgout.out() << std::endl;
}

static void printFunctionCalls( const std::vector<Automaton::Call>& calls, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- Function calls:" << std::endl;
	int callidx = 0;
	for (auto call : calls)
	{
		++callidx;
		dbgout.out() << "(" << callidx << ") " << call.function();
		switch (call.argtype())
		{
			case Automaton::Call::NoArg:
				break;
			case Automaton::Call::ReferenceArg:
				dbgout.out() << ", " << call.arg();
				break;
			case Automaton::Call::StringArg:
				dbgout.out() << ", \"" << call.arg() << "\"";
				break;
		}
		dbgout.out() << std::endl;
	}
	dbgout.out() << std::endl;
}

void Automaton::build( const std::string& source, std::vector<Error>& warnings, DebugOutput dbgout)
{
	// [1] Parse grammar and test completeness:
	LanguageDef langdef = parseLanguageDef( source);
	if (dbgout.enabled() && !langdef.language.empty()) printLanguageName( langdef.language, dbgout);
	if (dbgout.enabled( DebugOutput::Lexems)) printLexems( langdef.lexer, dbgout);
	if (dbgout.enabled( DebugOutput::Productions)) printProductions( langdef.prodlist, dbgout);

	// [2] Calculate some sets needed for the build:
	std::set<int> nullableNonterminalSet = getNullableNonterminalSet( langdef.prodlist);
	std::map<int, std::set<int> > nonTerminalFirstSetMap = getNonTerminalFirstSetMap( langdef.prodlist, nullableNonterminalSet);

	CalculateClosureLr0 calcClosureLr0( langdef.prodlist);
	CalculateClosureLr1 calcClosureLr1( langdef.prodlist, nonTerminalFirstSetMap, nullableNonterminalSet);

	std::map<TransitionState,int> stateAssignmentsLr0 = getAutomatonStateAssignments( langdef.prodlist, calcClosureLr0);
	std::map<TransitionState,int> stateAssignmentsLr1 = getAutomatonStateAssignments( langdef.prodlist, calcClosureLr1);

	if (dbgout.enabled( DebugOutput::States))
	{
		printLr0States( stateAssignmentsLr0, calcClosureLr1, langdef.prodlist, langdef.lexer, dbgout);
		printLalr1States( stateAssignmentsLr0, stateAssignmentsLr1, calcClosureLr1, langdef.prodlist, langdef.lexer, dbgout);
	}
	if (dbgout.enabled( DebugOutput::FunctionCalls))
	{
		printFunctionCalls( langdef.calls, dbgout);
	}
	// [3] Test complexity boundaries:
	if (stateAssignmentsLr0.size() >= MaxState)
	{
		throw Error( Error::ComplexityMaxStateInGrammarDef);
	}
	for (auto const& prod : langdef.prodlist) if (prod.right.size() > MaxProductionLength) 
	{
		throw Error( Error::ComplexityMaxProductionLengthInGrammarDef);
	}
	if (langdef.lexer.nofTerminals()+1 >= MaxTerminal)
	{
		throw Error( Error::ComplexityMaxTerminalInGrammarDef);
	}

	// [4] Build the LALR(1) automaton:
	std::map<ActionKey,Priority> priorityMap;
	int nofAcceptStates = 0;

	for (auto const& lr1StateAssign : stateAssignmentsLr1)
	{
		auto const& lr1State = lr1StateAssign.first;
		int stateidx = stateAssignmentsLr0.at( getLr0TransitionStateFromLr1State( lr1State));

		if (isAcceptState( lr1State, langdef.prodlist))
		{
			m_actions[ ActionKey( stateidx, 0/*EOF terminal*/)] = Action( Action::Accept, 0);
			++nofAcceptStates;
		}
		auto gtos = getGotoNodes( lr1State, langdef.prodlist);
		for (auto const& gto : gtos)
		{
			auto gtoState = getGotoState( lr1State, gto, langdef.prodlist, calcClosureLr1);
			if (gtoState.empty()) throw Error( Error::LogicError);
			int to_stateidx = stateAssignmentsLr0.at( getLr0TransitionStateFromLr1State( gtoState));
			if (gto.type() == ProductionNode::Terminal)
			{
				int terminal = gto.index();
				Priority shiftPriority = getShiftPriority( lr1State, terminal, langdef.prodlist);

				ActionKey key( stateidx, terminal);
				Action action( Action::Shift, to_stateidx);

				insertAction( priorityMap, m_actions, langdef.prodlist, lr1State, terminal, key, action, shiftPriority, warnings);
			}
			else
			{
				auto gtoins = m_gotos.insert( {GotoKey( stateidx, gto.index()), Goto( to_stateidx)} );
				if (gtoins.second == false && gtoins.first->second != Goto( to_stateidx))
				{
					throw Error( Error::LogicError); //.... should not happen
				}
			}
		}
		auto redus = getReduceNodes( lr1State, langdef.prodlist);
		for (auto const& redu : redus)
		{
			int terminal = redu.index();
			ReductionDef rd = getReductionDef( lr1State, terminal, langdef.prodlist);

			ActionKey key( stateidx, terminal);
			Action action( Action::Reduce, rd.head, rd.count);

			insertAction( priorityMap, m_actions, langdef.prodlist, lr1State, terminal, key, action, rd.priority, warnings);
		}
	}
	if (nofAcceptStates == 0)
	{
		throw Error( Error::NoAcceptStatesInGrammarDef);
	}
	std::swap( m_language, langdef.language);
	std::swap( m_calls, langdef.calls);
}




