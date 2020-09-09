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
#include "fileio.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

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

struct TestCase
{
	const char* key;
	Scope::Timestmp timestmp;
};

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testScope [-h][-V]" << std::endl;
			}
		}

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
		static const TestCase queries[] = {{"Ali",3},{"Ala",2},{"Baba",5},{"Baba",79},{"Baba",78},{0,0}};
		std::string expected{R"(
[3,99] Ali -> Ali 2
[2,121] Ala -> Ala 1
[3,93] Baba -> Baba 2
[3,93] Baba -> Baba 2
[7,79] Baba -> Baba 3
)"};
		std::ostringstream outputbuf;
		outputbuf << "\n";

		ScopedMap<std::string,std::string> smap;
		for (auto test : tests)
		{
			smap.insert( ScopedMap<std::string,std::string>::value_type(
					ScopedKey<std::string>( test.key(), test.scope()), test.value()));
		}
		int ti = 0;
		for (; queries[ti].key; ++ti)
		{
			if (verbose) std::cerr << "Find '" << queries[ti].key << "'/" << queries[ti].timestmp << ": " << std::endl;
			auto ri = smap.scoped_find( queries[ti].key, queries[ti].timestmp);
			if (ri != smap.end())
			{
				if (verbose)
				{
					std::cerr
						<< ri->first.scope().tostring() << " "
						<< ri->first.key() << " -> " << ri->second << std::endl;
				}
				outputbuf 
					<< ri->first.scope().tostring() << " "
					<< ri->first.key() << " -> " << ri->second << std::endl;
			}
		}
				std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testScope.out", output);
			writeFile( "build/testScope.exp", expected);
			std::cerr << "ERR test output (build/testScope.out) differs expected build/testScope.exp" << std::endl;
			return 3;
		}
		std::cerr << "OK" << std::endl;
		return 0;
	}
	catch (const mewa::Error& err)
	{
		std::cerr << "ERR " << err.what() << std::endl;
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

