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

static void writeFile( const std::string& filename, const std::string& content)
{
	FILE* ff = std::fopen( filename.c_str(), "w");
	if (!ff) throw std::runtime_error( std::strerror( errno));
	if (content.size() != std::fwrite( content.c_str(), 1, content.size(), ff))
	{
		std::fclose( ff);
		throw std::runtime_error( std::strerror( std::ferror( ff)));
	}
	std::fclose( ff);
}

#endif

