/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Lexer implementation
/// \file "lexer.cpp"
#include "lexer.hpp"
#include "error.hpp"
#include "utf8.hpp"
#include "export.hpp"

using namespace mewa;

#define REGEX_ESCAPE_CHARS "{}[]()*+.-\\"

static std::string parseChar( char const*& si)
{
	if ((unsigned char)*si > 128)
	{
		char const* start = si;
		for (++si; *si && utf8mid(*si); ++si){}
		return std::string( start, si-start);
	}
	else
	{
		return std::string( si, 1);
	}
}

static char parseFirstChar( char const*& si)
{
	if ((unsigned char)*si > 128 || (unsigned char)*si <= 32)
	{
		throw Error( Error::IllegalFirstCharacterInLexer, parseChar(si));
	}
	else if (*si == '\\')
	{
		++si;
		if (0!=std::strchr( REGEX_ESCAPE_CHARS, *si))
		{
			return *si++;
		}
		else
		{
			throw Error( Error::IllegalFirstCharacterInLexer, std::string( "\\") + parseChar( si));
		}
	}
	else
	{
		return *si++;
	}
}

static bool extractFirstCharacters( std::string& res, char const*& si, char eb)
{
	bool empty = true;
	while (*si && *si != eb)
	{
		if (*si == '-' && !empty)
		{
			++si;
			char from = res[ res.size()-1];
			if (!*si || *si == eb)
			{
				res.push_back( from);
				break;
			}
			else
			{
				char to = parseFirstChar( si);
				if (to < from) std::swap( to, from);
				for (; from <= to; ++from)
				{
					res.push_back( from);
				}
			}
		}
		else
		{
			res.push_back( parseFirstChar( si));
		}
		empty = false;
	}
	if (!*si) throw Error( Error::SyntaxErrorInLexer);
	++si;
	return !empty;
}

static bool skipBrackets( char const*& si, char eb)
{
	char sb = *si++;
	int cnt = 1;

	bool empty = true;
	for (; *si && cnt > 0; ++si,empty=false)
	{
		if (*si == eb)
		{
			--cnt;
		}
		else if (*si == sb)
		{
			++cnt;
		}
	}
	if (!*si) throw Error( Error::SyntaxErrorInLexer);
	return !empty;
}

static std::string inverseCharset( const std::string& charset)
{
	std::string rt;
	std::bitset<128> cset;
	for (char ch : charset) cset.set( ch);
	for (int ii=33; ii<128; ++ii) if (!cset.test(ii)) {rt.push_back(ii);}
	return rt;
}

DLL_LOCAL std::string LexemDef::activationCharacters( const std::string& source_)
{
	std::string rt;
	bool empty = true;
	char const* si = source_.c_str();
	if (*si == '^') ++si;
	while (empty)
	{
		if (*si == '\0')
		{
			return rt;
		}
		else if (*si == '[')
		{
			++si;
			bool inverse = false;
			if (*si == '^')
			{
				inverse = true;
				++si;
			}
			empty &= !extractFirstCharacters( rt, si, ']');
			if (inverse) rt = inverseCharset( rt);
			if (empty) rt.append( activationCharacters( std::string( si)));
		}
		else if (*si == '(')
		{
			for (;;)
			{
				char const* start = si+1;
				empty &= !skipBrackets( si, ')');
				rt.append( activationCharacters( std::string( start, si-start-1)));
				if (*si != '|') break;
				++si;
			}
		}
		else
		{
			rt.push_back( parseFirstChar( si));
			empty = false;
		}
	}
	return rt;
}

DLL_PUBLIC std::pair<std::string,int> LexemDef::match( const char* srcptr) const
{
	std::pair<std::string,int> rt;
	std::cmatch match;

	if (std::regex_match( srcptr, match, m_pattern) && match.size() > select())
	{
		std::csub_match sub_match = match[ select()];
		rt.first = sub_match.str();
		rt.second = match.length( 0);
	}
	return rt;
}

DLL_PUBLIC char const* Scanner::next( int incr)
{
	int pos = m_srcitr - m_src.c_str() + incr;
	if (pos < 0 || pos > (int)m_src.size()) throw Error( Error::ArrayBoundReadInLexer);
	for (; incr < 0; ++incr) {if (*--m_srcitr == '\n') --m_line;}
	for (; incr > 0; --incr) {if (*m_srcitr++ == '\n') ++m_line;}
	for (; *m_srcitr && (unsigned char)*m_srcitr <= 32; ++m_srcitr) {if (*m_srcitr++ == '\n') ++m_line;}
	return m_srcitr;
}

