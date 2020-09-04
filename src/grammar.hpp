/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator and parser implementation interface
/// \file "grammar.hpp"
#ifndef _MEWA_AUTOMATON_HPP_INCLUDED
#define _MEWA_AUTOMATON_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <utility>
#include <string>
#include <map>
#include <vector>

namespace mewa {

class Automaton
{
public:
	class ActionKey
	{
	public:
		ActionKey( int state_, int terminal_)
			:m_state(state_),m_terminal(terminal_){}
		ActionKey( const ActionKey& o)
			:m_state(o.m_state),m_terminal(o.m_terminal){}

		int state() const		{return m_state;}
		int terminal() const		{return m_terminal;}

		bool operator < (const ActionKey& o) const
		{
			return m_state == o.m_state ? m_terminal < o.m_terminal : m_state < o.m_state;
		}

	private:
		int m_state;
		int m_terminal;
	};

	class Action
	{
	public:
		enum Type {Shift,Reduce};

		Action( Type type_, int value_)
			:m_type(type_),m_value(value_){}
		Action( const Action& o)
			:m_type(o.m_type),m_value(o.m_value){}

		Type type() const		{return m_type;}
		int value() const		{return m_value;}

	private:
		Type m_type;
		int m_value;
	};

	class Goto
	{
	public:
		Goto( int state_)
			:m_state(state_){}
		Goto( const Goto& o)
			:m_state(o.m_state){}

		int state() const		{return m_state;}

	private:
		int m_state;
	};

public:
	Automaton()
		:m_actions(),m_gotos(){}
	Automaton( const Automaton& o)
		:m_actions(o.m_actions),m_gotos(o.m_gotos){}
	Automaton& operator=( const Automaton& o)
		{m_actions=o.m_actions; m_gotos=o.m_gotos; return *this;}
	Automaton( Automaton&& o)
		:m_actions(std::move(o.m_actions)),m_gotos(std::move(o.m_gotos)){}
	Automaton& operator=( Automaton&& o)
		{m_actions=std::move(o.m_actions); m_gotos=std::move(o.m_gotos); return *this;}

	void build( const std::string& fileName, const std::string& source);

	const std::map<ActionKey,Action>& actions() const		{return m_actions;}
	const std::map<int,Goto>& gotos() const				{return m_gotos;}

private:
	std::map<ActionKey,Action> m_actions;
	std::map<int,Goto> m_gotos;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


