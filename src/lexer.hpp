/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Lexer interface
/// \file "lexer.hpp"
#ifndef _MEWA_LEXER_HPP_INCLUDED
#define _MEWA_LEXER_HPP_INCLUDED
#if __cplusplus >= 201703L
#include <utility>
#include <regex>
#include <string>
#include <string_view>
#include <map>
#include <set>
#include <vector>

namespace mewa {

class LexemDef
{
public:
	LexemDef( const std::string& name_, const std::string& source_, int id_, bool keyword_, std::size_t select_)
		:m_name(name_),m_source(source_),m_pattern(source_),m_activate(),m_select(select_),m_id(id_),m_keyword(keyword_)
	{
		std::string achrs( activationCharacters( source_));
		for (char ch : achrs) {m_activate.set( (unsigned char)ch);}
	}

	LexemDef( const LexemDef& o)
		:m_name(o.m_name),m_source(o.m_source)
		,m_pattern(o.m_pattern),m_activate(o.m_activate)
		,m_select(o.m_select),m_id(o.m_id),m_keyword(o.m_keyword){}
	LexemDef& operator=( const LexemDef& o)
		{m_name=o.m_name; m_source=o.m_source;
		 m_pattern=o.m_pattern; m_activate=o.m_activate;
		 m_select=o.m_select; m_id=o.m_id; m_keyword=o.m_keyword;
		 return *this;}
	LexemDef( LexemDef&& o)
		:m_name(std::move(o.m_name)),m_source(std::move(o.m_source))
		,m_pattern(std::move(o.m_pattern)),m_activate(std::move(o.m_activate))
		,m_select(o.m_select),m_id(o.m_id),m_keyword(o.m_keyword){}
	LexemDef& operator=( LexemDef&& o)
		{m_name=std::move(o.m_name); m_source=std::move(o.m_source);
		 m_pattern=std::move(o.m_pattern); m_activate=std::move(o.m_activate);
		 m_select=o.m_select; m_id=o.m_id; m_keyword=o.m_keyword;
		 return *this;}

	const std::string& name() const			{return m_name;}
	const std::string& source() const		{return m_source;}
	const std::bitset<128> activate() const		{return m_activate;}
	std::size_t select() const			{return m_select;}
	int id() const					{return m_id;}
	bool keyword() const				{return m_keyword;}

	std::pair<std::string_view,int> match( const char* srcptr, std::size_t srclen) const;

private:
	static std::string activationCharacters( const std::string& source_);

private:
	std::string m_name;
	std::string m_source;
	std::regex m_pattern;
	std::bitset<128> m_activate;
	std::size_t m_select;
	int m_id;
	bool m_keyword;
};

class Lexem
{
public:
	explicit Lexem( int line_) //...empty Lexem ~ EOF
		:m_name(),m_value(),m_id(0),m_line(line_){}
	Lexem( const std::string_view& name_, int id_, const std::string_view& value_, int line_)
		:m_name(name_),m_value(value_),m_id(id_),m_line(line_){}
	Lexem( const Lexem& o)
		:m_name(o.m_name),m_value(o.m_value),m_id(o.m_id),m_line(o.m_line){}
	Lexem& operator=( const Lexem& o)
		{m_name=o.m_name; m_value=o.m_value; m_id=o.m_id; m_line=o.m_line; return *this;}
	Lexem( Lexem&& o)
		:m_name(std::move(o.m_name)),m_value(std::move(o.m_value)),m_id(o.m_id),m_line(o.m_line){}
	Lexem& operator=( Lexem&& o)
		{m_name=std::move(o.m_name); m_value=std::move(o.m_value); m_id=o.m_id; m_line=o.m_line; return *this;}

