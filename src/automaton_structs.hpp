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
#include <map>
#include <unordered_map>
#include <vector>
#include <functional>
#include <algorithm>
#include <iterator>

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
	ProductionNode() noexcept
		:m_type(Unresolved),m_index(0){}
	ProductionNode( Type type_, int index_) noexcept
		:m_type(type_),m_index(index_){}
	ProductionNode( const ProductionNode& o) noexcept
		:m_type(o.m_type),m_index(o.m_index){}
	ProductionNode& operator=( const ProductionNode& o) noexcept
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
	ProductionNodeDef( const std::string_view& name_, bool symbol_) noexcept
		:ProductionNode(),m_name(name_),m_symbol(symbol_){}
	ProductionNodeDef( const ProductionNodeDef& o) noexcept
		:ProductionNode(o),m_name(o.m_name),m_symbol(o.m_symbol){}
	ProductionNodeDef& operator=( const ProductionNodeDef& o) noexcept
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

	Priority( short value_=0, Assoziativity assoziativity_ = Assoziativity::Undefined) noexcept
		:value(value_),assoziativity(assoziativity_){}
	Priority( const Priority& o) noexcept
		:value(o.value),assoziativity(o.assoziativity){}
	Priority& operator=( const Priority& o) noexcept
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

typedef Automaton::Action::ScopeFlag ScopeFlag;

struct ProductionDef
{
	ProductionNodeDef left;
	ProductionNodeDefList right;
	Priority priority;
	int callidx;
	ScopeFlag scope;

	ProductionDef( const std::string_view& leftname_, const ProductionNodeDefList& right_, const Priority priority_, 
			int callidx_=0, const ScopeFlag scope_=Automaton::Action::NoScope)
		:left(leftname_,true/*symbol*/),right(right_),priority(priority_),callidx(callidx_),scope(scope_){}
	ProductionDef( const ProductionDef& o)
		:left(o.left),right(o.right),priority(o.priority),callidx(o.callidx),scope(o.scope){}
	ProductionDef& operator=( const ProductionDef& o)
		{left=o.left; right=o.right; priority=o.priority; callidx=o.callidx; scope=o.scope; return *this;}
	ProductionDef( ProductionDef&& o) noexcept
		:left(std::move(o.left)),right(std::move(o.right)),priority(o.priority),callidx(o.callidx),scope(o.scope){}
	ProductionDef& operator=( ProductionDef&& o) noexcept
		{left=std::move(o.left); right=std::move(o.right); priority=o.priority; callidx=o.callidx; scope=o.scope; return *this;}

	std::string prodstring() const
	{
		return head_tostring() + " = " + element_range_tostring( right.begin(), right.end());
	}

	std::string prodstring( int pos) const
	{
		std::string rt = head_tostring() + " = ";
		std::string prev = element_range_tostring( right.begin(), right.begin() + pos);
		if (prev.empty())
		{
			rt.append( ".");
		}
		else
		{
			rt.append( prev);
			rt.append( " .");
		}
		std::string post = element_range_tostring( right.begin() + pos, right.end());
		if (!post.empty())
		{
			rt.push_back( ' ');
			rt.append( post);
		}
		return rt;
	}

	std::string tostring() const
	{
		return prodstring() + callstring();
	}

	std::string tostring( int pos) const
	{
		return prodstring( pos) + callstring();
	}

	std::string prefix_tostring( int pos) const
	{
		return head_tostring() + " = "
			+ element_range_tostring( right.begin(), right.begin() + pos) + " ...";
	}

private:
	const char* scopeFlagName() const noexcept
	{
		return Automaton::Action::scopeFlagName(scope);
	}

	std::string callstring() const
	{
		std::string rt;
		if (callidx || scope)
		{
			char callstrbuf[ 64];
			callstrbuf[ 0] = 0;
			if (scope && callidx)
			{
				std::snprintf( callstrbuf, sizeof(callstrbuf), " (%s:%d)", scopeFlagName(), callidx);
			}
			else if (scope)
			{
				std::snprintf( callstrbuf, sizeof(callstrbuf), " (%s)", scopeFlagName());
			}
			else
			{
				std::snprintf( callstrbuf, sizeof(callstrbuf), " (%d)", callidx);
			}
			rt.append( callstrbuf);
		}
		return rt;
	}

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

	TransitionItem( int prodindex_, int prodpos_, int follow_) noexcept
		:prodindex(prodindex_),prodpos(prodpos_),follow(follow_){}
	TransitionItem( const TransitionItem& o) noexcept
		:prodindex(o.prodindex),prodpos(o.prodpos),follow(o.follow){}

