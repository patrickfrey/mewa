/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief LALR(1) automaton test program
/// \file "testAutomaton.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "automaton.hpp"
#include "error.hpp"
#include "fileio.hpp"
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
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testAutomaton [-h][-V]" << std::endl;
			}
		}
		PseudoRandom random;

		// [1] Test Automaton building
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
		if (!warnings.empty())
		{
			outputstream << "-- Warnings:" << std::endl;
			for (auto warning : warnings)
			{
				outputstream << warning.what() << std::endl;
			}
			outputstream << std::endl;
		}
		std::string output = outputstream.str();

		std::string expected{R"(
-- Lexems:
TOKEN IDENT [a-z] 0
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
	N =  . V "=" E, $ -> GOTO 4
	N =  . E, $ -> GOTO 3
	E =  . V, $ -> GOTO 4
	V =  . IDENT, $ -> SHIFT IDENT GOTO 5
	V =  . IDENT, = -> SHIFT IDENT GOTO 5
	V =  . "*" E, $ -> SHIFT "*" GOTO 6
	V =  . "*" E, = -> SHIFT "*" GOTO 6
[2]
	S = N . , $ -> ACCEPT
[3]
	N = E . , $ -> REDUCE N #1
[4]
	N = V . "=" E, $ -> SHIFT "=" GOTO 7
	E = V . , $ -> REDUCE E #1
[5]
	V = IDENT . , $ -> REDUCE V #1
	V = IDENT . , = -> REDUCE V #1
[6]
	E =  . V, $ -> GOTO 9
	E =  . V, = -> GOTO 9
	V =  . IDENT, $ -> SHIFT IDENT GOTO 5
	V =  . IDENT, = -> SHIFT IDENT GOTO 5
	V =  . "*" E, $ -> SHIFT "*" GOTO 6
	V =  . "*" E, = -> SHIFT "*" GOTO 6
	V = "*" . E, $ -> GOTO 8
	V = "*" . E, = -> GOTO 8
[7]
	N = V "=" . E, $ -> GOTO 10
	E =  . V, $ -> GOTO 9
	V =  . IDENT, $ -> SHIFT IDENT GOTO 5
	V =  . "*" E, $ -> SHIFT "*" GOTO 6
[8]
	V = "*" E . , $ -> REDUCE V #2
	V = "*" E . , = -> REDUCE V #2
[9]
	E = V . , $ -> REDUCE E #1
	E = V . , = -> REDUCE E #1
[10]
	N = V "=" E . , $ -> REDUCE N #3

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

		// [2] Test packing
		for (int ii=0; ii<1000; ++ii)
		{
			Automaton::ActionKey actionKey( 
							random.get( 0, Automaton::MaxState)/*state*/,
							random.get( 0, Automaton::MaxTerminal)/*terminal*/);
			Automaton::Action::Type atype = (Automaton::Action::Type)random.get( 0, 3);
			Automaton::Action action( atype,
							random.get( 0, Automaton::MaxState)/*value*/,
							random.get( 0, Automaton::MaxCall)/*call*/,
							random.get( 0, Automaton::MaxProductionLength)/*count*/);
			Automaton::GotoKey gtoKey( 
							random.get( 0, Automaton::MaxState)/*state*/,
							random.get( 0, Automaton::MaxNonterminal)/*nonterminal*/);
			Automaton::Goto gto( 
							random.get( 0, Automaton::MaxState)/*state*/);

			Automaton::ActionKey actionKey2 = Automaton::ActionKey::unpack( actionKey.packed());
			Automaton::Action action2 = Automaton::Action::unpack( action.packed());
			Automaton::GotoKey gtoKey2 = Automaton::GotoKey::unpack( gtoKey.packed());
			Automaton::Goto gto2 = Automaton::Goto::unpack( gto.packed());

			if (actionKey2 != actionKey)
			{
				throw std::runtime_error( "packing/unpacking of action key failed");
			}
			if (action2 != action)
			{
				throw std::runtime_error( "packing/unpacking of action structure failed");
			}
			if (gtoKey2 != gtoKey)
			{
				throw std::runtime_error( "packing/unpacking of goto key failed");
			}
			if (gto2 != gto)
			{
				throw std::runtime_error( "packing/unpacking of goto structure failed");
			}
		}
		std::cerr << "OK" << std::endl;

		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << err.what() << std::endl;
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

