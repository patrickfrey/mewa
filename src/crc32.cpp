/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Functions for calculating CRC32 value of a string (used for hashing)
/// \file "crc32.cpp"
#include "crc32.hpp"

using namespace mewa;

class Crc32Table
{
public:
	Crc32Table()
	{
		for (unsigned int ii = 0; ii <= 0xFF; ii++)
		{
			enum {Polynomial=0xEDB88320U};
			uint32_t crc = ii;
			for (unsigned int jj = 0; jj < 8; jj++)
			{
				crc = (crc >> 1) ^ (-int(crc & 1) & Polynomial);
			}
			m_ar[ ii] = crc;
		}
	}
	inline uint32_t operator[]( unsigned char idx) const
	{
		return m_ar[ idx];
	}

private:
	uint32_t m_ar[ 0x100];
};

static const Crc32Table g_crc32Lookup;

///\note CRC32 Standard implementation from blog post http://create.stephan-brumme.com/crc32/#sarwate, Thanks
static uint32_t crc32_standardImplementation_1byte( const void* data, std::size_t length, uint32_t previousCrc32)
{
	uint32_t crc = ~previousCrc32;
	unsigned char const* current = (unsigned char const*) data;
	while (length--)
	{
		crc = (crc >> 8) ^ g_crc32Lookup[(crc & 0xFF) ^ *current++];
	}
	return ~crc;
}

uint32_t Crc32::calc( const char* blk, std::size_t blksize)
{
	return crc32_standardImplementation_1byte( blk, blksize, 0);
}


