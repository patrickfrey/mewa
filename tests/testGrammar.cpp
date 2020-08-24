/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) automaton test program
/// \file "testGrammar.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "grammar.hpp"
#include "scope.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mewa;

int main( int argc, const char* argv[] )
{
	try
	{
		std::ostringstream output;
		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << (int)err.code() << " " << err.arg() << std::endl;
		return (int)err.code();
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERR runtime " << err.what() << std::endl;
		return 1;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "ERR out of memory" << std::endl;
		return 2;
	}
	return 0;
}

