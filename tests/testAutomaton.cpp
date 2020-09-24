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
#include "automaton_structs.hpp"
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
TOKEN IDENT [a-z] 0 [abcdefghijklmnopqrstuvwxyz]  ~ 1
KEYWORD = [=] ~ 2
KEYWORD * [*] ~ 3

-- Nonterminals:
(1) S
(2) N
(3) E
(4) V

-- Productions:
S = N
N = V "=" E
N = E
E = V
V = IDENT
V = "*" E

-- LR(0) states:
[1]
	S = . N
	N = . V "=" E
	N = . E
	E = . V
	V = . IDENT
	V = . "*" E
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
	E = . V
	V = . IDENT
	V = . "*" E
	V = "*" . E
[7]
	N = V "=" . E
	E = . V
	V = . IDENT
	V = . "*" E
[8]
	V = "*" E .
[9]
	E = V .
[10]
	N = V "=" E .

-- LR(0) state cores (for calculation of SHIFT follow state):
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
	V = "*" . E
[7]
	N = V "=" . E
[8]
	V = "*" E .
[9]
	E = V .
[10]
	N = V "=" E .

-- LR(1) used FOLLOW sets labeled:
[0]: {$}
[3]: {$ '='}

-- LALR(1) states (merged LR(1) elements assigned to LR(0) states):
[1]
	S = . N, FOLLOW [0] -> GOTO 2
	N = . E, FOLLOW [0] -> GOTO 3
	N = . V "=" E, FOLLOW [0] -> GOTO 4
	E = . V, FOLLOW [0] -> GOTO 4
	V = . IDENT, FOLLOW [3] -> SHIFT IDENT GOTO 5
	V = . "*" E, FOLLOW [3] -> SHIFT '*' GOTO 6
[2]
	S = N . -> ACCEPT
	S = N ., FOLLOW [0] -> REDUCE S #1
[3]
	N = E ., FOLLOW [0] -> REDUCE N #1
[4]
	N = V . "=" E, FOLLOW [0] -> SHIFT '=' GOTO 7
	E = V ., FOLLOW [0] -> REDUCE E #1
[5]
	V = IDENT ., FOLLOW [3] -> REDUCE V #1
[6]
	V = "*" . E, FOLLOW [3] -> GOTO 8
	E = . V, FOLLOW [3] -> GOTO 9
	V = . IDENT, FOLLOW [3] -> SHIFT IDENT GOTO 5
	V = . "*" E, FOLLOW [3] -> SHIFT '*' GOTO 6
[7]
	N = V "=" . E, FOLLOW [0] -> GOTO 10
	E = . V, FOLLOW [0] -> GOTO 9
	V = . IDENT, FOLLOW [0] -> SHIFT IDENT GOTO 5
	V = . "*" E, FOLLOW [0] -> SHIFT '*' GOTO 6
[8]
	V = "*" E ., FOLLOW [3] -> REDUCE V #2
[9]
	E = V ., FOLLOW [3] -> REDUCE E #1
[10]
	N = V "=" E ., FOLLOW [0] -> REDUCE N #3

-- Action table:
[1]
	IDENT => Shift goto 5
	'*' => Shift goto 6
[2]
	$ => Accept
[3]
	$ => Reduce #1 N
[4]
	$ => Reduce #1 E
	'=' => Shift goto 7
[5]
	$ => Reduce #1 V
	'=' => Reduce #1 V
[6]
	IDENT => Shift goto 5
	'*' => Shift goto 6
[7]
	IDENT => Shift goto 5
	'*' => Shift goto 6
[8]
	$ => Reduce #2 V
	'=' => Reduce #2 V
[9]
	$ => Reduce #1 E
	'=' => Reduce #1 E
[10]
	$ => Reduce #3 N

-- Goto table:
[1]
	N => 2
	E => 3
	V => 4
[6]
	E => 8
	V => 9
[7]
	E => 10
	V => 9

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
		else
		{
			removeFile( "build/testAutomaton.out");
			removeFile( "build/testAutomaton.exp");
		}

		// [2] Test packing
		for (int ii=0; ii<100000; ++ii)
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
			TransitionItem titm( 
					random.get( 0, Automaton::MaxNofProductions)/*prodindex*/,
					random.get( 0, Automaton::MaxProductionLength)/*prodpos*/,
					random.get( 0, Automaton::MaxTerminal)/*follow*/);

			Automaton::ActionKey actionKey2 = Automaton::ActionKey::unpack( actionKey.packed());
			Automaton::Action action2 = Automaton::Action::unpack( action.packed());
			Automaton::GotoKey gtoKey2 = Automaton::GotoKey::unpack( gtoKey.packed());
			Automaton::Goto gto2 = Automaton::Goto::unpack( gto.packed());
			TransitionItem titm2 = TransitionItem::unpack( titm.packed());

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
			if (titm != titm2)
			{
				throw std::runtime_error( "packing/unpacking of state transition item failed");
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

