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
		MemoryAllocationError=400,
		LogicError=401,
		FileReadError=402,
		SerializationError=403,
		InternalSourceExpectedNullTerminated=404,
		ExpectedStringArgument=405,
		ExpectedIntegerArgument=406,
		ExpectedNonNegativeIntegerArgument=407,
		ExpectedCardinalArgument=408,
		ExpectedFloatingPointArgument=409,
		ExpectedTableArgument=410,
		ExpectedArgumentScopeStructure=411,
		ExpectedArgumentParameterStructure=412,
		ExpectedArgumentNotNil=413,
		TooFewArguments=414,
		TooManyArguments=415,

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
		InvalidHandle=444,

		UnresolvableType=451,
		AmbiguousTypeReference=452,
		ScopeHierarchyError=453,

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
	Error( Code code_ = Ok, int line_=0)
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
	const char* arg() const noexcept	{return std::strchr( what(), ':');}
        int line() const noexcept		{return m_line;}

        const char* what() const noexcept override
        {
		return std::runtime_error::what();
	}

public:
        static const char* code2String( int code_)
	{
		if (code_ < 300)
		{
			return std::strerror( code_);
		}
		else switch ((Code)code_)
		{
			case Ok: return "";
			case MemoryAllocationError: return "memory allocation error";
			case LogicError: return "Logic error";
			case FileReadError: return "Unknown error reading file, could not read until end of file";
			case SerializationError: return "Unspecified error serializing a lua data structure";
			case InternalSourceExpectedNullTerminated: return "Logic error: String expected to be null terminated";
			case ExpectedStringArgument: return "Expected string as argument";
			case ExpectedIntegerArgument: return "Expected integer as argument";
			case ExpectedNonNegativeIntegerArgument: return "Expected non negative integer as argument";
			case ExpectedCardinalArgument: return "Expected positive integer as argument";
			case ExpectedFloatingPointArgument: return "Expected floating point number as argument";
			case ExpectedTableArgument: return "Expected table as argument";
			case ExpectedArgumentScopeStructure:  return "Expected argument to be a structure (pair of non negative integers, unsigned integer range)";
			case ExpectedArgumentParameterStructure:  return "Expected argument to be a type parameter structure (pair type,constructor)";
			case ExpectedArgumentNotNil: return "Expected argument to be not nil";
			case TooFewArguments: return "too few arguments";
			case TooManyArguments: return "too many arguments";

			case IllegalFirstCharacterInLexer: return "Bad character in a regular expression passed to the lexer";
			case SyntaxErrorInLexer: return "Syntax error in the lexer definition";
			case ArrayBoundReadInLexer: return "Logic error (array bound read) in the lexer definition";
			case InvalidRegexInLexer: return "Bad regular expression definition for the lexer";
			case KeywordDefinedTwiceInLexer: return "Keyword defined twice for the lexer (logic error)";
			case TooManyInstancesCreated: return "Too many instances created (internal counter overflow)";
			case CompiledSourceTooComplex: return "Too complex source file (counter overflow)";

			case BadMewaVersion: return "Bad mewa version";
			case MissingMewaVersion: return "Missing mewa version";
			case IncompatibleMewaMajorVersion: return "Incompatible mewa major version. You need a higher version of the mewa Lua module";

			case TooManyTypeArguments: return "Too many arguments in type definition";
			case PriorityOutOfRange: return "Priority value out of range";
			case DuplicateDefinition: return "Duplicate definition";
			case BadRelationWeight: return "Bad weight <= 0.0 given to relation, possibly leading to endless loop in search";
			case InvalidHandle: return "Invalid handle (constructor,type,object) assigned. Expected to be a positive/non negative cardinal number";

			case UnresolvableType: return "Failed to resolve type";
			case AmbiguousTypeReference: return "Ambiguous type reference";
			case ScopeHierarchyError: return "Error in scope hierarchy: Defined overlapping scopes without one including the other";

			case BadCharacterInGrammarDef: return "Bad character in the grammar definition";
			case ValueOutOfRangeInGrammarDef: return "Value out of range in the grammar definition";
			case UnexpectedEofInGrammarDef: return "Unexpected EOF in the grammar definition";
			case UnexpectedTokenInGrammarDef: return "Unexpected token in the grammar definition";
			case DuplicateScopeInGrammarDef: return "More than one scope marker '{}' of '>>' not allowed in a call definition";

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
			case ConflictsInGrammarDef: return "Conflicts detected in the grammar definition";

			case ComplexityMaxStateInGrammarDef: return "To many states in the resulting tables of the grammar";
			case ComplexityMaxNofProductionsInGrammarDef: return "Too many productions defined in the grammar";
			case ComplexityMaxProductionLengthInGrammarDef: return "To many productions in the resulting tables of the grammar";
			case ComplexityMaxProductionPriorityInGrammarDef: return "Priority value assigned to production exceeds maximum value allowed";
			case ComplexityMaxNonterminalInGrammarDef: return "To many nonterminals in the resulting tables of the grammar";
			case ComplexityMaxTerminalInGrammarDef: return "To many terminals (lexems) in the resulting tables of the grammar";
			case ComplexityLR1FollowSetsInGrammarDef: return "To many distinct FOLLOW sets of terminals (lexems) in the resulting tables of the grammar";

			case BadKeyInGeneratedLuaTable: return "Bad key encountered in table generated by mewa grammar compiler";
			case BadValueInGeneratedLuaTable: return "Bad value encountered in table generated by mewa grammar compiler";
			case UnresolvableFunctionInLuaCallTable: return "Function defined in Lua call table is undefined";
			case UnresolvableFunctionArgInLuaCallTable: return "Function context argument defined in Lua call table is undefined";
			case BadElementOnCompilerStack: return "Bad token on the Lua stack of the compiler";
			case NoLuaFunctionDefinedForItem: return "Item found with no Lua function defined for collecting it";

			case LuaCallErrorERRRUN: return "Lua runtime error (ERRRUN)";
			case LuaCallErrorERRMEM: return "Lua memory allocation error (ERRMEM)";
			case LuaCallErrorERRERR: return "Lua error handler error (ERRERR)";
			case LuaCallErrorUNKNOWN: return "Lua error handler error (unknown error)";
			case LuaStackOutOfMemory: return "Lua stack out of memory";
			case LuaInvalidUserData: return "Userdata argument of this call is invalid";

			case UnexpectedTokenNotOneOf: return "Syntax error, unexpected token (expected one of {...})";
			case LanguageAutomatonCorrupted: return "Logic error, the automaton for parsing the language is corrupt";
			case LanguageAutomatonMissingGoto: return "Logic error, the automaton has no goto defined for a follow symbol";
			case LanguageAutomatonUnexpectedAccept: return "Logic error, got into an unexpected accept state";
		}
		return "Unknown error";
	}

private:
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
			rt.push_back(':');
			rt.push_back(' ');
			rt.append( param_.data(), param_.size());
		}
                return rt;
	}

private:
	Code m_code;
	int m_line;
};

}//namespace
#else
#error Building mewa requires C++17
#endif
#endif

