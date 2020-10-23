/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Some special memory resources for testing some properties
/// \file "memory_resource.hpp"
#ifndef _MEWA_MEMORY_RESOURCE_HPP_INCLUDED
#define _MEWA_MEMORY_RESOURCE_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <memory_resource>
#include <cstddef>
#include <new>

#define MEWA_LOWLEVEL_DEBUG
//... using bad_alloc_memory_resource as upstream of monotonic buffer resource
#ifdef NDEBUG
#ifdef MEWA_LOWLEVEL_DEBUG
#error Switched on MEWA_LOWLEVEL_DEBUG in release build
#endif
#endif

namespace mewa {

/// \brief Memory resource that throws bad_alloc on every bad_alloc
/// \note Used as upstream for monotonic_buffer_resource to check that the upstream is never used
class bad_alloc_memory_resource : public std::pmr::memory_resource
{
public:
	bad_alloc_memory_resource(){}
	~bad_alloc_memory_resource(){}

	bad_alloc_memory_resource& operator=( const bad_alloc_memory_resource& o) = default;

	void* allocate(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		throw std::bad_alloc();
	}

	void deallocate( void* p, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		throw std::bad_alloc();
	}

	void* do_allocate( size_t __bytes, size_t __alignment)
	{
		throw std::bad_alloc();
	}
	void  do_deallocate( void* __p, size_t __bytes, size_t __alignment)
	{
		throw std::bad_alloc();
	}
	bool do_is_equal( const std::pmr::memory_resource& __other) const noexcept
	{
		return false;
	}
};

/// \brief Memory resource that uses local buffer whenever possible (std seems to use upstream for larger allocs)
class monotonic_buffer_resource : public std::pmr::memory_resource
{
public:
	monotonic_buffer_resource( std::size_t initsize, std::pmr::memory_resource* upstream_)
		:m_baseptr(nullptr),m_index(0),m_size(initsize),m_upstream(upstream_),m_ownership_upstream(false),m_ownership_baseptr(true)
	{
		m_baseptr = (char*)std::malloc( m_size);
		m_ownership_baseptr = m_baseptr != nullptr;
	}
	explicit monotonic_buffer_resource( std::size_t initsize)
		:m_baseptr(nullptr),m_index(0),m_size(initsize),m_upstream(nullptr),m_ownership_upstream(false),m_ownership_baseptr(true)
	{
		init_new_upstream();
		m_baseptr = (char*)std::malloc( m_size);
		m_ownership_baseptr = m_baseptr != nullptr;
	}
	~monotonic_buffer_resource()
	{
		if (m_ownership_baseptr) std::free( m_baseptr);
		if (m_ownership_upstream) delete m_upstream;
	}

	monotonic_buffer_resource( void* buffer, std::size_t buffersize)
		:m_baseptr((char*)buffer),m_index(0),m_size(buffersize),m_upstream(nullptr),m_ownership_upstream(false),m_ownership_baseptr(false)
	{
		init_new_upstream();
	}

	monotonic_buffer_resource( void* buffer, std::size_t buffersize, std::pmr::memory_resource* upstream_)
		:m_baseptr((char*)buffer),m_index(0),m_size(buffersize),m_upstream(upstream_),m_ownership_upstream(false),m_ownership_baseptr(false)
	{}

	monotonic_buffer_resource(const monotonic_buffer_resource&) = delete;

public:
	void* allocate( std::size_t bytes, std::size_t alignment = alignof(std::max_align_t))
	{
		return allocate_( bytes, alignment);
	}

	void deallocate( void* p, std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)){}

	void* do_allocate( size_t bytes, size_t alignment)
	{
		return allocate_( bytes, alignment);
	}

	void  do_deallocate( void* __p, size_t __bytes, size_t __alignment){}

	bool do_is_equal( const std::pmr::memory_resource& __other) const noexcept
	{
		return false;
	}

private:
	void init_new_upstream()
	{
#ifdef MEWA_LOWLEVEL_DEBUG
		m_upstream = new mewa::bad_alloc_memory_resource();
		m_ownership_upstream = true;
#else
		m_upstream = std::pmr::get_default_resource();
#endif
	}
private:
	int isPowerOfTwo( size_t x)
	{
		return ((x != 0) && ((x & (~x + 1)) == x));
	}

	void skip_align( size_t alignment_)
	{
		if (!isPowerOfTwo( alignment_)) throw std::bad_alloc();
		size_t rest = m_index & (alignment_-1);
		if (rest != 0)
		{
			m_index += (alignment_ - rest);
		}
	}

	void* allocate_( size_t __bytes, size_t __alignment)
	{
		skip_align( __alignment);
		if (m_index + __bytes > m_size)
		{
			return m_upstream->allocate( __bytes, __alignment);
		}
		void* rt = m_baseptr + m_index;
		m_index += __bytes;
		return rt;
	}

private:
	char* m_baseptr;
	std::size_t m_index;
	std::size_t m_size;
	std::pmr::memory_resource* m_upstream;
	bool m_ownership_upstream;
	bool m_ownership_baseptr;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


