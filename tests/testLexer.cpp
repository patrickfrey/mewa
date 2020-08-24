/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Lexer test program
/// \file "testLexer.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "lexer.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>
#include <string>

using namespace mewa;

int main( int argc, const char* argv[] )
{
	try
	{
		Lexer lexer;
		lexer.defineLexem( "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		lexer.defineLexem( "DQSTRING", "[\"](([^\"])|([\\\\][^\"]))[\"]", 1);
		lexer.defineLexem( "SQSTRING", "[\'](([^\'])|([\\\\][^\']))[\']", 1);
		lexer.defineLexem( "::");
		lexer.defineLexem( "<<");
		lexer.defineLexem( ">>");
		lexer.defineLexem( "<");
		lexer.defineLexem( ">");
		lexer.defineLexem( ">=");
		lexer.defineLexem( "<=");
		lexer.defineLexem( "==");
		lexer.defineLexem( "!=");
		lexer.defineLexem( "=");
		lexer.defineLexem( "{");
		lexer.defineLexem( "}");
		lexer.defineLexem( "(");
		lexer.defineLexem( ")");
		lexer.defineLexem( ";");
		lexer.defineLexem( ",");
		lexer.defineLexem( "/");
		lexer.defineLexem( "/=");
		lexer.defineLexem( "%");
		lexer.defineLexem( "%=");
		lexer.defineLexem( "+");
		lexer.defineLexem( "+=");
		lexer.defineLexem( "++");
		lexer.defineLexem( "-");
		lexer.defineLexem( "--");
		lexer.defineLexem( "-=");
		lexer.defineEolnComment( "//");
		lexer.defineBracketComment( "/*", "*/");
	
		std::string source{R"(
#include <string>
#include <iostream>

int main() {
    std::string s0{'h','e','l','l','o','\n','n','e','w','\n','w','o','r','l','d','\n'};
    std::string s1{"hello\nnew\nworld\n"};
    std::string s2 // Yes...
    std::cout << "--------------" << std::endl;
    std::cout << s0;
    std::cout << "--------------" << std::endl;
    std::cout << s1;
    std::cout << "--------------" << std::endl;
    std::cout << s2;
    std::cout << "--------------" << std::endl;
    return 0;
}
	)"};
		std::ostringstream output;
	
		Scanner scanner( "testLexer", source);
		Lexem lexem = lexer.next( scanner);
		while (!lexem.empty())
		{
			lexem = lexer.next( scanner);
			output << lexem.name() << " [" << lexem.value() << "]" << std::endl;
			std::cerr << lexem.name() << " [" << lexem.value() << "]" << std::endl;
			if (lexem.name() == "?") break;
		}
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


