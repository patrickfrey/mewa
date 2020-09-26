/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Structure describing the scope of a definition as pair of numbers
/// \file "scope.hpp"
#ifndef _MEWA_SCOPE_HPP_INCLUDED
#define _MEWA_SCOPE_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <utility>
#include <limits>
#include <cstdio>
#include <string>
#include <map>
#include <memory_resource>

namespace mewa {

struct Scope
{
public:
	typedef int Timestmp;

public:
	Scope( Timestmp first_, Timestmp second_)
		:first(first_ >= 0 ? first_ : 0),second(second_ >= 0 ? second_ : std::numeric_limits<Timestmp>::max()){}
	Scope( const Scope& o)
		:first(o.first),second(o.second){}
	Scope& operator=( const Scope& o)
		{first=o.first; second=o.second; return *this;}

	bool contains( const Scope& o) const noexcept
	{
		return o.first >= first && o.second <= second;
	}

	bool contains( Timestmp timestmp) const noexcept
	{
		return timestmp >= first && timestmp < second;
	}

	std::string tostring() const
	{
		char buf[ 128];
		if (second == std::numeric_limits<Timestmp>::max())
		{
			std::snprintf( buf, sizeof(buf), "[%d,INF]", first);
		}
		else
		{
			std::snprintf( buf, sizeof(buf), "[%d,%d]", first, second);
		}
		return std::string( buf);
	}

public:
	Timestmp first;
	Timestmp second;
};

template <typename KEYTYPE>
class ScopedKey
{
public:
	ScopedKey( const KEYTYPE& key_, const Scope scope_)
		:m_key(key_),m_scope(scope_){}
	ScopedKey( const ScopedKey& o)
		:m_key(o.m_key),m_scope(o.m_scope){}
	ScopedKey& operator=( const ScopedKey& o)
		{m_key=o.m_key; m_scope=o.m_scope; return *this;}
	ScopedKey( ScopedKey&& o)
		:m_key(std::move(o.m_key)),m_scope(o.m_scope){}
	ScopedKey& operator=( ScopedKey&& o)
		{m_key=std::move(o.m_key); m_scope=o.m_scope; return *this;}

	const KEYTYPE& key() const noexcept						{return m_key;}
	const Scope scope() const noexcept						{return m_scope;}

	bool matches( const KEYTYPE& key_, Scope::Timestmp timestmp) const noexcept	{return m_key == key_ && m_scope.contains( timestmp);}

private:
	KEYTYPE m_key;
	Scope m_scope;
};

template <typename KEYTYPE>
struct ScopedMapOrder
{
	bool operator()( const ScopedKey<KEYTYPE>& a, const ScopedKey<KEYTYPE>& b) const
	{
		return a.key() == b.key()
			? a.scope().second == b.scope().second
				? a.scope().first < b.scope().first
				: a.scope().second < b.scope().second
			: a.key() < b.key();
	}
};

template <typename KEYTYPE, typename VALTYPE>
class ScopedMap
	:public std::pmr::map<ScopedKey<KEYTYPE>, VALTYPE, ScopedMapOrder<KEYTYPE> >
{
public:
	typedef std::pmr::map<ScopedKey<KEYTYPE>, VALTYPE, ScopedMapOrder<KEYTYPE> > ParentClass;
	typedef typename ParentClass::const_iterator const_iterator;

	explicit ScopedMap( std::pmr::memory_resource* memrsc) :ParentClass( memrsc) {}
	ScopedMap( const ScopedMap& o) = default;
	ScopedMap& operator=( const ScopedMap& o) = default;
	ScopedMap( ScopedMap&& o) = default;
	ScopedMap& operator=( ScopedMap&& o) = default;

	const_iterator scoped_find( const KEYTYPE& key, const Scope::Timestmp timestmp) const noexcept
	{
		auto rt = ParentClass::end();
		auto it = ParentClass::lower_bound( ScopedKey<KEYTYPE>( key, Scope( 0, timestmp+1)));
		for (; it != this->end() && it->first.key() == key && it->first.scope().second > timestmp; ++it)
		{
			if (it->first.scope().first <= timestmp)
			{
				if (rt == ParentClass::end() || rt->first.scope().contains( it->first.scope()))
				{
					rt = it;
				}
			}
		}
		return rt;
	}
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

