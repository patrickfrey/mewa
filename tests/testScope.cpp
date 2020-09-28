/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Scope test program
/// \file "testScope.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include "scope.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <limits>
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
	Scope::Step step;
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
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testScope [-h][-V]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testScope [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testScope [-h][-V]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
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

		int buffer[ 2048];
		std::pmr::monotonic_buffer_resource memrsc( buffer, sizeof buffer);

		ScopedMap<std::string,std::string> smap( &memrsc);
		for (auto test : tests)
		{
			smap.insert( ScopedMap<std::string,std::string>::value_type(
					ScopedKey<std::string>( test.key(), test.scope()), test.value()));
		}
		int ti = 0;
		for (; queries[ti].key; ++ti)
		{
			if (verbose) std::cerr << "Find '" << queries[ti].key << "'/" << queries[ti].step << ": " << std::endl;
			auto ri = smap.scoped_find( queries[ti].key, queries[ti].step);
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
		else
		{
			removeFile( "build/testScope.out");
			removeFile( "build/testScope.exp");
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

