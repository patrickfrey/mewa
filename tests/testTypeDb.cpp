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
	const char* resultType;
	char const* ar[12];		//< nullptr terminated list of items
	int priority;

	std::string tostring() const
	{
		std::ostringstream buf;
		buf << ar[0];
		if (ar[0][0]) buf << "::";
		buf << ar[1];
		if (resultType[0])
		{
			buf << "(";
			for (std::size_t ai = 0; ar[ ai+2]; ++ai)
			{
				if (ai) buf << ", ";
				buf << ar[ ai+2];
			}
			buf << ") -> " << resultType;
		}
		return buf.str();
	}
};

struct TestReductionDef
{
	char const* fromType;
	char const* toType;

	std::string tostring() const
	{
		std::ostringstream buf;
		buf << toType << " <- " << fromType;
		return buf.str();
	}
};

struct TestScopeDef
{
	Scope scope;
	TestReductionDef redus[ 32]; 	//< {nullptr,..} terminated list of reduction definitions
	TestTypeDef types[ 32];		//< {nullptr,..} terminated list of type definitions
};

static int nofTestScopeDefs = 1;
static TestScopeDef testScopeDefs[1] = {
	{
		{0,1000},
		{
			{"byte","uint"},
			{"uint","byte"},
			{"int","uint"},
			{"uint","int"},
			{"long","int"},
			{"int","long"},
			{"uint","long"},
			{"long","uint"},
			{"long","double"},
			{"double","long"},
			{"int","float"},
			{"float","int"},
			{"float","double"},
			{"double","float"},
			{"myclass","float"},
			{"myclass","double"},
			{nullptr,nullptr}
		},
		{
			{"", {"", "byte", nullptr}, 0},
			{"", {"", "int", nullptr}, 0},
			{"", {"", "uint", nullptr}, 0},
			{"", {"", "long", nullptr}, 0},
			{"", {"", "float", nullptr}, 0},
			{"", {"", "double", nullptr}, 0},
			{"", {"", "func", "int", "int", "int", nullptr}, 0},
			{"", {"", "func", "uint", "int", "int", nullptr}, 0},
			{"", {"", "func", "double", "int", "int", nullptr}, 0},
			{"", {"", "func", "double", "byte", "uint", nullptr}, 0},
			{"", {"", "func", "long", "uint", "uint", nullptr}, 0},
			{"", {"", "func", "byte", "int", "int", nullptr}, 0},
			{"int", {"", "func", "byte", "int", nullptr}, 0},
			{"int", {"", "func", "int", "int", nullptr}, 0},
			{"long", {"", "func", "int", "long", nullptr}, 0},
			{"double", {"", "func", "int", "double", nullptr}, 0},
			{"float", {"", "func", "float", "float", nullptr}, 0},
			{"double", {"", "func", "double", "double", nullptr}, 0},
			{"", {"", "myclass", nullptr}, 0},
			{"", {"myclass", "constructor myclass", "int", nullptr}, 0},
			{"", {"myclass", "constructor myclass", "float", "float", nullptr}, 0},
			{"", {"myclass", "constructor myclass", "myclass", nullptr}, 0},
			{"", {"myclass", "~myclass", nullptr}, 0},
			{"", {"myclass", "member", "myclass", nullptr}, 0},
			{"", {"myclass", "member", "int", nullptr}, 0},
			{nullptr, {nullptr}, 0}
		}
	}
};

struct TestQueryDef
{
	Scope::Step step;
	char const* ar[12];	//< nullptr terminated list of items
};

static TestQueryDef testQueries[ 64] = {
	{0,{nullptr}}
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
		auto res = typedb->resolve( step, contextType, item, resbuf);
		if (res.items.empty()) throw Error( Error::UnresolvableType, item);
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

static std::string typeLabel( char const* const* arg)
{
	std::string rt;
	for (; *arg; ++arg)
	{
		if (!rt.empty()) rt.push_back('_');
		rt.append( *arg);
	}
	return rt;
}

static void defineType( TypeDatabaseImpl& tdbimpl, const TestTypeDef& tpdef, const Scope& scope)
{
	int contextType = getContextType( tdbimpl.typedb, scope.start(), tpdef.ar[0]);
	std::string name( typeLabel( tpdef.ar+1));
	std::vector<TypeDatabase::Parameter> parameter;
	int ti = 2;
	for (; tpdef.ar[ti]; ++ti)
	{
		int parameterType = getContextType( tdbimpl.typedb, scope.start(), tpdef.ar[ti]);
		parameter.push_back( {parameterType, tdbimpl.getConstructorFromName( std::string("#") + tpdef.ar[ti])} );
	}
	int constructorId = tdbimpl.getConstructorFromName( std::string("#") + name);
	int funcTypeId = tdbimpl.typedb->defineType( scope, contextType, name, constructorId, parameter, tpdef.priority);
	if (tpdef.resultType[0])
	{
		int resultTypeId = getContextType( tdbimpl.typedb, scope.start(), tpdef.resultType);
		int resultConstructorId = tdbimpl.getConstructorFromName( std::string("#") + tpdef.resultType);
		tdbimpl.typedb->defineReduction( scope, resultTypeId, funcTypeId, resultConstructorId);
	}
}

static void defineReduction( TypeDatabaseImpl& tdbimpl, const TestReductionDef& rddef, const Scope& scope)
{
	int fromTypeId = getContextType( tdbimpl.typedb, scope.start(), rddef.fromType);
	int toTypeId = getContextType( tdbimpl.typedb, scope.start(), rddef.toType);
	int constructorId = tdbimpl.getConstructorFromName( std::string("#") + rddef.toType + "<-" + rddef.fromType);
	tdbimpl.typedb->defineReduction( scope, toTypeId, fromTypeId, constructorId);
}

static void defineTestScopeDef( TypeDatabaseImpl& tdbimpl, const TestScopeDef& def, bool verbose)
{
	int ti = 0;
	if (verbose) std::cerr << "In scope " << def.scope.tostring() << std::endl;
	for (; def.types[ti].ar[0]; ++ti)
	{
		if (verbose) std::cerr << "Define type " << def.types[ ti].tostring() << std::endl;
		defineType( tdbimpl, def.types[ ti], def.scope);
	}
	int ri = 0;
	for (; def.redus[ri].fromType; ++ri)
	{
		if (verbose) std::cerr << "Define reduction " << def.redus[ ri].tostring() << std::endl;
		defineReduction( tdbimpl, def.redus[ ri], def.scope);
	}
	if (verbose) std::cerr << std::endl;
}

static void testQuery( const TypeDatabaseImpl& tdbimpl, const TestQueryDef& query, bool verbose)
{
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

		for (int ti=0; ti < nofTestScopeDefs; ++ti)
		{
			defineTestScopeDef( tdbimpl, testScopeDefs[ ti], verbose);
		}
		for (int qi=0; testQueries[ qi].ar[0]; ++qi)
		{
			testQuery( tdbimpl, testQueries[ qi], verbose);
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

