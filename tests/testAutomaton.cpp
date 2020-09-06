/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) automaton test program
/// \file "testAutomaton.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "automaton.hpp"
#include "error.hpp"
#include "utilitiesForTests.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mewa;

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-v"))
			{
				verbose = true;
			}
			if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testAutomaton [-h][-v]" << std::endl;
			}
		}

		std::ostringstream outputstream;
		outputstream << std::endl;

		Automaton::DebugOutput debugout( outputstream);
		debugout.enable( Automaton::DebugOutput::All);
		Automaton automaton;
		std::vector<Error> warnings;

		std::string source{R"( // Example taken from "https://web.cs.dal.ca/~sjackson/lalr1.html"
	IDENT 	: "[a-z]" ;
	S 	→ N ;
	N 	→ V "=" E ;
	N 	→ E ;
	E 	→ V ;
	V 	→ IDENT ;
	V 	→ "*" E ;
)"};
		automaton.build( source, warnings, debugout);
		std::string output = outputstream.str();

		std::string expected{R"(
-- Lexems:
DEF IDENT [a-z] 0
KEYWORD =
KEYWORD *

-- Productions:
S = N
N = V "=" E
N = E
E = V
V = IDENT
V = "*" E

-- LR(0) States:
[1]
	S =  . N
	N =  . V "=" E
	N =  . E
	E =  . V
	V =  . IDENT
	V =  . "*" E
[2]
	S = N . 
[3]
	N = E . 
[4]
	N = V . "=" E
	E = V . 
[5]
	V = IDENT . 
[6]
	E =  . V
	V =  . IDENT
	V =  . "*" E
	V = "*" . E
[7]
	N = V "=" . E
	E =  . V
	V =  . IDENT
	V =  . "*" E
[8]
	V = "*" E . 
[9]
	E = V . 
[10]
	N = V "=" E . 

-- LALR(1) States (Merged LR(1) elements assigned to LR(0) states):
[1]
	S =  . N, $ -> GOTO 2
	N =  . V "=" E, IDENT -> GOTO 4
	N =  . V "=" E, * -> GOTO 4
	N =  . E, IDENT -> GOTO 3
	N =  . E, * -> GOTO 3
	E =  . V, IDENT -> GOTO 4
	E =  . V, * -> GOTO 4
	V =  . IDENT, IDENT -> SHIFT IDENT GOTO 5
	V =  . IDENT, * -> SHIFT IDENT GOTO 5
	V =  . "*" E, IDENT -> SHIFT "*" GOTO 6
	V =  . "*" E, * -> SHIFT "*" GOTO 6
[2]
	S = N . , $ -> ACCEPT
[3]
	N = E . , IDENT -> REDUCE N #1
	N = E . , * -> REDUCE N #1
[4]
	N = V . "=" E, IDENT -> SHIFT "=" GOTO 7
	N = V . "=" E, * -> SHIFT "=" GOTO 7
	E = V . , IDENT -> REDUCE E #1
	E = V . , * -> REDUCE E #1
[5]
	V = IDENT . , IDENT -> REDUCE V #1
	V = IDENT . , * -> REDUCE V #1
[6]
	E =  . V, IDENT -> GOTO 9
	E =  . V, * -> GOTO 9
	V =  . IDENT, IDENT -> SHIFT IDENT GOTO 5
	V =  . IDENT, * -> SHIFT IDENT GOTO 5
	V =  . "*" E, IDENT -> SHIFT "*" GOTO 6
	V =  . "*" E, * -> SHIFT "*" GOTO 6
	V = "*" . E, IDENT -> GOTO 8
	V = "*" . E, * -> GOTO 8
[7]
	N = V "=" . E, IDENT -> GOTO 10
	N = V "=" . E, * -> GOTO 10
	E =  . V, IDENT -> GOTO 9
	E =  . V, * -> GOTO 9
	V =  . IDENT, IDENT -> SHIFT IDENT GOTO 5
	V =  . IDENT, * -> SHIFT IDENT GOTO 5
	V =  . "*" E, IDENT -> SHIFT "*" GOTO 6
	V =  . "*" E, * -> SHIFT "*" GOTO 6
[8]
	V = "*" E . , IDENT -> REDUCE V #2
	V = "*" E . , * -> REDUCE V #2
[9]
	E = V . , IDENT -> REDUCE E #1
	E = V . , * -> REDUCE E #1
[10]
	N = V "=" E . , IDENT -> REDUCE N #3
	N = V "=" E . , * -> REDUCE N #3

)"};
		if (verbose)
		{
			std::cerr << output << std::endl;
		}
		if (output != expected)
		{
			writeFile( "build/testAutomaton.out", output);
			writeFile( "build/testAutomaton.exp", expected);
			std::cerr << "ERR test output (build/testAutomaton.out) differs expected build/testAutomaton.exp" << std::endl;
			return 3;
		}
		std::cerr << "OK" << std::endl;

		return 0;
	}
	catch (const mewa::Error& err)
	{
		if (err.line())
		{
			std::cerr << "ERR " << (int)err.code() << " " << err.arg() << " AT LINE " << err.line() << std::endl;
		}
		else
		{
			std::cerr << "ERR " << (int)err.code() << " " << err.arg() << std::endl;
		}
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

