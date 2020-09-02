/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator and parser implementation
/// \file "grammar.cpp"
#include "grammar.hpp"
#include "lexer.hpp"

using namespace mewa;

class GrammarLexer
	:public Lexer
{
public:
	emum LexemType
	{
		NONE=0,
		IDENT=1,
		NUMBER,
		DQSTRING,
		SQSTRING,
		SLASH,
		EQUAL,
		DDOT
	};

	GrammarLexer()
	{
		defineLexem( "IDENT", "[a-zA-Z_][a-zA-Z_0-9]*");
		defineLexem( "NUMBER", "[0-9]+");
		defineLexem( "DQSTRING", "[\"]((([^\\\\\"\\n])|([\\\\][^\"\\n]))*)[\"]", 1);
		defineLexem( "SQSTRING", "[\']((([^\\\\\'\\n])|([\\\\][^\'\\n]))*)[\']", 1);
		defineLexem( "/");
		defineLexem( "=");
		defineLexem( ":");
	}
};






