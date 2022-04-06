/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief LALR(1) Parser generator interface
/// \file "automaton.hpp"
#ifndef _MEWA_AUTOMATON_HPP_INCLUDED
#define _MEWA_AUTOMATON_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "error.hpp"
#include "lexer.hpp"
#include "version.hpp"
#include <utility>
#include <string>
#include <map>
#include <vector>
#include <iostream>

namespace mewa {

class Automaton
{
public:
	/// \brief Here we declare some limits to be able to pack multiple values into one 32bit integer
	enum {
		ShiftActionType = 2,
			MaxActionType = 1<<ShiftActionType,
			MaskActionType = MaxActionType-1,
		ShiftState = 12,
			MaxState = 1<<ShiftState,
			MaskState = MaxState-1,
		ShiftProductionLength = 4,
			MaxProductionLength = 1<<ShiftProductionLength,
			MaskProductionLength = MaxProductionLength-1,
		ShiftNofProductions = 9,
			MaxNofProductions = 1<<ShiftNofProductions,
			MaskNofProductions = MaxNofProductions-1,
		ShiftNonterminal = 8,
			MaxNonterminal = 1<<ShiftNonterminal,
			MaskNonterminal = MaxNonterminal-1,
		ShiftTerminal = 10,
			MaxTerminal = 1<<ShiftTerminal,
			MaskTerminal = MaxTerminal-1,
		ShiftCall = 10,
			MaxCall = 1<<ShiftCall,
			MaskCall = MaxCall-1,
		ShiftPriority = 5,
			MaxPriority = 1<<ShiftPriority,
			MaskPriority = MaxPriority-1,
		ShiftAssoziativity = 2,
			MaxAssoziativity = 1<<ShiftAssoziativity,
			MaskAssoziativity = MaxAssoziativity-1,
		ShiftScopeFlag = 2,
			MaxScopeFlag = 1<<ShiftScopeFlag,
			MaskScopeFlag = MaxScopeFlag-1,
		ShiftPriorityWithAssoziativity = ShiftPriority+ShiftAssoziativity,
			MaxPriorityWithAssoziativity = 1<<ShiftPriorityWithAssoziativity,
			MaskPriorityWithAssoziativity = MaxPriorityWithAssoziativity-1
	};

public:
	/// \brief Debug output partially enabled by flags
	class DebugOutput
	{
	public:
		DebugOutput( std::ostream& out_ = std::cerr) noexcept
			:m_enabledMask(0),m_out(out_){}
		DebugOutput( const DebugOutput& o) noexcept
			:m_enabledMask(o.m_enabledMask),m_out(o.m_out){}

		enum Type {None=0x0, Productions=0x1, Lexems=0x2, Nonterminals=0x4, States=0x8, FunctionCalls=0x10, StateTransitions=0x20, All=0xFF};

		DebugOutput& enable( Type type_) noexcept	{m_enabledMask |= (int)(type_); return *this;}
		bool enabled( Type type_) const noexcept	{return (m_enabledMask & (int)(type_)) == (int)(type_);}
		bool enabled() const noexcept			{return m_enabledMask != 0;}

		std::ostream& out() const noexcept		{return m_out;}

	private:
		int m_enabledMask;
		std::ostream& m_out;
	};

	/// \brief Key in the LALR(1) action table (state,terminal -> action)
	class ActionKey
	{
	public:
		ActionKey( int state_, int terminal_)
			:m_state(state_),m_terminal(terminal_){}
		ActionKey( const ActionKey& o)
			:m_state(o.m_state),m_terminal(o.m_terminal){}

		int state() const noexcept				{return m_state;}
		int terminal() const noexcept				{return m_terminal;}

		bool operator < (const ActionKey& o) const noexcept	{return m_state == o.m_state ? m_terminal < o.m_terminal : m_state < o.m_state;}
		bool operator == (const ActionKey& o) const noexcept	{return m_state == o.m_state && m_terminal == o.m_terminal;}
		bool operator != (const ActionKey& o) const noexcept	{return m_state != o.m_state || m_terminal != o.m_terminal;}

