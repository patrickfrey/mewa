/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Structure for mapping identifiers to a cardinal number
/// \file "identmap.hpp"
#ifndef _MEWA_IDENT_MAP_HPP_INCLUDED
#define _MEWA_IDENT_MAP_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <utility>
#include <map>
#include <cstring>
#include <cstdint>
#include <string_view>
#include <memory_resource>

namespace mewa {

struct IdentKey
{
	unsigned short size;
	char buf[0];

	static const IdentKey* create( std::pmr::memory_resource* memrsc, std::string_view src)
	{
		unsigned short maxsize = std::numeric_limits<unsigned short>::max()-4;
		unsigned short strsize = src.size() > maxsize : maxsize : src.size();
		unsigned short blksize = (sizeof( IdentKey) + strsize);
		blksize += (4 - (blksize & 3)) & 3; //... align size of of key to 0 mod sizeof(uint32_t)
		unsigned short restsize = blksize - strsize;
		IdentKey* rt = memrsc->alloc( sizeof( IdentKey) + blksize, 4/*align*/);
		rt->size = size;
		std::memcpy( rt->buf, src.data(), size);
		std::memset( rt->buf + size, 0, restsize+1);
		return rt;
	}
};

struct IdentKeyEnvelop
{
	const IdentKey* key;
	IdentKeyEnvelop() :key(0){}
	IdentKeyEnvelop( const IdentKey* key_) :key(key_){}

	bool operator == (const IdentKeyEnvelop& o) const noexcept
	{
		int length = (key.size + 3) / 4;
		uint32_t const* kk = reinterpret_cast<uint32_t const*>( key);
		uint32_t const* ll = reinterpret_cast<uint32_t const*>( o.key);

		for (int ki = 0; ki < length && kk[ ki] == ll[ ki]; ++ki) {}
		return ki == length;
	}

	std::size_t hash() const noexcept
	{
		return hashword2( key);
	}

private:
	// Hash function copied from https://www.burtleburtle.net/bob/c/lookup3.c
	// lookup3.c, by Bob Jenkins, May 2006, Public Domain.

	static uint32_t rot( uint32_t n, uint32_t d) noexcept
	{
		return (n<<d)|(n>>(32-d));
	}
	struct Bank
	{
		uint32_t a;
		uint32_t b;
		uint32_t c;

		Bank( const Bank o) = default;
		Bank( uint32_t initval)
		{
			a = b = c = 0xdeadbeef + initval;
		}
		void mix() noexcept
		{
			a -= c;  a ^= rot(c, 4);  c += b;
			b -= a;  b ^= rot(a, 6);  a += c;
			c -= b;  c ^= rot(b, 8);  b += a;
			a -= c;  a ^= rot(c,16);  c += b;
			b -= a;  b ^= rot(a,19);  a += c;
			c -= b;  c ^= rot(b, 4);  b += a;
		}
		void final(a,b,c) noexcept
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
		int length = (key.size + 3) / 4;
		uint32_t const* kk = reinterpret_cast<uint32_t const*>( key);

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
			case 3 : c+=k[2];
			case 2 : b+=k[1];
			case 1 : a+=k[0];
				bank.final();
		}
		std::size_t rt = (std::size_t)bank.c << 32 + bank.b;
	}
};

}//namespace

class IdentMap
	:public std::pmr::unordered_map<IdentKeyEnvelop, int>
{
public:
	typedef std::pmr::unordered_map<IdentKeyEnvelop, int> ParentClass;
	typedef typename ParentClass::const_iterator const_iterator;

	explicit IdentMap( std::pmr::memory_resource* memrsc) :ParentClass( memrsc) {}
	IdentMap( const IdentMap& o) = default;
	IdentMap& operator=( const IdentMap& o) = default;
	IdentMap( IdentMap&& o) = default;
	IdentMap& operator=( IdentMap&& o) = default;
};

}//namespace

namespace std
{
	template<> struct hash<IdentKeyEnvelop>
	{
	public:
		hash<IdentKeyEnvelop>(){}
		std::size_t operator()( IdentKeyEnvelop const& key) const noexcept
		{
			return key.hash();
		}
	};
}


namespace mewa {

class IdentMap :public std::pmr::unordered_map<IdentKeyEnvelop, int>
{
public:
	typedef std::pmr::unordered_map<IdentKeyEnvelop, int> ParentClass;
	typedef typename ParentClass::const_iterator const_iterator;

	explicit IdentMap( std::pmr::memory_resource* memrsc) :ParentClass( memrsc) {}
	IdentMap( const IdentMap& o) = default;
	IdentMap& operator=( const IdentMap& o) = default;
	IdentMap( IdentMap&& o) = default;
	IdentMap& operator=( IdentMap&& o) = default;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

