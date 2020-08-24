/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Scope test program
/// \file "testScope.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "scope.hpp"
#include "error.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mewa;

class TestKey
	:public ScopedKey<std::string>
{
public:
	TestKey( const char* key_, const Scope scope_)
		:ScopedKey<std::string>( key_, scope_){}
};

class TestKeyValue
	:public TestKey
{
public:
	TestKeyValue( const char* key_, const Scope scope_, const char* value_)
		:TestKey( key_, scope_),m_value(value_){}

	const std::string& value() const		{return m_value;}

private:
	std::string m_value;
};

int main( int argc, const char* argv[] )
{
	try
	{
		std::ostringstream output;

		static const std::vector<TestKeyValue> tests = {
			{"Ali", {1,120}, "Ali 1"},
			{"Ali", {3,99}, "Ali 2"},
			{"Ali", {7,77}, "Ali 3"},
			{"Ala", {2,121}, "Ala 1"},
			{"Ala", {3,98}, "Ala 2"},
			{"Ala", {7,77}, "Ala 3"},
			{"Baba", {2,129}, "Baba 1"},
			{"Baba", {3,93}, "Baba 2"},
			{"Baba", {7,79}, "Baba 3"}
		};
		ScopedMap<std::string,std::string> smap;
		for (auto test : tests)
		{
			smap.insert( ScopedMap<std::string,std::string>::value_type(
					ScopedKey<std::string>( test.key(), test.scope()), test.value()));
		}
		auto rg = smap.scoped_find_range( ScopedKey<std::string>( "Ali", 3));
		for (auto ri = rg.first; ri != rg.second; ++ri)
		{
			std::cerr << ri->first.scope().tostring() << " " << ri->first.key() << " " << ri->second << std::endl;
			output << ri->first.scope().tostring() << " " << ri->first.key() << " " << ri->second << std::endl;
		}
		auto ri = smap.scoped_find_inner( ScopedKey<std::string>( "Ala", 2));
		std::cerr << std::endl << ri->first.scope().tostring() << " " << ri->first.key() << " " << ri->second << std::endl;
		output << std::endl << ri->first.scope().tostring() << " " << ri->first.key() << " " << ri->second << std::endl;

		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << (int)err.code() << " " << err.arg() << std::endl;
		return (int)err.code();
	}
	catch (const std::runtime_error& err)
	{
		std::cerr << "ERR runtime " << err.what() << std::endl;
		return 1;
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << "ERR out of memory" << std::endl;
		return 2;
	}
	return 0;
}

