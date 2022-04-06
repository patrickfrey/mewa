/// \brief Language definition structure to build the automaton
/// \file "languagedef.hpp"
#ifndef _MEWA_LANGUAGEDEF_HPP_INCLUDED
#define _MEWA_LANGUAGEDEF_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "automaton_structs.hpp"
#include "languagedef.hpp"
#include "error.hpp"
#include "lexer.hpp"
#include <utility>
#include <string>
#include <string_view>
#include <set>
#include <map>
#include <vector>

namespace mewa {

struct LanguageDef
{
	std::string language;
	std::string typesystem;
	std::string cmdline;
	Lexer lexer;
	std::vector<ProductionDef> prodlist;
	std::vector<Automaton::Call> calls;
	std::vector<std::string> nonterminals;

	LanguageDef() = default;
	LanguageDef( const LanguageDef& o) = default;
	LanguageDef( LanguageDef&& o) noexcept = default;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif
