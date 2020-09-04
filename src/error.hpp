/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Error structure
/// \file "error.hpp"
#ifndef _MEWA_ERROR_HPP_INCLUDED
#define _MEWA_ERROR_HPP_INCLUDED
#if __cplusplus >= 201103L
#include <utility>
#include <string>
#include <exception>
#include <cstdio>

namespace mewa {

class Error
	:public std::runtime_error
{
public:
	enum Code {
		Ok=0,
		LogicError=11,
		IllegalFirstCharacterInLexer=101,
		SyntaxErrorInLexer=102,
		ArrayBoundReadInLexer=103,
		InvalidRegexInLexer=104,

		BadCharacterInGrammarDef=201,
		ValueOutOfRangeInGrammarDef=202,
		UnexpectedEofInGrammarDef=203,
		UnexpectedTokenInGrammarDef=204,
		ExpectedRuleIdentifierInGrammarDef=205,
		ExpectedNumberInGrammarDef=206,

		PriorityDefNotForLexemsInGrammarDef=221,
		UnexpectedEndOfRuleInGrammarDef=222,

		CommandNumberOfArgumentsInGrammarDef=241,

		DefinedAsTerminalAndNonterminalInGrammarDef=301,
		UnresolvedIdentifierInGrammarDef=302,
		UnreachableNonTerminalInGrammarDef=303,
		StartSymbolReferencedInGrammarDef=304,
		StartSymbolDefinedTwiceInGrammarDef=305,
		EmptyGrammarDef=306,
		PriorityConflictInGrammarDef=307,
	};

public:
	Error( Code code_ = Ok, int line_=0)
                :std::runtime_error(map2string(code_,"",line_)),m_code(code_),m_param(),m_line(line_){}
	Error( Code code_, const std::string& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_param(param_),m_line(line_){}
	Error( Code code_, const std::string_view& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_param(std::string(param_)),m_line(line_){}
	Error( const Error& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(o.m_param),m_line(o.m_line){}
	Error( Error&& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(std::move(o.m_param)),m_line(o.m_line){}

	Code code() const			{return m_code;}
        const std::string& arg() const		{return m_param;}
        int line() const			{return m_line;}

private:
        static std::string map2string( Code code_, const std::string_view& param_, int line_)
	{
		char numbuf[ 128];
		if (line_ > 0)
		{
			std::snprintf( numbuf, sizeof(numbuf), param_.empty()?"%d at line %d":"%d at line %d ", (int)code_, line_);
		}
		else
		{
			std::snprintf( numbuf, sizeof(numbuf), param_.empty()?"%d":"%d ", (int)code_);
		}
                std::string rt(numbuf);
                rt.append( param_.data(), param_.size());
                return rt;
	}
private:
	Code m_code;
        std::string m_param;
	int m_line;
};

}//namespace
#else
#error Building mewa requires C++11
#endif
#endif

