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
	ScopedKey( ScopedKey&& o) noexcept
		:m_key(std::move(o.m_key)),m_scope(o.m_scope){}
	ScopedKey& operator=( ScopedKey&& o) noexcept
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
	bool operator()( const ScopedKey<KEYTYPE>& a, const ScopedKey<KEYTYPE>& b) const noexcept
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

	explicit ScopedMap( std::pmr::memory_resource* memrsc) noexcept :ParentClass( memrsc) {}
	ScopedMap( const ScopedMap& o) = default;
	ScopedMap& operator=( const ScopedMap& o) = default;
	ScopedMap( ScopedMap&& o) noexcept = default;
	ScopedMap& operator=( ScopedMap&& o) noexcept = default;

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


template <typename RELNODETYPE, typename VALTYPE>
class ScopedRelationMap
	:public std::pmr::multimap<ScopedKey<RELNODETYPE>, std::pair<RELNODETYPE,VALTYPE>, ScopedMapOrder<RELNODETYPE> >
{
public:
	typedef std::pmr::multimap<ScopedKey<RELNODETYPE>, std::pair<RELNODETYPE,VALTYPE>, ScopedMapOrder<RELNODETYPE> > ParentClass;

	class ResultElement
	{
	public:
		ResultElement( const RELNODETYPE type_, const VALTYPE value_)
			:m_type(type_),m_value(value_){}
		ResultElement( const ResultElement& o)
			:m_type(o.m_type),m_value(o.m_value){}

		RELNODETYPE type() const noexcept	{return m_type;}
		VALTYPE value() const noexcept		{return m_value;}

	private:
		RELNODETYPE m_type;
		VALTYPE m_value;
	};

	explicit ScopedRelationMap( std::pmr::memory_resource* memrsc) noexcept :ParentClass( memrsc) {}
	ScopedRelationMap( const ScopedRelationMap& o) = default;
	ScopedRelationMap& operator=( const ScopedRelationMap& o) = default;
	ScopedRelationMap( ScopedRelationMap&& o) noexcept = default;
	ScopedRelationMap& operator=( ScopedRelationMap&& o) noexcept = default;

	std::pmr::vector<ResultElement> scoped_find( const RELNODETYPE& key, const Scope::Step step, std::pmr::memory_resource* res_memrsc) const noexcept
	{
		int local_membuffer[ 512];
		std::pmr::monotonic_buffer_resource local_memrsc( local_membuffer, sizeof local_membuffer);
		std::pmr::map<RELNODETYPE,int> candidatemap( &local_memrsc);

		std::pmr::vector<ResultElement> rt( res_memrsc);
		std::pmr::vector<Scope> scopear( res_memrsc);

		auto it = ParentClass::upper_bound( ScopedKey<RELNODETYPE>( key, Scope( step, step)));
		for (; it != this->end() && it->first.key() == key && it->first.scope().end() > step; ++it)
		{
			if (it->first.scope().start() <= step)
			{
				auto ins = candidatemap.insert( {it->second.first, rt.size()});
				if (ins.second /*insert took place*/)
				{
					scopear.push_back( it->first.scope());
					rt.push_back( ResultElement( it->second.first, it->second.second));
				}
				else if (scopear[ ins.first->second].contains( it->first.scope()))
				{
					scopear[ ins.first->second] = it->first.scope();
					rt[ ins.first->second] = ResultElement( it->second.first, it->second.second);
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