	const std::string_view& name() const	{return m_name;}
	const std::string_view& value() const	{return m_value;}
	int id() const				{return m_id;}
	int line() const			{return m_line;}
	bool empty() const			{return m_value.empty() && m_name.empty();}

private:
	std::string_view m_name;
	std::string_view m_value;
	int m_id;
	int m_line;
};

class Scanner
{
public:
	explicit Scanner( const std::string_view& src_)
		:m_src(src_),m_line(1){m_srcitr=m_src.c_str();}
	Scanner( const Scanner& o)
		:m_src(o.m_src),m_line(o.m_line)
	{
		m_srcitr = m_src.c_str() + (o.m_srcitr - o.m_src.c_str());
	}
	Scanner& operator=( const Scanner& o)
		{m_src=o.m_src; m_srcitr=o.m_srcitr; m_line=o.m_line;
		 return *this;}
	Scanner( Scanner&& o)
		:m_src(std::move(o.m_src)) ,m_srcitr(o.m_srcitr),m_line(o.m_line){}
	Scanner& operator=( Scanner&& o)
		{m_src=std::move(o.m_src); m_srcitr=o.m_srcitr; m_line=o.m_line;
		 return *this;}

	const std::string& src() const		{return m_src;}
	int line() const			{return m_line;}

	char const* next( int incr=0);
	bool scan( const char* end);
	bool match( const char* str);
	std::size_t restsize() const		{return m_src.size() - (m_srcitr - m_src.c_str());}

private:
	std::string m_src;
	char const* m_srcitr;
	int m_line;
};

class Lexer
{
public:
	Lexer()
		:m_errorLexemName("?"),m_defar(),m_firstmap(),m_nameidmap(),m_namelist(),m_bracketComments(),m_eolnComments(){}
	Lexer( const Lexer& o)
		:m_errorLexemName(o.m_errorLexemName),m_defar(o.m_defar)
		,m_firstmap(o.m_firstmap),m_nameidmap(o.m_nameidmap),m_namelist(o.m_namelist)
		,m_bracketComments(o.m_bracketComments),m_eolnComments(o.m_eolnComments){}
	Lexer& operator=( const Lexer& o)
		{m_errorLexemName=o.m_errorLexemName; m_defar=o.m_defar;
		 m_firstmap=o.m_firstmap; m_nameidmap=o.m_nameidmap; m_namelist=o.m_namelist;
		 m_bracketComments=o.m_bracketComments; m_eolnComments=o.m_eolnComments;
		 return *this;}
	Lexer( Lexer&& o)
		:m_errorLexemName(std::move(o.m_errorLexemName)),m_defar(std::move(o.m_defar))
		,m_firstmap(std::move(o.m_firstmap)),m_nameidmap(std::move(o.m_nameidmap)),m_namelist(std::move(o.m_namelist))
		,m_bracketComments(std::move(o.m_bracketComments)),m_eolnComments(std::move(o.m_eolnComments)){}
	Lexer& operator=( Lexer&& o)
		{m_errorLexemName=std::move(o.m_errorLexemName); m_defar=std::move(o.m_defar);
		 m_firstmap=std::move(o.m_firstmap); m_nameidmap=std::move(o.m_nameidmap); m_namelist=std::move(o.m_namelist);
		 m_bracketComments=std::move(o.m_bracketComments); m_eolnComments=std::move(o.m_eolnComments);
		 return *this;}

	void defineBadLexem( const std::string_view& name_);

	void defineLexem( const std::string_view& name, const std::string_view& pattern, std::size_t select=0);
	void defineLexem( const std::string_view& opr);
	void defineIgnore( const std::string_view& pattern);
	void defineEolnComment( const std::string_view& opr);
	void defineBracketComment( const std::string_view& start, const std::string_view& end);

	int lexemId( const std::string_view& name) const;
	std::string_view lexemName( int id) const;
	Lexem next( Scanner& scanner) const;

private:
	void defineLexem_( const std::string_view& name, const std::string_view& pattern, bool keyword_, std::size_t select);

private:
	typedef std::pair<std::string,std::string> BracketCommentDef;
	typedef std::vector<BracketCommentDef> BracketCommentDefList;
	typedef std::vector<std::string> EolnCommentDefList;

private:
	bool matchEolnComment( Scanner& scanner) const;
	int matchBracketCommentStart( Scanner& scanner) const;

private:
	std::string m_errorLexemName;
	std::vector<LexemDef> m_defar;
	std::multimap<char,int> m_firstmap;
	std::map<std::string, int, std::less<> > m_nameidmap;
	std::vector<std::string> m_namelist;
	BracketCommentDefList m_bracketComments;
	EolnCommentDefList m_eolnComments;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

