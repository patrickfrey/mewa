/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Tree data structure implemented as array of nodes
/// \file "tree.hpp"
#ifndef _MEWA_TREE_HPP_INCLUDED
#define _MEWA_TREE_HPP_INCLUDED
#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include "error.hpp"
#include "strings.hpp"
#include "iterator.hpp"
#include <iostream>
#include <vector>

namespace mewa {

template <typename ITEM>
class Tree
{
public:
	explicit Tree( const ITEM& rootNode)
		:m_ar(),m_lastar()
	{
		m_ar.push_back( Node( rootNode));
		m_lastar.push_back( 0);
	}
	Tree( const Tree& o)
		:m_ar(o.m_ar),m_lastar(o.m_lastar){}

	Tree& operator=( const Tree& o)
	{
		m_ar = o.m_ar;
		m_lastar = o.m_lastar;
		return *this;
	}
	Tree( Tree&& o)
		:m_ar( std::move( o.m_ar)),m_lastar( std::move( o.m_lastar)){}

	Tree& operator=( Tree&& o)
	{
		m_ar = std::move( o.m_ar);
		m_lastar = std::move( o.m_lastar);
		return *this;
	}

	class Node
	{
	public:
		const ITEM& item() const noexcept 	{return m_item;}
		ITEM& item() noexcept 			{return m_item;}
		std::size_t next() const noexcept 	{return m_next;}
		std::size_t chld() const noexcept 	{return m_chld;}

		Node( const Node& o)
			:m_item(o.m_item),m_next(o.m_next),m_chld(o.m_chld){}
		explicit Node( const ITEM& item_)
			:m_item(item_),m_next(0),m_chld(0){}

	private:
		friend class Tree;

		ITEM m_item;
		std::size_t m_next;
		std::size_t m_chld;
	};

	class depthfirst_iterator
		:public CArrayBaseForwardIterator::const_iterator<ITEM>
	{
	public:
		typedef CArrayBaseForwardIterator::const_iterator<ITEM> Parent;

		explicit depthfirst_iterator( const depthfirst_iterator& o)
			:Parent(o),m_stack(o.m_stack){}
		explicit depthfirst_iterator( const ITEM* ar, std::size_t arpos, std::size_t arsize) noexcept
			:Parent(ar,arpos),m_stack()
		{
			if (arpos != arsize) m_stack.push_back( arpos);
		}
		depthfirst_iterator& operator++()
		{
			skip();
			return *this;
		}
		depthfirst_iterator operator++(int)
		{
			depthfirst_iterator rt( *this);
			skip();
			return rt;
		}
		std::size_t indent() const noexcept
		{
			return m_stack.size();
		}

	private:
		void skip()
		{
			if ((*this)->chld())
			{
				m_stack.push_back( (*this)->position()+1);
				Parent::setPosition( (*this)->chld()-1);
			}
			else if ((*this)->next())
			{
				Parent::setPosition( (*this)->next()-1);
			}
			else if (!m_stack.empty())
			{
				while (!m_stack.empty())
				{
					Parent::setPosition( m_stack.back()-1);
					m_stack.pop_back();
					if (!m_stack.empty() && (*this)->next())
					{
						Parent::setPosition( (*this)->next()-1);
						break;
					}
				}
			}
			else
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
		}

	private:
		std::vector<std::size_t> m_stack;
	};

	depthfirst_iterator begin() const
	{
		return depthfirst_iterator( m_ar, 0, m_ar.size());
	}
	depthfirst_iterator end() const
	{
		return depthfirst_iterator( m_ar, m_ar.size(), m_ar.size());
	}

	const Node& operator[]( std::size_t idx) const
	{
		if (!idx) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		return m_ar[ idx-1];
	}
	Node& operator[]( std::size_t idx)
	{
		if (!idx) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		return m_ar[ idx-1];
	}

	std::size_t addSibling( std::size_t nodeidx, const ITEM& item)
	{
		if (!nodeidx) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		std::size_t newindex = m_ar.size()+1;
		m_ar.push_back( Node( item));
		m_lastar.push_back( 0);

		std::size_t& lastref = m_lastar[ nodeidx-1];
		if (!lastref)
		{
			// ... Possibly added sibling to root node
			throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}
		else
		{
			if (!m_ar[ lastref-1].m_next) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			lastref = m_ar[ lastref-1].m_next = newindex;
		}
	}

	std::size_t addChild( std::size_t nodeidx, const ITEM& item)
	{
		if (!nodeidx) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		if (!m_ar[ nodeidx-1].m_chld)
		{
			std::size_t newindex = m_ar.size()+1;
			m_ar.push_back( Node( item));
			m_lastar.push_back( newindex);
			m_ar[ nodeidx-1].m_chld = newindex;
			return newindex;
		}
		else
		{
			return addSibling( m_ar[ nodeidx-1].m_chld, item);
		}
	}

	std::size_t nextAddIndex() const noexcept
	{
		return m_ar.size()+1;
	}

private:
	std::vector<Node> m_ar;
	std::vector<std::size_t> m_lastar;
};

}//namespace
#endif


