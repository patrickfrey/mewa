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

#define REGEX_ESCAPE_CHARS "#{}[]()*+?.-\\"

static std::string_view parseChar( char const*& si)
{
	if ((unsigned char)*si > 128)
	{
		char const* start = si;
		for (++si; *si && utf8mid(*si); ++si){}
		return std::string_view( start, si-start);
	}
	else
	{
		return std::string_view( si, 1);
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
			throw Error( Error::IllegalFirstCharacterInLexer, std::string( "\\") + std::string( parseChar( si)));
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

static bool skipMultiplicator( char const*& si)
{
	if (*si)
	{
		if (*si == '*')
		{
			++si;
			return false;
		}
		if (*si == '{')
		{
			bool empty = (si[1] == '0' && si[2] == ',');
			(void)skipBrackets( si, '}');
			return !empty;
		}
	}
	return true;
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
			empty &= !skipMultiplicator( si);
			if (inverse) rt = inverseCharset( rt);
			if (empty) rt.append( activationCharacters( std::string( si)));
		}
		else if (*si == '(')
		{
			for (;;)
			{
				char const* start = si+1;
				empty &= !skipBrackets( si, ')');
				empty &= !skipMultiplicator( si);
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

DLL_PUBLIC std::pair<std::string_view,int> LexemDef::match( const char* srcptr, std::size_t srclen) const
{
	std::match_results<char const*> pieces_match;

	std::regex_constants::match_flag_type match_flags
		= std::regex_constants::match_continuous
		| std::regex_constants::match_not_null;

	if (std::regex_search( srcptr, srcptr+srclen, pieces_match, m_pattern, match_flags) && pieces_match.size() > select())
	{
		return std::pair<std::string_view,int>( 
				std::string_view( srcptr + pieces_match.position( select()), pieces_match.length( select())),
				pieces_match.length(0));
	}
	else
	{
		return std::pair<std::string_view,int>( std::string_view( "", 0), 0);
	}
}

DLL_PUBLIC char const* Scanner::next( int incr)
{
	int pos = m_srcitr - m_src.c_str() + incr;
	if (pos < 0 || pos > (int)m_src.size()) throw Error( Error::ArrayBoundReadInLexer);
	for (; incr < 0; ++incr) {if (*--m_srcitr == '\n') --m_line;}
	for (; incr > 0; --incr) {if (*m_srcitr++ == '\n') ++m_line;}
	for (; *m_srcitr && (unsigned char)*m_srcitr <= 32; ++m_srcitr) {if (*m_srcitr == '\n') ++m_line;}
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

void Lexer::defineLexem_( const std::string_view& name, const std::string_view& pattern, bool keyword_, std::size_t select)
{
	try
	{
		if (name.empty())
		{
			m_defar.push_back( LexemDef( std::string(), std::string(pattern), 0/*id*/, false/*keyword*/, select));
		}
		else
		{
			auto ins = m_nameidmap.insert( std::pair<std::string,int>( std::string(name), m_nameidmap.size()+1));
			m_defar.push_back( LexemDef( ins.first->first, std::string(pattern), ins.first->second, false/*keyword*/, select));
		}
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

DLL_PUBLIC void Lexer::defineLexem( const std::string_view& name, const std::string_view& pattern, std::size_t select)
{
	defineLexem_( name, pattern, false/*keyword*/, select);
}

DLL_PUBLIC void Lexer::defineIgnore( const std::string_view& pattern)
{
	defineLexem_( "", pattern, true/*keyword*/, 0);
}

static std::string stringToRegex( const std::string_view& opr)
{
	std::string rt;
	char const* si = opr.data();
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

DLL_PUBLIC void Lexer::defineLexem( const std::string_view& opr)
{
	defineLexem_( opr, stringToRegex(opr), true/*keyword*/, 0);
}

DLL_PUBLIC void Lexer::defineBadLexem( const std::string_view& name_)
{
	m_errorLexemName = name_;
}

#define MATCH_EOLN_COMMENT -1
#define MATCH_BRACKET_COMMENT -2

DLL_PUBLIC void Lexer::defineEolnComment( const std::string_view& opr)
{
	m_eolnComments.push_back( std::string(opr));
	if (opr.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( opr[0], MATCH_EOLN_COMMENT));
}

DLL_PUBLIC void Lexer::defineBracketComment( const std::string_view& start, const std::string_view& end)
{
	m_bracketComments.push_back( BracketCommentDef( std::string(start), std::string(end)));
	if (start.empty() || end.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( start[0], MATCH_BRACKET_COMMENT));
}

DLL_LOCAL bool Lexer::matchEolnComment( Scanner& scanner) const
{
	for (auto eolnComment : m_eolnComments)
	{
		if (scanner.match( eolnComment.c_str()))
		{
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
			return rt;
		}
	}
	return -1;
}

DLL_PUBLIC int Lexer::lexemId( const std::string_view& name) const
{
	auto lx = m_nameidmap.find( name);
	return (lx == m_nameidmap.end()) ? 0 : lx->second;
}

DLL_PUBLIC Lexem Lexer::next( Scanner& scanner) const
{
	for (;;)
	{
		char const* start = scanner.next();
		if (*start == '\0') return Lexem( scanner.line());
		auto range = m_firstmap.equal_range( *start);
		int maxlen = 0;
		int matchidx = -1;
		const char* matchstart = nullptr;
		std::size_t matchsize = 0;
		int cidx;
		auto ri = range.first;
		for (; ri != range.second; ++ri)
		{
			int idx = ri->second;
			if (idx == MATCH_EOLN_COMMENT && matchEolnComment( scanner))
			{
				if (!scanner.scan( "\n"))
				{
					return Lexem( scanner.line()); //... no EOLN then return EOF
				}
				break; //... comment skipped, fetch next lexem
			}
			else if (idx == MATCH_BRACKET_COMMENT && 0<=(cidx=matchBracketCommentStart( scanner)))
			{
				if (!scanner.scan( m_bracketComments[ cidx].second.c_str()))
				{
					return Lexem( m_errorLexemName, -1/*id*/, m_bracketComments[ cidx].first, scanner.line());
					//... no matching end bracket of comment found, then return ERROR
				}
				break; //... comment skipped, fetch next lexem
			}
			else
			{
				if (idx > (int)m_defar.size()) throw Error( Error::ArrayBoundReadInLexer);
				auto mm = m_defar[ idx].match( start, scanner.restsize());
				if (maxlen < mm.second)
				{
					maxlen = mm.second;
					matchstart = mm.first.data();
					matchsize = mm.first.size();
					matchidx = idx;
				}
			}
		}
		if (ri == range.second)
		{
			int line = scanner.line();
			if (matchidx < 0)
			{
				std::string_view chr = parseChar( start);
				scanner.next( chr.size());
				return Lexem( m_errorLexemName, -1/*id*/, chr, line);
			}
			else if (0 != m_defar[ matchidx].id())
			{
				scanner.next( maxlen);
				return Lexem( m_defar[ matchidx].name(), m_defar[ matchidx].id(), std::string_view( matchstart, matchsize), line);
			}
		}
	}
}


