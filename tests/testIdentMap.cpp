/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief IdentMap test program
/// \file "testIdentMap.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif

#include "identmap.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include "utilitiesForTests.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <unordered_map>

using namespace mewa;

static PseudoRandom g_random;

static std::string genTestKey( const char* alphabet, std::size_t maxkeylen)
{
	std::string rt;
	int keylen = g_random.get( 0, g_random.get( 0, g_random.get( 0, maxkeylen)));
	int ki = 0;
	for (; ki < keylen; ++ki)
	{
		char ch;
		if (alphabet)
		{
			ch = alphabet[ g_random.get( 0, std::strlen( alphabet))];
		}
		else
		{
			ch = g_random.get( 1, 256);
		}
		rt.push_back( ch);
	}
	return rt;
}

static bool isDigit( char ch) noexcept
{
	return ch >= '0' && ch <= '9';
}

static int getCardinalNumberArgument( const char* arg, const char* what)
{
	char const* ai = arg;
	int rt = 0;
	for (; isDigit(*ai); ++ai)
	{
		rt = rt * 10 + (*ai - '0');
		if (rt < 0) throw std::runtime_error( string_format( "value %s out of range", what));
	}
	if (*ai || rt == 0)
	{
		throw std::runtime_error( string_format( "value %s is not a positive number", what));
	}
	return rt;
}

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int argi = 1;
		int nofInitBuckets = 1024 * 1024;
		int nofTests = 100000;
		int maxKeyLength = 200;

		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testIdentMap [-h][-V] [<nof tests>] [<max key length>] [<nof buckets>]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testIdentMap [-h][-V] [<nof tests>] [<max key length>] [<nof buckets>]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc) nofTests = getCardinalNumberArgument( argv[ argi++], "number of tests");
		if (argi < argc) maxKeyLength = getCardinalNumberArgument( argv[ argi++], "max key length");
		if (argi < argc) nofInitBuckets = getCardinalNumberArgument( argv[ argi++], "number of buckets on initialization");

		if (argi < argc)
		{
			std::cerr << "Usage: testIdentMap [-h][-V] [<nof tests>] [<max key length>] [<nof buckets>]" << std::endl;
			throw std::runtime_error( "too many arguments");
		}
		static const char* alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJLMNOPQRSTUVWXYZ_-0123456789     ";

		std::unordered_map<std::string,int> refmap;
		std::unordered_map<std::size_t,std::string> hashmap;

		struct Buffer
		{
			void* ptr;

			Buffer( void* ptr_) :ptr(ptr_){}
			~Buffer() {if (ptr) std::free( ptr);}
		};
		std::size_t map_bufsize = nofInitBuckets * sizeof( std::pair<IdentKeyEnvelop, int>);
		std::size_t str_bufsize = nofTests * (maxKeyLength / 10 + 3);
		Buffer map_buffer( std::malloc( map_bufsize));
		Buffer str_buffer( std::malloc( str_bufsize));

		std::pmr::monotonic_buffer_resource map_memrsc( map_buffer.ptr, map_bufsize);
		std::pmr::monotonic_buffer_resource str_memrsc( str_buffer.ptr, str_bufsize);

		IdentMap testmap( &map_memrsc, &str_memrsc, nofInitBuckets);
		int countHashCollisions = 0;

		for (int ti=0; ti<nofTests; ++ti)
		{
			std::string key = genTestKey( alphabet, maxKeyLength);
			auto ins = refmap.insert( {key, ti+1});
			while (ins.second == false)
			{
				if (testmap.get( key) != ins.first->second)
				{
					throw std::runtime_error( string_format( "ident assigned to existing key '%s' does not match value %d", key.c_str(), ins.first->second));
				}
				key = genTestKey( alphabet, maxKeyLength);
				ins = refmap.insert( {key, ti+1});
			}
			// Test quality of hashing (number of collisions):
			std::size_t hs = testmap.hash( key);
			auto hins = hashmap.insert( {hs, key});
			if (hins.second == false)
			{
				if (verbose)
				{
					std::cerr
						<< string_format( "hash collision (%zu) for keys '%s' and '%s'", hs, key.c_str(), hins.first->second.c_str())
						<< std::endl;
				}
				++countHashCollisions;
			}

			// Test value of id assigned to inserted key:
			int testid = testmap.get( key);
			if (testid != ins.first->second)
			{
				throw std::runtime_error(
					string_format( "ident value %d assigned to new key '%s' does not match value %d",
							testid, key.c_str(), ins.first->second));
			}
			if (verbose) std::cerr << string_format( "test key '%s'", key.c_str()) << std::endl;
		}
		for (auto kv : refmap)
		{
			if (verbose) std::cerr << string_format( "lookup key '%s'", kv.first.c_str()) << std::endl;
			if (testmap.get( kv.first) != kv.second)
			{
				throw std::runtime_error( string_format( "ident of key '%s' not assigned to value %d", kv.first.c_str(), kv.second));
			}
		}
		int ki = 1, ke = testmap.size()+1;
		for (; ki != ke; ++ki)
		{
			std::string_view key = testmap.inv( ki);
			if (verbose) std::cerr << string_format( "verify key '%s'", key.data()) << std::endl;
			auto fi = refmap.find( std::string(key.data()));
			if (fi == refmap.end())
			{
				throw std::runtime_error( string_format( "ident assigned to key '%s' not found", key.data()));
			}
			else if (fi->second != ki)
			{
				throw std::runtime_error( string_format( "ident assigned to key '%s' does not match value %d (%d)", key.data(), ki, fi->second));
			}
		}
		std::cerr << string_format( "verified %d test key inserts with %d hash collisions", nofTests, countHashCollisions) << std::endl;
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

