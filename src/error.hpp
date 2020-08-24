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
                :std::runtime_error(map2string(code_,"")),m_code(code_),m_param(){}
	Error( Code code_, const std::string& param_)
                :std::runtime_error(map2string(code_,param_)),m_code(code_),m_param(param_){}
	Error( Code code_, const std::string_view& param_)
                :std::runtime_error(map2string(code_,param_)),m_code(code_),m_param(std::string(param_)){}
	Error( const Error& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(o.m_param){}
	Error( Error&& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(std::move(o.m_param)){}

	Code code() const			{return m_code;}
        const std::string& arg() const		{return m_param;}

private:
        static std::string map2string( Code code_, const std::string_view& param_)
	{
		char numbuf[ 64];
                std::snprintf( numbuf, sizeof(numbuf), param_.empty()?"%d":"%d ", (int)code_);
                std::string rt(numbuf);
                rt.append( param_.data(), param_.size());
                return rt;
	}
private:
	Code m_code;
        std::string m_param;
};

}//namespace
#else
#error Building mewa requires C++11
#endif
#endif

