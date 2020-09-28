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
	typedef int Step;

public:
	Scope( Step start_, Step end_)
		:m_start(start_ >= 0 ? start_ : 0),m_end(end_ >= 0 ? end_ : std::numeric_limits<Step>::max()){}
	Scope( const Scope& o)
		:m_start(o.m_start),m_end(o.m_end){}
	Scope& operator=( const Scope& o)
		{m_start=o.m_start; m_end=o.m_end; return *this;}

	bool contains( const Scope& o) const noexcept
	{
		return o.m_start >= m_start && o.m_end <= m_end;
	}

	bool contains( Step step) const noexcept
	{
		return step >= m_start && step < m_end;
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

	bool matches( const KEYTYPE& key_, Scope::Step step) const noexcept		{return m_key == key_ && m_scope.contains( step);}

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
			? a.scope().end() == b.scope().end()
				? a.scope().start() < b.scope().start()
				: a.scope().end() < b.scope().end()
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

	const_iterator scoped_find( const KEYTYPE& key, const Scope::Step step) const noexcept
	{
		auto rt = ParentClass::end();
		auto it = ParentClass::upper_bound( ScopedKey<KEYTYPE>( key, Scope( step, step)));
		for (; it != this->end() && it->first.key() == key && it->first.scope().end() > step; ++it)
		{
			if (it->first.scope().start() <= step)
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

#if 0
template <typename KEYTYPE, typename VALTYPE>
class ScopedRelationMap
{
public:
	class ResultElement
	{
	public:
		ResultElement( const Scope scope_, const KEYTYPE type_, const VALTYPE value_)
			:m_scope(scope_),m_type(type_),m_value(value_){}
		ResultElement( const ResultElement& o)
			:m_scope(o.m_scope),m_type(o.m_type),m_value(o.m_value){}

		Scope scope() const noexcept		{return m_scope;}
		KEYTYPE type() const noexcept		{return m_type;}
		VALTYPE value() const noexcept		{return m_value;}

	private:
		Scope m_scope;
		KEYTYPE m_type;
		VALTYPE m_value;
	};

	typedef std::pmr::multimap<ScopedKey<KEYTYPE>, std::pair<KEYTYPE,VALTYPE>, ScopedMapOrder<KEYTYPE> > ScopedRelMapType;

	explicit ScopedRelationMap( std::pmr::memory_resource* memrsc)
		:m_scoped_map( memrsc) {}
	ScopedRelationMap( const ScopedRelationMap& o) = default;
	ScopedRelationMap& operator=( const ScopedRelationMap& o) = default;
	ScopedRelationMap( ScopedRelationMap&& o) = default;
	ScopedRelationMap& operator=( ScopedRelationMap&& o) = default;

	std::pmr::vector<ResultElement> scoped_find( const KEYTYPE& key, const Scope::Step step, std::pmr::memory_resource* res_memrsc) const noexcept
	{
		std::pmr::vector<ResultElement> rt( res_memrsc);
		!!!!
		return rt;
	}
private:
	ScopedRelMapType m_scoped_map;
};
#endif

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