	bool operator < (const TransitionItem& o) const noexcept
	{
		return prodindex == o.prodindex
			? prodpos == o.prodpos
				? follow < o.follow
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
		return prodindex == o.prodindex && prodpos == o.prodpos && follow == o.follow;
	}

	int packed() const noexcept
	{
		static_assert (Automaton::ShiftNofProductions
				+ Automaton::ShiftProductionLength
				+ Automaton::ShiftTerminal <= 31, "sizeof packed transition item"); 
		int rt = 0;
		rt |= prodindex << (Automaton::ShiftProductionLength + Automaton::ShiftTerminal);
		rt |= prodpos << (Automaton::ShiftTerminal);
		rt |= follow;
		return rt;
	}

	static TransitionItem unpack( int pkg) noexcept
	{
		int prodindex_ = pkg >> (Automaton::ShiftProductionLength + Automaton::ShiftTerminal);
		int prodpos_ = (pkg >> Automaton::ShiftTerminal) & Automaton::MaskProductionLength;
		int follow_ = pkg & Automaton::MaskTerminal;

		return TransitionItem( prodindex_, prodpos_, follow_);
	}
};


template <typename TYPE>
class FlatSet :public std::vector<TYPE>
{
public:
	FlatSet() noexcept
		:std::vector<TYPE>(){}
	FlatSet( const FlatSet<TYPE>& o)
		{std::vector<TYPE>::reserve( reserveSize( o.size())); std::vector<TYPE>::insert( std::vector<TYPE>::end(), o.begin(), o.end());}
	FlatSet( FlatSet<TYPE>&& o) noexcept
		:std::vector<TYPE>(std::move(o)){}
	FlatSet& operator=( const FlatSet<TYPE>& o)
		{std::vector<TYPE>::reserve( reserveSize( o.size())); 
		 std::vector<TYPE>::insert( std::vector<TYPE>::end(), o.begin(), o.end()); return *this;}
	FlatSet& operator=( FlatSet<TYPE>&& o) noexcept
		{std::vector<TYPE>::operator=(std::move(o)); return *this;}
	FlatSet( const std::initializer_list<TYPE>& itemlist)
		{std::vector<TYPE>::reserve( reserveSize( itemlist.size()));
		 for (const auto& item : itemlist) insertInOrder( item);}
	~FlatSet(){}

	template <typename ITERATOR>
	bool insert( ITERATOR start, ITERATOR end)
	{
		bool rt = false;
		std::vector<TYPE>::reserve( reserveSize( std::distance<ITERATOR>( start, end)));
		for (auto itr = start; itr != end; ++itr) rt |= insertInOrder( *itr);
		return rt;
	}

	bool insert( TYPE elem)
	{
		return insertInOrder( elem);
	}

	void clear()
	{
		std::vector<TYPE>::clear();
	}

	bool operator == (const FlatSet<TYPE>& o) const noexcept	{return compare( o) == 0;}
	bool operator != (const FlatSet<TYPE>& o) const noexcept	{return compare( o) != 0;}
	bool operator < (const FlatSet<TYPE>& o) const noexcept		{return compare( o) < 0;}

	bool contains( TYPE elem) const noexcept
	{
		typename std::vector<TYPE>::const_iterator pi = std::lower_bound( std::vector<TYPE>::begin(), std::vector<TYPE>::end(), elem);
		return (pi != std::vector<TYPE>::end() && *pi == elem);
	}

private:
	static std::size_t reserveSize( std::size_t minCapacity)
	{
		enum {MinSize=128};
		std::size_t mm = MinSize;
		for (; mm < minCapacity && mm >= (std::size_t)MinSize; mm*=2){}
		if (mm < minCapacity) throw std::bad_alloc();
		return mm;
	}
	void increaseCapacity( std::size_t nofElements)
	{
		if (std::vector<TYPE>::capacity() == std::vector<TYPE>::size())
		{
			std::vector<TYPE>::reserve( reserveSize( std::vector<TYPE>::size()+nofElements));
		}
	}
	bool insertInOrder( TYPE elem)
	{
		increaseCapacity( 1);
		typename std::vector<TYPE>::iterator pi = std::lower_bound( std::vector<TYPE>::begin(), std::vector<TYPE>::end(), elem);
		if (pi == std::vector<TYPE>::end())
		{
			std::vector<TYPE>::push_back( elem);
			return true;
		}
		else if (*pi > elem)
		{
			std::vector<TYPE>::insert( pi, elem);
			return true;
		}
		else
		{
			return false;
		}
	}

