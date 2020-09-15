/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Some internal structures used to build the LALR(1) automaton
/// \file "automaton_structs.hpp"
#ifndef _MEWA_AUTOMATON_STRUCTS_HPP_INCLUDED
#define _MEWA_AUTOMATON_STRUCTS_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "error.hpp"
#include <utility>
#include <string>
#include <string_view>
#include <set>
#include <vector>

namespace mewa {

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

        Type type() const noexcept					{return m_type;}
        int index() const noexcept					{return m_index;}

        void defineAsTerminal( int index_) noexcept			{m_type = Terminal; m_index = index_;}
        void defineAsNonTerminal( int index_) noexcept			{m_type = NonTerminal; m_index = index_;}

        bool operator == (const ProductionNode& o) const noexcept	{return m_type == o.m_type && m_index == o.m_index;}
        bool operator != (const ProductionNode& o) const noexcept	{return m_type != o.m_type || m_index != o.m_index;}

	bool operator < (const ProductionNode& o) const noexcept
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

	const std::string_view& name() const noexcept		{return m_name;}

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
	Priority& operator=( const Priority& o)
		{value=o.value; assoziativity=o.assoziativity; return *this;}

	bool operator == (const Priority& o) const noexcept	{return value == o.value && assoziativity == o.assoziativity;}
	bool operator != (const Priority& o) const noexcept	{return value != o.value || assoziativity != o.assoziativity;}
	bool operator < (const Priority& o) const noexcept 	{return value == o.value ? assoziativity < o.assoziativity : value < o.value;}
	bool defined() const noexcept				{return value != 0 || assoziativity != Assoziativity::Undefined;}

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
	int packed() const noexcept
	{
		return (value << 2) + (int)assoziativity;
	}
	static Priority unpack( int pkg) noexcept
	{
		return Priority( pkg >> 2, (Assoziativity)(pkg & 3));
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
	int prodindex;		// [0..MaxNofProductions]
	int prodpos;		// [0..MaxProductionLength]
	int follow;		// [0..MaxTerminal]
	Priority priority;	// [0..2*MaxPriority]

	TransitionItem( int prodindex_, int prodpos_, int follow_, const Priority priority_)
		:prodindex(prodindex_),prodpos(prodpos_),follow(follow_),priority(priority_){}
	TransitionItem( const TransitionItem& o)
		:prodindex(o.prodindex),prodpos(o.prodpos),follow(o.follow),priority(o.priority){}

	bool operator < (const TransitionItem& o) const noexcept
	{
		return prodindex == o.prodindex
			? prodpos == o.prodpos
				? follow == o.follow
					? priority < o.priority
					: follow < o.follow
				: prodpos < o.prodpos
			: prodindex < o.prodindex;
	}
	bool operator == (const TransitionItem& o) const noexcept
	{
		return equal( o);
	}
	bool operator != (const TransitionItem& o) const noexcept
	{
		return !equal( o);
	}

	bool equal( const TransitionItem& o) const noexcept
	{
		return prodindex == o.prodindex && prodpos == o.prodpos && follow == o.follow && priority == o.priority;
	}

	int packed() const noexcept
	{
		static_assert (Automaton::ShiftNofProductions
				+ Automaton::ShiftProductionLength
				+ Automaton::ShiftTerminal
				+ Automaton::ShiftPriorityWithAssoziativity <= 31, "sizeof packed transition item"); 
		int rt = 0;
		rt |= prodindex << (Automaton::ShiftProductionLength + Automaton::ShiftTerminal + Automaton::ShiftPriorityWithAssoziativity);
		rt |= prodpos << (Automaton::ShiftTerminal + Automaton::ShiftPriorityWithAssoziativity);
		rt |= follow << Automaton::ShiftPriorityWithAssoziativity;
		rt |= priority.packed();
		return rt;
	}

	static TransitionItem unpack( int pkg) noexcept
	{
		int prodindex_ = pkg >> (Automaton::ShiftProductionLength + Automaton::ShiftTerminal + Automaton::ShiftPriorityWithAssoziativity);
		int prodpos_ = (pkg >> (Automaton::ShiftTerminal + Automaton::ShiftPriorityWithAssoziativity)) & Automaton::MaskProductionLength;
		int follow_ = (pkg >> Automaton::ShiftPriorityWithAssoziativity) & Automaton::MaskTerminal;
		Priority priority_( Priority::unpack( pkg & Automaton::MaskPriorityWithAssoziativity));

		return TransitionItem( prodindex_, prodpos_, follow_, priority_);
	}
};

class TransitionState
{
public:
	TransitionState()
		:m_packedElements(){}
	TransitionState( const TransitionState& o)
		:m_packedElements(o.m_packedElements){}
	TransitionState( TransitionState&& o)
		:m_packedElements(std::move(o.m_packedElements)){}
	TransitionState& operator=( const TransitionState& o)
		{m_packedElements=o.m_packedElements; return *this;}
	TransitionState& operator=( TransitionState&& o)
		{m_packedElements=std::move(o.m_packedElements); return *this;}
	TransitionState( const std::initializer_list<TransitionItem>& itemlist)
		{for (const auto& item : itemlist) m_packedElements.insert( item.packed());}

	bool insert( const TransitionItem& itm)
	{
		return m_packedElements.insert( itm.packed()).second;
	}
	void join( const TransitionState& o)
	{
		m_packedElements.insert( o.m_packedElements.begin(), o.m_packedElements.end());
	}

	bool empty() const noexcept
	{
		return m_packedElements.empty();
	}

	std::vector<TransitionItem> unpack() const noexcept
	{
		std::vector<TransitionItem> rt;
		for (auto packedElement : m_packedElements)
		{
			rt.push_back( TransitionItem::unpack( packedElement));
		}
		return rt;
	}

	bool operator < (const TransitionState& o) const noexcept
	{
		if (m_packedElements.size() == o.m_packedElements.size())
		{
			auto ti = m_packedElements.begin(), te = m_packedElements.end();
			auto oi = o.m_packedElements.begin(), oe = o.m_packedElements.end();
			for (; ti != te && oi != oe; ++oi,++ti)
			{
				if (*ti != *oi) return *ti < *oi;
			}
			return false;
		}
		else
		{
			return m_packedElements.size() < o.m_packedElements.size();
		}
	}

private:
	std::set<int> m_packedElements;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