		int packed() const noexcept
		{
			return ((int)m_state << ShiftTerminal) | (int)m_terminal;
		}
		static ActionKey unpack( int pkg) noexcept
		{
			static_assert (ShiftState + ShiftTerminal <= 31, "sizeof packed action key structure");

			return ActionKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*terminal*/);
		}

	private:
		short m_state;
		short m_terminal;
	};

	/// \brief Action in the LALR(1) action table (state/terminal -> action)
	class Action
	{
	public:
		enum Type 	:char {Shift,Reduce,Accept};
		enum ScopeFlag	:char {NoScope,Step,Scope};

		Action() noexcept
			:m_type(Shift),m_scopeflag(NoScope),m_value(0),m_call(0),m_count(0){}
		Action( Type type_, ScopeFlag scopeflag_, int value_, int call_, int count_) noexcept
			:m_type(type_),m_scopeflag(scopeflag_),m_value(value_),m_call(call_),m_count(count_){}
		Action( const Action& o) noexcept
			:m_type(o.m_type),m_scopeflag(o.m_scopeflag),m_value(o.m_value),m_call(o.m_call),m_count(o.m_count){}

		static const char* scopeFlagName( ScopeFlag scopeflag_) noexcept
			{static const char* ar[3] = {"","step","scope"}; return ar[ scopeflag_];}

		Type type() const noexcept		{return m_type;}
		ScopeFlag scopeflag() const noexcept	{return m_scopeflag;}
		int state() const noexcept		{return m_value;}
		int nonterminal() const noexcept	{return m_value;}
		int call() const noexcept		{return m_call;}
		int count() const noexcept		{return m_count;}

		bool operator == (const Action& o) const noexcept	{return m_type == o.m_type && m_scopeflag == o.m_scopeflag
										&& m_value == o.m_value && m_call == o.m_call && m_count == o.m_count;}
		bool operator != (const Action& o) const noexcept	{return m_type != o.m_type || m_scopeflag != o.m_scopeflag
										|| m_value != o.m_value || m_call != o.m_call || m_count != o.m_count;}
		int packed() const noexcept
		{
			return ((int)m_type << (ShiftProductionLength + ShiftCall + ShiftState + ShiftScopeFlag))
				| ((int)m_scopeflag << (ShiftProductionLength + ShiftCall + ShiftState))
				| ((int)m_value << (ShiftProductionLength + ShiftCall))
				| ((int)m_call << ShiftProductionLength)
				| (int)m_count;
		}
		static Action unpack( int pkg) noexcept
		{
			static_assert (ShiftProductionLength + ShiftCall + ShiftState + ShiftScopeFlag + ShiftActionType <= 31, "sizeof packed action structure");

			return Action( (Type)((pkg >> (ShiftProductionLength + ShiftCall + ShiftState + ShiftScopeFlag)) & MaskActionType)/*type*/,
					(ScopeFlag)((pkg >> (ShiftProductionLength + ShiftCall + ShiftState)) & MaskScopeFlag)/*scopeflag*/,
					(pkg >> (ShiftProductionLength + ShiftCall)) & MaskState/*value*/,
					(pkg >> ShiftProductionLength) & MaskCall/*call*/,
					(pkg) & MaskProductionLength/*count*/);
		}

		static Action accept( int nonterminal_, ScopeFlag scopeflag_, int call_, int count_) noexcept
			{return Action( Accept, scopeflag_, nonterminal_, call_, count_);}
		static Action shift( int follow_state) noexcept
			{return Action( Shift, NoScope/*scopeflag*/, follow_state, 0/*call*/, 0/*count*/);}
		static Action reduce( int nonterminal_, ScopeFlag scopeflag_, int call_, int count_) noexcept
			{return Action( Reduce, scopeflag_, nonterminal_, call_, count_);}

	private:
		Type m_type;							//< Type of action
		ScopeFlag m_scopeflag;						//< Scope/Step flag
		short m_value;							//< Follow state (SHIFT), Nonterminal (REDUCE)
		short m_call;							//< Index of function to call (REDUCE)
		short m_count;							//< Number of elements on stack (right hand of production) to replace (REDUCE)
	};

