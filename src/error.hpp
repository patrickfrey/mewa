/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Error structure
/// \file "error.hpp"
#ifndef _MEWA_ERROR_HPP_INCLUDED
#define _MEWA_ERROR_HPP_INCLUDED
#if __cplusplus >= 201103L
#include <utility>
#include <string>
#include <stdexcept>
#include <exception>
#include <cstdio>
#include <cstring>

namespace mewa {

class Error
	:public std::runtime_error
{
public:
	enum Code {
		Ok=0,
		LogicError=401,
		FileReadError=402,
		IllegalFirstCharacterInLexer=501,
		SyntaxErrorInLexer=502,
		ArrayBoundReadInLexer=503,
		InvalidRegexInLexer=504,

		BadCharacterInGrammarDef=601,
		ValueOutOfRangeInGrammarDef=602,
		UnexpectedEofInGrammarDef=603,
		UnexpectedTokenInGrammarDef=604,
		ExpectedPatternInGrammarDef=605,

		PriorityDefNotForLexemsInGrammarDef=621,
		UnexpectedEndOfRuleInGrammarDef=631,
		EscapeQuoteErrorInString=632,

		CommandNumberOfArgumentsInGrammarDef=641,
		CommandNameUnknownInGrammarDef=642,

		DefinedAsTerminalAndNonterminalInGrammarDef=651,
		UnresolvedIdentifierInGrammarDef=652,
		UnreachableNonTerminalInGrammarDef=653,
		StartSymbolReferencedInGrammarDef=654,
		StartSymbolDefinedTwiceInGrammarDef=655,
		EmptyGrammarDef=656,
		PriorityConflictInGrammarDef=657,
		NoAcceptStatesInGrammarDef=658,

		ShiftReduceConflictInGrammarDef=661,
		ReduceReduceConflictInGrammarDef=662,
		ShiftShiftConflictInGrammarDef=663,

		ComplexityMaxStateInGrammarDef=671,
		ComplexityMaxProductionLengthInGrammarDef=672,
		ComplexityMaxNonterminalInGrammarDef=673,
		ComplexityMaxTerminalInGrammarDef=674,

		BadKeyInGeneratedLuaTable=701,
		BadValueInGeneratedLuaTable=702,
	};

public:
	Error( Code code_ = Ok, int line_=0)
                :std::runtime_error(map2string(code_,"",line_)),m_code(code_),m_param(),m_line(line_){}
	Error( Code code_, const std::string& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_param(param_),m_line(line_){}
	Error( Code code_, const std::string_view& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_param(std::string(param_)),m_line(line_){}
	Error( Code code_, const char* param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_param(std::string(param_)),m_line(line_){}
	Error( const Error& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(o.m_param),m_line(o.m_line){}
	Error( Error&& o)
                :std::runtime_error(o),m_code(o.m_code),m_param(std::move(o.m_param)),m_line(o.m_line){}

	Code code() const			{return m_code;}
        const std::string& arg() const		{return m_param;}
        int line() const			{return m_line;}

        const char* what() const noexcept override
        {
		return std::runtime_error::what();
	}

private:
        static const char* code2String( int code_)
	{
		if (code_ < 300)
		{
			return std::strerror( code_);
		}
		else switch ((Code)code_)
		{
			case Ok: return "";
			case FileReadError: return "Unknown error reading file, could not read until end of file";
			case LogicError: return "Logic error";
			case IllegalFirstCharacterInLexer: return "Bad character in a regular expression passed to the lexer";
			case SyntaxErrorInLexer: return "Syntax error in the lexer definition";
			case ArrayBoundReadInLexer: return "Logic error (array bound read) in the lexer definition";
			case InvalidRegexInLexer: return "Bad regular expression definition for the lexer";

			case BadCharacterInGrammarDef: return "Bad character in the grammar definition";
			case ValueOutOfRangeInGrammarDef: return "Value out of range in the grammar definition";
			case UnexpectedEofInGrammarDef: return "Unexpected EOF in the grammar definition";
			case UnexpectedTokenInGrammarDef: return "Unexpected token in the grammar definition";
			case ExpectedPatternInGrammarDef: return "Expected regular expression as first element of a lexem definition in the grammar";

			case PriorityDefNotForLexemsInGrammarDef: return "Priority definition for lexems not implemented";
			case UnexpectedEndOfRuleInGrammarDef: return "Unexpected end of rule in the grammar definition";
			case EscapeQuoteErrorInString: return "Some internal pattern string mapping error in the grammar definition";

			case CommandNumberOfArgumentsInGrammarDef: return "Wrong number of arguments for command (followed by '%') in the grammar definition";
			case CommandNameUnknownInGrammarDef: return "Unknown command (followed by '%') in the grammar definition";

			case DefinedAsTerminalAndNonterminalInGrammarDef: return "Identifier defined as nonterminal and as lexem in the grammar definition not allowed";
			case UnresolvedIdentifierInGrammarDef: return "Unresolved identifier in the grammar definition";
			case UnreachableNonTerminalInGrammarDef: return "Unreachable nonteminal in the grammar definition";
			case StartSymbolReferencedInGrammarDef: return "Start symbol referenced on the right side of a rule in the grammar definition";
			case StartSymbolDefinedTwiceInGrammarDef: return "Start symbol defined on the left side of more than one rule of the grammar definition";
			case EmptyGrammarDef: return "The grammar definition is empty";
			case PriorityConflictInGrammarDef: return "Priority definition conflict in the grammar definition";
			case NoAcceptStatesInGrammarDef: return "No accept states in the grammar definition";

			case ShiftReduceConflictInGrammarDef: return "SHIFT/REDUCE conflict in the grammar definition";
			case ReduceReduceConflictInGrammarDef: return "REDUCE/REDUCE conflict in the grammar definition";
			case ShiftShiftConflictInGrammarDef: return "SHIFT/SHIFT conflict in the grammar definition";

			case ComplexityMaxStateInGrammarDef: return "To many states in the resulting tables of the grammar";
			case ComplexityMaxProductionLengthInGrammarDef: return "To many productions in the resulting tables of the grammar";
			case ComplexityMaxNonterminalInGrammarDef: return "To many nonterminals in the resulting tables of the grammar";
			case ComplexityMaxTerminalInGrammarDef: return "To many terminals (lexems) in the resulting tables of the grammar";

			case BadKeyInGeneratedLuaTable: return "Bad key encountered in table generated by mewa grammar compiler";
			case BadValueInGeneratedLuaTable: return "Bad value encountered in table generated by mewa grammar compiler";
		}
		return "Unknown error";
	}

        static std::string map2string( Code code_, const std::string_view& param_, int line_)
	{
		char numbuf[ 128];
		if (line_ > 0)
		{
			std::snprintf( numbuf, sizeof(numbuf), "[%d] \"%s\" at line %d", (int)code_, code2String((int)code_), line_);
		}
		else
		{
			std::snprintf( numbuf, sizeof(numbuf), "[%d] \"%s\"", (int)code_, code2String((int)code_));
		}
                std::string rt(numbuf);
		if (!rt.empty())
		{
			rt.push_back(' ');
			rt.append( param_.data(), param_.size());
		}
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

