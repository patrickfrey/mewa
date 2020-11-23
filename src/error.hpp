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
#if __cplusplus >= 201703L
#include <utility>
#include <string>
#include <string_view>
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
		RuntimeException=402,
		UnexpectedException=403,
		SerializationError=404,
		InternalSourceExpectedNullTerminated=405,
		ExpectedStringArgument=406,
		ExpectedIntegerArgument=407,
		ExpectedNonNegativeIntegerArgument=408,
		ExpectedCardinalArgument=409,
		ExpectedFloatingPointArgument=410,
		ExpectedTableArgument=411,
		ExpectedArgumentScopeStructure=412,
		ExpectedArgumentParameterStructureList=413,
		ExpectedArgumentTypeList=414,
		ExpectedArgumentTypeOrParameterStructureList=415,
		ExpectedArgumentNotNil=416,
		TooFewArguments=417,
		TooManyArguments=418,
		CompileError=419,

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
		InvalidBoundary=446,

		AmbiguousReductionDefinitions=452,
		ScopeHierarchyError=454,

		BadCharacterInGrammarDef=501,
		ValueOutOfRangeInGrammarDef=502,
		UnexpectedEofInGrammarDef=503,
		UnexpectedTokenInGrammarDef=504,
		DuplicateScopeInGrammarDef=505,
		NestedCallArgumentStructureInGrammarDef=506,

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

	class Location
	{
	public:
		Location( int line_=0) noexcept :m_line(line_) {m_filename[0]=0;}
		Location( const Location& o) noexcept :m_line(o.m_line) {std::memcpy( m_filename, o.m_filename, sizeof(o.m_filename));}
		Location( const std::string_view& filename_, int line_) noexcept :m_line(line_)
		{
			std::size_t len = (filename_.size() >= sizeof( m_filename)) ? (sizeof( m_filename)-1) : filename_.size();
			std::memcpy( m_filename, filename_.data(), len);
			m_filename[ len] = 0;
		}

		int line() const noexcept		{return m_line;}
		const char* filename() const noexcept	{return m_filename;}

	private:
		int m_line;
		char m_filename[ 80];
	};

public:
	Error()
		:std::runtime_error(""),m_code(Ok),m_location(){}
	Error( Code code_, const Location& location_ = Location())
                :std::runtime_error(map2string(code_,"",location_)),m_code(code_),m_location(location_){}
	Error( Code code_, const std::string& param_, const Location& location_ = Location())
                :std::runtime_error(map2string(code_,param_.c_str(),location_)),m_code(code_),m_location(location_){}
	Error( Code code_, const std::string_view& param_, const Location& location_ = Location())
                :std::runtime_error(map2string(code_,param_,location_)),m_code(code_),m_location(location_){}
	Error( Code code_, const char* param_, const Location& location_ = Location())
                :std::runtime_error(map2string(code_,param_,location_)),m_code(code_),m_location(location_){}
	Error( const Error& o)
                :std::runtime_error(map2string(o.code(),o.arg(),o.location())),m_code(o.m_code),m_location(o.location()){}
	Error( const Error& o, const Location& location_)
                :std::runtime_error(map2string(o.code(),o.arg(),location_)),m_code(o.m_code),m_location(location_){}

	Code code() const noexcept			{return m_code;}
	const char* arg() const noexcept		{char const* rt = std::strstr( what(), ": "); return rt?(rt+2):rt;}
        const Location& location() const noexcept	{return m_location;}

        const char* what() const noexcept override
        {
		return std::runtime_error::what();
	}

	static Error parseError( const char* errstr) noexcept;

public:
	static int parseInteger( char const*& si) noexcept;
	static std::string_view parseString( char const*& si) noexcept;
	static void skipSpaces( char const*& si) noexcept;
	static bool skipUntil( char const*& si, char eb) noexcept;
        static const char* code2String( int code_) noexcept;

private:
        static std::string map2string( Code code_, const char* param_, const Location& location_);
        static std::string map2string( Code code_, const std::string_view& param_, const Location& location_);

private:
	Code m_code;
	Location m_location;
};

}//namespace
#else
#error Building mewa requires C++17
#endif
#endif

