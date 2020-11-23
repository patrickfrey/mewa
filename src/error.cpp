/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Error structure
/// \file "error.cpp"
#include "error.hpp"
#include <utility>
#include <string>
#include <stdexcept>
#include <exception>
#include <cstdio>
#include <cstring>
#include <limits>

#if __cplusplus < 201703L
#error Building mewa requires C++17
#endif

using namespace mewa;

Error Error::parseError( const char* errstr) noexcept
{
	Code code_ = Ok;
	int line_ = 0;
	std::string_view filename_ = "";
	char const* ei = errstr;
	char const* param_ = 0;
	char const* msgstart = 0;
	enum State {StateInit, StateParseMessage, StateParseEndOfMessage, StateParseLineNo, StateParseFileName, StateParseArgument, StateEnd};
	State state = StateInit;

	if (*errstr) while (state != StateEnd) switch (state)
	{
		case StateInit:
			if (*ei == '#')
			{
				++ei;
				int cd = parseInteger( ei);
				skipSpaces( ei);

				if (cd < 0 || cd > std::numeric_limits<short>::max())
				{
					code_ = RuntimeException;
					param_ = errstr;
					state = StateEnd;
				}
				else
				{
					code_ = (Error::Code)cd;
					state = StateParseMessage;
				}
			}
			else
			{
				if (0==std::memcmp( ei, "Error at line ", 14))
				{
					code_ = CompileError;
					ei += 6;
				}
				else
				{
					param_ = errstr;
					code_ = RuntimeException;
				}
				state = StateParseLineNo;
			}
			break;
		case StateParseMessage:
			if (*ei == '\"')
			{
				++ei;
				msgstart = ei;
				state = StateParseEndOfMessage;
			}
			else
			{
				code_ = RuntimeException;
				param_ = errstr;
				state = StateEnd;
			}
			break;
		case StateParseEndOfMessage:
			if (!skipUntil( ei, '\"'))
			{
				code_ = RuntimeException;
				param_ = errstr;
				state = StateEnd;
			}
			else if (0==std::memcmp( msgstart, code2String( code_), ei-msgstart))
			{
				++ei;
				skipSpaces( ei);
				state = StateParseLineNo;
			}
			else
			{
				++ei; //... continue parse end of message
			}
			break;
		case StateParseLineNo:
			if (0==std::memcmp( ei, "at line ", 8))
			{
				ei += 8;
				line_ = parseInteger( ei);
				if (*ei == ' ')
				{
					++ei;
					state = StateParseFileName;
				}
				else
				{
					state = StateParseArgument;
				}
			}
			else
			{
				state = StateParseArgument;
			}
			break;
		case StateParseFileName:
			if (0==std::memcmp( ei, "in file ", 8))
			{
				ei += 8;
				if (*ei == '"' || *ei == '\'')
				{
					filename_ = parseString( ei);
				}
			}
			state = StateParseArgument;
			break;
		case StateParseArgument:
			if (ei[0] == ':' && ei[1] == ' ')
			{
				param_ = ei +2;
				state = StateEnd;
			}
			else if (!*ei)
			{
				param_ = "";
				state = StateEnd;
			}
			else
			{
				state = StateEnd;
			}
			break;
		case StateEnd:
			break;
			
	}//... end for switch
	if (filename_[0])
	{
		return Error( code_, param_, Location( filename_, line_));
	}
	else
	{
		return Error( code_, param_, Location( line_));
	}
}

int Error::parseInteger( char const*& si) noexcept
{
	int rt = 0;
	for (; *si >= '0' && *si <= '9'; ++si)
	{
		rt = (rt * 10) + (*si - '0');
	}
	return rt;
}

std::string_view Error::parseString( char const*& si) noexcept
{
	char eb = *si++;
	char const* start = si;
	for (;*si && *si != eb; ++si){}
	std::string_view rt( start, si-start);
	if (*si) ++si;
	return rt;
}

void Error::skipSpaces( char const*& si) noexcept
{
	for (; *si && (unsigned char)*si <= 32; ++si){}
}

bool Error::skipUntil( char const*& si, char eb) noexcept
{
	for (; *si && (unsigned char)*si != eb; ++si){}
	return *si == eb;
}