	/// \brief Key in the LALR(1) goto table (state/nonterminal -> state)
	class GotoKey
	{
	public:
		GotoKey( int state_, int nonterminal_) noexcept
			:m_state(state_),m_nonterminal(nonterminal_){}
		GotoKey( const GotoKey& o) noexcept
			:m_state(o.m_state),m_nonterminal(o.m_nonterminal){}

		int state() const noexcept				{return m_state;}
		int nonterminal() const noexcept			{return m_nonterminal;}

		bool operator < (const GotoKey& o) const noexcept	{return m_state == o.m_state ? m_nonterminal < o.m_nonterminal : m_state < o.m_state;}
		bool operator == (const GotoKey& o) const noexcept	{return m_state == o.m_state && m_nonterminal == o.m_nonterminal;}
		bool operator != (const GotoKey& o) const noexcept	{return m_state != o.m_state || m_nonterminal != o.m_nonterminal;}

		int packed() const noexcept
		{
			return ((int)m_state << ShiftTerminal) | (int)m_nonterminal;
		}
		static GotoKey unpack( int pkg) noexcept
		{
			static_assert (ShiftState + ShiftTerminal <= 31, "sizeof packed goto-key structure");

			return GotoKey( pkg >> ShiftTerminal /*state*/, pkg & MaskTerminal /*nonterminal*/);
		}

	private:
		short m_state;
		short m_nonterminal;
	};

	/// \brief Value in the LALR(1) goto table (state/nonterminal -> state)
	class Goto
	{
	public:
		Goto() noexcept
			:m_state(0){}
		explicit Goto( int state_) noexcept
			:m_state(state_){}
		Goto( const Goto& o) noexcept
			:m_state(o.m_state){}

		int state() const noexcept				{return m_state;}

		bool operator == (const Goto& o) const noexcept 	{return m_state == o.m_state;}
		bool operator != (const Goto& o) const noexcept 	{return m_state != o.m_state;}

		int packed() const noexcept
		{
			return (int)m_state;
		}
		static Goto unpack( int pkg) noexcept
		{
			static_assert (ShiftState <= 31, "sizeof packed goto structure");

			return Goto( pkg);
		}

	private:
		short m_state;
	};

	/// \brief Encoded node call reference attached to a production to be performed after its reduction
	class Call
	{
	public:
		enum ArgumentType
		{
			NoArg,
			StringArg,
			ReferenceArg
		};

		Call() noexcept
			:m_function(),m_arg(),m_argtype(NoArg){}
		Call( const std::string& function_, const std::string& arg_, ArgumentType argtype_)
			:m_function(function_),m_arg(arg_),m_argtype(argtype_){}
		Call( std::string&& function_, std::string&& arg_, ArgumentType argtype_) noexcept
			:m_function(std::move(function_)),m_arg(std::move(arg_)),m_argtype(argtype_){}
		Call( const Call& o)
			:m_function(o.m_function),m_arg(o.m_arg),m_argtype(o.m_argtype){}
		Call& operator=( const Call& o)
			{m_function=o.m_function; m_arg=o.m_arg; m_argtype=o.m_argtype; return *this;}
		Call( Call&& o) noexcept
			:m_function(std::move(o.m_function)),m_arg(std::move(o.m_arg)),m_argtype(o.m_argtype){}
		Call& operator=( Call&& o) noexcept
			{m_function=std::move(o.m_function); m_arg=std::move(o.m_arg); m_argtype=o.m_argtype; return *this;}

		const std::string& function() const noexcept			{return m_function;}
		const std::string& arg() const noexcept				{return m_arg;}
		ArgumentType argtype() const noexcept				{return m_argtype;}

		bool operator < (const Call& o) const noexcept
		{
			return m_function == o.m_function
					? m_arg == o.m_arg
						? m_argtype < o.m_argtype
						: m_arg < o.m_arg
					: m_function < o.m_function;
		}

		std::string tostring() const;

