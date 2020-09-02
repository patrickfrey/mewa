/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator and parser implementation interface
/// \file "grammar.hpp"
#ifndef _MEWA_GRAMMAR_HPP_INCLUDED
#define _MEWA_GRAMMAR_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "scope.hpp"
#include "lexer.hpp"
#include <utility>
#include <string>
#include <string_view>
#include <map>
#include <vector>

namespace mewa {

class Production
{
public:
	Production( const std::vector<int>& ar_)
		:m_ar(ar_){}
	Production( const Production& o)
		:m_ar(o.m_ar){}
	Production& operator=( const Production& o)
		{m_ar=o.m_ar; return *this;}
	Production( Production&& o)
		:m_ar(std::move(o.m_ar)){}
	Production& operator=( Production&& o)
		{m_ar=std::move(o.m_ar); return *this;}
private:
	std::vector<int> m_ar;
};

class Grammar
{
public:
	Grammar( const Lexer& lexer_)
		:m_lexer(lexer_),m_productions(){}
	Grammar( Lexer&& lexer_)
		:m_lexer(std::move(lexer_)),m_productions(){}
	Grammar( const Grammar& o)
		:m_lexer(o.m_lexer),m_productions(o.m_productions){}
	Grammar& operator=( const Grammar& o)
		{m_lexer=o.m_lexer; m_productions=o.m_productions; return *this;}
	Grammar( Grammar&& o)
		:m_lexer(std::move(o.m_lexer)),m_productions(std::move(o.m_productions)){}
	Grammar& operator=( Grammar&& o)
		{m_lexer=std::move(o.m_lexer); m_productions=std::move(o.m_productions); return *this;}

	const Lexer& lexer() const		{return m_lexer;}

private:
	Lexer m_lexer;
	std::vector<Production> m_productions;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


