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
			MaskTerminal = MaxTerminal-1,
		ShiftCall = 10,
			MaxCall = 1<<ShiftCall,
			MaskCall = MaxCall-1
	};

public:
	class DebugOutput
	{
	public:
		DebugOutput( std::ostream& out_ = std::cerr) 
			:m_out(out_){}
		DebugOutput( const DebugOutput& o) 
			:m_enabledMask(o.m_enabledMask),m_out(o.m_out){}

		enum Type {None=0, Productions=1, Lexems=2, States=4, FunctionCalls=8, All=0xFF};

		DebugOutput& enable( Type type_)	{m_enabledMask |= (int)(type_); return *this;}
		bool enabled( Type type_) const		{return (m_enabledMask & (int)(type_)) == (int)(type_);}
		bool enabled() const			{return m_enabledMask != 0;}

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

		bool operator < (const ActionKey& o) const	{return m_state == o.m_state ? m_terminal < o.m_terminal : m_state < o.m_state;}
		bool operator == (const ActionKey& o) const	{return m_state == o.m_state && m_terminal == o.m_terminal;}
		bool operator != (const ActionKey& o) const	{return m_state != o.m_state || m_terminal != o.m_terminal;}

		int packed() const
		{
			return ((int)m_state << ShiftTerminal) | (int)m_terminal;
		}
		static ActionKey unpack( int pkg)
		{
			return ActionKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*terminal*/);
		}

	private:
		short m_state;
		short m_terminal;
	};

	class Action
	{
	public:
		enum Type :short {Shift,Reduce,Accept};

		Action()
			:m_type(Shift),m_value(0),m_call(0),m_count(0){}
		Action( Type type_, int value_, int call_=0, int count_=0)
			:m_type(type_),m_value(value_),m_call(call_),m_count(count_){}
		Action( const Action& o)
			:m_type(o.m_type),m_value(o.m_value),m_call(o.m_call),m_count(o.m_count){}

		Type type() const		{return m_type;}
		int value() const		{return m_value;}
		int call() const		{return m_call;}
		int count() const		{return m_count;}

		bool operator == (const Action& o) const	{return m_type == o.m_type && m_value == o.m_value && m_call == o.m_call && m_count == o.m_count;}
		bool operator != (const Action& o) const	{return m_type != o.m_type || m_value != o.m_value || m_call != o.m_call || m_count != o.m_count;}

		int packed() const
		{
			return ((int)m_type << (ShiftState + ShiftProductionLength + ShiftCall))
				| ((int)m_value << (ShiftProductionLength + ShiftCall))
				| ((int)m_call << ShiftProductionLength)
				| (int)m_count;
		}
		static Action unpack( int pkg)
		{
			return Action( (Type)((pkg >> (ShiftState + ShiftProductionLength + ShiftCall)) & 3)/*type*/,
					(pkg >> (ShiftProductionLength + ShiftCall)) & MaskState/*value*/,
				        (pkg >> ShiftProductionLength) & MaskCall/*call*/,
					(pkg) & MaskProductionLength/*count*/);
		}

	private:
		Type m_type;
		short m_value;
		short m_call;
		short m_count;
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

		bool operator < (const GotoKey& o) const	{return m_state == o.m_state ? m_nonterminal < o.m_nonterminal : m_state < o.m_state;}
		bool operator == (const GotoKey& o) const	{return m_state == o.m_state && m_nonterminal == o.m_nonterminal;}
		bool operator != (const GotoKey& o) const	{return m_state != o.m_state || m_nonterminal != o.m_nonterminal;}

		int packed() const
		{
			return ((int)m_state << ShiftTerminal) | (int)m_nonterminal;
		}
		static GotoKey unpack( int pkg)
		{
			return GotoKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*nonterminal*/);
		}

	private:
		short m_state;
		short m_nonterminal;
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

		int state() const				{return m_state;}

		bool operator == (const Goto& o) const 	{return m_state == o.m_state;}
		bool operator != (const Goto& o) const 	{return m_state != o.m_state;}

		int packed() const
		{
			return (int)m_state;
		}
		static Goto unpack( int pkg)
		{
			return Goto( pkg);
		}

	private:
		short m_state;
	};

	class Call
	{
	public:
		enum ArgumentType
		{
			NoArg,
			StringArg,
			ReferenceArg
		};

		Call()
			:m_function(),m_arg(),m_argtype(NoArg){}
		Call( const std::string& function_, const std::string& arg_, ArgumentType argtype_)
			:m_function(function_),m_arg(arg_),m_argtype(argtype_){}
		Call( std::string&& function_, std::string&& arg_, ArgumentType argtype_)
			:m_function(std::move(function_)),m_arg(std::move(arg_)),m_argtype(argtype_){}
		Call( const Call& o)
			:m_function(o.m_function),m_arg(o.m_arg),m_argtype(o.m_argtype){}
		Call& operator=( const Call& o)
			{m_function=o.m_function; m_arg=o.m_arg; m_argtype=o.m_argtype; return *this;}
		Call( Call&& o)
			:m_function(std::move(o.m_function)),m_arg(std::move(o.m_arg)),m_argtype(o.m_argtype){}
		Call& operator=( Call&& o)
			{m_function=std::move(o.m_function); m_arg=std::move(o.m_arg); m_argtype=o.m_argtype; return *this;}

		const std::string& function() const			{return m_function;}
		const std::string& arg() const				{return m_arg;}
		ArgumentType argtype() const				{return m_argtype;}

		bool operator < (const Call& o) const
		{
			return m_function == o.m_function
					? m_arg == o.m_arg
						? m_argtype < o.m_argtype
						: m_arg < o.m_arg
					: m_function < o.m_function;
		}

	private:
		std::string m_function;
		std::string m_arg;
		ArgumentType m_argtype;
	};

public:
	Automaton()
		:m_language(),m_actions(),m_gotos(),m_calls(){}
	Automaton( const Automaton& o)
		:m_language(o.m_language),m_actions(o.m_actions),m_gotos(o.m_gotos),m_calls(o.m_calls){}
	Automaton& operator=( const Automaton& o)
		{m_language=o.m_language; m_actions=o.m_actions; m_gotos=o.m_gotos; m_calls=o.m_calls; return *this;}
	Automaton( Automaton&& o)
		:m_language(std::move(o.m_language)),m_actions(std::move(o.m_actions)),m_gotos(std::move(o.m_gotos)),m_calls(std::move(o.m_calls)){}
	Automaton& operator=( Automaton&& o)
		{m_language=std::move(o.m_language); m_actions=std::move(o.m_actions); m_gotos=std::move(o.m_gotos); m_calls=std::move(o.m_calls); return *this;}

	void build( const std::string& source, std::vector<Error>& warnings, DebugOutput dbgout = DebugOutput());

	const std::string& language() const				{return m_language;}
	const std::map<ActionKey,Action>& actions() const		{return m_actions;}
	const std::map<GotoKey,Goto>& gotos() const			{return m_gotos;}
	const Call call( int callidx) const				{return m_calls[ callidx-1];}
	const std::vector<Call>& calls() const				{return m_calls;}

private:
	std::string m_language;
	std::map<ActionKey,Action> m_actions;
	std::map<GotoKey,Goto> m_gotos;
	std::vector<Call> m_calls;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


