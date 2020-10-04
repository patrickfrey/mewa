/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Typedb test program
/// \file "testTypeDb.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include "typedb.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include <utility>
#include <memory>
#include <cstring>

using namespace mewa;

struct TestTypeDef
{
	char const* ar[12];		//< nullptr terminated list of items
	char const* constructor;
	int priority;
};

struct TestReductionDef
{
	char const* fromType;
	char const* toType;
	char const* constructor;
};

struct TestScopeTypeDefs
{
	Scope scope;
	TestReductionDef redus[ 32]; 	//< {nullptr,..} terminated list of reduction definitions
	TestTypeDef types[ 32];		//< {nullptr,..} terminated list of type definitions
};

static int nofTests = 1;
static TestScopeTypeDefs tests[1] = {
	{
		{0,1000},
		{
			{"byte","uint","byte2uint"},
			{"uint","byte","uint2byte"},
			{"int","uint","int2uint"},
			{"uint","int","uint2int"},
			{"long","int","long2int"},
			{"int","long","int2long"},
			{"uint","long","uint2long"},
			{"long","uint","long2uint"},
			{"long","double","long2double"},
			{"double","long","double2long"},
			{"int","float","int2float"},
			{"float","int","float2int"},
			{"float","double","float2double"},
			{"double","float","double2float"},
			{nullptr,nullptr,nullptr}
		},
		{
			{{"", "byte", nullptr}, "#byte", 0},
			{{"", "int", nullptr}, "#int", 0},
			{{"", "uint", nullptr}, "#uint", 0},
			{{"", "long", nullptr}, "#long", 0},
			{{"", "float", nullptr}, "#float", 0},
			{{"", "double", nullptr}, "#double", 0},
			{{"", "func_int_int_int", "int", "int", "int", nullptr}, "#func_int_int_int", 0},
			{{"", "func_uint_int_int", "uint", "int", "int", nullptr}, "#func_uint_int_int", 0},
			{{"", "func_double_int_int", "double", "int", "int", nullptr}, "#func_double_int_int", 0},
			{{"", "func_double_byte_uint", "double", "byte", "uint", nullptr}, "#func_double_byte_uint", 0},
			{{"", "func_long_uint_uint", "long", "uint", "uint", nullptr}, "#func_long_uint_uint", 0},
			{{"", "func_byte_int_int", "byte", "int", "int", nullptr}, "#func_byte_int_int", 0},
			{{"", "func_byte_int", "byte", "int", nullptr}, "#func_byte_int", 0},
			{{"", "func_int_int", "int", "int", nullptr}, "#func_int_int", 0},
			{{"", "func_int_long", "int", "long", nullptr}, "#func_int_long", 0},
			{{"", "func_int_double", "int", "double", nullptr}, "#func_int_double", 0},
			{{"", "func_float_float", "float", "float", nullptr}, "#func_float_float", 0},
			{{"", "func_double_double", "double", "double", nullptr}, "#func_double_double", 0},
			{{nullptr}, nullptr, 0}
		}
	}
};

static int getContextType( TypeDatabase* typedb, const Scope::Step step, const char* tpstr)
{
	std::vector<std::string_view> split;
	char const* si = tpstr;
	char const* sn = std::strchr( si, ' ');
	for (; sn; si=sn+1,sn = std::strchr( si, ' '))
	{
		split.push_back( {si, std::size_t(sn-si)});
	}
	if (*si)
	{
		split.push_back( si);
	}
	int contextType = 0;
	for (auto const& item : split)
	{
		TypeDatabase::ResultBuffer resbuf;
		auto res = typedb->resolve( step, contextType, item, 0, resbuf);
		if (res.items.empty() != 1) throw Error( Error::UnresolvableType, item);
		if (res.items.size() > 1) throw Error( Error::AmbiguousTypeReference, item);

		contextType = res.items[0].type;
	}
	return contextType;
}

struct TypeDatabaseImpl
{
	TypeDatabase* typedb;
	std::map<std::string,int> constructorMap;
	std::vector<std::string> constructorInv;

	explicit TypeDatabaseImpl( TypeDatabase* typedb_)
		:typedb(typedb_),constructorMap(){}

	int getConstructorFromName( const std::string& name)
	{
		auto ins = constructorMap.insert( {name,constructorInv.size()+1} );
		if (ins.second/*insert took place*/)
		{
			constructorInv.push_back( name);
		}
		return ins.first->second;
	}

	const std::string& getConstructorName( int constructor)
	{
		return constructorInv[ constructor-1];
	}
};

static int defineType( TypeDatabaseImpl& tdbimpl, const TestTypeDef& tpdef, const Scope& scope)
{
	int contextType = getContextType( tdbimpl.typedb, scope.start(), tpdef.ar[0]);
	std::string_view name( tpdef.ar[1]);
	std::vector<TypeDatabase::Parameter> parameter;
	int ti = 2;
	for (; tpdef.ar[ti]; ++ti)
	{
		int parameterType = getContextType( tdbimpl.typedb, scope.start(), tpdef.ar[ti]);
		parameter.push_back( {parameterType, tdbimpl.getConstructorFromName( std::string("#") + tpdef.ar[ti])} );
	}
	int constructor = tdbimpl.getConstructorFromName( tpdef.constructor);
	return tdbimpl.typedb->defineType( scope, contextType, name, constructor, parameter, tpdef.priority);
}

static void defineTestScopeTypeDefs( TypeDatabaseImpl& tdbimpl, const TestScopeTypeDefs& defs)
{
	int ti = 0;
	for (; defs.types[ti].ar[0]; ++ti)
	{
		(void)defineType( tdbimpl, defs.types[ti], defs.scope);
	}
}

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
				std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testTypeDb [-h][-V]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}
		std::unique_ptr<TypeDatabase> typedb( new TypeDatabase());
		TypeDatabaseImpl tdbimpl( typedb.get());

		for (int ti=1; ti < nofTests; ++ti)
		{
			defineTestScopeTypeDefs( tdbimpl, tests[ ti]);
		}
		std::string expected{R"(
)"};
		std::ostringstream outputbuf;
		outputbuf << "\n";

		std::string output = outputbuf.str();
		if (output != expected)
		{
			writeFile( "build/testTypeDb.out", output);
			writeFile( "build/testTypeDb.exp", expected);
			std::cerr << "ERR test output (build/testTypeDb.out) differs expected build/testTypeDb.exp" << std::endl;
			return 3;
		}
		else
		{
			removeFile( "build/testTypeDb.out");
			removeFile( "build/testTypeDb.exp");
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

