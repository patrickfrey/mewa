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
#include "strings.hpp"
#include <set>
#include <unordered_map>
#include <string_view>
#include <algorithm>
#include <type_traits>
#include <memory_resource>

using namespace mewa;

static std::string getLexemName( const Lexer& lexer, int terminal)
{
        return terminal ? lexer.lexemName( terminal) : std::string("$");
}

static std::pair<FlatSet<int>,bool> getLr1FirstSet(
				const ProductionDef& prod, int prodpos, 
				const std::map<int, std::set<int> >& nonTerminalFirstSetMap,
				const std::set<int>& nullableNonterminalSet)
{
	std::pair<FlatSet<int>,bool> rt;
	rt.second = false;
	for (; prodpos < (int)prod.right.size(); ++prodpos)
	{
		const ProductionNodeDef& nd = prod.right[ prodpos];
		if (nd.type() == ProductionNodeDef::Terminal)
		{
			rt.first.insert( nd.index());
			break;
		}
		else if (nd.type() == ProductionNodeDef::NonTerminal)
		{
			auto fi = nonTerminalFirstSetMap.find( nd.index());
			if (fi != nonTerminalFirstSetMap.end())
			{
				rt.first.insert( fi->second.begin(), fi->second.end());
				if (nullableNonterminalSet.find( nd.index()) == nullableNonterminalSet.end())
				{
					break;
				}
			}
		}
	}
	if (prodpos == (int)prod.right.size())
	{
		rt.second = true;
	}
	return rt;
}

static TransitionState getLr0TransitionStateClosure( const TransitionState& ts, const ProductionDefList& prodlist)
{
	TransitionState rt( ts);
	std::vector<int> orig;
	orig.reserve( ts.packedElements().capacity());
	orig.insert( orig.end(), ts.packedElements().begin(), ts.packedElements().end());

	for (std::size_t oidx = 0; oidx < orig.size(); ++oidx)
	{
		auto item = TransitionItem::unpack( orig[ oidx]);

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				auto prodrange = prodlist.equal_range( nd.index());
				for (auto hi = prodrange.first; hi != prodrange.second; ++hi)
				{
					TransitionItem insitem( hi.index(), 0, 0/*follow*/, 0/*priority*/);
					if (rt.insert( insitem) == true/*insert took place*/)
					{
						orig.push_back( insitem.packed());
					}
				}
			}
		}
	}
	return rt;
}

struct FollowMap
{
	FollowMap( const ProductionDefList& prodlist, const std::map<int, std::set<int> >& nonTerminalFirstSetMap, const std::set<int>& nullableNonterminalSet)
		:m_nonterminalNodeToFollowHandleMap(),m_followMap( 0/*startidx*/)
	{
		m_followMap.get( {0}); // ... set 0 => {0}
		for (int prodindex = 0; prodindex < (int)prodlist.size(); ++prodindex)
		{
			const ProductionDef& prod = prodlist[ prodindex];
			for (int prodpos=0; prodpos < (int)prod.right.size(); ++prodpos)
			{
				if (prod.right[ prodpos].type() == ProductionNodeDef::NonTerminal)
				{
					std::pair<FlatSet<int>,bool> firstOfRest = getLr1FirstSet( prod, prodpos+1, nonTerminalFirstSetMap, nullableNonterminalSet);
					Handle fh( m_followMap.get( firstOfRest.first), firstOfRest.second);
					FollowKey key( prodindex, prodpos);
					if (!m_nonterminalNodeToFollowHandleMap.insert( {key, fh}).second)
					{
						throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
					}
				}
			}
		}
	}

	struct FollowKey
	{
		int prodindex;
		int prodpos;

		FollowKey( int prodindex_, int prodpos_)
			:prodindex(prodindex_),prodpos(prodpos_){}
		FollowKey( const FollowKey& o)
			:prodindex(o.prodindex),prodpos(o.prodpos){}

		bool operator < (const FollowKey& o) const noexcept
		{
			return prodindex == o.prodindex ? prodpos < o.prodpos : prodindex < o.prodindex;
		}
	};
	struct Handle
	{
		int handle;
		bool inherit;

		Handle()
			:handle(0),inherit(false){}
		Handle( int handle_, bool inherit_)
			:handle(handle_),inherit(inherit_){}
		Handle( const Handle& o)
			:handle(o.handle),inherit(o.inherit){}
	};

