/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
	LexemDef( int line_, const std::string& name_, const std::string& source_, int id_, bool keyword_, std::size_t select_)
		:m_line(line_),m_name(name_),m_source(source_),m_pattern(source_),m_activate(),m_select(select_),m_id(id_),m_keyword(keyword_)
	{
		std::string achrs( activation( source_));
		for (char ch : achrs) {m_activate.set( (unsigned char)ch);}
	}
	LexemDef( int line_, const std::string& name_, int id_)
		:m_line(line_),m_name(name_),m_source(),m_pattern(),m_activate(),m_select(0),m_id(id_),m_keyword(false)
	{}
	LexemDef( const LexemDef& o) = default;
	LexemDef& operator=( const LexemDef& o) = default;
	LexemDef( LexemDef&& o) = default;
	LexemDef& operator=( LexemDef&& o) = default;

	int line() const noexcept				{return m_line;}
	const std::string& name() const noexcept		{return m_name;}
	const std::string& source() const noexcept		{return m_source;}
	const std::bitset<256> activate() const noexcept	{return m_activate;}
	std::size_t select() const noexcept			{return m_select;}
	int id() const noexcept					{return m_id;}
	bool keyword() const noexcept				{return m_keyword;}
	std::string activation() const				{return activation( m_source);}

	std::pair<std::string_view,int> match( const char* srcptr, std::size_t srclen) const;

private:
	static std::string activation( const std::string& source_);

private:
	int m_line;
	std::string m_name;
	std::string m_source;
	std::regex m_pattern;
	std::bitset<256> m_activate;
	std::size_t m_select;
	int m_id;
	bool m_keyword;
};


class Lexem
{
public:
	explicit Lexem( int line_) noexcept //...empty Lexem ~ EOF
		:m_name(),m_value(),m_id(0),m_line(line_){}
	Lexem( const std::string_view& name_, int id_, const std::string_view& value_, int line_) noexcept
		:m_name(name_),m_value(value_),m_id(id_),m_line(line_){}
	Lexem( const Lexem& o) noexcept
		:m_name(o.m_name),m_value(o.m_value),m_id(o.m_id),m_line(o.m_line){}
	Lexem& operator=( const Lexem& o) noexcept
		{m_name=o.m_name; m_value=o.m_value; m_id=o.m_id; m_line=o.m_line; return *this;}

	const std::string_view& name() const noexcept	{return m_name;}
	const std::string_view& value() const noexcept	{return m_value;}
	int id() const noexcept				{return m_id;}
	int line() const noexcept			{return m_line;}
	bool empty() const noexcept			{return m_value.empty() && m_name.empty();}

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
		:m_src(src_),m_srcitr(src_.data()),m_line(1),m_indentstk(),m_indentConsumed(false){void checkNullTerminated();}
	explicit Scanner( const std::string& src_)
		:m_src(src_),m_srcitr(src_.c_str()),m_line(1),m_indentstk(),m_indentConsumed(false){}
	Scanner( const Scanner& o)
		:m_src(o.m_src),m_srcitr(o.m_srcitr),m_line(o.m_line),m_indentstk(o.m_indentstk),m_indentConsumed(o.m_indentConsumed){}
	Scanner& operator=( const Scanner& o)
		{m_src=o.m_src; m_srcitr=o.m_srcitr; m_line=o.m_line; m_indentstk=o.m_indentstk; m_indentConsumed=o.m_indentConsumed; return *this;}

	int line() const noexcept			{return m_line;}

	char const* next( int incr=0);
	char const* current() const noexcept		{return m_srcitr;}
	bool scan( const char* end);
	bool match( const char* str);
	int indentCount( int tabSize) const noexcept;

	std::size_t restsize() const noexcept		{return m_src.size() - (m_srcitr - m_src.data());}

	enum IndentToken { IndentNone, IndentNewLine, IndentOpen, IndentClose};
	IndentToken getIndentToken( int tabSize);
	IndentToken getEofIndentToken();

private:
	void checkNullTerminated();
private:
	std::string_view m_src;
	char const* m_srcitr;
	int m_line;
	std::vector<int> m_indentstk;
	bool m_indentConsumed;
};


class Lexer
{
public:
	class Definition
	{
	public:
		enum Type
		{
			BadLexem,
			NamedPatternLexem,
			KeywordLexem,
			IgnoreLexem,
			EolnComment,
			BracketComment,
			IndentLexems
		};
		static const char* typeName( Type type_)
		{
			static const char* ar[] = {"bad","token","keyword","ignore","comment","comment","indentl"};
			return ar[ type_];
		}
		const char* typeName() const noexcept
		{
			return typeName( m_type);
		}
		Definition( const Definition& o) = default;
		Definition( int line_, Type type_, const std::vector<std::string>& arg_, int select_, int id_, const std::string& activation_)
			:m_line(line_),m_type(type_),m_arg(arg_),m_select(select_),m_id(id_),m_activation(activation_){}

