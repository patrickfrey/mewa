/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Some templates for custom C++/STL conform iterators. 
/// \file "iterator.hpp"
#ifndef _MEWA_ITERATOR_HPP_INCLUDED
#define _MEWA_ITERATOR_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <iterator>
#include <cstddef>

namespace mewa {

struct CArrayForwardIterator
{
	template <class TYPE>
	class const_iterator
	{
	public:
		typedef const_iterator self_type;
                typedef TYPE value_type;
                typedef TYPE& reference;
                typedef TYPE* pointer;
                typedef std::forward_iterator_tag iterator_category;
		typedef std::forward_iterator_tag iterator_concept;
                typedef std::ptrdiff_t difference_type;

		explicit const_iterator( TYPE const* ptr_) noexcept			:ptr(ptr_){}
		const_iterator( const const_iterator& o) noexcept			:ptr(o.ptr){}
		const_iterator() noexcept						:ptr(nullptr){}
		const_iterator& operator++() noexcept					{++ptr; return *this;}
		const_iterator operator++(int) noexcept					{const_iterator rt(ptr); ++ptr; return rt;}

		bool operator < (const const_iterator& o) const noexcept		{return ptr < o.ptr;}
		bool operator > (const const_iterator& o) const noexcept		{return ptr > o.ptr;}
		bool operator <= (const const_iterator& o) const noexcept		{return ptr <= o.ptr;}
		bool operator >= (const const_iterator& o) const noexcept		{return ptr >= o.ptr;}
		bool operator == (const const_iterator& o) const noexcept		{return ptr == o.ptr;}
		bool operator != (const const_iterator& o) const noexcept		{return ptr != o.ptr;}

		std::ptrdiff_t operator - (const const_iterator& o) const noexcept	{return ptr - o.ptr;}

		TYPE const& operator*() const noexcept					{return *ptr;}
		TYPE const* operator->() const noexcept					{return ptr;}

	private:
		TYPE const* ptr;
	};
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