	private:
		std::string m_function;
		std::string m_arg;
		ArgumentType m_argtype;
	};

public:
	Automaton()
		:m_version(MEWA_VERSION_NUMBER),m_language(),m_typesystem(),m_cmdline(),m_lexer(),m_actions(),m_gotos(),m_calls(),m_nonterminals(){}
	Automaton( const Automaton& o)
		:m_version(o.m_version),m_language(o.m_language),m_typesystem(o.m_typesystem),m_cmdline(o.m_cmdline)
		,m_lexer(o.m_lexer),m_actions(o.m_actions),m_gotos(o.m_gotos)
		,m_calls(o.m_calls),m_nonterminals(o.m_nonterminals){}
	Automaton& operator=( const Automaton& o)
		{m_version=o.m_version; m_language=o.m_language; m_typesystem=o.m_typesystem; m_cmdline=o.m_cmdline;
		 m_lexer=o.m_lexer; m_actions=o.m_actions; m_gotos=o.m_gotos;
		 m_calls=o.m_calls; m_nonterminals=o.m_nonterminals; return *this;}
	Automaton( Automaton&& o) noexcept
		:m_version(o.m_version),m_language(std::move(o.m_language)),m_typesystem(std::move(o.m_typesystem)),m_cmdline(std::move(o.m_cmdline))
		,m_lexer(std::move(o.m_lexer)),m_actions(std::move(o.m_actions)),m_gotos(std::move(o.m_gotos))
		,m_calls(std::move(o.m_calls)),m_nonterminals(std::move(o.m_nonterminals)){}
	Automaton& operator=( Automaton&& o) noexcept
		{m_version=o.m_version; m_language=std::move(o.m_language); m_typesystem=std::move(o.m_typesystem); m_cmdline=std::move(o.m_cmdline);
		m_lexer=std::move(o.m_lexer); m_actions=std::move(o.m_actions); m_gotos=std::move(o.m_gotos);
		m_calls=std::move(o.m_calls); m_nonterminals=std::move(o.m_nonterminals); return *this;}
	Automaton( int version_, const std::string& language_, const std::string& typesystem_, const std::string& cmdline_,
			const Lexer& lexer_, const std::map<ActionKey,Action>& actions_, const std::map<GotoKey,Goto>& gotos_,
			const std::vector<Call>& calls_, const std::vector<std::string>& nonterminals_)
		:m_version(version_),m_language(language_),m_typesystem(typesystem_),m_cmdline(cmdline_)
		,m_lexer(lexer_),m_actions(actions_),m_gotos(gotos_)
		,m_calls(calls_),m_nonterminals(nonterminals_){}
	Automaton( int version_, std::string&& language_, std::string&& typesystem_, std::string&& cmdline_,
			Lexer&& lexer_, std::map<ActionKey,Action>&& actions_, std::map<GotoKey,Goto>&& gotos_,
			std::vector<Call>&& calls_, std::vector<std::string>&& nonterminals_) noexcept
		:m_version(version_),m_language(std::move(language_)),m_typesystem(std::move(typesystem_)),m_cmdline(std::move(cmdline_))
		,m_lexer(std::move(lexer_)),m_actions(std::move(actions_)),m_gotos(std::move(gotos_))
		,m_calls(std::move(calls_)),m_nonterminals(std::move(nonterminals_)){}

	void build( const std::string& source, std::vector<Error>& warnings, DebugOutput dbgout = DebugOutput());

	const std::string& language() const noexcept				{return m_language;}
	const std::string& typesystem() const noexcept				{return m_typesystem;}
	const std::string& cmdline() const noexcept				{return m_cmdline;}
	const Lexer& lexer() const noexcept					{return m_lexer;}
	const std::map<ActionKey,Action>& actions() const noexcept		{return m_actions;}
	const std::map<GotoKey,Goto>& gotos() const noexcept			{return m_gotos;}
	const Call& call( int callidx) const					{return m_calls[ callidx-1];}
	const std::vector<Call>& calls() const noexcept				{return m_calls;}

	const std::string& nonterminal( int nonterminalidx) const		{return m_nonterminals[ nonterminalidx-1];}
	const std::vector<std::string>& nonterminals() const noexcept		{return m_nonterminals;}
	std::string tostring() const;
	std::string actionString( const Action& action) const;

private:
	int m_version;
	std::string m_language;
	std::string m_typesystem;
	std::string m_cmdline;
	Lexer m_lexer;
	std::map<ActionKey,Action> m_actions;
	std::map<GotoKey,Goto> m_gotos;
	std::vector<Call> m_calls;
	std::vector<std::string> m_nonterminals;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


