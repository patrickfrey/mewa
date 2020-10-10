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
#include <string>
#include <map>
#include <vector>

namespace mewa {

struct Scope
{
public:
	typedef int Step;

public:
	Scope( Step start_, Step end_) noexcept
		:m_start(start_ >= 0 ? start_ : 0),m_end(end_ >= 0 ? end_ : std::numeric_limits<Step>::max()){}
	Scope( const Scope& o) noexcept
		:m_start(o.m_start),m_end(o.m_end){}
	Scope& operator=( const Scope& o) noexcept
		{m_start=o.m_start; m_end=o.m_end; return *this;}

	bool contains( const Scope& o) const noexcept
	{
		return o.m_start >= m_start && o.m_end <= m_end;
	}

	bool contains( Step step) const noexcept
	{
		return step >= m_start && step < m_end;
	}

	bool operator == ( const Scope& o) const noexcept
	{
		return o.m_start == m_start && o.m_end == m_end;
	}
	bool operator != ( const Scope& o) const noexcept
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
	bool operator()( const Scope& aa, const Scope& bb) const
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
	ScopedUpValue( const ScopedUpValue& o) noexcept
		:scope(o.scope),value(o.value),prev(o.prev){}
};


template <typename VALTYPE>
class ScopedUpValueInvTree
{
public:
	typedef ScopedUpValue<VALTYPE> UpValue;

public:
	ScopedUpValueInvTree( const VALTYPE nullval_, std::size_t initsize)
		:m_ar(),m_nullval(nullval_) {m_ar.reserve(initsize);}
	ScopedUpValueInvTree( const ScopedUpValueInvTree& o) = default;
	ScopedUpValueInvTree& operator=( const ScopedUpValueInvTree& o) = default;
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
		explicit const_iterator( const std::vector<UpValue>& ar_, int index_=-1) noexcept
			:m_ar(ar_.data()),m_index(index_){}

		const_iterator& operator++() noexcept				{m_index = m_ar[ m_index].prev; return *this;}
		const_iterator operator++(int) noexcept				{const_iterator rt(m_ar,m_index); m_index = m_ar[ m_index].prev; return rt;}

		bool operator==( const const_iterator& o) const noexcept	{return m_ar==o.m_ar && m_index == o.m_index;}
		bool operator!=( const const_iterator& o) const noexcept	{return m_ar!=o.m_ar || m_index != o.m_index;}

		VALTYPE const& operator*() const noexcept			{return m_ar[ m_index].value;}
		VALTYPE const* operator->() const noexcept			{return &m_ar[ m_index].value;}

		const Scope& scope() const noexcept				{return m_ar[ m_index].scope;}

	private:
		UpValue const* m_ar;
		int m_index;
	};

