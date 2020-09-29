/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \file utf8.hpp
/// \brief Helpers for UTF-8 encoding/decoding
#ifndef _MEWA_UTF8_INCLUDED
#define _MEWA_UTF8_INCLUDED

namespace mewa
{
enum {
	B11111111=0xFF,
	B01111111=0x7F,
	B00111111=0x3F,
	B00011111=0x1F,
	B00001111=0x0F,
	B00000111=0x07,
	B00000011=0x03,
	B00000001=0x01,
	B00000000=0x00,
	B10000000=0x80,
	B11000000=0xC0,
	B11100000=0xE0,
	B11110000=0xF0,
	B11111000=0xF8,
	B11111100=0xFC,
	B11111110=0xFE,

	B11011111=B11000000|B00011111,
	B11101111=B11100000|B00001111,
	B11110111=B11110000|B00000111,
	B11111011=B11111000|B00000011,
	B11111101=B11111100|B00000001
};

/// \brief Return true, if the character passed as argument is a non start character of a multi byte encoded unicode character
static inline bool utf8mid( unsigned char ch) noexcept
{
	return (ch & B11000000) == B10000000;
}

static inline int bitScanReverse( const unsigned char& idx) noexcept
{
	uint32_t xx = idx;
	if (!xx) return 0;
#ifdef __x86_64__
	uint32_t result; 
	asm(" bsr %1, %0 \n" : "=r"(result) : "r"(xx) ); 
	return result+1;
#else
	int ee = 1;
	if ((xx & 0xF0))   { ee += 4; xx >>= 4; }
	if ((xx & 0x0C))   { ee += 2; xx >>= 2; }
	if ((xx & 0x02))   { ee += 1; }
	return ee;
#endif
}

static inline int utf8charlen( unsigned char ch) noexcept
{
	unsigned char cl = 9-bitScanReverse( (ch^0xFFU));
	return cl>2?(cl-1):1;
}

} //namespace
#endif