	const Handle& get( const FollowKey& nd)
	{
		auto fi = m_nonterminalNodeToFollowHandleMap.find( nd);
		if (fi == m_nonterminalNodeToFollowHandleMap.end())
		{
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		return fi->second;
	}

	int join( int handle1, int handle2)
	{
		if (m_followMap.size() >= Automaton::MaxTerminal-1)
		{
			throw Error( Error::ComplexityLR1FollowSetsInGrammarDef);
		}
		auto ji = m_joinMap.find( {handle1, handle2});
		if (ji != m_joinMap.end()) return ji->second;

		return m_joinMap[ {handle1, handle2}] = m_followMap.join( handle1, handle2);
	}

	std::string follow2String( int follow, const Lexer& lexer) const
	{
		std::string rt;
		int tidx = 0;
		for (int terminal : m_followMap.content( follow))
		{
			if (tidx++)
			{
				rt.push_back( ' ');
			}
			rt.append( getLexemName( lexer, terminal));
		}
		return rt;
	}

	void printFollowSets( std::ostream& out, const Lexer& lexer) const
	{
		for (std::size_t follow=0; follow != m_followMap.size(); ++follow)
		{
			out << "[" << follow << "]: {" << follow2String( follow, lexer) << "}\n";
		}
	}

	const FlatSet<int>& content( int handle) const noexcept
	{
		return m_followMap.content( handle);
	}

	void remap( const std::set<int>& usedHandles)
	{
		m_joinMap.clear();
		m_nonterminalNodeToFollowHandleMap.clear();
		IntSetHandleMap newFollowMap( 0/*startidx*/);
		if (usedHandles.empty() || *usedHandles.begin() != 0)
		{
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		int hidx = 0;
		for (auto handle : usedHandles)
		{
			if (hidx != newFollowMap.get( m_followMap.content( handle)))
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			++hidx;
		}
		m_followMap = std::move( newFollowMap);
	}

private:
	std::map<FollowKey, Handle> m_nonterminalNodeToFollowHandleMap;
	std::map<std::pair<int,int>,int> m_joinMap;
	IntSetHandleMap m_followMap;
};

static void checkTransitionStateFollow( TransitionState& state, const char* what)
{
	for (std::size_t ii=1; ii<state.size(); ++ii)
	{
		auto item = TransitionItem::unpack( state.packedElements()[ ii]);
		auto prev = TransitionItem::unpack( state.packedElements()[ ii-1]);
		if (item.prodindex == prev.prodindex && item.prodpos == prev.prodpos)
		{
			std::cerr << "HALLY GALLY " << what << std::endl;
		}
	}
}

static void mergeTransitionStateFollow( TransitionState& state, FollowMap& followMap, const ProductionDefList& prodlist)
{
	TransitionState newstate;
	std::size_t oidx = 0;
	while (oidx < state.size())
	{
		auto item = TransitionItem::unpack( state.packedElements()[ oidx]);
		int joinedFollow = item.follow;

		std::size_t oidx2 = oidx+1; 
		for (; oidx2 < state.size(); ++oidx2)
		{
			auto item2 = TransitionItem::unpack( state.packedElements()[ oidx2]);
			if (item2.prodindex != item.prodindex || item2.prodpos != item.prodpos)
			{
				break;
			}
			joinedFollow = followMap.join( item2.follow, joinedFollow);
			if (item.priority != item2.priority)
			{
				throw Error( Error::PriorityConflictInGrammarDef,
						prodlist[ item.prodindex].tostring( item.prodpos)
						+ ", " + prodlist[ item2.prodindex].tostring( item2.prodpos));
			}
		}
		newstate.insert( {item.prodindex, item.prodpos, joinedFollow, item.priority});
		oidx = oidx2;
	}
	checkTransitionStateFollow( newstate, "merge");
	state = std::move( newstate);
}

static TransitionState getLr1TransitionStateClosure(
				const TransitionState& ts,
				const ProductionDefList& prodlist,
				FollowMap& followMap)
{
	TransitionState rt( ts);
	std::vector<int> orig;
	orig.reserve( ts.packedElements().capacity());
	orig.insert( orig.end(), ts.packedElements().begin(), ts.packedElements().end());

	for (std::size_t oidx = 0; oidx < orig.size(); ++oidx)
	{
		auto item = TransitionItem::unpack( orig[ oidx]);

		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos];
			if (nd.type() == ProductionNodeDef::NonTerminal)
			{
				auto followHandle = followMap.get( {item.prodindex, item.prodpos});
				auto prodrange = prodlist.equal_range( nd.index());
				for (auto hi = prodrange.first; hi != prodrange.second; ++hi)
				{
					{
						TransitionItem insitem( hi.index(), 0/*prodpos*/, followHandle.handle, hi->priority);
						if (rt.insert( insitem) == true/*insert took place*/)
						{
							orig.push_back( insitem.packed());
						}
					}
					if (followHandle.inherit)
					{
						TransitionItem insitem( hi.index(), 0/*prodpos*/, item.follow, hi->priority);
						if (rt.insert( insitem) == true/*insert took place*/)
						{
							orig.push_back( insitem.packed());
						}
					}
				}
			}
		}
	}
	mergeTransitionStateFollow( rt, followMap, prodlist);
	return rt;
}