	int compare(const FlatSet<TYPE>& o) const noexcept
	{
		if (std::vector<TYPE>::size() == o.size())
		{
			auto ti = std::vector<TYPE>::begin(), te = std::vector<TYPE>::end();
			auto oi = o.begin(), oe = o.end();
			for (; ti != te && oi != oe; ++oi,++ti)
			{
				if (*ti != *oi) return *ti < *oi ? -1 : +1;
			}
			return 0;
		}
		else
		{
			return std::vector<TYPE>::size() < o.size() ? -1:+1;
		}
	}
};

struct IntHash
{
	// \brief Robert Jenkins' 32 bit integer hash function
	static unsigned int jenkins32bitIntegerHash( unsigned int aa) noexcept
	{
		aa = (aa+0x7ed55d16) + (aa<<12);
		aa = (aa^0xc761c23c) ^ (aa>>19);
		aa = (aa+0x165667b1) + (aa<<5);
		aa = (aa+0xd3a2646c) ^ (aa<<9);
		aa = (aa+0xfd7046c5) + (aa<<3);
		aa = (aa^0xb55a4f09) ^ (aa>>16);
		return aa;
	}

	static std::size_t hashIntVector( const std::vector<int>& ar) noexcept
	{
		constexpr std::size_t kc = 2654435761/*Knuth's multiplicative hashing scheme*/;
		std::size_t rt = kc * ar.size();
		for (auto elem : ar)
		{
			rt += (rt << 13) + IntHash::jenkins32bitIntegerHash( ((rt >> 17) ^ rt) + elem);
		}
		return rt;
	}
};

class TransitionState
{
public:
	TransitionState()
		:m_packedElements(){}
	TransitionState( const TransitionState& o)
		:m_packedElements( o.m_packedElements){} 
	TransitionState( TransitionState&& o) noexcept
		:m_packedElements(std::move(o.m_packedElements)){}
	TransitionState& operator=( const TransitionState& o)
		{m_packedElements=o.m_packedElements; return *this;} 
	TransitionState& operator=( TransitionState&& o) noexcept
		{m_packedElements=std::move(o.m_packedElements); return *this;}
	TransitionState( const std::initializer_list<TransitionItem>& itemlist)
		{for (const auto& item : itemlist) m_packedElements.insert( item.packed());}
	~TransitionState(){}

	bool insert( const TransitionItem& itm)
	{
		return m_packedElements.insert( itm.packed());
	}
	void join( const TransitionState& o)
	{
		for (auto elem : o.m_packedElements) m_packedElements.insert( elem);
	}

	std::size_t size() const noexcept
	{
		return m_packedElements.size();
	}
	bool empty() const noexcept
	{
		return m_packedElements.empty();
	}

	std::vector<TransitionItem> unpack() const noexcept
	{
		std::vector<TransitionItem> rt;
		rt.reserve( m_packedElements.size());
		for (auto packedElement : m_packedElements)
		{
			rt.push_back( TransitionItem::unpack( packedElement));
		}
		return rt;
	}

	bool operator == (const TransitionState& o) const noexcept
	{
		return (m_packedElements == o.m_packedElements);
	}
	bool operator != (const TransitionState& o) const noexcept
	{
		return (m_packedElements != o.m_packedElements);
	}
	bool operator < (const TransitionState& o) const noexcept
	{
		return (m_packedElements < o.m_packedElements);
	}

	const FlatSet<int>& packedElements() const noexcept
	{
		return m_packedElements;
	}
	FlatSet<int>& packedElements() noexcept
	{
		return m_packedElements;
	}

	void clear()
	{
		m_packedElements.clear();
	}

	std::size_t hash() const noexcept
	{
		return IntHash::hashIntVector( m_packedElements);
	}

private:
	FlatSet<int> m_packedElements;
};

class ProductionDefList
{
public:
	typedef std::unordered_multimap<int,std::size_t> LeftIndexMap;

	ProductionDefList( const std::vector<ProductionDef>& ar_)
		:m_ar(ar_){init_leftindexmap();}

	typedef std::vector<ProductionDef>::const_iterator const_iterator;

	const_iterator begin() const noexcept		{return m_ar.begin();}
	const_iterator end() const noexcept		{return m_ar.end();}

	class head_iterator
	{
	public:
		head_iterator( const_iterator start_, LeftIndexMap::const_iterator range_itr_)
			:m_start( start_),m_range_itr(range_itr_){}
		head_iterator( const head_iterator& o)
			:m_start( o.m_start),m_range_itr(o.m_range_itr){}

