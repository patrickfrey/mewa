/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Lexer implementation
/// \file "lexer.cpp"
#include "lexer.hpp"
#include "error.hpp"
#include "strings.hpp"
#include "utf8.hpp"

using namespace mewa;

#define REGEX_ESCAPE_CHARS "#{}[]()*+?.-\\|&^"

static std::string_view parseChar( char const*& si)
{
	if ((unsigned char)*si >= 128)
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

static char parseFirstChar( char const*& si, bool allowNonAscii)
{
	if ((unsigned char)*si < 32U)
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
		char rt = *si;
		int len = utf8charlen( *si);
		if (!allowNonAscii && len > 1)
		{
			throw Error( Error::IllegalFirstCharacterInLexer, parseChar(si));
		}
		si += len;
		return rt;
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
				char to = parseFirstChar( si, false/*!allowNonAscii*/);
				if (to < from) std::swap( to, from);
				for (++from; from <= to; ++from)
				{
					res.push_back( from);
				}
			}
		}
		else
		{
			res.push_back( parseFirstChar( si, false/*!allowNonAscii*/));
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
	for (; *si && cnt > 0; ++si)
	{
		if (*si == eb)
		{
			--cnt;
		}
		else if (*si == sb)
		{
			++cnt;
		}
		else
		{
			empty = false;
		}
	}
	if (!*si && cnt != 0) throw Error( Error::SyntaxErrorInLexer);
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
	std::bitset<256> cset;
	for (char ch : charset) cset.set( ch);
	for (int ii=33; ii<256; ++ii) if (!cset.test(ii)) {rt.push_back(ii);}
	return rt;
}

std::string LexemDef::activation( const std::string& source_)
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
			bool hasCharacters = extractFirstCharacters( rt, si, ']');
			bool potentiallyIgnored = (skipMultiplicator( si) == false);
			empty = !hasCharacters || potentiallyIgnored;
			if (inverse) rt = inverseCharset( rt);
		}
		else if (*si == '(')
		{
			for (;;)
			{
				char const* start = si+1;
				bool hasCharacters = skipBrackets( si, ')');
				bool potentiallyIgnored = (skipMultiplicator( si) == false);
				empty = !hasCharacters || potentiallyIgnored;
				rt.append( activation( std::string( start, si-start-1)));
				if (*si != '|') break;
				++si;
			}
		}
		else
		{
			rt.push_back( parseFirstChar( si, true/*allowNonAscii*/));
			empty = false;
		}
	}
	return rt;
}

std::pair<std::string_view,int> LexemDef::match( const char* srcptr, std::size_t srclen) const
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

void Scanner::checkNullTerminated()
{
	if (m_src.data()[ m_src.size()] != 0) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
}

char const* Scanner::next( int incr)
{
	int pos = m_srcitr - m_src.data() + incr;
	if (pos < 0 || pos > (int)m_src.size()) throw Error( Error::ArrayBoundReadInLexer);
	for (; incr < 0; ++incr) {if (*--m_srcitr == '\n') --m_line;}
	for (; incr > 0; --incr) {if (*m_srcitr++ == '\n') ++m_line;}
	for (; *m_srcitr && (unsigned char)*m_srcitr <= 32; ++m_srcitr) {if (*m_srcitr == '\n') ++m_line;}
	return m_srcitr;
}

bool Scanner::scan( const char* str)
{
	char const* end = std::strstr( m_srcitr, str);
	return (end && next( end-m_srcitr + std::strlen(str)));
}

int Scanner::indentCount( int tabSize) const noexcept
{
	int rt = 0;
	char const* se = m_srcitr;
	char const* si = m_src.data();
	for (; se != si && *(se-1) != '\n' && (unsigned char)*(se-1) <= 32; --se)
	{
		rt += *(se-1)=='\t' ? tabSize : 1;
	}
	return (se == si || *(se-1) == '\n') ? rt : -1;
}

