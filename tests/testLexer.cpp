/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Lexer test program
/// \file "testLexer.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "lexer.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include <iostream>
#include <sstream>
#include <string>

bool g_verbose = false;
using namespace mewa;

void testActivation( const char* rxstr, const char* expected)
{
	LexemDef lxdef( 0/*no line*/, ""/*name*/, rxstr, 0/*id*/, false/*keyword*/, 0/*select*/);
	std::string output = lxdef.activation();
	if (g_verbose)
	{
		std::cerr << "TEST LEXEM " << rxstr << " ACTIVATION " << output << std::endl;
	}
	if (output != expected)
	{
		std::cerr << "EXPECTED " << expected << std::endl;
		throw std::runtime_error( "lexem activation not as expected");
	}
}

void runLexer( const char* name, const Lexer& lexer, const std::string& source, const std::string& expected)
{
	std::ostringstream outputbuf;
	outputbuf << "\n";
	Scanner scanner( source);
	Lexem lexem = lexer.next( scanner);
	for (; !lexem.empty(); lexem = lexer.next( scanner))
	{
		outputbuf << lexem.name() << " [" << lexem.value() << "]" << "\n";
		if (g_verbose) std::cerr << lexem.name() << " [" << lexem.value() << "]" << std::endl;
		if (lexem.name() == "?")
		{
			break;
		}
	}
	std::string output = outputbuf.str();
	if (output != expected)
	{
		writeFile( std::string("build/testLexer.") + name + ".out", output);
		writeFile( std::string("build/testLexer.") + name + ".exp", expected);
		std::cerr << "ERR test output (build/testLexer." << name << ".out) differs expected build/testLexer." << name << ".exp" << std::endl;
		throw std::runtime_error("test failed");
	}
	else
	{
		removeFile( std::string("build/testLexer.") + name + ".out");
		removeFile( std::string("build/testLexer.") + name + ".exp");
	}
}

void testLexer1()
{
	Lexer lexer;
	lexer.defineLexem( 0/*no line*/, "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
	lexer.defineLexem( 0/*no line*/, "UINT", "[0-9]*");
	lexer.defineLexem( 0/*no line*/, "FLOAT", "[0-9]*([.][0-9]*){0,1}[ ]*([Ee][+-]{0,1}[0-9]+){0,1}");
	lexer.defineLexem( 0/*no line*/, "DQSTRING", "[\"]((([^\\\\\"\\n]+)|([\\\\][^\"\\n]))*)[\"]", 1);
	lexer.defineLexem( 0/*no line*/, "SQSTRING", "[\']((([^\\\\\'\\n]+)|([\\\\][^\'\\n]))*)[\']", 1);
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
	lexer.defineEolnComment( 0/*no line*/, "//");
	lexer.defineBracketComment( 0/*no line*/, "/*", "*/");

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
UINT [0]
; [;]
} [}]
)"};
	runLexer( "1", lexer, source, expected);
}


void testLexer2()
{
	Lexer lexer;
	lexer.defineLexem( 0/*no line*/, "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
	lexer.defineLexem( 0/*no line*/, "UINT", "[0-9]*");
	lexer.defineLexem( 0/*no line*/, "FLOAT", "[0-9]*([.][0-9]*){0,1}[ ]*([Ee][+-]{0,1}[0-9]+){0,1}");
	lexer.defineLexem( 0/*no line*/, "DQSTRING", "[\"]((([^\\\\\"\\n]+)|([\\\\][^\"\\n]))*)[\"]", 1);
	lexer.defineLexem( 0/*no line*/, "SQSTRING", "[\']((([^\\\\\'\\n]+)|([\\\\][^\'\\n]))*)[\']", 1);
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
	lexer.defineEolnComment( 0/*no line*/, "//");
	lexer.defineBracketComment( 0/*no line*/, "/*", "*/");
	lexer.defineIndentLexems( 0/*no line*/, "open_ind", "close_ind", "nl_ind", 4);

	std::string source{R"(
//This is a program
fn bla( a, b)
    if (a > b)
        return 1
    else
        let z = a - b
        return z
        // Return z

//This is a program
fn main( a, b)
    if (bla( a, b) > 67)
        return 1
    else
        let z = a - b
        return z
        // Return z
)"};

        std::string expected{R"(
"nl_ind" []
IDENT [fn]
IDENT [bla]
( [(]
IDENT [a]
, [,]
IDENT [b]
) [)]
"open_ind" []
"nl_ind" []
IDENT [if]
( [(]
IDENT [a]
> [>]
IDENT [b]
) [)]
"open_ind" []
"nl_ind" []
IDENT [return]
UINT [1]
"close_ind" []
"nl_ind" []
IDENT [else]
"open_ind" []
"nl_ind" []
IDENT [let]
IDENT [z]
= [=]
IDENT [a]
- [-]
IDENT [b]
"nl_ind" []
IDENT [return]
IDENT [z]
"close_ind" []
"close_ind" []
"nl_ind" []
IDENT [fn]
IDENT [main]
( [(]
IDENT [a]
, [,]
IDENT [b]
) [)]
"open_ind" []
"nl_ind" []
IDENT [if]
( [(]
IDENT [bla]
( [(]
IDENT [a]
, [,]
IDENT [b]
) [)]
> [>]
UINT [67]
) [)]
"open_ind" []
"nl_ind" []
IDENT [return]
UINT [1]
"close_ind" []
"nl_ind" []
IDENT [else]
"open_ind" []
"nl_ind" []
IDENT [let]
IDENT [z]
= [=]
IDENT [a]
- [-]
IDENT [b]
"nl_ind" []
IDENT [return]
IDENT [z]
"close_ind" []
"close_ind" []
)"};
	runLexer( "2", lexer, source, expected);
}


int main( int argc, const char* argv[] )
{
	try
	{
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				g_verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testLexer [-h][-V]" << std::endl;
				break;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testLexer [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testLexer [-h][-V]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}
		testActivation( "0123456789", "0");
		testActivation( "[0123456789]", "0123456789");
		testActivation( "[0-9]", "0123456789");
		testActivation( "[0-9]*([.][0-9]*){0,1}", "0123456789.");
		testActivation( "[0-9]*([.][0-9]*){0,1}[ ]*([Ee][+-]{0,1}[0-9]+){0,1}", "0123456789. Ee");
		testActivation( "[-]{0,1}[0-9]*([.][0-9]*){0,1}[ ]*([Ee][+-]{0,1}[0-9]+){0,1}", "-0123456789. Ee");
		testActivation( "((true)|(false))", "tf");
		testActivation( "[a-zA-Z_]+[a-zA-Z_0-9]*", "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");
		testActivation( "[\"]((([^\\\"\n]+)|([\\][^\"\n]))*)[\"]", "\"");
		testActivation( "[']((([^\\'\n]+)|([\\][^'\n]))*)[']", "'");

		testLexer1();
		testLexer2();
		std::cerr << "OK" << std::endl;
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


