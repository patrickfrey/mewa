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
#include <utility>
#include <initializer_list>
#include <string>
#include <string_view>
#include <map>
#include <vector>
#include "scope.hpp"

namespace mewa {

class ProductionNode
{
public:
	enum Type
	{
		Unresolved,
		NonTerminal,
		Terminal
	};

public:
	ProductionNode( const std::string_view& name_)
		:m_name(std::string(name_)),m_type(Unresolved),m_index(0){}
	ProductionNode( const ProductionNode& o)
		:m_name(o.m_name),m_type(o.m_type),m_index(o.m_index){}
	ProductionNode& operator=( const ProductionNode& o)
		{m_name=o.m_name; m_type=o.m_type; m_index=o.m_index;
		 return *this;}
	ProductionNode( ProductionNode&& o)
		:m_name(std::move(o.m_name)),m_type(o.m_type),m_index(o.m_index){}
	ProductionNode& operator=( ProductionNode&& o)
		{m_name=std::move(o.m_name); m_type=o.m_type; m_index=o.m_index;
		 return *this;}

	const std::string& name() const			{return m_name;}
	Type type() const				{return m_type;}
	int index() const				{return m_index;}

	void defineAsTerminal( int index_)		{m_type = Terminal; m_index = index_;}
	void defineAsNonTerminal( int index_)		{m_type = NonTerminal; m_index = index_;}

private:
	std::string m_name;
	Type m_type;
	int m_index;
};

class Production
{
public:
	Production( const std::vector<std::string>& ar)
	{
		for (auto ai : ar) m_ar.push_back( ProductionNode( ai));
	}
	Production( const std::initializer_list<std::string>& ar)
	{
		for (auto ai : ar) m_ar.push_back( ProductionNode( ai));
	}
	Production( const Production& o)
		:m_ar(o.m_ar){}
	Production& operator=( const Production& o)
		{m_ar=o.m_ar; return *this;}
	Production( Production&& o)
		:m_ar(std::move(o.m_ar)){}
	Production& operator=( Production&& o)
		{m_ar=std::move(o.m_ar); return *this;}

private:
	std::vector<ProductionNode> m_ar;
};

class Grammar
{
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