DLL_PUBLIC bool Scanner::scan( const char* str)
{
	char const* end = std::strstr( m_srcitr, str);
	return (end && next( end-m_srcitr + std::strlen(str)));
}

DLL_PUBLIC bool Scanner::match( const char* str)
{
	int ii = 0;
	int ll = 0;
	while (m_srcitr[ii] && m_srcitr[ii] == str[ii])
	{
		if (m_srcitr[ii] == '\n') ++ll;
		++ii;
	}
	if (!str[ii])
	{
		m_srcitr += ii;
		m_line += ll;
		return true;
	}
	return false;
}

DLL_PUBLIC void Lexer::defineLexem( const std::string& name, const std::string& pattern, std::size_t select)
{
	try
	{
		m_defar.push_back( LexemDef( name, pattern, select));
	}
	catch (const std::regex_error&)
	{
		throw Error( Error::InvalidRegexInLexer, pattern);
	}
	int firstChars = 0;
	for (int ii=0; ii<128; ++ii)
	{
		if (m_defar.back().activate().test(ii))
		{
			m_firstmap.insert( std::pair<char,int>( ii, m_defar.size()-1));
			++firstChars;
		}
	}
	if (firstChars == 0) throw Error( Error::SyntaxErrorInLexer);
}

static std::string stringToRegex( const std::string& opr)
{
	std::string rt;
	char const* si = opr.c_str();
	for (; *si; ++si)
	{
		if (0!=std::strchr( REGEX_ESCAPE_CHARS, *si))
		{
			rt.push_back( '\\');
		}
		rt.push_back( *si);
	}
	return rt;
}

DLL_PUBLIC void Lexer::defineLexem( const std::string& opr)
{
	defineLexem( opr, stringToRegex(opr));
}

DLL_PUBLIC void Lexer::defineBadLexem( const std::string& name_)
{
	m_errorLexemName = name_;
}

#define MATCH_EOLN_COMMENT -1
#define MATCH_BRACKET_COMMENT -2

DLL_PUBLIC void Lexer::defineEolnComment( const std::string& opr)
{
	m_eolnComments.push_back( opr);
	if (opr.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( opr[0], MATCH_EOLN_COMMENT));
}

DLL_PUBLIC void Lexer::defineBracketComment( const std::string& start, const std::string& end)
{
	m_bracketComments.push_back( BracketCommentDef( start, end));
	if (start.empty() || end.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( start[0], MATCH_BRACKET_COMMENT));
}

DLL_LOCAL bool Lexer::matchEolnComment( Scanner& scanner) const
{
	for (auto eolnComment : m_eolnComments)
	{
		if (scanner.match( eolnComment.c_str()))
		{
			scanner.next( eolnComment.size());
			return true;
		}
	}
	return false;
}

DLL_LOCAL int Lexer::matchBracketCommentStart( Scanner& scanner) const
{
	int rt = -1;
	for (auto bracketComment : m_bracketComments)
	{
		++rt;
		if (scanner.match( bracketComment.first.c_str()))
		{
			scanner.next( bracketComment.first.size());
			return rt;
		}
	}
	return -1;
}

DLL_PUBLIC Lexem Lexer::next( Scanner& scanner) const
{
	char const* start = scanner.next();
	if (*start == '\0') return Lexem( scanner.line());
	auto range = m_firstmap.equal_range( *start);
	int maxlen = 0;
	int matchidx = -1;
	int cidx;
	for (auto ri = range.first; ri != range.second; ++ri)
	{
		int idx = ri->second;
		if (idx == MATCH_EOLN_COMMENT && matchEolnComment( scanner))
		{
			scanner.scan( "\n");
		}
		else if (idx == MATCH_BRACKET_COMMENT && 0>(cidx=matchBracketCommentStart( scanner)))
		{
			scanner.scan( m_bracketComments[ cidx].second.c_str());
		}
		else
		{
			if (idx > (int)m_defar.size()) throw Error( Error::ArrayBoundReadInLexer);
			auto mm = m_defar[ idx].match( start);
			if (maxlen < mm.second)
			{
				maxlen = mm.second;
				matchidx = idx;
			}
		}
	}
	int line = scanner.line();
	if (matchidx < 0)
	{
		std::string chrstr = parseChar( start);
		scanner.next( chrstr.size());
		return Lexem( m_errorLexemName, chrstr, line);
	}
	else
	{
		scanner.next( maxlen);
		return Lexem( m_defar[ matchidx].name(), std::string( start, maxlen), line);
	}
}


