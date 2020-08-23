/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
static inline bool utf8mid( unsigned char ch)
{
	return (ch & B11000000) == B10000000;
}

} //namespace
#endif