static TransitionState joinLr1TransitionStates( const TransitionState& s1, const TransitionState& s2, const ProductionDefList& prodlist, FollowMap& followMap)
{
	TransitionState rt( s1);
	rt.join( s2);
	mergeTransitionStateFollow( rt, followMap, prodlist);
	return rt;
}

static TransitionState getLr0TransitionStateFromLr1State( const TransitionState& ts)
{
	TransitionState rt;
	for (auto elem : ts.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
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
	ProductionDefList m_prodlist;
};

class CalculateClosureLr1
{
public:
	CalculateClosureLr1(
			const std::vector<ProductionDef>& prodlist_,
			const std::map<int, std::set<int> >& nonTerminalFirstSetMap_,
			const std::set<int>& nullableNonterminalSet_)
		:m_prodlist(prodlist_),m_followMap(prodlist_,nonTerminalFirstSetMap_,nullableNonterminalSet_){}

	TransitionState operator()( const TransitionState& state)
	{
		return getLr1TransitionStateClosure( state, m_prodlist, m_followMap);
	}

	const FollowMap& followMap() const
	{
		return m_followMap;
	}
	FollowMap& followMap()
	{
		return m_followMap;
	}

private:
	ProductionDefList m_prodlist;
	FollowMap m_followMap;
};

static std::vector<ProductionNode> getGotoNodes( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	std::vector<ProductionNode> rt;
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			rt.push_back( prod.right[ item.prodpos]);
		}
	}
	return rt;
}

static std::string getStateTransitionString(
	const TransitionState& state, int terminal, const std::vector<ProductionDef>& prodlist,
	const Lexer& lexer, const FollowMap& followMap)
{
	const char* redustr = nullptr;
	const char* shiftstr = nullptr;
	std::string prodstr;
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
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
		else if (followMap.content( item.follow).contains( terminal))
		{
			redustr = "<-";
			if (prodstr.empty()) prodstr = prod.tostring( item.prodpos);
		}
	}
	if (redustr && shiftstr)
	{
		return prodstr + " " + shiftstr + "|" + redustr + " " + getLexemName( lexer, terminal);
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

struct ProductionShiftNode
{
	ProductionNode node;
	int goto_stateidx;
	Priority priority;

	ProductionShiftNode( ProductionNode node_, int goto_stateidx_, const Priority priority_)
		:node(node_),goto_stateidx(goto_stateidx_),priority(priority_){}
	ProductionShiftNode( const ProductionShiftNode& o)
		:node(o.node),goto_stateidx(o.goto_stateidx),priority(o.priority){}
};

static std::vector<ProductionShiftNode> getShiftNodes(
	const TransitionState& state, const TransitionItemGotoMap& gotoMap, const std::vector<ProductionDef>& prodlist)
{
	std::vector<ProductionShiftNode> rt;
	int buffer[ 2048];
	std::pmr::monotonic_buffer_resource memrsc( buffer, sizeof buffer);
	std::pmr::multimap<ProductionNode,int> nodemap( &memrsc);
	std::pmr::map<ProductionNode,Priority> prioritymap;

	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos < (int)prod.right.size())
		{
			const ProductionNodeDef& nd = prod.right[ item.prodpos]; 
			int gkey = TransitionItem( item.prodindex, item.prodpos+1, 0/*follow*/, Priority()).packed();
			auto gtorange = gotoMap.equal_range( gkey);

			auto crange = nodemap.equal_range( nd);
			if (crange.first == crange.second)
			{
				for (auto gtoi = gtorange.first; gtoi != gtorange.second; ++gtoi)
				{
					nodemap.insert( {nd,gtoi->second} );
				}
			}
			else
			{
				std::pmr::vector<int> keep( &memrsc); //... intersection of goto candidates
				for (auto ci = crange.first; ci != crange.second; ++ci)
				{
					for (auto gtoi = gtorange.first; gtoi != gtorange.second; ++gtoi)
					{
						if (ci->second == gtoi->second)
						{
							keep.push_back( ci->second);
							break;
						}
					}
				}
				if (keep.empty())
				{
					std::string nodestr = nd.tostring();
					std::string prodprefix = prod.prefix_tostring( item.prodpos);
					throw Error( Error::ShiftShiftConflictInGrammarDef, prodprefix + " -> " + nodestr);
				}
				nodemap.erase( nd);
				for (auto kp : keep)
				{
					nodemap.insert( {nd,kp} );
				}
			}
			auto pins = prioritymap.insert( {nd,prod.priority} );
			if (pins.second == false/*no insert took place*/)
			{
				if (pins.first->second != prod.priority)
				{
					std::string nodestr = nd.tostring();
					std::string prodprefix = prod.prefix_tostring( item.prodpos);
					throw Error( Error::PriorityConflictInGrammarDef, prodprefix + " -> " + nodestr);
				}
			}
		}
	}
	for (auto const& nda : nodemap)
	{
		rt.push_back( ProductionShiftNode( nda.first, nda.second, prioritymap.at( nda.first)));
	}
	return rt;
}