		Type type() const noexcept		{return m_type;}
		int id() const noexcept			{return m_id;}
		int line() const noexcept		{return m_line;}
		const std::string& name() const		{return m_arg[0];}
		const std::string bad() const		{return m_arg.empty() ? std::string() : m_arg[0];}
		const std::string pattern() const	{return m_arg.size() <= 1 ? std::string() : m_arg[1];}
		int select() const noexcept		{return m_select;}
		const std::string ignore() const	{return m_arg.empty() ? std::string() : m_arg[0];}
		const std::string start() const		{return m_arg.empty() ? std::string() : m_arg[0];}
		const std::string end() const		{return m_arg.size() <= 1 ? std::string() : m_arg[1];}
		const std::string& activation() const	{return m_activation;}

		int tabsize() const noexcept		{return m_select;}
		int openind() const noexcept		{return m_id+0;}
		int closeind() const noexcept		{return m_id+1;}
		int newline() const noexcept		{return m_id+2;}

	private:
		int m_line;
		Type m_type;
		std::vector<std::string> m_arg;
		int m_select;
		int m_id;
		std::string m_activation;
	};

public:
	Lexer()
		:m_errorLexem(0/*no line*/,"?"),m_defar(),m_firstmap(),m_nameidmap(),m_namelist()
		,m_id2keyword(),m_bracketComments(),m_eolnComments()
		,m_indentLexems({0,0,0,0}){}
	Lexer( const Lexer& o) = default;
	Lexer& operator=( const Lexer& o) = default;
	Lexer( Lexer&& o) = default;
	Lexer& operator=( Lexer&& o) = default;
	Lexer( const std::vector<Definition>& definitions);

	void defineBadLexem( int line, const std::string_view& name_);

	int defineLexem( int line, const std::string_view& name, const std::string_view& pattern, std::size_t select=0);
	int defineLexem( const std::string_view& opr);

	void defineIgnore( int line, const std::string_view& pattern);
	void defineEolnComment( int line, const std::string_view& opr);
	void defineBracketComment( int line, const std::string_view& start, const std::string_view& end);

	void defineIndentLexems( int line, const std::string_view& open, const std::string_view& close, const std::string_view& newline, int tabsize);
	struct IndentLexems
	{
		int line;
		int open;
		int close;
		int newLine;
		int tabSize;

		bool defined() const noexcept {return open;}
	};
	const IndentLexems& indentLexems() const noexcept
	{
		return m_indentLexems;
	}
	void setIndentLexems( const IndentLexems& il)
	{
		m_indentLexems = il;
	}

	int lexemId( const std::string_view& name) const noexcept;
	const std::string& lexemName( int id) const;
	bool isKeyword( int id) const;
	Lexem next( Scanner& scanner) const;

	int nofTerminals() const noexcept		{return m_namelist.size();}

	std::vector<Definition> getDefinitions() const;

private:
	int defineLexem_( int line, const std::string_view& name, const std::string_view& pattern, bool keyword_, std::size_t select);
	int defineLexemId( const std::string_view& name_);

private:
	struct BracketCommentDef
	{
		int line;
		std::string open;
		std::string close;

		BracketCommentDef( int line_, const std::string& open_, std::string close_) :line(line_),open(open_),close(close_){}
		BracketCommentDef( const BracketCommentDef& o) = default;
		BracketCommentDef& operator=( const BracketCommentDef& o) = default;
		BracketCommentDef( BracketCommentDef&& o) = default;
		BracketCommentDef& operator=( BracketCommentDef&& o) = default;
	};

	struct EolnCommentDef
	{
		int line;
		std::string open;

		EolnCommentDef( int line_, const std::string& open_) :line(line_),open(open_){}
		EolnCommentDef( const EolnCommentDef& o) = default;
		EolnCommentDef& operator=( const EolnCommentDef& o) = default;
		EolnCommentDef( EolnCommentDef&& o) = default;
		EolnCommentDef& operator=( EolnCommentDef&& o) = default;
	};

	struct ErrorLexemDef
	{
		int line;
		std::string value;

		ErrorLexemDef() :line(0),value(){}
		ErrorLexemDef( int line_, const std::string& value_) :line(line_),value(value_){}
		ErrorLexemDef( const ErrorLexemDef& o) = default;
		ErrorLexemDef& operator=( const ErrorLexemDef& o) = default;
		ErrorLexemDef( ErrorLexemDef&& o) = default;
		ErrorLexemDef& operator=( ErrorLexemDef&& o) = default;
	};

	typedef std::vector<BracketCommentDef> BracketCommentDefList;
	typedef std::vector<EolnCommentDef> EolnCommentDefList;

private:
	bool matchEolnComment( Scanner& scanner) const;
	int matchBracketCommentStart( Scanner& scanner) const;

private:
	ErrorLexemDef m_errorLexem;
	std::vector<LexemDef> m_defar;
	std::multimap<unsigned char,int> m_firstmap;
	std::map<std::string, int, std::less<> > m_nameidmap;
	std::vector<std::string> m_namelist;
	std::vector<bool> m_id2keyword;
	BracketCommentDefList m_bracketComments;
	EolnCommentDefList m_eolnComments;
	IndentLexems m_indentLexems;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif

