/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Structure for mapping a string identifier to a cardinal number
/// \file "identmap.hpp"
#ifndef _MEWA_IDENT_MAP_HPP_INCLUDED
#define _MEWA_IDENT_MAP_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "error.hpp"
#include "strings.hpp"
#include <utility>
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <string_view>
#include <memory_resource>

namespace mewa {

struct IdentKey
{
	unsigned short size;
	char buf[1];

	static const IdentKey* create( std::pmr::memory_resource* memrsc, const std::string_view& src)
	{
		constexpr unsigned short maxsize = std::numeric_limits<unsigned short>::max()-4;
		unsigned short strsize = src.size() > maxsize ? maxsize : src.size();
		ptrdiff_t offs_buf = reinterpret_cast<const char*>(&((const IdentKey*)nullptr)->buf) - (const char*)nullptr;
		unsigned short blksize = offs_buf + strsize + 1;
		blksize += (4 - (blksize & 3)) & 3; //... align size of of key to 0 mod sizeof(uint32_t)
		unsigned short restsize = blksize - strsize - offs_buf;
		IdentKey* rt = (IdentKey*)memrsc->allocate( blksize, 4/*align*/);
		rt->size = strsize;
		std::memcpy( rt->buf, src.data(), strsize);
		std::memset( rt->buf + strsize, 0, restsize);
		return rt;
	}

	uint32_t const* alignedptr() const noexcept
	{
		return reinterpret_cast<uint32_t const*>( this);
	}
	int alignedsize() const noexcept
	{
		ptrdiff_t offs_buf = reinterpret_cast<const char*>(&((const IdentKey*)nullptr)->buf) - (const char*)nullptr;
		unsigned short blksize = offs_buf + size + 1;
		blksize += (4 - (blksize & 3)) & 3; //... align size of of key to 0 mod sizeof(uint32_t)
		return blksize / 4;
	}
};

struct IdentKeyEnvelop
{
	const IdentKey* key;

	IdentKeyEnvelop() :key(0){}
	IdentKeyEnvelop( const IdentKey* key_) :key(key_){}

	bool operator == (const IdentKeyEnvelop& o) const noexcept
	{
		int length = key->alignedsize();
		uint32_t const* kk = key->alignedptr();
		uint32_t const* ll = o.key->alignedptr();

		int ki = 0;
		for (; ki < length && kk[ ki] == ll[ ki]; ++ki) {}
		return ki == length;
	}

	std::size_t hash() const noexcept
	{
		return hashword2( key);
	}

private:
	// Hash function copied from https://www.burtleburtle.net/bob/c/lookup3.c
	// lookup3.c, by Bob Jenkins, May 2006, Public Domain.

	static inline uint32_t rot( uint32_t n, uint32_t d) noexcept
	{
		return (n<<d)|(n>>(32-d));
	}
	struct Bank
	{
		uint32_t a;
		uint32_t b;
		uint32_t c;

		explicit Bank( uint32_t initval=0)
		{
			a = b = c = 0xdeadbeef + initval;
		}
		inline void mix() noexcept
		{
			a -= c;  a ^= rot(c, 4);  c += b;
			b -= a;  b ^= rot(a, 6);  a += c;
			c -= b;  c ^= rot(b, 8);  b += a;
			a -= c;  a ^= rot(c,16);  c += b;
			b -= a;  b ^= rot(a,19);  a += c;
			c -= b;  c ^= rot(b, 4);  b += a;
		}
		inline void final() noexcept
		{
			c ^= b; c -= rot(b,14);
			a ^= c; a -= rot(c,11);
			b ^= a; b -= rot(a,25);
			c ^= b; c -= rot(b,16);
			a ^= c; a -= rot(c,4); 
			b ^= a; b -= rot(a,14);
			c ^= b; c -= rot(b,24);
		}
	};
	static std::size_t hashword2( const IdentKey* key)
	{
		Bank bank;
		int length = key->alignedsize();
		uint32_t const* kk = key->alignedptr();

		while (length > 3)
		{
			bank.a += kk[0];
			bank.b += kk[1];
			bank.c += kk[2];
			bank.mix();
			length -= 3;
			kk += 3;
		}
		switch(length)
		{ 
			case 3 : bank.c += kk[2];
			case 2 : bank.b += kk[1];
			case 1 : bank.a += kk[0];
				bank.final();
		}
		return ((std::size_t)bank.c << 32) + (std::size_t)(bank.b + bank.a);
	}
};

}//namespace

namespace std
{
	template<> struct hash<mewa::IdentKeyEnvelop>
	{
	public:
		hash<mewa::IdentKeyEnvelop>(){}
		std::size_t operator()( mewa::IdentKeyEnvelop const& key) const noexcept
		{
			return key.hash();
		}
	};
}


namespace mewa {

class IdentMap
	:public std::pmr::unordered_map<IdentKeyEnvelop, int>
{
public:
	typedef std::pmr::unordered_map<IdentKeyEnvelop, int> ParentClass;
	typedef typename ParentClass::const_iterator const_iterator;

	IdentMap( std::pmr::memory_resource* map_memrsc_, std::pmr::memory_resource* str_memrsc_, int nofInitBuckets)
		:ParentClass(nofInitBuckets,map_memrsc_),m_str_memrsc(str_memrsc_),m_inv() {}
	IdentMap( const IdentMap& o)
		:ParentClass(o),m_str_memrsc(o.m_str_memrsc),m_inv(o.m_inv) {}
	IdentMap& operator=( const IdentMap& o)
		{ParentClass::operator=(o); m_str_memrsc=o.m_str_memrsc; m_inv=o.m_inv; return *this;}
	IdentMap( IdentMap&& o)
		:ParentClass(o),m_str_memrsc(o.m_str_memrsc),m_inv(std::move(o.m_inv)) {}
	IdentMap& operator=( IdentMap&& o)
		{ParentClass::operator=(o); m_str_memrsc=o.m_str_memrsc; m_inv=std::move(o.m_inv); return *this;}

	int get( const std::string_view& str)
	{
		int rt;
		int buffer[ 1024];
		std::pmr::monotonic_buffer_resource local_memrsc( buffer, sizeof buffer);
		IdentKeyEnvelop env( IdentKey::create( &local_memrsc, str));
		auto fi = ParentClass::find( env);
		if (fi == ParentClass::end())
		{
			env = IdentKeyEnvelop( IdentKey::create( m_str_memrsc, str));
			m_inv.reserve( growSize());
			rt = (int)m_inv.size()+1;
			if (ParentClass::insert( {env, rt}).second == false)
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			m_inv.push_back( env.key);
		}
		else
		{
			rt = fi->second;
		}
		return rt;
	}

	std::size_t hash( const std::string_view& str) const noexcept
	{
		int buffer[ 1024];
		std::pmr::monotonic_buffer_resource local_memrsc( buffer, sizeof buffer);
		IdentKeyEnvelop env( IdentKey::create( &local_memrsc, str));
		return env.hash();
	}

	std::string_view inv( int id) const noexcept
	{
		const IdentKey* key = m_inv[ id-1];
		return std::string_view( key->buf, key->size);
	}

	std::size_t size() const noexcept
	{
		return m_inv.size();
	}

private:
	std::size_t growSize() const
	{
		// ... array duplication strategy
		enum {InitSize=1<<14};
		std::size_t mm = InitSize;
		while (mm <= m_inv.size() && mm > InitSize) mm*=2;
		return mm;
	}

private:
	std::pmr::memory_resource* m_str_memrsc;
	std::vector<const IdentKey*> m_inv;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

