/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Version of mewa
/// \file version.hpp
#ifndef _MEWA_VERSION_HPP_INCLUDED
#define _MEWA_VERSION_HPP_INCLUDED

namespace mewa
{

#define MEWA_MAJOR_VERSION 0
#define MEWA_MINOR_VERSION 1
#define MEWA_PATCH_VERSION 0
#define MEWA_VERSION_NUMBER (MEWA_MAJOR_VERSION * 1000000 + MEWA_MINOR_VERSION * 10000 + MEWA_PATCH_VERSION)

#define MEWA_VERSION_STRING  "0.1.0"

}//namespace
#endif
