/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Tree data structure for building test szenarios
/// \file "tree_node.hpp"
#ifndef _MEWA_TEST_TREE_NODE_HPP_INCLUDED
#define _MEWA_TEST_TREE_NODE_HPP_INCLUDED
#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include <iostream>

namespace mewa {
namespace test {

template <typename ITEM>
struct TreeNode
{
	ITEM item;
	TreeNode* next;
	TreeNode* chld;

	TreeNode( const ITEM& item_)
		:item(item_),next(nullptr),chld(nullptr){}
	TreeNode( const TreeNode& o)
		:item(o.item)
		,next( o.next ? new TreeNode( *o.next) : nullptr)
		,chld( o.chld ? new TreeNode( *o.chld) : nullptr){}

	TreeNode& operator=( const TreeNode& o)
	{
		item = o.item;
		next = o.next ? new TreeNode( *o.next) : nullptr;
		chld = o.chld ? new TreeNode( *o.chld) : nullptr;
		return *this;
	}
	TreeNode( TreeNode&& o)
		:item( std::move( o.item))
		,next( o.next)
		,chld( o.chld)
		{o.next=nullptr; o.chld=nullptr;}

	TreeNode& operator=( TreeNode&& o)
	{
		item = std::move( o.item);
		next = o.next; o.next = nullptr;
		chld = o.chld; o.chld = nullptr;
		return *this;
	}
	~TreeNode()
	{
		if (next) delete next;
		if (chld) delete chld;
	}

	void addChild( const TreeNode& nd)
	{
		if (chld)
		{
			chld->addSibling( nd);
		}
		else
		{
			chld = new TreeNode( nd);
		}
	}

	void addSibling( const TreeNode& nd)
	{
		TreeNode* np = this;
		for (; np->next; np = np->next){}
		np->next = new TreeNode( nd);
	}

	// \note Pass ownership
	void addChild( TreeNode* nd)
	{
		if (chld)
		{
			chld->addSibling( nd);
		}
		else
		{
			chld = nd;
		}
	}

	// \note Pass ownership
	void addSibling( TreeNode* nd)
	{
		TreeNode* np = this;
		for (; np->next; np = np->next){}
		np->next = nd;
	}

	void print( std::ostream& out)
	{
		print( out, 0);
	}

	int size() const noexcept
	{
		TreeNode const* np = this;
		int ii = 1;
		for (; np->next; np=np->next,++ii){}
		return ii;
	}

private:
	void print( std::ostream& out, std::size_t indent)
	{
		std::string indentstr( indent*2, ' ');
		TreeNode* np = this;
		do
		{
			out << indentstr << np->item << std::endl;
			if (np->chld) np->chld->print( out, indent+2);
			np = np->next;
		} while (np);
	}
};

}}//namespace
#endif