static FlatSet<int> getReduceFollow( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	FlatSet<int> rt;
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos == (int)prod.right.size())
		{
			rt.insert( item.follow);
		}
	}
	return rt;
}

struct ReductionDef
{
	Priority priority;
	int head;
	int callidx;
	int count;
	int prodindex;
	std::string_view headname;

	ReductionDef()
		:priority(),head(-1),callidx(-1),count(-1),prodindex(-1),headname("",0){}
	ReductionDef( const ReductionDef& o)
		:priority(o.priority),head(o.head),callidx(o.callidx),count(o.count),prodindex(o.prodindex),headname(o.headname){}
};

static ReductionDef getReductionDef( const TransitionState& state, int follow, const std::vector<ProductionDef>& prodlist, const Lexer& lexer, const FollowMap& followMap)
{
	ReductionDef rt;
	int last_prodidx = -1;
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
		const ProductionDef& prod = prodlist[ item.prodindex];
		if (item.prodpos == (int)prod.right.size() && item.follow == follow)
		{
			if (last_prodidx == -1)
			{
				rt.priority = item.priority;
				rt.head = prod.left.index();
				rt.headname = prod.left.name();
				rt.callidx = prod.callidx;
				rt.count = prod.right.size();
				rt.prodindex = item.prodindex;
				last_prodidx = item.prodindex;
			}
			else if (rt.head != prod.left.index() || rt.count != (int)prod.right.size() || rt.callidx != prod.callidx)
			{
				throw Error( Error::ReduceReduceConflictInGrammarDef,
						prod.tostring() + ", "
						+ prodlist[ last_prodidx].tostring() + " <- "
						+ followMap.follow2String( follow, lexer));
			}
			else if (rt.priority != item.priority)
			{
				throw Error( Error::PriorityConflictInGrammarDef,
						prod.tostring() + ", "
						+ prodlist[ last_prodidx].tostring() + " <- "
						+ followMap.follow2String( follow, lexer));
			}
		}
	}
	return rt;
}

template <class CalculateClosureFunctor>
static TransitionState getGotoState(
		const TransitionState& state, const ProductionNode& gto,
		const std::vector<ProductionDef>& prodlist, CalculateClosureFunctor& calculateClosure)
{
	TransitionState gtoState;
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
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
	return calculateClosure( gtoState);
}

template <class CalculateClosureFunctor>
static std::unordered_map<TransitionState,int> getAutomatonStateAssignments(
	const std::vector<ProductionDef>& prodlist, CalculateClosureFunctor& calculateClosure)
{
	std::unordered_map<TransitionState,int> rt;

	rt.insert( {calculateClosure( {{0,0,0,0}}), 1});
	std::vector<TransitionState> statestk( {rt.begin()->first});

	for (std::size_t stkidx = 0; stkidx < statestk.size(); ++stkidx)
	{
		std::vector<TransitionState> newstates;
		auto const state = statestk[ stkidx];
		auto gtos = getGotoNodes( state, prodlist);
		for (auto const& gto : gtos)
		{
			auto newState = getGotoState( state, gto, prodlist, calculateClosure);
			if (newState.empty()) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			if (rt.insert( {newState,rt.size()+1}).second == true/*insert took place*/)
			{
				newstates.push_back( newState);
			}
		}
		statestk.insert( statestk.end(), newstates.begin(), newstates.end());
	}
	return rt;
}

