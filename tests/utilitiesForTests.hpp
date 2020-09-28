/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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


struct ArgParser
{
	static bool isDigit( char ch) noexcept
	{
		return ch >= '0' && ch <= '9';
	}

	static int getCardinalNumberArgument( const char* arg, const char* what)
	{
		char const* ai = arg;
		int rt = 0;
		for (; isDigit(*ai); ++ai)
		{
			rt = rt * 10 + (*ai - '0');
			if (rt < 0) throw std::runtime_error( mewa::string_format( "value %s out of range", what));
		}
		if (*ai || rt == 0)
		{
			throw std::runtime_error( mewa::string_format( "value %s is not a positive number", what));
		}
		return rt;
	}
};

#endif

