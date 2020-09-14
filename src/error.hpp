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
		InternalSourceExpectedNullTerminated=403,
		ExpectedFilenameAsArgument=404,
		IllegalFirstCharacterInLexer=421,
		SyntaxErrorInLexer=422,
		ArrayBoundReadInLexer=423,
		InvalidRegexInLexer=424,

		BadCharacterInGrammarDef=501,
		ValueOutOfRangeInGrammarDef=502,
		UnexpectedEofInGrammarDef=503,
		UnexpectedTokenInGrammarDef=504,
		ExpectedPatternInGrammarDef=505,

		PriorityDefNotForLexemsInGrammarDef=521,
		UnexpectedEndOfRuleInGrammarDef=531,
		EscapeQuoteErrorInString=532,

		CommandNumberOfArgumentsInGrammarDef=541,
		CommandNameUnknownInGrammarDef=542,

		DefinedAsTerminalAndNonterminalInGrammarDef=551,
		UnresolvedIdentifierInGrammarDef=552,
		UnreachableNonTerminalInGrammarDef=553,
		StartSymbolReferencedInGrammarDef=554,
		StartSymbolDefinedTwiceInGrammarDef=555,
		EmptyGrammarDef=556,
		PriorityConflictInGrammarDef=557,
		NoAcceptStatesInGrammarDef=558,

		ShiftReduceConflictInGrammarDef=561,
		ReduceReduceConflictInGrammarDef=562,
		ShiftShiftConflictInGrammarDef=563,

		ComplexityMaxStateInGrammarDef=571,
		ComplexityMaxProductionLengthInGrammarDef=572,
		ComplexityMaxNonterminalInGrammarDef=573,
		ComplexityMaxTerminalInGrammarDef=574,

		BadKeyInGeneratedLuaTable=591,
		BadValueInGeneratedLuaTable=592,

		LanguageSyntaxErrorExpectedOneOf=601,
		LanguageAutomatonCorrupted=602,
		LanguageAutomatonMissingGoto=603,
		LanguageAutomatonUnexpectedAccept=604,
	};

public:
	Error( Code code_ = Ok, int line_=0)
                :std::runtime_error(map2string(code_,"",line_)),m_code(code_),m_line(line_) 	{m_param[0] = 0;}
	Error( Code code_, const std::string& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_line(line_)	{initParam(param_);}
	Error( Code code_, const std::string_view& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_line(line_)	{initParam(param_);}
	Error( Code code_, const char* param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_line(line_)	{initParam(param_);}
	Error( const Error& o)
                :std::runtime_error(o),m_code(o.m_code),m_line(o.m_line) 			{initParam(o.m_param);}

	Code code() const			{return m_code;}
        const char* arg() const			{return m_param;}
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
			case LogicError: return "Logic error";
			case FileReadError: return "Unknown error reading file, could not read until end of file";
			case InternalSourceExpectedNullTerminated: return "Logic error: String expected to be null terminated";
			case ExpectedFilenameAsArgument: return "Expected file name as argument";
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

			case LanguageSyntaxErrorExpectedOneOf: return "Syntax error, expected one of the following";
			case LanguageAutomatonCorrupted: return "Logic error, the automaton for parsing the language is corrupt";
			case LanguageAutomatonMissingGoto: return "Logic error, the automaton has no goto defined for a follow symbol";
			case LanguageAutomatonUnexpectedAccept: return "Logic error, got into an unexpected accept state";
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

	void initParam( const std::string_view& param_)
	{
		if (param_.size() >= MaxParamLen)
		{
			std::memcpy( m_param, param_.data(), MaxParamLen-1);
			m_param[ MaxParamLen-1] = '\0';
		}
		else
		{
			std::memcpy( m_param, param_.data(), param_.size());
			m_param[ param_.size()] = '\0';
		}
	}

private:
	Code m_code;
	enum {MaxParamLen=2048};
        char m_param[ MaxParamLen];
	int m_line;
};

}//namespace
#else
#error Building mewa requires C++11
#endif
#endif

