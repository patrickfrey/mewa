/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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

namespace mewa {

struct Scope
{
public:
	Scope( int first_, int second_=-1)
		:first(first_ >= 0 ? first_ : 0),second(second_ >= 0 ? second_ : std::numeric_limits<int>::max()){}
	Scope( const Scope& o)
		:first(o.first),second(o.second){}
	Scope& operator=( const Scope& o)
		{first=o.first; second=o.second; return *this;}

	bool operator < (const Scope& o) const
	{
		return second == o.second
			? first < o.first
			: second < o.second;
	}

	bool contains( const Scope& o) const
	{
		return o.first >= first && o.second <= second;
	}

	bool contains( int timestmp) const
	{
		return timestmp >= first && timestmp < second;
	}

	std::string tostring() const
	{
		char buf[ 128];
		if (second == std::numeric_limits<int>::max())
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
	int first;
	int second;
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

	const KEYTYPE& key() const				{return m_key;}
	const Scope scope() const				{return m_scope;}

	bool matches( const ScopedKey<KEYTYPE>& o) const	{return m_key == o.m_key && m_scope.contains(o.m_scope);}

	bool operator < (const ScopedKey& o) const
	{
		return m_key == o.m_key
			? m_scope < o.m_scope
			: m_key < o.m_key;
	}

private:
	KEYTYPE m_key;
	Scope m_scope;
};

template <typename KEYTYPE, typename VALTYPE>
class ScopedMap
	:public std::map<ScopedKey<KEYTYPE>, VALTYPE>
{
public:
	typedef std::map<ScopedKey<KEYTYPE>, VALTYPE> ParentClass;
	typedef typename ParentClass::const_iterator const_iterator;

	ScopedMap() = default;
	ScopedMap( const ScopedMap& o) = default;
	ScopedMap& operator=( const ScopedMap& o) = default;
	ScopedMap( ScopedMap&& o) = default;
	ScopedMap& operator=( ScopedMap&& o) = default;

	std::pair<const_iterator,const_iterator>
		scoped_find_range( const ScopedKey<KEYTYPE>& key) const
	{
		std::pair<const_iterator,const_iterator> rt;
		const_iterator it = ParentClass::lower_bound( key);
		rt.first = it;
		for (; it != ParentClass::end() && it->first.matches( key); ++it){}
		rt.second = it;
		return rt;
	}

	const_iterator scoped_find_inner( const ScopedKey<KEYTYPE>& key) const
	{
		auto rt = ParentClass::end();
		auto it = ParentClass::lower_bound( key);
		if (it != this->end() && it->first.matches( key))
		{
			rt = it++;
			for (; it != this->end() && it->first.matches( key); ++it)
			{
				if (it->first.scope().contains( rt->first.scope()))
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