Scanner::IndentToken Scanner::getIndentToken( int tabSize)
{
	if (m_indentConsumed)
	{
		m_indentConsumed = false;
		return IndentNone;
	}
	int cnt = indentCount( tabSize);
	if (cnt == -1) return IndentNone;
	if (m_indentstk.empty())
	{
		if (cnt == 0)
		{
			m_indentConsumed = true;
			return IndentNewLine;
		}
		else
		{
			m_indentstk.push_back( cnt);
			return IndentOpen;
		}
	}
	else
	{
		int bk = m_indentstk.back();
		if (bk < cnt)
		{
			m_indentstk.push_back( cnt);
			return IndentOpen;
		}
		else if (bk > cnt)
		{
			m_indentstk.pop_back();
			return IndentClose;
		}
		else
		{
			m_indentConsumed = true;
			return IndentNewLine;
		}
	}
}

Scanner::IndentToken Scanner::getEofIndentToken()
{
	if (m_indentstk.empty())
	{
		return IndentNone;
	}
	else
	{
		m_indentstk.pop_back();
		return IndentClose;
	}
}

bool Scanner::match( const char* str)
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

static std::string nameString( const std::string_view& name)
{
	std::string rt;
	void const* sqpos = std::memchr( name.data(), '\'', name.size());
	void const* dqpos = std::memchr( name.data(), '\"', name.size());
	if (sqpos && dqpos)
	{
		rt.append( name.data(), name.size());
	}
	else if (sqpos)
	{
		rt.push_back( '"');
		rt.append( name.data(), name.size());
		rt.push_back( '"');
	}
	else if (dqpos)
	{
		rt.push_back( '\'');
		rt.append( name.data(), name.size());
		rt.push_back( '\'');
	}
	else if (name.size() == 1)
	{
		rt.push_back( '\'');
		rt.append( name.data(), name.size());
		rt.push_back( '\'');
	}
	else
	{
		rt.push_back( '"');
		rt.append( name.data(), name.size());
		rt.push_back( '"');
	}
	return rt;
}

int Lexer::defineLexem_( int line, const std::string_view& name, const std::string_view& pattern, bool keyword_, std::size_t select)
{
	int rt = 0;
	try
	{
		if (name.empty())
		{
			m_defar.emplace_back( line, std::string(), std::string(pattern), 0/*id*/, false/*keyword*/, select);
		}
		else
		{
			std::size_t id_ = m_nameidmap.size()+1;
			while (m_id2keyword.size() <= id_)
			{
				m_id2keyword.push_back( false);
			}
			auto ins = m_nameidmap.insert( {std::string(name), id_} );
			if (ins.second/*insert took place*/)
			{
				if (keyword_)
				{
					m_namelist.push_back( nameString( name));
					m_id2keyword[ id_] = true;
				}
				else
				{
					m_namelist.push_back( std::string( name));
				}
			}
			else if (keyword_)
			{
				throw Error( Error::KeywordDefinedTwiceInLexer, name);
			}
			m_defar.emplace_back( line, ins.first->first, std::string(pattern), ins.first->second, keyword_, select);
			rt = ins.first->second;
		}
	}
	catch (const std::regex_error&)
	{
		throw Error( Error::InvalidRegexInLexer, pattern);
	}
	if (pattern.size() > 0)
	{
		int firstChars = 0;
		for (unsigned int ii=0; ii<256; ++ii)
		{
			if (m_defar.back().activate().test(ii))
			{
				m_firstmap.insert( std::pair<unsigned char,int>( ii, m_defar.size()-1));
				++firstChars;
			}
		}
		if (firstChars == 0) throw Error( Error::SyntaxErrorInLexer);
	}
	return rt;
}

int Lexer::defineLexem( int line, const std::string_view& name, const std::string_view& pattern, std::size_t select)
{
	return defineLexem_( line, name, pattern, false/*keyword*/, select);
}

void Lexer::defineIgnore( int line, const std::string_view& pattern)
{
	(void)defineLexem_( line, "", pattern, false/*keyword*/, 0/*select*/);
}

int Lexer::defineLexemId( const std::string_view& name)
{
	std::size_t id_ = m_nameidmap.size()+1;
	while (m_id2keyword.size() <= id_) {m_id2keyword.push_back( false);}
	auto ins = m_nameidmap.insert( {std::string(name), id_} );
	if (ins.second/*insert took place*/)
	{
		m_namelist.push_back( nameString( name));
	}
	else
	{
		throw std::runtime_error( Error( Error::KeywordDefinedTwiceInLexer));
	}
	m_defar.emplace_back( 0/*no line*/, std::string(name), id_);
	return ins.first->second;
}

