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
		IllegalFirstCharacterInLexer=101,
		SyntaxErrorInLexer=102,
		ArrayBoundReadInLexer=103,
		InvalidRegexInLexer=104
	};

public:
	Error( Code code_ = Ok)
		:std::runtime_error(map2string(code_,"")),m_code(code_),m_arg(){}
	Error( Code code_, const std::string& arg_)
		:std::runtime_error(map2string(code_,arg_)),m_code(code_),m_arg(arg_){}
	Error( Code code_, const std::string_view& arg_)
		:std::runtime_error(map2string(code_,std::string(arg_))),m_code(code_),m_arg(arg_){}
	Error( const Error& o)
		:std::runtime_error(o),m_code(o.m_code),m_arg(o.m_arg){}
	Error( Error&& o)
		:std::runtime_error(o),m_code(o.m_code),m_arg(std::move(o.m_arg)){}

	Code code() const			{return m_code;}
	const std::string& arg() const		{return m_arg;}

private:
	static std::string map2string( Code code_, const std::string& arg_)
	{
		char numbuf[ 64];
		std::snprintf( numbuf, sizeof(numbuf), arg_.empty()?"%d":"%d ", (int)code_);
		return std::string(numbuf) + arg_;
	}
private:
	Code m_code;
	std::string m_arg;
};

}//namespace
#else
#error Building mewa requires C++11
#endif
#endif