static bool isAcceptState( const TransitionState& state, const std::vector<ProductionDef>& prodlist)
{
	for (int elem : state.packedElements())
	{
		auto item = TransitionItem::unpack( elem);
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
						rt[ prod.left.index()];
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
		throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
	}
	return rt;
}

static std::map<int,TransitionState> calculateLalr1StateMap(
		const std::unordered_map<TransitionState,int>& lr0statemap,
		const std::unordered_map<TransitionState,int>& lr1statemap,
		const std::vector<ProductionDef>& prodlist,
		FollowMap& followMap)
{
	std::map<int,TransitionState> rt;
	for (auto const& st : lr1statemap)
	{
		int stateidx = lr0statemap.at( getLr0TransitionStateFromLr1State( st.first));
		rt[ stateidx] = joinLr1TransitionStates( rt[ stateidx], st.first, prodlist, followMap);
	}
	return rt;
}

static std::set<int> getLalr1UsedFollowHandles( const std::map<int,TransitionState>& lalr1States)
{
	std::set<int> rt;
	for (const auto& st : lalr1States)
	{
		for (auto elem : st.second.packedElements())
		{
			auto item = TransitionItem::unpack( elem);
			rt.insert( item.follow);
		}
	}
	return rt;
}

TransitionItemGotoMap calculateLalr1TransitionItemGotoMap(
	const std::map<int,TransitionState>& lalr1States, const std::vector<ProductionDef>& prodlist)
{
	TransitionItemGotoMap rt;
	for (auto const& st : lalr1States)
	{
		for (auto elem : st.second.packedElements())
		{
			auto item = TransitionItem::unpack( elem);
			if (item.prodpos > 0)
			{
				int key = TransitionItem( item.prodindex, item.prodpos, 0/*follow*/, Priority()).packed();
				rt.insert( {key, st.first} );
			}
		}
	}
	return rt;
}

static void printGotoMap( std::ostream& out, const TransitionItemGotoMap& map)
{
	std::multimap<int,int> orderedmap( map.begin(), map.end());
	for (auto kv : orderedmap)
	{
		auto item = TransitionItem::unpack( kv.first);
		out << string_format( "prod %d, pos %d => %d", item.prodindex, item.prodpos, kv.second) << std::endl;
	}
}

static void insertAction(
	std::map<Automaton::ActionKey,Priority>& priorityMap,
	std::map<Automaton::ActionKey,Automaton::Action>& actionMap,
	const std::vector<ProductionDef>& prodlist,
	const FollowMap& followMap,
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
					getStateTransitionString( lr1State, terminal, prodlist, lexer, followMap)));
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
				getStateTransitionString( lr1State, terminal, prodlist, lexer, followMap)));
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

static void printLr0States(
		const std::unordered_map<TransitionState,int>& lr0statemap,
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
		for (int elem : invst.second.packedElements())
		{
			auto item = TransitionItem::unpack( elem);
			dbgout.out() << "\t" << prodlist[ item.prodindex].tostring( item.prodpos) << std::endl;
		}
	}
	dbgout.out() << std::endl;
}

