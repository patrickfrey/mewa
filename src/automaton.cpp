/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief LALR(1) Parser generator implementation
/// \file "automaton.cpp"
#include "automaton.hpp"
#include "automaton_structs.hpp"
#include "automaton_parser.hpp"
#include "lexer.hpp"
#include "error.hpp"
#include <set>
#include <string_view>
#include <algorithm>
#include <type_traits>

using namespace mewa;

static std::string getLexemName( const Lexer& lexer, int terminal)
{
        return terminal ? lexer.lexemName( terminal) : std::string("$");
}

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
	std::vector<TransitionItem> orig( ts.unpack());

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
						if (rt.insert( insitem) == true/*insert took place*/)
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
	std::vector<TransitionItem> orig( ts.unpack());

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
										prod, item.prodpos+1, item.follow,
										nonTerminalFirstSetMap, nullableNonterminalSet);
						for (auto follow :firstOfRest)
						{
							TransitionItem insitem( prodidx, 0, follow, trigger_prod.priority);
							if (rt.insert( insitem) == true/*insert took place*/)
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
	std::vector<TransitionItem> itemlist = ts.unpack();
	for (auto const& item : itemlist)
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
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
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
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos == (int)prod.right.size())
		{
			rt.insert( ProductionNode( ProductionNode::Terminal, item.follow));
		}
	}
	return rt;
}

static Priority getShiftPriority( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist, const Lexer& lexer)
{
	Priority priority;
	int last_prodidx = -1;
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
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
					throw Error( Error::PriorityConflictInGrammarDef, prod.tostring( item.prodpos)
						+ ", " + prodlist[ last_prodidx].tostring( item.prodpos)
						+ " -> " + getLexemName(lexer,terminal));
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
	int callidx;
	int count;
	std::string_view headname;

	ReductionDef()
		:priority(),head(-1),callidx(-1),count(-1),headname("",0){}
	ReductionDef( const ReductionDef& o)
		:priority(o.priority),head(o.head),callidx(o.callidx),count(o.count),headname(o.headname){}
};

static ReductionDef getReductionDef( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist, const Lexer& lexer)
{
	ReductionDef rt;
	int last_prodidx = -1;
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
	{
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.follow == terminal && item.prodpos == (int)prod.right.size())
		{
			if (last_prodidx == -1)
			{
				rt.priority = item.priority;
				rt.head = prod.left.index();
				rt.headname = prod.left.name();
				rt.callidx = prod.callidx;
				rt.count = prod.right.size();
				last_prodidx = item.prodindex;
			}
			else if (rt.head != prod.left.index() || rt.count != (int)prod.right.size() || rt.callidx != prod.callidx)
			{
				throw Error( Error::ReduceReduceConflictInGrammarDef, prod.tostring() + ", " + prodlist[ last_prodidx].tostring() + " <- " + getLexemName(lexer,terminal));
			}
			else if (rt.priority != item.priority)
			{
				throw Error( Error::PriorityConflictInGrammarDef, prod.tostring() + ", " + prodlist[ last_prodidx].tostring() + " <- " + getLexemName(lexer,terminal));
			}
		}
	}
	return rt;
}