const char* Error::code2String( int code_) noexcept
{
	if (code_ && code_ < 300)
	{
		return std::strerror( code_);
	}
	else switch ((Code)code_)
	{
		case Ok: return "";
		case MemoryAllocationError: return "memory allocation error";
		case LogicError: return "Logic error";
		case RuntimeException: return "Runtime error exception";
		case UnexpectedException: return "Unexpected exception";
		case SerializationError: return "Error serializing a lua data structure";
		case InternalSourceExpectedNullTerminated: return "Logic error: String expected to be null terminated";
		case ExpectedStringArgument: return "Expected string as argument";
		case ExpectedIntegerArgument: return "Expected integer as argument";
		case ExpectedNonNegativeIntegerArgument: return "Expected non negative integer as argument";
		case ExpectedCardinalArgument: return "Expected positive integer as argument";
		case ExpectedFloatingPointArgument: return "Expected floating point number as argument";
		case ExpectedTableArgument: return "Expected table as argument";
		case ExpectedArgumentScopeStructure:  return "Expected argument to be a structure (pair of non negative integers, unsigned integer range)";
		case ExpectedArgumentParameterStructureList:  return "Expected argument to be a list of type,constructor pairs (named or positional)";
		case ExpectedArgumentTypeList: return "Expected argument to be a list of types (integers)";
		case ExpectedArgumentTypeOrParameterStructureList: return "Expected argument to be a list of types (integers) or type,constructor pairs";

		case ExpectedArgumentNotNil: return "Expected argument to be not nil";
		case TooFewArguments: return "Too few arguments";
		case TooManyArguments: return "Too many arguments";
		case CompileError: return "Compile error";

		case IllegalFirstCharacterInLexer: return "Bad character in a regular expression passed to the lexer";
		case SyntaxErrorInLexer: return "Syntax error in the lexer definition";
		case ArrayBoundReadInLexer: return "Logic error (array bound read) in the lexer definition";
		case InvalidRegexInLexer: return "Bad regular expression definition for the lexer";
		case KeywordDefinedTwiceInLexer: return "Keyword defined twice for the lexer";

		case TooManyInstancesCreated: return "Too many instances created (internal counter overflow)";
		case CompiledSourceTooComplex: return "Too complex source file (counter overflow)";

		case BadMewaVersion: return "Bad mewa version";
		case MissingMewaVersion: return "Missing mewa version";
		case IncompatibleMewaMajorVersion: return "Incompatible mewa major version. You need a higher version of the mewa Lua module";

		case TooManyTypeArguments: return "Too many arguments in type definition";
		case PriorityOutOfRange: return "Priority value out of range";
		case DuplicateDefinition: return "Duplicate definition";
		case BadRelationWeight: return "Bad weight <= 0.0 given to relation";
		case BadRelationTag: return "Bad value tor tag attached to relation, must be a cardinal number in {1..32}";
		case InvalidHandle: return "Invalid handle (constructor,type,object) assigned. Expected to be a positive/non negative cardinal number";
		case InvalidBoundary: return "Invalid processing boundary value. Expected to be a non negative number";

		case AmbiguousReductionDefinitions: return "Ambiguous definitions of reductions";
		case ScopeHierarchyError: return "Error in scope hierarchy: Defined overlapping scopes without one including the other";

		case BadCharacterInGrammarDef: return "Bad character in the grammar definition";
		case ValueOutOfRangeInGrammarDef: return "Value out of range in the grammar definition";
		case UnexpectedEofInGrammarDef: return "Unexpected EOF in the grammar definition";
		case UnexpectedTokenInGrammarDef: return "Unexpected token in the grammar definition";
		case DuplicateScopeInGrammarDef: return "More than one scope marker '{}' of '>>' not allowed in a call definition";
		case NestedCallArgumentStructureInGrammarDef: return "Nested structures as callback function arguments are not allowed";

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
		case ConflictsInGrammarDef: return "Conflicts detected in the grammar definition. No output written.";

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

std::string Error::map2string( Code code_, const std::string_view& param_, const Location& location_)
{
	char parambuf[ 512];
	std::size_t len = param_.size() >= sizeof(parambuf) ? (sizeof(parambuf)-1):param_.size();
	std::memcpy( parambuf, param_.data(), len);
	parambuf[ len] = 0;
	return map2string( code_, parambuf, location_);
}

std::string Error::map2string( Code code_, const char* param_, const Location& location_)
{
	char msgbuf[ 256];
	if (code_ == CompileError)
	{
		if (location_.filename()[0])
		{
			std::snprintf( msgbuf, sizeof(msgbuf), "Error at line %d in file \"%s\"", location_.line(), location_.filename());
		}
		else
		{
			std::snprintf( msgbuf, sizeof(msgbuf), "Error at line %d", location_.line());
		}
	}
	else if (code_ == Ok)
	{
		msgbuf[0] = 0;
	}
	else if (location_.line())
	{
		if (location_.filename()[0])
		{
			std::snprintf( msgbuf, sizeof(msgbuf), "#%d \"%s\" at line %d in file \"%s\"",
					(int)code_, code2String((int)code_), location_.line(), location_.filename());
		}
		else
		{
			std::snprintf( msgbuf, sizeof(msgbuf), "#%d \"%s\" at line %d",
					(int)code_, code2String((int)code_), location_.line());
		}
	}
	else
	{
		std::snprintf( msgbuf, sizeof(msgbuf), "#%d \"%s\"", (int)code_, code2String((int)code_));
	}
	try
	{
		std::string rt( msgbuf);
		if (param_ && param_[0])
		{
			rt.push_back(':');
			rt.push_back(' ');
			rt.append( param_);
		}
		return rt;
	}
	catch (...)
	{
		std::snprintf( msgbuf, sizeof(msgbuf), "#%d", (int)MemoryAllocationError);
		std::string rt( msgbuf);
		return rt;
	}
}

