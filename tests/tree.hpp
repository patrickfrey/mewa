/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Tree data structure for building test szenarios
/// \file "tree.hpp"
#ifndef _MEWA_TEST_TREE_HPP_INCLUDED
#define _MEWA_TEST_TREE_HPP_INCLUDED
#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include <iostream>

namespace mewa {
namespace test {

template <typename ITEM>
struct Tree
{
	ITEM item;
	Tree* next;
	Tree* chld;

	Tree( const ITEM& item_)
		:item(item_),next(0),chld(0){}
	Tree( const Tree& o)
		:item(o.item)
		,next( o.next ? new Tree( *o.next) : 0)
		,chld( o.chld ? new Tree( *o.chld) : 0){}

	Tree& operator=( const Tree& o)
	{
		item = o.item;
		next = o.next ? new Tree( *o.next) : 0;
		chld = o.chld ? new Tree( *o.chld) : 0;
		return *this;
	}
	Tree( Tree&& o)
		:item( std::move( o.item))
		,next( o.next)
		,chld( o.chld)
		{o.next=0; o.chld=0;}

	Tree& operator=( Tree&& o)
	{
		item = std::move( o.item);
		next = o.next; o.next = 0;
		chld = o.chld; o.chld = 0;
		return *this;
	}
	~Tree()
	{
		if (next) delete next;
		if (chld) delete chld;
	}

	void addChild( const Tree& nd)
	{
		if (chld)
		{
			chld->addSibling( nd);
		}
		else
		{
			chld = new Tree( nd);
		}
	}

	void addSibling( const Tree& nd)
	{
		Tree* np = this;
		for (; np->next; np = np->next){}
		np->next = new Tree( nd);
	}

	// \note Pass ownership
	void addChild( Tree* nd)
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
	void addSibling( Tree* nd)
	{
		Tree* np = this;
		for (; np->next; np = np->next){}
		np->next = nd;
	}

	void print( std::ostream& out)
	{
		print( out, 0);
	}

	int size() const
	{
		Tree* np = this;
		int ii = 1;
		for (; np->next; np=np->next,++ii){}
		return ii;
	}

private:
	void print( std::ostream& out, std::size_t indent)
	{
		std::string indentstr( indent*2, ' ');
		Tree* np = this;
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