static std::string getStateTransitionString( const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist, const Lexer& lexer)
{
	const char* redustr = nullptr;
	const char* shiftstr = nullptr;
	std::string prodstr;
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
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
		return prodstr + " " + shiftstr + "|" + redustr + " " + getLexemName(lexer,terminal);
	}
	else if (redustr)
	{
		return prodstr + " " + redustr + " " + getLexemName(lexer,terminal);
	}
	else if (shiftstr)
	{
		return prodstr + " " + shiftstr + " " + getLexemName(lexer,terminal);
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
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
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
	std::vector<TransitionItem> itemlist = state.unpack();
	for (auto const& item : itemlist)
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
			changed |= (ei == ee && rt.insert( prod.left.index()).second/*insert took place*/);
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

static void insertAction( 
	std::map<Automaton::ActionKey,Priority>& priorityMap,
	std::map<Automaton::ActionKey,Automaton::Action>& actionMap,
	const std::vector<ProductionDef>& prodlist,
	const TransitionState& lr1State,
	int terminal,
	const Automaton::ActionKey& key,
	const Automaton::Action& action,
	const Priority priority, 
	std::vector<Error>& warnings,
	const Lexer& lexer)
{
	auto ains = actionMap.insert( {key, action} );
	auto pi =  priorityMap.find( key);

	if (pi == priorityMap.end() || ains.second/*is new*/)
	{
		priorityMap[ key] = priority;
	}
	else if (action.type() == ains.first->second.type())
	{
		if (ains.first->second != action)
		{
			warnings.push_back(
				Error( action.type() == Automaton::Action::Shift ? Error::ShiftShiftConflictInGrammarDef : Error::ReduceReduceConflictInGrammarDef,
					getStateTransitionString( lr1State, terminal, prodlist, lexer)));
		}
	}
	else if (priority.value > pi->second.value)
	{
		actionMap[ key] = action;
		priorityMap[ key] = priority;
	}
	else if (priority.value == pi->second.value)
	{
		if (priority.assoziativity != pi->second.assoziativity || priority.assoziativity == Assoziativity::Undefined)
		{
			warnings.push_back(
				Error( Error::ShiftReduceConflictInGrammarDef, 
				getStateTransitionString( lr1State, terminal, prodlist, lexer)));
		}
		else
		{
			if (action.type() == Automaton::Action::Reduce)
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

static std::string numTohex( unsigned char nm)
{
	static const char hx[ 17] = "0123456789ABCDEF";
	char buf[ 3];
	buf[ 0] = hx[ nm / 16];
	buf[ 1] = hx[ nm % 16];
	buf[ 2] = 0;
	return std::string( buf);
}

static std::string activationToPrintableString( const std::string& astr)
{
	std::string rt;
	for (char const* ai = astr.c_str(); *ai; ++ai)
	{
		if ((unsigned char)*ai >= 128U || *ai == '#')
		{
			rt.push_back( '#');
			rt.append( numTohex( *ai));
		}
		else
		{
			rt.push_back( *ai);
		}
	}
	return rt;
}

static void printLexems( const Lexer& lexer, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- Lexems:" << std::endl;
	std::vector<Lexer::Definition> definitions = lexer.getDefinitions();

	for (auto const& def : definitions)
	{
		std::string deftypename = def.typeName();
		std::transform( deftypename.begin(), deftypename.end(), deftypename.begin(), ::toupper);

		switch (def.type())
		{
			case Lexer::Definition::BadLexem:
				dbgout.out() << deftypename << " " << def.bad() << std::endl;
				break;
			case Lexer::Definition::NamedPatternLexem:
				dbgout.out() << deftypename << " " << def.name() << " " << def.pattern()
						<< " " << def.select() << " [" << def.activation() << "] "
						<< " ~ " << def.id() << std::endl;
				break;
			case Lexer::Definition::KeywordLexem:
				dbgout.out() << deftypename << " " << def.name()
						<< " [" << activationToPrintableString( def.activation()) << "]"
						<<  " ~ " << def.id() << std::endl;
				break;
			case Lexer::Definition::IgnoreLexem:
				dbgout.out() << deftypename << " " << def.ignore()
						<< " [" << activationToPrintableString( def.activation()) << "]" << std::endl;
				break;
			case Lexer::Definition::EolnComment:
				dbgout.out() << deftypename << " " << def.start()
						<< " [" << activationToPrintableString( def.activation()) << "]" << std::endl;
				break;
			case Lexer::Definition::BracketComment:
				dbgout.out() << deftypename << " " << def.start() << " " << def.end() << " [" << def.activation() << "]" << std::endl;
				break;
		}
	}
	dbgout.out() << std::endl;
}

static void printNonterminals( const std::vector<std::string>& nonterminals, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- Nonterminals:" << std::endl;
	int ntidx = 0;
	for (auto const& ident : nonterminals)
	{
		++ntidx;
		dbgout.out() << "(" << ntidx << ") " << ident << std::endl;
	}
	dbgout.out() << std::endl;
}

static void printLanguageHeader( const std::string& language, const std::string& typesystem, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "== " << language;
	if (!typesystem.empty()) dbgout.out() << " (" << typesystem << ")";
	dbgout.out() << " ==" << std::endl;
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
		std::vector<TransitionItem> itemlist = invst.second.unpack();
		for (auto const& item : itemlist)
		{
			dbgout.out() << "\t" << prodlist[ item.prodindex].tostring( item.prodpos) << std::endl;
		}
	}
	dbgout.out() << std::endl;
}

static void printLalr1States( const std::map<TransitionState,int>& lr0statemap, const std::map<TransitionState,int>& lr1statemap, const CalculateClosureLr1& calcClosureLr1,
			 const std::vector<ProductionDef>& prodlist, const Lexer& lexer, const std::vector<Automaton::Call>& calls, Automaton::DebugOutput dbgout)
{
	std::map<int,TransitionState> stateinvmap;
	for (auto const& st : lr1statemap)
	{
		int stateidx = lr0statemap.at( getLr0TransitionStateFromLr1State( st.first));
		stateinvmap[ stateidx].join( st.first);
	}
	dbgout.out() << "-- LALR(1) States (Merged LR(1) elements assigned to LR(0) states):" << std::endl;
	for (auto const& invst : stateinvmap)
	{
		dbgout.out() << "[" << invst.first << "]" << std::endl;
		std::vector<TransitionItem> itemlist = invst.second.unpack();
		for (auto const& item : itemlist)
		{
			dbgout.out() << "\t" << prodlist[ item.prodindex].tostring( item.prodpos) << ", " << getLexemName(lexer,item.follow);
			if (item.prodpos == (int)prodlist[ item.prodindex].right.size())
			{
				if (item.prodindex == 0 && item.follow == 0)
				{
					dbgout.out() << " -> ACCEPT";
				}
				else
				{
					try
					{
						ReductionDef rd = getReductionDef( invst.second, item.follow, prodlist, lexer);
						dbgout.out() << " -> REDUCE " << rd.headname << " #" << rd.count;
						if (rd.callidx) dbgout.out() << " CALL " << calls[ rd.callidx-1].tostring();
					}
					catch (const Error& err)
					{
						dbgout.out() << std::endl;
						throw err;
					}
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
	if (!calls.empty())
	{
		dbgout.out() << "-- Function calls:" << std::endl;
		int callidx = 0;
		for (auto const& call : calls)
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
}

static void printStateTransitions( const Automaton& automaton, const Lexer& lexer, Automaton::DebugOutput dbgout)
{
	if (!automaton.actions().empty())
	{
		dbgout.out() << "-- Action table:" << std::endl;
		int prevState = -1;
		for (auto const& at : automaton.actions())
		{
			auto actionKey = at.first;
			auto actionVal = at.second;
			if (prevState != actionKey.state())
			{
				dbgout.out() << "[" << actionKey.state() << "]" << std::endl;
				prevState = actionKey.state();
			}
			dbgout.out() << "\t" << getLexemName( lexer, actionKey.terminal()) << " => " << automaton.actionString( actionVal) << std::endl;
		}
		dbgout.out() << std::endl;
	}
	if (!automaton.gotos().empty())
	{
		dbgout.out() << "-- Goto table:" << std::endl;
		int prevState = -1;
		for (auto const& gto : automaton.gotos())
		{
			auto gotoKey = gto.first;
			auto gotoVal = gto.second;
			if (prevState != gotoKey.state())
			{
				dbgout.out() << "[" << gotoKey.state() << "]" << std::endl;
				prevState = gotoKey.state();
			}
			dbgout.out() << "\t" << automaton.nonterminal( gotoKey.nonterminal()) << " => " << gotoVal.state() << std::endl;
		}
		dbgout.out() << std::endl;
	}
}

std::string Automaton::actionString( const Action& action) const
{
	char buf[ 2048];
	std::size_t len = 0;
	switch (action.type())
	{
		case Automaton::Action::Shift:
			len = std::snprintf( buf, sizeof( buf), "%s goto %d", "Shift", action.state());
			break;
		case Automaton::Action::Reduce:
		{
			if (action.call())
			{
				std::string callstr = call( action.call()).tostring();
				len = std::snprintf( buf, sizeof( buf), "%s #%d %s call %s", "Reduce", action.count(), nonterminal( action.nonterminal()).c_str(), callstr.c_str());
			}
			else
			{
				len = std::snprintf( buf, sizeof( buf), "%s #%d %s", "Reduce", action.count(), nonterminal( action.nonterminal()).c_str());
			}
			break;
		}
		case Automaton::Action::Accept:
			len = std::snprintf( buf, sizeof( buf), "%s", "Accept");
	}
	if (len >= sizeof(buf)) len = sizeof(buf);
	return std::string( buf, len);
}

std::string Automaton::Call::tostring() const
{
	std::string rt( m_function);
	switch (m_argtype)
	{
		case Automaton::Call::NoArg:
			break;
		case Automaton::Call::StringArg:
			rt.push_back( ' ');
			rt.push_back( '"');
			rt.append( m_arg);
			rt.push_back( '"');
			break;
		case Automaton::Call::ReferenceArg:
			rt.push_back( ' ');
			rt.append( m_arg);
			break;
	}
	return rt;
}

void Automaton::build( const std::string& source, std::vector<Error>& warnings, DebugOutput dbgout)
{
	// [1] Parse grammar and test completeness:
	LanguageDef langdef = parseLanguageDef( source);
	if (dbgout.enabled() && !langdef.language.empty()) printLanguageHeader( langdef.language, langdef.typesystem, dbgout);
	if (dbgout.enabled( DebugOutput::Lexems)) printLexems( langdef.lexer, dbgout);
	if (dbgout.enabled( DebugOutput::Nonterminals)) printNonterminals( langdef.nonterminals, dbgout);
	if (dbgout.enabled( DebugOutput::Productions)) printProductions( langdef.prodlist, dbgout);

	// [2] Test complexity boundaries:
	if (langdef.prodlist.size() >= (std::size_t)MaxNofProductions)
	{
		throw Error( Error::ComplexityMaxStateInGrammarDef);
	}
	if (langdef.lexer.nofTerminals() >= MaxTerminal)
	{
		throw Error( Error::ComplexityMaxTerminalInGrammarDef);
	}
	for (auto const& prod : langdef.prodlist) 
	{
		if (prod.right.size() >= MaxProductionLength)
		{
			throw Error( Error::ComplexityMaxProductionLengthInGrammarDef);
		}
		if (prod.priority.value < 0 || prod.priority.value >= MaxPriority)
		{
			throw Error( Error::ComplexityMaxProductionPriorityInGrammarDef);
		}
	}
	// [3] Calculate some sets needed for the build:
	std::set<int> nullableNonterminalSet = getNullableNonterminalSet( langdef.prodlist);
	std::map<int, std::set<int> > nonTerminalFirstSetMap = getNonTerminalFirstSetMap( langdef.prodlist, nullableNonterminalSet);

	CalculateClosureLr0 calcClosureLr0( langdef.prodlist);
	CalculateClosureLr1 calcClosureLr1( langdef.prodlist, nonTerminalFirstSetMap, nullableNonterminalSet);

	std::map<TransitionState,int> stateAssignmentsLr0 = getAutomatonStateAssignments( langdef.prodlist, calcClosureLr0);
	std::map<TransitionState,int> stateAssignmentsLr1 = getAutomatonStateAssignments( langdef.prodlist, calcClosureLr1);

	if (dbgout.enabled( DebugOutput::States))
	{
		printLr0States( stateAssignmentsLr0, calcClosureLr1, langdef.prodlist, langdef.lexer, dbgout);
		printLalr1States( stateAssignmentsLr0, stateAssignmentsLr1, calcClosureLr1, langdef.prodlist, langdef.lexer, langdef.calls, dbgout);
	}
	if (dbgout.enabled( DebugOutput::FunctionCalls))
	{
		printFunctionCalls( langdef.calls, dbgout);
	}
	// [3.1] Test complexity boundaries:
	if (stateAssignmentsLr0.size() >= MaxState)
	{
		throw Error( Error::ComplexityMaxStateInGrammarDef);
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
			m_actions[ ActionKey( stateidx, 0/*EOF terminal*/)] = Action::accept();
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
				Priority shiftPriority = getShiftPriority( lr1State, terminal, langdef.prodlist, langdef.lexer);

				ActionKey key( stateidx, terminal);
				insertAction( priorityMap, m_actions, langdef.prodlist, lr1State, terminal, key, Action::shift(to_stateidx), shiftPriority, warnings, langdef.lexer);
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
			ReductionDef rd = getReductionDef( lr1State, terminal, langdef.prodlist, langdef.lexer);

			ActionKey key( stateidx, terminal);
			insertAction( priorityMap, m_actions, langdef.prodlist, lr1State, terminal, key, Action::reduce( rd.head, rd.callidx, rd.count), rd.priority, warnings, langdef.lexer);
		}
	}
	if (nofAcceptStates == 0)
	{
		throw Error( Error::NoAcceptStatesInGrammarDef);
	}
	std::swap( m_language, langdef.language);
	std::swap( m_typesystem, langdef.typesystem);
	std::swap( m_lexer, langdef.lexer);
	std::swap( m_calls, langdef.calls);
	std::swap( m_nonterminals, langdef.nonterminals);

	if (dbgout.enabled( DebugOutput::StateTransitions))
	{
		printStateTransitions( *this, m_lexer, dbgout);
	}
}

