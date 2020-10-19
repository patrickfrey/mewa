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
#include <limits>

namespace mewa {

class Error
	:public std::runtime_error
{
public:
	enum Code {
		Ok=0,
		MemoryAllocationError=400,
		LogicError=401,
		UnspecifiedError=402,
		FileReadError=403,
		SerializationError=404,
		InternalSourceExpectedNullTerminated=405,
		ExpectedStringArgument=406,
		ExpectedIntegerArgument=407,
		ExpectedNonNegativeIntegerArgument=408,
		ExpectedCardinalArgument=409,
		ExpectedFloatingPointArgument=410,
		ExpectedTableArgument=411,
		ExpectedArgumentScopeStructure=412,
		ExpectedArgumentParameterStructure=413,
		ExpectedArgumentNotNil=414,
		TooFewArguments=415,
		TooManyArguments=416,

		IllegalFirstCharacterInLexer=421,
		SyntaxErrorInLexer=422,
		ArrayBoundReadInLexer=423,
		InvalidRegexInLexer=424,
		KeywordDefinedTwiceInLexer=425,
		TooManyInstancesCreated=426,
		CompiledSourceTooComplex=427,

		BadMewaVersion=437,
		MissingMewaVersion=438,
		IncompatibleMewaMajorVersion=439,

		TooManyTypeArguments=440,
		PriorityOutOfRange=441,
		DuplicateDefinition=442,
		BadRelationWeight=443,
		BadRelationTag=444,
		InvalidHandle=445,

		UnresolvableType=451,
		AmbiguousTypeReference=452,
		AmbiguousReductionDefinition=453,
		ScopeHierarchyError=454,

		BadCharacterInGrammarDef=501,
		ValueOutOfRangeInGrammarDef=502,
		UnexpectedEofInGrammarDef=503,
		UnexpectedTokenInGrammarDef=504,
		DuplicateScopeInGrammarDef=505,

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
		ConflictsInGrammarDef=564,

		ComplexityMaxStateInGrammarDef=571,
		ComplexityMaxNofProductionsInGrammarDef=572,
		ComplexityMaxProductionLengthInGrammarDef=573,
		ComplexityMaxProductionPriorityInGrammarDef=574,
		ComplexityMaxNonterminalInGrammarDef=575,
		ComplexityMaxTerminalInGrammarDef=576,
		ComplexityLR1FollowSetsInGrammarDef=577,

		BadKeyInGeneratedLuaTable=581,
		BadValueInGeneratedLuaTable=582,
		UnresolvableFunctionInLuaCallTable=583,
		UnresolvableFunctionArgInLuaCallTable=584,
		BadElementOnCompilerStack=585,
		NoLuaFunctionDefinedForItem=586,

		LuaCallErrorERRRUN=591,
		LuaCallErrorERRMEM=592,
		LuaCallErrorERRERR=593,
		LuaCallErrorUNKNOWN=594,
		LuaStackOutOfMemory=595,
		LuaInvalidUserData=596,

		UnexpectedTokenNotOneOf=601,
		LanguageAutomatonCorrupted=602,
		LanguageAutomatonMissingGoto=603,
		LanguageAutomatonUnexpectedAccept=604,
	};

public:
	Error()
		:std::runtime_error(""),m_code(Ok),m_line(0){}
	Error( Code code_, int line_=0)
                :std::runtime_error(map2string(code_,"",line_)),m_code(code_),m_line(line_){}
	Error( Code code_, const std::string& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_line(line_){}
	Error( Code code_, const std::string_view& param_, int line_=0)
                :std::runtime_error(map2string(code_,param_,line_)),m_code(code_),m_line(line_){}
	Error( Code code_, const char* param_, int line_=0)
                :std::runtime_error(map2string(code_,param_?param_:"",line_)),m_code(code_),m_line(line_){}
	Error( const Error& o)
                :std::runtime_error(o),m_code(o.m_code),m_line(o.m_line){}

	Code code() const noexcept		{return m_code;}
	const char* arg() const noexcept	{char const* rt = std::strstr( what(), ": "); return rt?(rt+2):rt;}
        int line() const noexcept		{return m_line;}

        const char* what() const noexcept override
        {
		return std::runtime_error::what();
	}

	static Error parseError( const char* errstr) noexcept;

public:
	static int parseInteger( char const*& si) noexcept;
	static void skipSpaces( char const*& si) noexcept;
	static bool skipUntil( char const*& si, char eb) noexcept;
        static const char* code2String( int code_) noexcept;

private:
        static std::string map2string( Code code_, const std::string_view& param_, int line_);

private:
	Code m_code;
	int m_line;
};

}//namespace
#else
#error Building mewa requires C++17
#endif
#endif

