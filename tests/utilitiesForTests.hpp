/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some utilities for the mewa test programs
/// \file "utilitiesForTests.hpp"
#ifndef _MEWA_UTILITIES_FOR_TESTS_HPP_INCLUDED
#define _MEWA_UTILITIES_FOR_TESTS_HPP_INCLUDED

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include <string>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <ctime>
#include <cstdint>

class PseudoRandom
{
public:
	PseudoRandom()
	{
		init( generateSeed());
	}
	PseudoRandom( unsigned int seed_)
	{
		init( seed_);
	}

	int seed() const
	{
		return m_seed;
	}

	int get( int min_, int max_)
	{
		if (min_ >= max_)
		{
			if (min_ == max_) return min_;
			std::swap( min_, max_);
		}
		m_value = uint32_hash( m_value + 1 + m_incr++);
		unsigned int iv = max_ - min_;
		return (int)(m_value % iv) + min_;
	}

private:
	enum {KnuthIntegerHashFactor=2654435761U};

	void init( unsigned int seed_)
	{
		m_seed = seed_;
		m_value = uint32_hash( seed_);
		m_incr = m_value * KnuthIntegerHashFactor;
	}

	static uint32_t uint32_hash( uint32_t a)
	{
		a += ~(a << 15);
		a ^=  (a >> 10);
		a +=  (a << 3);
		a ^=  (a >> 6);
		a += ~(a << 11);
		a ^=  (a >> 16);
		return a;
	}

	static unsigned int generateSeed()
	{
		time_t nowtime;
		struct tm* now;

		::time( &nowtime);
		now = ::localtime( &nowtime);

		return (now->tm_year+1900)*10000 + (now->tm_mon+1)*100 + now->tm_mday;
	}

	unsigned int m_seed;
	unsigned int m_value;
	unsigned int m_incr;
};

#endif

