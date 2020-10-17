/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Structure describing the scope of a definition as pair of numbers and some map templates using scope for partial visibility depending of a concept of scope.
/// \file "scope.hpp"
#ifndef _MEWA_SCOPE_HPP_INCLUDED
#define _MEWA_SCOPE_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "error.hpp"
#include "strings.hpp"
#include "memory_resource.hpp"
#include "tree.hpp"
#include <utility>
#include <limits>
#include <cstdio>
#include <cstddef>
#include <string>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

namespace mewa {

struct Scope
{
public:
	typedef int Step;

public:
	Scope( Step start_, Step end_) noexcept
		:m_start(start_ >= 0 ? start_ : 0),m_end(end_ >= 0 ? end_ : std::numeric_limits<Step>::max()){}
	Scope( Scope const& o) noexcept
		:m_start(o.m_start),m_end(o.m_end){}
	Scope& operator=( Scope const& o) noexcept
		{m_start=o.m_start; m_end=o.m_end; return *this;}

	bool contains( Scope const& o) const noexcept
	{
		return o.m_start >= m_start && o.m_end <= m_end;
	}

	bool contains( Step step) const noexcept
	{
		return step >= m_start && step < m_end;
	}

	bool operator == ( Scope const& o) const noexcept
	{
		return o.m_start == m_start && o.m_end == m_end;
	}
	bool operator != ( Scope const& o) const noexcept
	{
		return o.m_start != m_start || o.m_end != m_end;
	}

	std::string tostring() const
	{
		char buf[ 128];
		if (m_end == std::numeric_limits<Step>::max())
		{
			std::snprintf( buf, sizeof(buf), "[%d,INF]", m_start);
		}
		else
		{
			std::snprintf( buf, sizeof(buf), "[%d,%d]", m_start, m_end);
		}
		return std::string( buf);
	}

	Step start() const noexcept	{return m_start;}
	Step end() const noexcept	{return m_end;}

public:
	Step m_start;
	Step m_end;
};

struct ScopeCompare
{
	ScopeCompare(){}
	bool operator()( Scope const& aa, Scope const& bb) const
	{
		return aa.end() == bb.end() ? aa.start() < bb.start() : aa.end() < bb.end();
	}
};

template <typename VALTYPE>
struct ScopedUpValue
{
	Scope scope;
	VALTYPE value;
	int prev;

	ScopedUpValue( const Scope scope_, const VALTYPE value_, int prev_) noexcept
		:scope(scope_),value(value_),prev(prev_){}
	ScopedUpValue( ScopedUpValue const& o) noexcept
		:scope(o.scope),value(o.value),prev(o.prev){}
};


template <typename VALTYPE>
class ScopedUpValueInvTree
{
public:
	typedef ScopedUpValue<VALTYPE> UpValue;

public:
	ScopedUpValueInvTree( const VALTYPE nullval_, std::size_t initsize)
		:m_ar(),m_nullval(nullval_) {if (initsize) m_ar.reserve(initsize);}
	ScopedUpValueInvTree( ScopedUpValueInvTree const& o) = default;
	ScopedUpValueInvTree& operator=( ScopedUpValueInvTree const& o) = default;
	ScopedUpValueInvTree( ScopedUpValueInvTree&& o) noexcept = default;
	ScopedUpValueInvTree& operator=( ScopedUpValueInvTree&& o) noexcept = default;

	VALTYPE getInnerValue( int index, const Scope::Step step) const noexcept
	{
		int ai = index;
		while (ai >= 0)
		{
			if (m_ar[ ai].scope.start() <= step) return m_ar[ ai].value;
			ai = m_ar[ ai].prev;
		}
		return m_nullval;
	}

	class const_iterator
	{
	public:
		explicit const_iterator( std::vector<UpValue> const& ar_, int index_=-1) noexcept
			:m_ar(ar_.data()),m_index(index_){}

		const_iterator& operator++() noexcept				{m_index = m_ar[ m_index].prev; return *this;}
		const_iterator operator++(int) noexcept				{const_iterator rt(m_ar,m_index); m_index = m_ar[ m_index].prev; return rt;}

