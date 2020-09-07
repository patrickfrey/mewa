/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for reading and writing files
/// \file "fileio.hpp"
#ifndef _MEWA_FILEIO_HPP_INCLUDED
#define _MEWA_FILEIO_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <string>

namespace mewa {

void writeFile( const std::string& filename, const std::string& content);
std::string readFile( const std::string& filename);

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