		bool operator == (const head_iterator& o) const noexcept
		{
			return m_range_itr == o.m_range_itr;
		}
		bool operator != (const head_iterator& o) const noexcept
		{
			return m_range_itr != o.m_range_itr;
		}

		head_iterator& operator++() noexcept
		{
			++m_range_itr;
			return *this;
		}
		head_iterator operator++(int) noexcept
		{
			head_iterator rt( *this);
			++m_range_itr;
			return rt;
		}
		const ProductionDef& operator*() const noexcept
		{
			return *(m_start + m_range_itr->second);
		}
		const ProductionDef* operator->() const noexcept
		{
			return &*(m_start + m_range_itr->second);
		}
		std::size_t index() const noexcept
		{
			return m_range_itr->second;
		}

	private:
		const_iterator m_start;
		LeftIndexMap::const_iterator m_range_itr;
	};

	std::pair<head_iterator,head_iterator> equal_range( int leftindex) const noexcept
	{
		auto range = m_leftindexmap.equal_range( leftindex);
		return std::pair<head_iterator,head_iterator>( {m_ar.begin(), range.first}, {m_ar.begin(), range.second});
	}

	const ProductionDef& operator[]( std::size_t idx) const noexcept
	{
		return m_ar[ idx];
	}

	std::size_t size() const noexcept
	{
		return m_ar.size();
	}

private:
	void init_leftindexmap()
	{
		for (std::size_t pidx=0; pidx<m_ar.size(); ++pidx)
		{
			m_leftindexmap.insert( {m_ar[pidx].left.index(), pidx});
		}
	}

private:
	std::vector<ProductionDef> m_ar;
 	LeftIndexMap m_leftindexmap;
};

}//namespace

namespace std
{
	template<> struct hash<mewa::TransitionState>
	{
	public:
		hash<mewa::TransitionState>(){}
		std::size_t operator()( mewa::TransitionState const& st) const noexcept
		{
			return st.hash();
		}
	};

	template<> struct hash<mewa::FlatSet<int> >
	{
	public:
		hash<mewa::FlatSet<int> >(){}
		std::size_t operator()( mewa::FlatSet<int> const& fs) const noexcept
		{
			return mewa::IntHash::hashIntVector( fs);
		}
	};
}

namespace mewa {
	
class IntSetHandleMap
{
public:
	explicit IntSetHandleMap( int startidx_=1)
		:m_map(),m_inv(),m_startidx(startidx_){m_inv.reserve(1024);}
	IntSetHandleMap( const IntSetHandleMap& o)
		:m_map(o.m_map),m_inv(o.m_inv),m_startidx(o.m_startidx){}
	IntSetHandleMap& operator=( const IntSetHandleMap& o)
		{m_map=o.m_map; m_inv=o.m_inv; m_startidx=o.m_startidx; return *this;}
	IntSetHandleMap( IntSetHandleMap&& o) noexcept
		:m_map(std::move(o.m_map)),m_inv(std::move(o.m_inv)),m_startidx(o.m_startidx){}
	IntSetHandleMap& operator=( IntSetHandleMap&& o) noexcept
		{m_map=std::move(o.m_map); m_inv=std::move(o.m_inv); m_startidx=o.m_startidx; return *this;}

	int get( const FlatSet<int>& elem)
	{
		int rt;
		auto ins = m_map.insert( {elem, m_inv.size()+m_startidx});
		if (ins.second/*insert took place*/)
		{
			rt = m_inv.size() + m_startidx;
			m_inv.push_back( elem);
		}
		else
		{
			rt = ins.first->second;
		}
		return rt;
	}

	const FlatSet<int>& content( int handle) const noexcept
	{
		return m_inv[ handle-m_startidx];
	}

	int join( int handle1, int handle2)
	{
		FlatSet<int> newelem( content( handle1));
		const FlatSet<int>& add = content( handle2);
		newelem.insert( add.begin(), add.end());
		return get( newelem);
	}

	std::size_t size() const noexcept
	{
		return m_inv.size()+m_startidx;
	}

private:
	typedef FlatSet<int> Sequence;

private:
	std::map<Sequence,int> m_map;
	std::vector<Sequence> m_inv;
	int m_startidx;
};

typedef std::unordered_map<FlatSet<int>,int> TransitionItemGotoMap;

} //namespace

#else
#error Building mewa requires C++17
#endif
#endif