		bool operator==( const const_iterator& o) const noexcept	{return m_ar==o.m_ar && m_index == o.m_index;}
		bool operator!=( const const_iterator& o) const noexcept	{return m_ar!=o.m_ar || m_index != o.m_index;}

		VALTYPE const& operator*() const noexcept			{return m_ar[ m_index].value;}
		VALTYPE const* operator->() const noexcept			{return &m_ar[ m_index].value;}

		const Scope& scope() const noexcept				{return m_ar[ m_index].scope;}
		VALTYPE const& value() const noexcept				{return m_ar[ m_index].value;}

	private:
		UpValue const* m_ar;
		int m_index;
	};

	const_iterator start( int index) const noexcept
	{
		return index >= 0 ? const_iterator( m_ar, index) : const_iterator( m_ar);
	}

	const_iterator first( int index, const Scope::Step step) const noexcept
	{
		int ai = index;
		while (ai >= 0)
		{
			if (m_ar[ ai].scope.start() <= step) return const_iterator( m_ar, ai);
			ai = m_ar[ ai].prev;
		}
		return const_iterator( m_ar);
	}
	const_iterator end() const noexcept
	{
		return const_iterator( m_ar);
	}

	int nextIndex() const noexcept
	{
		return m_ar.size();
	}

	int pushUpValue( const Scope scope, const VALTYPE value, int uplink)
	{
		int rt = m_ar.size();
		m_ar.push_back( UpValue( scope, value, uplink));
		return rt;
	}

	VALTYPE findUpValue( int index, const Scope scope) const noexcept
	{
		VALTYPE rt = m_nullval;
		int ai = index;
		while (ai >= 0)
		{
			const UpValue& uv = m_ar[ ai];
			if (uv.scope.end() != scope.end()) return rt;
			if (uv.scope.start() == scope.start())
			{
				rt = uv.value;
				break;
			}
			if (uv.scope.start() < scope.start())
			{
				break;
			}
			ai = uv.prev;
		}
		return rt;
	}

	int insertUpValue( int& index, const Scope scope, const VALTYPE value, int uplink)
	{
		int rt = -1;
		int pred_ai = -1;
		int ai = index;
		while (ai >= 0)
		{
			UpValue& uv = m_ar[ ai];

			if (uv.scope == scope)
			{
				// ... already known, no insert, but overwrite
				uv.value = value;
				break;
			}
			else if (scope.contains( uv.scope))
			{
				//... The covering scope gets further up the list  {(A)..{(B)..{(C)..}}} ~ (C)-prev>(B)-prev->(A)
				pred_ai = ai;
				ai = uv.prev;
			}
			else if (!uv.scope.contains( scope))
			{
				throw Error( Error::ScopeHierarchyError, uv.scope.tostring() + " <-> " + scope.tostring());
			}
			else if (pred_ai == -1)
			{
				// ... insert in front
				rt = index = pushUpValue( scope, value, ai);
				break;
			}
			else
			{
				// ... insert inbetween pred_ai and ai
				rt = m_ar[ pred_ai].prev = pushUpValue( scope, value, ai);
				break;
			}
		}
		if (ai == -1)
		{
			//... insert at end of list
			rt = m_ar[ pred_ai].prev = pushUpValue( scope, value, uplink);
		}
		return rt;
	}

	int rightNeighborUpLink( int index, const Scope scope) const noexcept
	{
		int ai = index;

		while (ai >= 0)
		{
			const UpValue& uv = m_ar[ ai];

			if (uv.scope.contains( scope))
			{
				return ai;
			}
			else
			{
				ai = uv.prev;
			}
		}
		return ai;
	}

	void linkLeftNeighbours( int index, const Scope scope, int newindex)
	{
		int pred_ai = -1;
		int ai = index;

		while (ai >= 0)
		{
			UpValue& uv = m_ar[ ai];

			if (scope == uv.scope)
			{
				pred_ai = -1;
				break;
			}
			else if (scope.contains( uv.scope))
			{
				pred_ai = ai;
				ai = uv.prev;
			}
			else
			{
				break;
			}
		}
		if (pred_ai != -1)
		{
			m_ar[ pred_ai].prev = newindex;
		}
	}