	const_iterator start( int index) const noexcept
	{
		return index ? const_iterator( m_ar, index) : const_iterator( m_ar);
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
template <typename NODEVAL>
struct ScopeHierarchyTreeNode
{
	Scope scope;
	NODEVAL value;

	ScopeHierarchyTreeNode( const Scope scope_, const NODEVAL& value_)
		:scope(scope_),value(value_){}
	ScopeHierarchyTreeNode( const ScopeHierarchyTreeNode& o)
		:scope(o.scope),value(o.value){}
};

/// \class ScopeHierarchyTreeBuilder
/// \brief Template for a builder of a tree displaying the scope hierarchy from its inverse representation (ScopedUpValueInvTree)
/// \param NODEVAL type of an item attached to a tree node 
/// \param ELEMVAL type of an element of NODEVAL, same as NODEVAL if a node has only one item assigned
/// \param ScopeHierarchyTreeNodeAssign Template parameter implements the following interface to insert/update elements in method insert()
/*
* template <typename NODEVAL, typename ELEMVAL>
* struct ScopeHierarchyTreeNodeAssign
* {
*	NODEVAL create( const ELEMVAL& elemval) const=0;
*	void add( NODEVAL& ndval, const ELEMVAL& elemval) const=0;
* };
*/
template <typename NODEVAL, typename ELEMVAL, class ScopeHierarchyTreeNodeAssignType>
class ScopeHierarchyTreeBuilder
{
public:
	typedef ScopeHierarchyTreeNode<NODEVAL> Node;
	typedef ScopedUpValueInvTree<ELEMVAL> InvTree;

	explicit ScopeHierarchyTreeBuilder( const NODEVAL& nullval_)
		:m_tree(Node( Scope( 0, std::numeric_limits<Scope::Step>::max()), nullval_))
		,m_scope2nodeidxMap()
	{
		m_scope2nodeidxMap.insert( {Scope( 0, std::numeric_limits<Scope::Step>::max()),1/*root node index*/} );
	}

	void insert( const ScopeHierarchyTreeNodeAssignType& assign, typename InvTree::const_iterator ni, const typename InvTree::const_iterator ne)
	{
		int buffer_stk[ 256];
		mewa::monotonic_buffer_resource memrsc_stk( buffer_stk, sizeof buffer_stk);
		std::pmr::vector<typename InvTree::const_iterator> stk( &memrsc_stk);
		stk.reserve( sizeof buffer_stk / sizeof InvTree::const_iterator);
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
				assign.add( m_tree[ ndidx].item(), *ni);
				break;
			}
		}
		for (; !stk.empty(); stk.pop_back())
		{
			typename InvTree::const_iterator stk_ni = stk.top();
			ndidx = m_tree.addChild( ndidx, Node( stk_ni.scope(), assign.create( *stk_ni)));
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
class ScopedSet
{
public:
	ScopedSet( std::pmr::memory_resource* memrsc, const VALTYPE nullval_, std::size_t initsize)
		:m_map( memrsc),m_invtree(nullval_,initsize){}
	ScopedSet( const ScopedSet& o) = default;
	ScopedSet& operator=( const ScopedSet& o) = default;
	ScopedSet( ScopedSet&& o) noexcept = default;
	ScopedSet& operator=( ScopedSet&& o) noexcept = default;

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
		TreeNodeAssign assign;
		ScopeHierarchyTreeBuilder<VALTYPE,VALTYPE,TreeNodeAssign> treeBuilder( m_invtree.nullval());
		auto mi = m_map.begin();
		for (; mi != m_map.end(); ++mi)
		{
			treeBuilder.insert( assign, m_invtree.start( mi->second), m_invtree.end());
		}
		return treeBuilder.tree();
	}

	const ScopedUpValueInvTree<VALTYPE>& invtree() const noexcept
	{
		return m_invtree;
	}

private:
	/// \brief Implements the interface needed by the parameter ScopeHierarchyTreeNodeAssignType of the ScopeHierarchyTreeBuilder template
	struct TreeNodeAssign
	{
		TreeNodeAssign(){}
		VALTYPE create( const VALTYPE& elemval) const
		{
			return elemval;
		}
		void add( VALTYPE& ndval, const VALTYPE& elemval) const
		{
			ndval = elemval;
		}
	};

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
		ScopeHierarchyTreeBuilder<VALTYPE,VALTYPE,TreeNodeAssign> treeBuilder( m_invtree.nullval());
		auto mi = m_map.begin();
		for (; mi != m_map.end(); ++mi)
		{
			TreeNodeAssign assign( mi->first.key());
			treeBuilder.insert( assign, m_invtree.start( mi->second), m_invtree.end());
		}
		return treeBuilder.tree();
	}

private:
	/// \brief Implements the interface needed by the parameter ScopeHierarchyTreeNodeAssignType of the ScopeHierarchyTreeBuilder template
	struct TreeNodeAssign
	{
		KEYTYPE key;

		TreeNodeAssign( const KEYTYPE& key_)
			:key(key_){}

		TreeItem create( const KeyValue& val) const
		{
			TreeItem rt;
			rt.push_back( KeyValue( key, val));
			return rt;
		}
		void add( TreeItem& ndval, const VALTYPE& val) const
		{
			ndval.push_back( KeyValue( key, val));
		}
	};	

private:
	Map m_map;
	ScopedUpValueInvTree<VALTYPE> m_invtree;
};


template <typename RELNODETYPE, typename VALTYPE>
class ScopedRelationMap
{
public:
	class ResultElement
	{
	public:
		ResultElement( const RELNODETYPE type_, const VALTYPE value_, float weight_)
			:m_type(type_),m_value(value_),m_weight(weight_){}
		ResultElement( const ResultElement& o)
			:m_type(o.m_type),m_value(o.m_value),m_weight(o.m_weight){}

		RELNODETYPE type() const noexcept	{return m_type;}
		VALTYPE value() const noexcept		{return m_value;}
		float weight() const noexcept		{return m_weight;}

