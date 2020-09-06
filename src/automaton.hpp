/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief LALR(1) Parser generator interface
/// \file "automaton.hpp"
#ifndef _MEWA_AUTOMATON_HPP_INCLUDED
#define _MEWA_AUTOMATON_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "error.hpp"
#include <utility>
#include <string>
#include <map>
#include <vector>
#include <iostream>

namespace mewa {

class Automaton
{
public:
	enum {
		ShiftState = 15, 
			MaxState = 1<<ShiftState,
			MaskState = MaxState-1,
		ShiftProductionLength = 5,
			MaxProductionLength = 1<<ShiftProductionLength,
			MaskProductionLength = MaxProductionLength-1,
		ShiftNonterminal = 10,
			MaxNonterminal = 1<<ShiftNonterminal,
			MaskNonterminal = MaxNonterminal-1,
		ShiftTerminal = 10,
			MaxTerminal = 1<<ShiftTerminal,
			MaskTerminal = MaxTerminal-1
	};

public:
	class DebugOutput
	{
	public:
		DebugOutput( std::ostream& out_ = std::cerr) 
			:m_out(out_){}
		DebugOutput( const DebugOutput& o) 
			:m_enabledMask(o.m_enabledMask),m_out(o.m_out){}

		enum Type {None=0, Productions=1, Lexems=2, States=4, All=0xFF};

		DebugOutput& enable( Type type_)	{m_enabledMask |= (int)(type_); return *this;}
		bool enabled( Type type_) const		{return (m_enabledMask & (int)(type_)) == (int)(type_);}

		std::ostream& out() const		{return m_out;}

	private:
		int m_enabledMask;
		std::ostream& m_out;
	};

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

		int packed() const
		{
			return (m_state << ShiftTerminal) | m_terminal;
		}
		static ActionKey unpack( int pkg)
		{
			return ActionKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*terminal*/);
		}

	private:
		int m_state;
		int m_terminal;
	};

	class Action
	{
	public:
		enum Type {Shift,Reduce,Accept};

		Action()
			:m_type(Shift),m_value(0),m_count(0){}
		Action( Type type_, int value_, int count_=0)
			:m_type(type_),m_value(value_),m_count(count_){}
		Action( const Action& o)
			:m_type(o.m_type),m_value(o.m_value),m_count(o.m_count){}

		Type type() const		{return m_type;}
		int value() const		{return m_value;}
		int count() const		{return m_count;}

		int packed() const
		{
			return (m_type << (ShiftState + ShiftProductionLength)) | (m_value << ShiftProductionLength) | m_count;
		}
		static Action unpack( int pkg)
		{
			return Action( (Type)(pkg >> (ShiftState + ShiftProductionLength))/*type*/,
					(pkg >> ShiftProductionLength) & MaskState/*value*/,
					(pkg) & MaskProductionLength/*count*/);
		}

	private:
		Type m_type;
		int m_value;
		int m_count;
	};

	class GotoKey
	{
	public:
		GotoKey( int state_, int nonterminal_)
			:m_state(state_),m_nonterminal(nonterminal_){}
		GotoKey( const GotoKey& o)
			:m_state(o.m_state),m_nonterminal(o.m_nonterminal){}

		int state() const		{return m_state;}
		int nonterminal() const		{return m_nonterminal;}

		bool operator < (const GotoKey& o) const
		{
			return m_state == o.m_state ? m_nonterminal < o.m_nonterminal : m_state < o.m_state;
		}
		int packed() const
		{
			return (m_state << ShiftTerminal) | m_nonterminal;
		}
		static GotoKey unpack( int pkg)
		{
			return GotoKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*nonterminal*/);
		}

	private:
		int m_state;
		int m_nonterminal;
	};

	class Goto
	{
	public:
		Goto()
			:m_state(0){}
		explicit Goto( int state_)
			:m_state(state_){}
		Goto( const Goto& o)
			:m_state(o.m_state){}

		int state() const		{return m_state;}

		int packed() const
		{
			return m_state;
		}
		static Goto unpack( int pkg)
		{
			return Goto( pkg);
		}

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

	void build( const std::string& source, std::vector<Error>& warnings, DebugOutput dbgout = DebugOutput());

	const std::map<ActionKey,Action>& actions() const		{return m_actions;}
	const std::map<GotoKey,Goto>& gotos() const				{return m_gotos;}

private:
	std::map<ActionKey,Action> m_actions;
	std::map<GotoKey,Goto> m_gotos;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


