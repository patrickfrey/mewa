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
	char const* toType;
	char const* fromType;
	float weight;

	std::string tostring() const
	{
		std::ostringstream buf;
		buf << toType << " <- " << fromType;
		if (weight != 1.0)
		{
			buf << " (" << weight << ")";
		}
		return buf.str();
	}
};

struct TestDef
{
	Scope scope;
	TestReductionDef redus[ 64]; 	//< {nullptr,..} terminated list of reduction definitions
	TestTypeDef types[ 64];		//< {nullptr,..} terminated list of type definitions
};

static TestDef testDef = {
	{0,1000},
	{
		{"byte","uint",0.5},
		{"uint","byte",1.0},
		{"int","uint",0.8},
		{"uint","int",0.8},
		{"long","int",1.0},
		{"int","long",0.5},
		{"uint","long",0.5},
		{"long","uint",1.0},
		{"long","double",0.5},
		{"double","long",1.0},
		{"int","float",0.5},
		{"float","int",1.0},
		{"float","double",0.5},
		{"double","float",1.0},
		{"float","myclass",0.5},
		{"double","myclass",1.0},
		{"myclass","inhclass",1.0},
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
		{"", {"myclass", "member1", "myclass", nullptr}, 0},
		{"", {"myclass", "member1", "int", nullptr}, 0},
		{"", {"myclass", "member2", "int", nullptr}, 0},
		{"", {"", "inhclass", nullptr}, 0},
		{"", {"inhclass", "constructor inhclass", "float", "float", nullptr}, 0},
		{"", {"inhclass", "constructor inhclass", "inhclass", nullptr}, 0},
		{"", {"inhclass", "~inhclass", nullptr}, 0},
		{"", {"inhclass", "member1", "myclass", nullptr}, 0},
		{"", {"inhclass", "member1", "int", nullptr}, 0},
		{nullptr, {nullptr}, 0}
	}
};

struct TestQueryDef
{
	Scope::Step step;	//< query scope step
	char const* ar[12];	//< nullptr terminated list of items
};

static TestQueryDef testQueries[ 64] = {
	{1, {"inhclass", "member1", "long"}},
	{2, {"inhclass", "member2", "long"}},
	{12, {"myclass", "member2", "long"}},
	{871, {"myclass", "member2", "myclass"}},
	{999, {"inhclass", "member1", "inhclass"}},
	{0,{nullptr}}
};


static int getContextType( TypeDatabase const* typedb, const Scope::Step step, const char* tpstr)
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

struct FunctionDef
{
	int contextType;
	std::string name;
	std::string label;
	std::vector<TypeDatabase::Parameter> parameter;

	FunctionDef( const FunctionDef& o)
		:contextType(o.contextType),name(o.name),label(o.label),parameter(o.parameter){}
	FunctionDef( TypeDatabaseImpl& tdbimpl, char const* const* ar, const Scope::Step step)
	{
		contextType = getContextType( tdbimpl.typedb, step, ar[0]);
		name = ar[1];
		label = typeLabel( ar+1);
		int ti = 2;
		for (; ar[ti]; ++ti)
		{
			int parameterType = getContextType( tdbimpl.typedb, step, ar[ti]);
			parameter.push_back( {parameterType, tdbimpl.getConstructorFromName( std::string("#") + ar[ti])} );
		}
	}
	std::string tostring( TypeDatabaseImpl& tdbimpl) const
	{
		std::string rt = tdbimpl.typedb->typeToString( contextType);
		rt.append( " -> ");
		rt.append( name);
		rt.append( "( ");
		for (std::size_t pi = 0; pi < parameter.size(); ++pi)
		{
			if (pi) rt.append( ", "); 
			rt.append( tdbimpl.typedb->typeToString( parameter[ pi].type));
		}
		rt.push_back( ')');
		return rt;
	}
};

static void defineType( TypeDatabaseImpl& tdbimpl, const TestTypeDef& tpdef, const Scope& scope)
{
	FunctionDef fdef( tdbimpl, tpdef.ar, scope.start());
	int constructorId = tdbimpl.getConstructorFromName( std::string("#") + fdef.label);
	int funcTypeId = tdbimpl.typedb->defineType( scope, fdef.contextType, fdef.name, constructorId, fdef.parameter, tpdef.priority);
	if (tpdef.resultType[0])
	{
		int resultTypeId = getContextType( tdbimpl.typedb, scope.start(), tpdef.resultType);
		int resultConstructorId = tdbimpl.getConstructorFromName( std::string("#") + tpdef.resultType);
		tdbimpl.typedb->defineReduction( scope, resultTypeId, funcTypeId, resultConstructorId, 1.0/*weight*/);
	}
}

static void defineReduction( TypeDatabaseImpl& tdbimpl, const TestReductionDef& rddef, const Scope& scope)
{
	int fromTypeId = getContextType( tdbimpl.typedb, scope.start(), rddef.fromType);
	int toTypeId = getContextType( tdbimpl.typedb, scope.start(), rddef.toType);
	int constructorId = tdbimpl.getConstructorFromName( std::string("#") + rddef.toType + "<-" + rddef.fromType);
	tdbimpl.typedb->defineReduction( scope, toTypeId, fromTypeId, constructorId, rddef.weight);
}

