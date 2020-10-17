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
		{"", {"myclass", "member2", "float", nullptr}, 0},
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
	{12, {"myclass", "member2", "myclass"}},
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
		auto res = typedb->resolveType( step, contextType, item, resbuf);
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
	std::string typeToString( int type) const
	{
		std::string rt;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto typestr = typedb->typeToString( type, resbuf);
		rt.append( typestr);
		return rt;
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
		std::string rt;
		rt.append( tdbimpl.typeToString( contextType));
		rt.append( " -> ");
		rt.append( name);
		rt.append( "( ");
		for (std::size_t pi = 0; pi < parameter.size(); ++pi)
		{
			if (pi) rt.append( ", "); 
			rt.append( tdbimpl.typeToString( parameter[ pi].type));
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

struct ParameterOp
{
	int constructor;
	int reduConstructor;

	ParameterOp() = default;
	ParameterOp( int constructor_, int reduConstructor_) :constructor(constructor_),reduConstructor(reduConstructor_){}
	ParameterOp( const ParameterOp& o) = default;
};

static void testQuery( std::ostream& outbuf, TypeDatabaseImpl& tdbimpl, const TestQueryDef& query, bool verbose)
{
	TestOutput out( &outbuf, verbose ? &std::cerr : nullptr );
	FunctionDef fdef( tdbimpl, query.ar, query.step);
	{
		out << "Reductions of context type " << tdbimpl.typeToString( fdef.contextType) << " [" << query.step << "] :" << std::endl;
		mewa::TypeDatabase::ResultBuffer redu_resbuf;
		auto reductions = tdbimpl.typedb->reductions( query.step, fdef.contextType, redu_resbuf);
		out << "Context Type reductions:" << std::endl;
		for (auto const& redu : reductions)
		{
			out << tdbimpl.typeToString( redu.type) << " ~ " << tdbimpl.getConstructorName( redu.constructor) << std::endl;
		}
		out << std::endl;
	}
	{
		out << "Resolve type " << fdef.tostring( tdbimpl) << " [" << query.step << "] :" << std::endl;
		TypeDatabase::ResultBuffer resolve_resbuf;
		TypeDatabase::ResolveResult result = tdbimpl.typedb->resolveType( query.step, fdef.contextType, fdef.name, resolve_resbuf);
		out << "Result candidates:" << std::endl;
		int minDistance = 0;
		int bestCandidate = -1;
		std::vector<std::vector<ParameterOp> > popar;
		int ii = 0;
		for (auto const& item : result.items)
		{
			bool matched = false;
			++ii;
			int distance = 0;
			out << "Candidate [" << ii << "]: " << tdbimpl.typeToString( item.type) << std::endl;
			TypeDatabase::ParameterList parameters = tdbimpl.typedb->typeParameters( item.type);
			int pi = 0;
			if (fdef.parameter.size() != parameters.size())
			{
				out << string_format( "- number of parameters (%zu) do not match (%zu)", fdef.parameter.size(), parameters.size()) << std::endl;
			}
			for (auto const& parameter : parameters)
			{
				popar.push_back( {} ); 
				if (fdef.parameter[ pi].type != parameter.type)
				{
					++distance;
					int reduConstructor
						= tdbimpl.typedb->reduction( 
							query.step, parameter.type/*toType*/, fdef.parameter[ pi].type/*fromType*/);
					if (reduConstructor < 0)
					{
						auto pto = tdbimpl.typeToString( parameter.type);
						auto pfrom = tdbimpl.typeToString( fdef.parameter[ pi].type);
						out << string_format( "- parameter [%d] does not match ('%s' <-/- '%s', )", pi, pto.c_str(), pfrom.c_str()) << std::endl;
					}
					else
					{
						auto pcstr = tdbimpl.getConstructorName( parameter.constructor);
						auto rcstr = tdbimpl.getConstructorName( reduConstructor);
						out << string_format( "- parameter [%d]: %s %s", pi, pcstr.c_str(), rcstr.c_str()) << std::endl;
						matched = true;
					}
					popar.back().push_back( {parameter.constructor, reduConstructor});
				}
				else
				{
					std::string pcstr = tdbimpl.getConstructorName( parameter.constructor);
					out << string_format( "- parameter [%d]: %s", pi, pcstr.c_str()) << std::endl;
					popar.back().push_back( {parameter.constructor, -1/*reduConstructor*/});
				}
				++pi;
			}
			if (matched)
			{
				if (bestCandidate == -1)
				{
					bestCandidate = ii-1;
					minDistance = distance;
				}
				else if (distance < minDistance)
				{
					bestCandidate = ii-1;
					minDistance = distance;
				}
				else if (distance == minDistance)
				{
					std::string cstr1 = tdbimpl.getConstructorName( result.items[ bestCandidate].constructor);
					std::string cstr2 = tdbimpl.getConstructorName( result.items[ ii-1].constructor);
					std::cerr << string_format( "ambiguous parameter signature (for '%s' dist %d and '%s' dist %d), can't resolve type",
									cstr1.c_str(), minDistance, cstr2.c_str(), distance) << std::endl;
				}
			}
		}
		if (bestCandidate >= 0)
		{
			out << "Found match: " << tdbimpl.getConstructorName( result.items[ bestCandidate].constructor);
			if (!popar[ bestCandidate].empty())
			{
				out << "( ";
				int pidx = 0;
				for (auto pop : popar[ bestCandidate])
				{
					if (pidx++) out << ", ";
					out << tdbimpl.getConstructorName( pop.constructor);
					if (pop.reduConstructor >= 0)
					{
						out << " <- " <<  tdbimpl.getConstructorName( pop.reduConstructor);
					}
				}
				out << ")";
			}
			out << std::endl;
		}
		else
		{
			out << "no match" << std::endl;
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
		tdbimpl.typedb->setObjectInstance( "blublu", {0,100}, 1);
		tdbimpl.typedb->setObjectInstance( "blublu", {1,78}, 2);
		tdbimpl.typedb->setObjectInstance( "blublu", {33,45}, 3);
		tdbimpl.typedb->setObjectInstance( "gurke", {0,100}, 1);
		tdbimpl.typedb->setObjectInstance( "gurke", {1,78}, 2);
		tdbimpl.typedb->setObjectInstance( "gurke", {33,45}, 3);
		tdbimpl.typedb->setObjectInstance( "register", {0,100}, 1);
		tdbimpl.typedb->setObjectInstance( "register", {1,78}, 2);
		tdbimpl.typedb->setObjectInstance( "register", {33,45}, 3);
		tdbimpl.typedb->setObjectInstance( "blabla", {0,100}, 1);
		tdbimpl.typedb->setObjectInstance( "blabla", {1,78}, 2);
		tdbimpl.typedb->setObjectInstance( "blabla", {33,45}, 3);

		if (tdbimpl.typedb->getObjectInstance( "register", 55) != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "register", 45) != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "register", 44) != 3) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "register", 100) != -1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "gurke", 1) != 2) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "gurke", 0) != 1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));
		if (tdbimpl.typedb->getObjectInstance( "gurke", 99) != 1) throw std::runtime_error( string_format("name object loopup error %d", (int)__LINE__));

		// Test type resolving:
		if (verbose) std::cerr << "Do some simple type definition/resolving tests ..." << std::endl;
		std::string expected{R"(
Reductions of context type inhclass [1] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve type inhclass -> member1( long) [1] :
Result candidates:
Candidate [1]: inhclass member1( myclass)
- parameter [0] does not match ('myclass' <-/- 'long', )
Candidate [2]: inhclass member1( int)
- parameter [0]: #int #int<-long
Found match: #member1_int( #int <- #int<-long)

Reductions of context type inhclass [2] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve type inhclass -> member2( long) [2] :
Result candidates:
Candidate [1]: myclass member2( int)
- parameter [0]: #int #int<-long
Candidate [2]: myclass member2( float)
- parameter [0] does not match ('float' <-/- 'long', )
Found match: #member2_int( #int <- #int<-long)

Reductions of context type myclass [12] :
Context Type reductions:
float ~ #float<-myclass
double ~ #double<-myclass

Resolve type myclass -> member2( long) [12] :
Result candidates:
Candidate [1]: myclass member2( int)
- parameter [0]: #int #int<-long
Candidate [2]: myclass member2( float)
- parameter [0] does not match ('float' <-/- 'long', )
Found match: #member2_int( #int <- #int<-long)

Reductions of context type myclass [12] :
Context Type reductions:
float ~ #float<-myclass
double ~ #double<-myclass

Resolve type myclass -> member2( myclass) [12] :
Result candidates:
Candidate [1]: myclass member2( int)
- parameter [0] does not match ('int' <-/- 'myclass', )
Candidate [2]: myclass member2( float)
- parameter [0]: #float #float<-myclass
Found match: #member2_float( #float <- #float<-myclass)

Reductions of context type myclass [871] :
Context Type reductions:
float ~ #float<-myclass
double ~ #double<-myclass

Resolve type myclass -> member2( myclass) [871] :
Result candidates:
Candidate [1]: myclass member2( int)
- parameter [0] does not match ('int' <-/- 'myclass', )
Candidate [2]: myclass member2( float)
- parameter [0]: #float #float<-myclass
Found match: #member2_float( #float <- #float<-myclass)

Reductions of context type inhclass [999] :
Context Type reductions:
myclass ~ #myclass<-inhclass

Resolve type inhclass -> member1( inhclass) [999] :
Result candidates:
Candidate [1]: inhclass member1( myclass)
- parameter [0]: #myclass #myclass<-inhclass
Candidate [2]: inhclass member1( int)
- parameter [0] does not match ('int' <-/- 'inhclass', )
Found match: #member1_myclass( #myclass <- #myclass<-inhclass)

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