	private:
		RELNODETYPE m_type;
		VALTYPE m_value;
		float m_weight;
	};

	ScopedRelationMap( std::pmr::memory_resource* memrsc, std::size_t initsize)
		:m_map( memrsc, -1/*nullval*/, initsize),m_list() {m_list.reserve(initsize);}
	ScopedRelationMap( const ScopedRelationMap& o) = default;
	ScopedRelationMap& operator=( const ScopedRelationMap& o) = default;
	ScopedRelationMap( ScopedRelationMap&& o) noexcept = default;
	ScopedRelationMap& operator=( ScopedRelationMap&& o) noexcept = default;

	void set( const Scope scope, const RELNODETYPE& left, const RELNODETYPE& right, const VALTYPE& value, float weight)
	{
		if (weight <= std::numeric_limits<float>::epsilon()*10) throw Error( Error::BadRelationWeight, string_format("%.4f",weight));

		int newlistindex = m_list.size();
		int li = m_map.getOrSet( scope, left, newlistindex);
		int prev_li = -1;
		while (li >= 0)
		{
			const ListElement& le = m_list[ li];
			if (le.related == right)
			{
				throw Error( Error::DuplicateDefinition);
			}
			prev_li = li;
			li = le.next;
		}
		m_list.push_back( ListElement( right, value, weight, -1/*next*/));
		if (prev_li >= 0)
		{
			m_list[ prev_li].next = newlistindex;
		}
	}

	std::pmr::vector<ResultElement> get( const Scope::Step step, const RELNODETYPE& key, std::pmr::memory_resource* res_memrsc) const noexcept
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
				auto ins = candidatemap.insert( {le.related, rt.size()});
				if (ins.second /*insert took place*/)
				{
					rt.push_back( ResultElement( le.related/*type*/, le.value, le.weight));
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

		TreeNodeElement( const std::pair<RELNODETYPE,RELNODETYPE>& relation_, const VALTYPE& value_, float weight_)
			:relation(relation_),value(value_),weight(weight_){}
		TreeNodeElement( const TreeNodeElement& o)
			:relation(o.relation),value(o.value),weight(o.weight){}
	};
	typedef std::vector<TreeNodeElement> TreeItem;
	typedef ScopeHierarchyTreeNode<TreeItem> TreeNode;

	Tree<TreeNode> getTree() const
	{
		ScopeHierarchyTreeBuilder<VALTYPE,VALTYPE,TreeNodeAssign> treeBuilder( m_map.invtree().nullval());
		auto mi = m_map.begin();
		for (; mi != m_map.end(); ++mi)
		{
			TreeNodeAssign assign( mi->first.key(), m_list.data(), m_list.size());
			treeBuilder.insert( assign, m_map.invtree().start( mi->second), m_map.invtree().end());
		}
		return treeBuilder.tree();
	}

private:
	struct ListElement
	{
		RELNODETYPE related;
		VALTYPE value;
		float weight;
		int next;

		ListElement( const RELNODETYPE& related_, const VALTYPE& value_, float weight_, int next_)
			:related(related_),value(value_),weight(weight_),next(next_){}
		ListElement( const ListElement& o)
			:related(o.related),value(o.value),weight(o.weight),next(o.next){}
	};

	/// \brief Implements the interface needed by the parameter ScopeHierarchyTreeNodeAssignType of the ScopeHierarchyTreeBuilder template
	struct TreeNodeAssign
	{
		ListElement const* elemar;
		std::size_t elemarsize;
		RELNODETYPE key;

		TreeNodeAssign( const RELNODETYPE& key_, ListElement const* elemar_, std::size_t elemarsize_)
			:key(key_),elemar(elemar_),elemarsize(elemarsize_){}

		TreeItem create( int li) const
		{
			TreeItem rt;
			while (li >= 0)
			{
				const ListElement& le = elemar[ li];
				rt.push_back( TreeNodeElement( {key, le.related}, le.value, le.weight));
				li = le.next;
			}
			return rt;
		}
		void add( TreeItem& ndval, int li) const
		{
			while (li >= 0)
			{
				const ListElement& le = elemar[ li];
				ndval.push_back( TreeNodeElement( {key, le.related}, le.value, le.weight));
				li = le.next;
			}
		}
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