static void defineTest( TypeDatabaseImpl& tdbimpl, const TestDef& def, bool verbose)
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

struct TestOutput
{
	std::ostream* out1;
	std::ostream* out2;

	TestOutput( std::ostream* out1_, std::ostream* out2_)
		:out1(out1_),out2(out2_){}

	template <typename TYPE>
	TestOutput& operator << (TYPE elem)
	{
		if (out1) *out1 << elem;
		if (out2) *out2 << elem;
		return *this;
	}
	TestOutput& operator << (std::ostream& (*pf)(std::ostream&))
	{
		if (out1) pf(*out1);
		if (out2) pf(*out2);
		return *this;
	}
};

static void testQuery( std::ostream& outbuf, TypeDatabaseImpl& tdbimpl, const TestQueryDef& query, bool verbose)
{
	TestOutput out( &outbuf, verbose ? &std::cerr : nullptr );
	FunctionDef fdef( tdbimpl, query.ar, query.step);
	{
		out << "Reductions of context type " << tdbimpl.typedb->typeToString( fdef.contextType) << " [" << query.step << "] :" << std::endl;
		TypeDatabase::ResultBuffer resbuf;
		TypeDatabase::ReduceResult result = tdbimpl.typedb->reductions( query.step, fdef.contextType, resbuf);
		out << "Context Type reductions:" << std::endl;
		for (auto const& redu : result.reductions)
		{
			out << tdbimpl.typedb->typeToString( redu.type) << " ~ " << tdbimpl.getConstructorName( redu.constructor) << std::endl;
		}
		out << std::endl;
	}
	{
		out << "Resolve " << fdef.tostring( tdbimpl) << " [" << query.step << "] :" << std::endl;
		TypeDatabase::ResultBuffer resbuf;
		TypeDatabase::ResolveResult result = tdbimpl.typedb->resolve( query.step, fdef.contextType, fdef.name, resbuf);
		out << "Result candidates:" << std::endl;
		for (auto const& item : result.items)
		{
			out << tdbimpl.typedb->typeToString( item.type) << " ~ " << tdbimpl.getConstructorName( item.constructor) << std::endl;
		}
		out << std::endl;
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

		// Test scope sets:
		if (verbose) std::cerr << "Do some simple scope set tests ..." << std::endl;
		tdbimpl.typedb->setNamedObject( {0,100}, "blublu", 1);
		tdbimpl.typedb->setNamedObject( {1,78}, "blublu", 2);
		tdbimpl.typedb->setNamedObject( {33,45}, "blublu", 3);
		tdbimpl.typedb->setNamedObject( {0,100}, "gurke", 1);
		tdbimpl.typedb->setNamedObject( {1,78}, "gurke", 2);
		tdbimpl.typedb->setNamedObject( {33,45}, "gurke", 3);
		tdbimpl.typedb->setNamedObject( {0,100}, "register", 1);
		tdbimpl.typedb->setNamedObject( {1,78}, "register", 2);
		tdbimpl.typedb->setNamedObject( {33,45}, "register", 3);
		tdbimpl.typedb->setNamedObject( {0,100}, "blabla", 1);
		tdbimpl.typedb->setNamedObject( {1,78}, "blabla", 2);
		tdbimpl.typedb->setNamedObject( {33,45}, "blabla", 3);

		if (tdbimpl.typedb->getNamedObject( 55, "register") != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 45, "register") != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 44, "register") != 3) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 100, "register") != -1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 1, "gurke") != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 0, "gurke") != 1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getNamedObject( 99, "gurke") != 1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));

		// Test type resolving:
		if (verbose) std::cerr << "Do some simple type definition/resolving tests ..." << std::endl;
		std::string expected{R"(
Reductions of context type inhclass [1] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve inhclass -> member1( long) [1] :
Result candidates:
inhclass member1( myclass) ~ #member1_myclass
inhclass member1( int) ~ #member1_int

Reductions of context type inhclass [2] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve inhclass -> member2( long) [2] :
Result candidates:
myclass member2( int) ~ #member2_int

Reductions of context type myclass [12] :
Context Type reductions:
float ~ #float<-myclass
double ~ #double<-myclass

Resolve myclass -> member2( long) [12] :
Result candidates:
myclass member2( int) ~ #member2_int

Reductions of context type myclass [871] :
Context Type reductions:
float ~ #float<-myclass
double ~ #double<-myclass

Resolve myclass -> member2( myclass) [871] :
Result candidates:
myclass member2( int) ~ #member2_int

Reductions of context type inhclass [999] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve inhclass -> member1( inhclass) [999] :
Result candidates:
inhclass member1( myclass) ~ #member1_myclass
inhclass member1( int) ~ #member1_int

)"};
		std::ostringstream outputbuf;
		outputbuf << "\n";

		defineTest( tdbimpl, testDef, verbose);
		for (int qi=0; testQueries[ qi].ar[0]; ++qi)
		{
			testQuery( outputbuf, tdbimpl, testQueries[ qi], verbose);
		}
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

