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
#include <cstdio>
#include <cstring>

using namespace mewa;

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

int main( int argc, const char* argv[] )
{
	try
	{
		Lexer lexer;
		lexer.defineLexem( "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		lexer.defineLexem( "CARDINAL", "[0-9]*");
		lexer.defineLexem( "FLOAT", "[0-9]*([.][0-9]*){0,1}[ ]*([Ee][+-]{0,1}[0-9]+){0,1}");
		lexer.defineLexem( "DQSTRING", "[\"]((([^\\\\\"\\n])|([\\\\][^\"\\n]))*)[\"]", 1);
		lexer.defineLexem( "SQSTRING", "[\']((([^\\\\\'\\n])|([\\\\][^\'\\n]))*)[\']", 1);
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
		lexer.defineLexem( "#include");
		lexer.defineEolnComment( "//");
		lexer.defineBracketComment( "/*", "*/");

		std::string source{R"(
#include <string>
#include <iostream>

/*
 *  Main program:
*/
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

		std::string expected{R"(
#include [#include]
< [<]
IDENT [string]
> [>]
#include [#include]
< [<]
IDENT [iostream]
> [>]
IDENT [int]
IDENT [main]
( [(]
) [)]
{ [{]
IDENT [std]
:: [::]
IDENT [string]
IDENT [s0]
{ [{]
SQSTRING [h]
, [,]
SQSTRING [e]
, [,]
SQSTRING [l]
, [,]
SQSTRING [l]
, [,]
SQSTRING [o]
, [,]
SQSTRING [\n]
, [,]
SQSTRING [n]
, [,]
SQSTRING [e]
, [,]
SQSTRING [w]
, [,]
SQSTRING [\n]
, [,]
SQSTRING [w]
, [,]
SQSTRING [o]
, [,]
SQSTRING [r]
, [,]
SQSTRING [l]
, [,]
SQSTRING [d]
, [,]
SQSTRING [\n]
} [}]
; [;]
IDENT [std]
:: [::]
IDENT [string]
IDENT [s1]
{ [{]
DQSTRING [hello\nnew\nworld\n]
} [}]
; [;]
IDENT [std]
:: [::]
IDENT [string]
IDENT [s2]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
DQSTRING [--------------]
<< [<<]
IDENT [std]
:: [::]
IDENT [endl]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
IDENT [s0]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
DQSTRING [--------------]
<< [<<]
IDENT [std]
:: [::]
IDENT [endl]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
IDENT [s1]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
DQSTRING [--------------]
<< [<<]
IDENT [std]
:: [::]
IDENT [endl]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
IDENT [s2]
; [;]
IDENT [std]
:: [::]
IDENT [cout]
<< [<<]
DQSTRING [--------------]
<< [<<]
IDENT [std]
:: [::]
IDENT [endl]
; [;]
IDENT [return]
CARDINAL [0]
; [;]
} [}]
	)"};
		std::ostringstream outputbuf;
		outputbuf << "\n";
		Scanner scanner( "testLexer", source);
		Lexem lexem = lexer.next( scanner);
		for (; !lexem.empty(); lexem = lexer.next( scanner))
		{
			outputbuf << lexem.name() << " [" << lexem.value() << "]" << "\n";
			std::cerr << lexem.name() << " [" << lexem.value() << "]" << std::endl;
			if (lexem.name() == "?")
			{
				break;
			}
		}
		std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testLexer.out", output);
			writeFile( "build/testLexer.exp", expected);
			std::cerr << "ERR test output (build/testLexer.out) differs expected build/testLexer.exp" << std::endl;
			return 3;
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