	const VALTYPE& nullval() const noexcept
	{
		return m_nullval;
	}

private:
	std::vector<UpValue> m_ar;		//> upvalue lists list
	VALTYPE m_nullval;			//> value returned by get if not found
};

/// \brief Node of a tree displaying the scope hierarchy
template <typename VALTYPE>
struct ScopeHierarchyTreeNode
{
	Scope scope;
	VALTYPE value;

	ScopeHierarchyTreeNode()
		:scope( 0, std::numeric_limits<Scope::Step>::max()){}
	ScopeHierarchyTreeNode( const Scope scope_, const VALTYPE& value_)
		:scope(scope_),value(value_){}
	ScopeHierarchyTreeNode( const ScopeHierarchyTreeNode& o)
		:scope(o.scope),value(o.value){}
};

/// \class ScopeHierarchyTreeBuilder
/// \brief Template for a builder of a tree displaying the scope hierarchy from its inverse representation (ScopedUpValueInvTree).
/// \param VALTYPE type of an item attached to a tree node 
template <typename VALTYPE>
class ScopeHierarchyTreeBuilder
{
public:
	typedef ScopeHierarchyTreeNode<VALTYPE> Node;
	typedef ScopedUpValueInvTree<VALTYPE> InvTree;

	explicit ScopeHierarchyTreeBuilder( const VALTYPE& nullval_)
		:m_tree(Node( Scope( 0, std::numeric_limits<Scope::Step>::max()), nullval_))
		,m_scope2nodeidxMap()
	{
		m_scope2nodeidxMap.insert( {Scope( 0, std::numeric_limits<Scope::Step>::max()),1/*root node index*/} );
	}

	void insert( typename InvTree::const_iterator ni, const typename InvTree::const_iterator ne)
	{
		std::vector<typename InvTree::const_iterator> stk;
		std::size_t ndidx = 1;

		for (; ni != ne; ++ni)
		{
			auto sni = m_scope2nodeidxMap.find( ni.scope());
			if (sni == m_scope2nodeidxMap.end())
			{
				stk.push_back( ni);
			}
			else
			{
				ndidx = sni->second;
				if (m_tree[ ndidx].item().value != *ni)
				{
					throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
				}
				break;
			}
		}
		for (; !stk.empty(); stk.pop_back())
		{
			typename InvTree::const_iterator stk_ni = stk.back();
			ndidx = m_tree.addChild( ndidx, Node( stk_ni.scope(), *stk_ni));
			m_scope2nodeidxMap.insert( {stk_ni.scope(), ndidx} );
		}
	}

	const Tree<Node>& tree() const
	{
		return m_tree;
	}
	Tree<Node>&& tree()
	{
		return std::move( m_tree);
	}

private:
	Tree<Node> m_tree;
	std::map<Scope,std::size_t,ScopeCompare> m_scope2nodeidxMap;
};


template <typename VALTYPE>
class ScopedInstance
{
public:
	ScopedInstance( std::pmr::memory_resource* memrsc, const VALTYPE nullval_, std::size_t initsize)
		:m_map( memrsc),m_invtree(nullval_,initsize){}
	ScopedInstance( const ScopedInstance& o) = default;
	ScopedInstance& operator=( const ScopedInstance& o) = default;
	ScopedInstance( ScopedInstance&& o) noexcept = default;
	ScopedInstance& operator=( ScopedInstance&& o) noexcept = default;

	void set( const Scope scope, const VALTYPE value)
	{
		auto rightnode = m_map.upper_bound( scope.end());
		int uplink = (rightnode == m_map.end()) ? -1 : m_invtree.rightNeighborUpLink( rightnode->second, scope);
		int newindex = -1;

		auto ins = m_map.insert( {scope.end(), m_invtree.nextIndex()});
		if (ins.second/*insert took place*/)
		{
			newindex = m_invtree.pushUpValue( scope, value, uplink);
		}
		else
		{
			newindex = m_invtree.insertUpValue( ins.first->second, scope, value, uplink);
		}
		if (newindex >= 0)
		{
			auto leftnode = m_map.upper_bound( scope.start());
			if (leftnode == m_map.end()) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			for (; leftnode->first != scope.end(); ++leftnode)
			{
				m_invtree.linkLeftNeighbours( leftnode->second, scope, newindex);
			}
		}
	}