void Lexer::defineIndentLexems( int line, const std::string_view& open, const std::string_view& close, const std::string_view& newline, int tabsize)
{
	if (m_indentLexems.open) throw Error( Error::ConflictingCommandInGrammarDef);
	m_indentLexems.line = line;
	m_indentLexems.open = defineLexemId( open);
	m_indentLexems.close = defineLexemId( close);
	m_indentLexems.newLine = defineLexemId( newline);
	m_indentLexems.tabSize = tabsize;
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

int Lexer::defineLexem( const std::string_view& opr)
{
	std::string pattern = stringToRegex(opr);
	return defineLexem_( 0/*no line*/, opr, pattern, true/*keyword*/, 0);
}

void Lexer::defineBadLexem( int line, const std::string_view& name_)
{
	m_errorLexem = ErrorLexemDef( line, std::string(name_));
}

#define MATCH_EOLN_COMMENT -1
#define MATCH_BRACKET_COMMENT -2

void Lexer::defineEolnComment( int line, const std::string_view& opr)
{
	m_eolnComments.emplace_back( line, std::string(opr));
	if (opr.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( opr[0], MATCH_EOLN_COMMENT));
}

void Lexer::defineBracketComment( int line, const std::string_view& start, const std::string_view& end)
{
	m_bracketComments.emplace_back( line, std::string(start), std::string(end));
	if (start.empty() || end.empty()) throw Error( Error::SyntaxErrorInLexer);
	m_firstmap.insert( std::pair<char,int>( start[0], MATCH_BRACKET_COMMENT));
}

bool Lexer::matchEolnComment( Scanner& scanner) const
{
	for (auto const& eolnComment : m_eolnComments)
	{
		if (scanner.match( eolnComment.open.c_str()))
		{
			return true;
		}
	}
	return false;
}

int Lexer::matchBracketCommentStart( Scanner& scanner) const
{
	int rt = -1;
	for (auto const& bracketComment : m_bracketComments)
	{
		++rt;
		if (scanner.match( bracketComment.open.c_str()))
		{
			return rt;
		}
	}
	return -1;
}

int Lexer::lexemId( const std::string_view& name) const noexcept
{
	auto lx = m_nameidmap.find( name);
	return (lx == m_nameidmap.end()) ? 0 : lx->second;
}

const std::string& Lexer::lexemName( int id) const
{
	if (id < 1 || id > (int)m_namelist.size()) throw Error( Error::ArrayBoundReadInLexer);
	return m_namelist[ id-1];
}

bool Lexer::isKeyword( int id) const
{
	return m_id2keyword[ id];
}

Lexem Lexer::next( Scanner& scanner) const
{
	for (;;)
	{
		char const* start = scanner.next();
		if (*start == '\0')
		{
			if (m_indentLexems.defined() && scanner.getEofIndentToken() == Scanner::IndentClose)
			{
				return Lexem( m_namelist[ m_indentLexems.close-1], m_indentLexems.close, ""/*value*/, scanner.line());
			}
			return Lexem( scanner.line());
		}
		auto range = m_firstmap.equal_range( *start);
		int maxlen = 0;
		int matchidx = -1;
		const char* matchstart = nullptr;
		std::size_t matchsize = 0;
		auto ri = range.first;
		for (; ri != range.second; ++ri)
		{
			int idx = ri->second;
			if (idx == MATCH_EOLN_COMMENT)
			{
				if (matchEolnComment( scanner))
				{
					if (!scanner.scan( "\n"))
					{
						return Lexem( scanner.line()); //... no EOLN then return EOF
					}
					break; //... comment skipped, fetch next lexem
				}
			}
			else if (idx == MATCH_BRACKET_COMMENT)
			{
				int cidx = matchBracketCommentStart( scanner);
				if (cidx >= 0)
				{
					if (!scanner.scan( m_bracketComments[ cidx].close.c_str()))
					{
						return Lexem( m_errorLexem.value, -1/*id*/, m_bracketComments[ cidx].open, scanner.line());
						//... no matching end bracket of comment found, then return ERROR
					}
					break; //... comment skipped, fetch next lexem
				}
			}
			else
			{
				if (idx >= (int)m_defar.size()) throw Error( Error::ArrayBoundReadInLexer);
				auto mm = m_defar[ idx].match( start, scanner.restsize());
				if (maxlen <= mm.second && (maxlen < mm.second || m_defar[ idx].keyword())) //... keywords preferred if length is the same
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
				return Lexem( m_errorLexem.value, -1/*id*/, chr, line);
			}
			else if (0 != m_defar[ matchidx].id())
			{
				if (m_indentLexems.defined())
				{
					switch (scanner.getIndentToken( m_indentLexems.tabSize))
					{
						case Scanner::IndentNone: break;
						case Scanner::IndentNewLine: return Lexem( m_namelist[ m_indentLexems.newLine-1], m_indentLexems.newLine, ""/*value*/, line);
						case Scanner::IndentOpen: return Lexem( m_namelist[ m_indentLexems.open-1], m_indentLexems.open, ""/*value*/, line);
						case Scanner::IndentClose: return Lexem( m_namelist[ m_indentLexems.close-1], m_indentLexems.close, ""/*value*/, line);
					}
				}
				scanner.next( maxlen);
				return Lexem( m_defar[ matchidx].name(), m_defar[ matchidx].id(), std::string_view( matchstart, matchsize), line);
			}
		}
	}
}

std::vector<Lexer::Definition> Lexer::getDefinitions() const
{
	std::vector<Lexer::Definition> rt;
	if (m_errorLexem.value != "?")
	{
		rt.push_back( Definition( m_errorLexem.line, Definition::BadLexem, {m_errorLexem.value}, 0/*select*/, 0/*id*/, ""/*activation*/));
	}
	for (auto const& def : m_defar)
	{
		if (def.name().empty())
		{
			rt.push_back( Definition( def.line(), Definition::IgnoreLexem, {def.source()}, 0/*select*/,def.id(), def.activation()));
		}
		else if (def.keyword())
		{
			rt.push_back( Definition( def.line(),  Definition::KeywordLexem, {def.name()}, 0/*select*/, def.id(), def.activation()));
		}
		else
		{
			rt.push_back( Definition( def.line(), Definition::NamedPatternLexem, {def.name(), def.source()}, def.select(), def.id(), def.activation()));
		}
	}
	for (auto const& bc : m_bracketComments)
	{
		rt.push_back(
			Definition( bc.line, Definition::BracketComment, {bc.open,bc.close}, 0/*select*/,
					MATCH_BRACKET_COMMENT/*id*/, std::string( bc.open.c_str(), 1)/*activation*/));
	}
	for (auto const& cc : m_eolnComments)
	{
		rt.push_back(
			Definition( cc.line, Definition::EolnComment, {cc.open}, 0/*select*/,
					MATCH_EOLN_COMMENT/*id*/, std::string( cc.open.c_str(), 1)/*activation*/));
	}
	if (m_indentLexems.defined())
	{
		rt.push_back(
			Definition( m_indentLexems.line, Definition::IndentLexems,
				    {}, m_indentLexems.tabSize/*select*/,m_indentLexems.open/*id*/, std::string()/*activation*/));
	}
	return rt;
}

Lexer::Lexer( const std::vector<Definition>& definitions)
{
	for (auto const& def : definitions)
	{
		switch (def.type())
		{
			case Definition::BadLexem:		defineBadLexem( def.line(), def.name()); break;
			case Definition::NamedPatternLexem:	defineLexem( def.line(), def.name(), def.pattern(), def.select()); break;
			case Definition::KeywordLexem:		defineLexem( def.name()); break;
			case Definition::IgnoreLexem:		defineIgnore( def.line(), def.pattern());
			case Definition::EolnComment:		defineEolnComment( def.line(), def.start()); break;
			case Definition::BracketComment:	defineBracketComment( def.line(), def.start(), def.end()); break;
			case Definition::IndentLexems:		m_indentLexems = {def.line(),def.openind(),def.closeind(),def.newline(),def.tabsize()}; break;
		}
	}
}

