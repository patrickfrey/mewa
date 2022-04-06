/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/// \brief Function to print Lexem definitions as Lua table
/// \file "automaton_structs.cpp"
#include "automaton_structs.hpp"

using namespace mewa;

void LanguageDecoratorMap::addDecorator( int line_, const LanguageDecorator& dc)
{
	linemap.insert( {line_, decorators.size()});
	decorators.push_back( dc);
}

std::vector<LanguageDecorator> LanguageDecoratorMap::get( int line) const
{
	std::vector<LanguageDecorator> rt;
	auto range = linemap.equal_range( line);
	auto li = range.first;
	auto le = range.second;
	for (; li != le; ++li)
	{
		rt.push_back( decorators[ li->second]);
	}
	return rt;
}