	VALTYPE get( const Scope::Step step) const noexcept
	{
		VALTYPE rt = m_invtree.nullval();
		auto ref = m_map.upper_bound( step);
		if (ref != m_map.end())
		{
			rt = m_invtree.getInnerValue( ref->second, step);
		}
		return rt;
	}

	typedef ScopeHierarchyTreeNode<VALTYPE> TreeNode;

	Tree<TreeNode> getTree() const
	{
		ScopeHierarchyTreeBuilder<VALTYPE> treeBuilder( m_invtree.nullval());
		auto mi = m_map.begin();
		for (; mi != m_map.end(); ++mi)
		{
			treeBuilder.insert( m_invtree.start( mi->second), m_invtree.end());
		}
		return treeBuilder.tree();
	}

	const ScopedUpValueInvTree<VALTYPE>& invtree() const noexcept
	{
		return m_invtree;
	}

private:
	typedef ScopedUpValue<VALTYPE> UpValue;

private:
	std::pmr::map<Scope::Step,int> m_map;		//> map scope end -> index in m_ar upvalue list
	ScopedUpValueInvTree<VALTYPE> m_invtree;	//> upvalue inv tree
};


template <typename KEYTYPE>
class ScopedKey
{
public:
	ScopedKey( const KEYTYPE& key_, const Scope::Step end_)
		:m_key(key_),m_end(end_){}
	ScopedKey( const ScopedKey& o)
		:m_key(o.m_key),m_end(o.m_end){}
	ScopedKey& operator=( const ScopedKey& o)
		{m_key=o.m_key; m_end=o.m_end; return *this;}

	const KEYTYPE& key() const noexcept						{return m_key;}
	const Scope::Step end() const noexcept						{return m_end;}

	bool operator < (const ScopedKey& o) const noexcept
	{
		return m_key == o.m_key ? m_end < o.m_end : m_key < o.m_key;
	}

private:
	KEYTYPE m_key;
	Scope::Step m_end;
};

template <typename VALTYPE>
class ScopeListMap
{
public:
	typedef std::vector<VALTYPE> ValueList;
	typedef std::set<VALTYPE> ValueSet;
	typedef ScopeHierarchyTreeNode<ValueList> TreeNode;

	ScopeListMap() :m_map(),m_valueAr(),m_valueSets(){}

	void insert( const Scope scope, const VALTYPE& value)
	{
		auto ins = m_map.insert( {scope, m_valueAr.size()+1} );
		if (ins.second/*new*/)
		{
			m_valueAr.push_back( ValueList());
			m_valueSets.push_back( ValueSet());
		}
		if (m_valueSets[ ins.first->second-1].insert( value).second == true/*new*/)
		{
			m_valueAr[ ins.first->second-1].push_back( value);
		}
	}

	Tree<TreeNode> getTree() const
	{
		// [1] Build a scoped set from m_map and build a tree out of it:
		ScopedInstance<std::size_t> scopedInstance( std::pmr::get_default_resource(), 0/*nullval*/, 1<<16/*initsize*/);
		for (auto const& kv : m_map)
		{
			scopedInstance.set( kv.first, kv.second);
		}
		Tree< ScopeHierarchyTreeNode<std::size_t> > tree = scopedInstance.getTree();

		// [2] Copy the built tree replacing the references to the result VALTYPE lists with the real VALTYPE lists as node items:
		struct TranslateItem
		{
			ValueList const* ar;
			std::size_t arsize;

			TranslateItem( const std::vector<ValueList>& ar_) :ar(ar_.data()),arsize(ar_.size()){}

			ScopeHierarchyTreeNode<ValueList> operator()( const ScopeHierarchyTreeNode<std::size_t>& node) const
			{
				return ScopeHierarchyTreeNode<ValueList>( node.scope, ar[ node.value-1]);
			}
		};
		TranslateItem translateItem( m_valueAr);
		return tree.translate<TreeNode>( translateItem);
	}

private:
	std::map<Scope,std::size_t,ScopeCompare> m_map;		//< map with references to the VALTYPE lists associated with the scope
	std::vector<ValueList> m_valueAr;			//< collected list of values referenced by m_map (index starting from 1..)
	std::vector<ValueSet> m_valueSets;			//< sets for duplicate elimination in m_valueAr
};


template <typename KEYTYPE, typename VALTYPE>
class ScopedMap
{
public:
	typedef std::pmr::map<ScopedKey<KEYTYPE>, int> Map;

	ScopedMap( std::pmr::memory_resource* memrsc, const VALTYPE nullval_, std::size_t initsize)
		:m_map( memrsc),m_invtree(nullval_,initsize){}
	ScopedMap( const ScopedMap& o) = default;
	ScopedMap& operator=( const ScopedMap& o) = default;
	ScopedMap( ScopedMap&& o) noexcept = default;
	ScopedMap& operator=( ScopedMap&& o) noexcept = default;

	void set( const Scope scope, const KEYTYPE& key, const VALTYPE& value)
	{
		ScopedKey<KEYTYPE> sk( key, scope.end());
		auto rightnode = m_map.upper_bound( sk);
		int uplink = (rightnode == m_map.end() || rightnode->first.key() != key)
			? -1
			: m_invtree.rightNeighborUpLink( rightnode->second, scope);
		int newindex = -1;

		auto ins = m_map.insert( {sk, m_invtree.nextIndex()} );
		if (ins.second/*insert took place*/)
		{
			newindex = m_invtree.pushUpValue( scope, value, uplink);
		}
		else
		{
			newindex = m_invtree.insertUpValue( ins.first->second, scope, value, uplink);
		}
		if (newindex >= 0)
		{
			auto leftnode = m_map.upper_bound( {key,scope.start()} );
			if (leftnode == m_map.end() || leftnode->first.key() != key)
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			for (; leftnode->first.end() != scope.end(); ++leftnode)
			{
				m_invtree.linkLeftNeighbours( leftnode->second, scope, newindex);
			}
		}
	};

	VALTYPE getOrSet( const Scope scope, const KEYTYPE& key, const VALTYPE value)
	{
		VALTYPE rt = m_invtree.nullval();
		ScopedKey<KEYTYPE> sk( key, scope.end());
		auto rightnode = m_map.upper_bound( sk);
		int uplink = (rightnode == m_map.end() || rightnode->first.key() != key)
			? -1
			: m_invtree.rightNeighborUpLink( rightnode->second, scope);
		int newindex = -1;

		auto ins = m_map.insert( {sk, m_invtree.nextIndex()});
		if (ins.second/*insert took place*/)
		{
			newindex = m_invtree.pushUpValue( scope, value, uplink);
		}
		else
		{
			rt = m_invtree.findUpValue( ins.first->second, scope);
			if (rt != m_invtree.nullval()) return rt;

			newindex = m_invtree.insertUpValue( ins.first->second, scope, value, uplink);
		}
		if (newindex >= 0)
		{
			auto leftnode = m_map.upper_bound( {key,scope.start()});
			if (leftnode == m_map.end() || leftnode->first.key() != key)
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			for (; leftnode->first.end() != scope.end(); ++leftnode)
			{
				m_invtree.linkLeftNeighbours( leftnode->second, scope, newindex);
			}
		}
		return rt;
	}

	VALTYPE get( const Scope::Step step, const KEYTYPE& key) const noexcept
	{
		auto rt = m_invtree.nullval();
		ScopedKey<KEYTYPE> sk( key, step);
		auto it = m_map.upper_bound( sk);
		if (it != m_map.end() && it->first.key() == key)
		{
			rt = m_invtree.getInnerValue( it->second, step);
		}
		return rt;
	}

	typedef typename ScopedUpValueInvTree<VALTYPE>::const_iterator invtree_iterator;

	std::pair<invtree_iterator,invtree_iterator> match_range( const Scope::Step step, const KEYTYPE& key) const noexcept
	{
		std::pair<invtree_iterator,invtree_iterator> rt( m_invtree.end(), m_invtree.end());
		ScopedKey<KEYTYPE> sk( key, step);
		auto it = m_map.upper_bound( sk);
		if (it != m_map.end() && it->first.key() == key)
		{
			rt.first = m_invtree.first( it->second, step);
		}
		return rt;
	}

	typedef std::pair<KEYTYPE,VALTYPE> KeyValue;
	typedef std::vector<KeyValue> TreeItem;
	typedef ScopeHierarchyTreeNode<TreeItem> TreeNode;

	Tree<TreeNode> getTree() const
	{
		ScopeListMap<KeyValue> scopeKeyValueListMap;

		auto mi = m_map.begin();
		for (; mi != m_map.end(); ++mi)
		{
			typename ScopedUpValueInvTree<VALTYPE>::const_iterator ii = m_invtree.start( mi->second), ie = m_invtree.end();
			for (; ii != ie; ++ii)
			{
				scopeKeyValueListMap.insert( ii.scope(), {mi->first.key(), ii.value()} );
			}
		}
		return scopeKeyValueListMap.getTree();
	}

	const ScopedUpValueInvTree<VALTYPE>& invtree() const noexcept
	{
		return m_invtree;
	}

	typedef typename Map::const_iterator const_iterator;
	const_iterator begin() const noexcept
	{
		return m_map.begin();
	}
	const_iterator end() const noexcept
	{
		return m_map.end();
	}

private:
	Map m_map;
	ScopedUpValueInvTree<VALTYPE> m_invtree;
};


class TagMask
{
public:
	enum {MinTag=1,MaxTag=32};
	typedef std::uint32_t BitSet;

public:
	TagMask( const TagMask& o) :m_mask(o.m_mask){}
	TagMask() :m_mask(0){}

	explicit constexpr TagMask( BitSet mask_) :m_mask(mask_){}

	BitSet mask() const noexcept
	{
		return m_mask;
	}
	TagMask& operator |= (int tag_)
	{
		add( tag_);
		return *this;
	}
	TagMask operator | (int tag_) const
	{
		TagMask rt;
		rt.add( tag_);
		return rt;
	}
	bool matches( std::uint8_t tagval) const noexcept
	{
		return (m_mask & (1U << tagval)) != 0;
	}
	std::string tostring() const
	{
		std::string rt;
		BitSet sl = 1;
		for (int ii=MinTag; ii<=MaxTag; ++ii,sl<<=1)
		{
			if ((m_mask & sl) != 0)
			{
				char buf[ 32];
				std::snprintf( buf, sizeof(buf), rt.empty() ? "%d":",%d", ii);
				rt.append( buf);
			}
		}
		return rt;
	}
	static constexpr TagMask matchAll() noexcept
	{
		return TagMask( 0xffFFffFFU);
	}

private:
	void add( int tag_)
	{
		if (tag_ > MaxTag || tag_ < MinTag) throw Error( Error::BadRelationTag, string_format("%d",tag_));
		m_mask |= (1U << (tag_-1));
	}

private:
	BitSet m_mask;
};


template <typename RELNODETYPE, typename VALTYPE>
class ScopedRelationMap
{
public:
	class ResultElement
	{
	public:
		ResultElement( const RELNODETYPE right_, const VALTYPE value_, float weight_)
			:m_right(right_),m_value(value_),m_weight(weight_){}
		ResultElement( const ResultElement& o)
			:m_right(o.m_right),m_value(o.m_value),m_weight(o.m_weight){}

		RELNODETYPE right() const noexcept	{return m_right;}
		VALTYPE value() const noexcept		{return m_value;}
		float weight() const noexcept		{return m_weight;}

	private:
		RELNODETYPE m_right;
		VALTYPE m_value;
		float m_weight;
	};

	ScopedRelationMap( std::pmr::memory_resource* memrsc, std::size_t initsize)
		:m_map( memrsc, -1/*nullval*/, initsize),m_list() {m_list.reserve(initsize);}
	ScopedRelationMap( const ScopedRelationMap& o) = default;
	ScopedRelationMap& operator=( const ScopedRelationMap& o) = default;
	ScopedRelationMap( ScopedRelationMap&& o) noexcept = default;
	ScopedRelationMap& operator=( ScopedRelationMap&& o) noexcept = default;

 	void set( const Scope scope, const RELNODETYPE& left, const RELNODETYPE& right, const VALTYPE& value, int tag, float weight)
	{
		if (tag > TagMask::MaxTag || tag < TagMask::MinTag) throw Error( Error::BadRelationTag, string_format("%d",tag));
		if (weight <= std::numeric_limits<float>::epsilon()*10) throw Error( Error::BadRelationWeight, string_format("%.4f",weight));

		std::uint8_t tagval = tag -1;
		int newlistindex = m_list.size();
		int li = m_map.getOrSet( scope, left, newlistindex);
		int prev_li = -1;
		while (li >= 0)
		{
			const ListElement& le = m_list[ li];
			if (le.related == right && tagval == le.tagval)
			{
				throw Error( Error::DuplicateDefinition);
			}
			prev_li = li;
			li = le.next;
		}
		m_list.push_back( ListElement( right, value, tagval, weight, -1/*next*/));
		if (prev_li >= 0)
		{
			m_list[ prev_li].next = newlistindex;
		}
	}

	std::pmr::vector<ResultElement> get( const Scope::Step step, const RELNODETYPE& key, const TagMask& selectTags, std::pmr::memory_resource* res_memrsc) const noexcept
	{
		int local_membuffer[ 2048];
		mewa::monotonic_buffer_resource local_memrsc( local_membuffer, sizeof local_membuffer);
		std::pmr::map<RELNODETYPE,int> candidatemap( &local_memrsc);

		std::pmr::vector<ResultElement> rt( res_memrsc);

		auto range = m_map.match_range( step, key);
		for (auto ri = range.first; ri != range.second; ++ri)
		{
			int li = *ri;
			while (li >= 0)
			{
				const ListElement& le = m_list[ li];
				if (selectTags.matches( le.tagval))
				{
					auto ins = candidatemap.insert( {le.related, rt.size()});
					if (ins.second /*insert took place*/)
					{
						rt.push_back( ResultElement( le.related/*right*/, le.value, le.weight));
					}
				}
				li = le.next;
			}
		}
		return rt;
	}

	struct TreeNodeElement
	{
		std::pair<RELNODETYPE,RELNODETYPE> relation;
		VALTYPE value;
		float weight;
		int tag;

		TreeNodeElement( const std::pair<RELNODETYPE,RELNODETYPE>& relation_, const VALTYPE& value_, std::uint8_t tagval_, float weight_)
			:relation(relation_),value(value_),weight(weight_),tag(tagval_+1){}
		TreeNodeElement( const TreeNodeElement& o)
			:relation(o.relation),value(o.value),weight(o.weight),tag(o.tag){}
	};
	typedef std::vector<TreeNodeElement> TreeItem;
	typedef ScopeHierarchyTreeNode<TreeItem> TreeNode;

	Tree<TreeNode> getTree() const
	{
		struct TranslateItem
		{
			ListElement const* list;
			std::size_t listsize;

			explicit TranslateItem( const std::vector<ListElement>& list_) :list(list_.data()),listsize(list_.size()){}

			ScopeHierarchyTreeNode<TreeItem> operator()( const ScopeHierarchyTreeNode< std::vector<std::pair<RELNODETYPE,int> > >& node) const
			{
				ScopeHierarchyTreeNode<TreeItem> rt( node.scope, {});
				for (auto const& kv : node.value)
				{
					int li = kv.second;
					while (li >= 0)
					{
						ListElement const& le = list[ li];
						rt.value.push_back( {{kv.first, le.related}, le.value, le.tagval, le.weight} );
						li = le.next;
					}
				}
				return rt;
			}
		};
		Tree<typename Map::TreeNode> listIndexTree = m_map.getTree();
		TranslateItem translateItem( m_list);
		return listIndexTree.template translate<TreeNode>( translateItem);
	}

private:
	struct ListElement
	{
		RELNODETYPE related;
		VALTYPE value;
		float weight;
		int next;
		std::uint8_t tagval;

		ListElement( RELNODETYPE const& related_, VALTYPE const& value_, std::uint8_t tagval_, float weight_, int next_)
			:related(related_),value(value_),weight(weight_),next(next_),tagval(tagval_){}
		ListElement( ListElement const& o)
			:related(o.related),value(o.value),weight(o.weight),next(o.next),tagval(o.tagval){}
	};

	typedef ScopedMap<RELNODETYPE,int> Map;
	typedef std::vector<ListElement> List;

private:
	Map m_map;
	List m_list;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