static void printLalr1States(
		const std::map<int,TransitionState>& lalr1States,
		const TransitionItemGotoMap& gotoMap,
		const std::vector<ProductionDef>& prodlist, const FollowMap& followMap, const Lexer& lexer,
		const std::vector<Automaton::Call>& calls, Automaton::DebugOutput dbgout)
{
	dbgout.out() << "-- LR(1) FOLLOW sets:" << std::endl;
	followMap.printFollowSets( dbgout.out(), lexer);
	dbgout.out() << std::endl;

	std::cerr << "-- LALR(1) GOTO Map:" << std::endl;
	printGotoMap( dbgout.out(), gotoMap);
	std::cerr << std::endl;

	dbgout.out() << "-- LALR(1) States (Merged LR(1) elements assigned to LR(0) states):" << std::endl;
	for (auto const& invst : lalr1States)
	{
		dbgout.out() << "[" << invst.first << "]" << std::endl;
		if (isAcceptState( invst.second, prodlist))
		{
			dbgout.out() << " -> ACCEPT" << std::endl;
		}
		auto shiftNodes = getShiftNodes( invst.second, gotoMap, prodlist);
		for (auto const& shft : shiftNodes)
		{
			if (shft.node.type() == ProductionNodeDef::Terminal)
			{
				dbgout.out() << " -> SHIFT " << getLexemName( lexer, shft.node.index()) << " GOTO " << shft.goto_stateidx << std::endl;
			}
			else
			{
				dbgout.out() << " -> GOTO " << shft.goto_stateidx << std::endl;
			}
		}
		auto reduFollows = getReduceFollow( invst.second, prodlist);
		for (auto follow : reduFollows)
		{
			try
			{
				ReductionDef rd = getReductionDef( invst.second, follow, prodlist, lexer, followMap);
				dbgout.out() << "\t" << prodlist[ rd.prodindex].tostring() << ", FOLLOW [" << follow << "]";			
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

	CalculateClosureLr0 calculateClosureLr0( langdef.prodlist);
	CalculateClosureLr1 calculateClosureLr1( langdef.prodlist, nonTerminalFirstSetMap, nullableNonterminalSet);
	FollowMap& followMap = calculateClosureLr1.followMap();

	std::unordered_map<TransitionState,int> stateAssignmentsLr0 = getAutomatonStateAssignments( langdef.prodlist, calculateClosureLr0);
	std::unordered_map<TransitionState,int> stateAssignmentsLr1 = getAutomatonStateAssignments( langdef.prodlist, calculateClosureLr1);
	std::map<int,TransitionState> lalr1States = calculateLalr1StateMap( stateAssignmentsLr0, stateAssignmentsLr1, langdef.prodlist, followMap);
	TransitionItemGotoMap lalr1TransitionItemGotoMap = calculateLalr1TransitionItemGotoMap( lalr1States, langdef.prodlist);

	std::set<int> usedFollowHandles = getLalr1UsedFollowHandles( lalr1States);
	followMap.remap( usedFollowHandles);

	if (dbgout.enabled( DebugOutput::States))
	{
		printLr0States( stateAssignmentsLr0, langdef.prodlist, langdef.lexer, dbgout);
		printLalr1States( lalr1States, lalr1TransitionItemGotoMap, langdef.prodlist, followMap, langdef.lexer, langdef.calls, dbgout);
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

	for (auto const& lalr1StateAssign : lalr1States)
	{
		int stateidx = lalr1StateAssign.first;
		auto const& lalr1State = lalr1StateAssign.second;

		if (isAcceptState( lalr1State, langdef.prodlist))
		{
			m_actions[ ActionKey( stateidx, 0/*EOF terminal*/)] = Action::accept();
			++nofAcceptStates;
		}
		std::vector<ProductionShiftNode> shiftNodes = getShiftNodes( lalr1State, lalr1TransitionItemGotoMap, langdef.prodlist);
		for (auto const& shft : shiftNodes)
		{
			int to_stateidx = shft.goto_stateidx;
			if (shft.node.type() == ProductionNode::Terminal)
			{
				int terminal = shft.node.index();

				ActionKey key( stateidx, terminal);
				insertAction( priorityMap, m_actions, langdef.prodlist, followMap,
						lalr1State, terminal, key, Action::shift(to_stateidx),
						shft.priority, warnings, langdef.lexer);
			}
			else
			{
				auto gtoins = m_gotos.insert( {GotoKey( stateidx, shft.node.index()), Goto( to_stateidx)} );
				if (gtoins.second == false && gtoins.first->second != Goto( to_stateidx))
				{
					throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__)); //.... should not happen
				}
			}
		}
		auto reduFollows = getReduceFollow( lalr1State, langdef.prodlist);
		for (auto follow : reduFollows)
		{
			ReductionDef rd = getReductionDef( lalr1State, follow, langdef.prodlist, langdef.lexer, followMap);
			for (int terminal : followMap.content( follow))
			{
				ActionKey key( stateidx, terminal);
				insertAction( priorityMap, m_actions, langdef.prodlist, followMap, lalr1State, terminal, key, 
						Action::reduce( rd.head, rd.callidx, rd.count), rd.priority, warnings, langdef.lexer);
			}
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

