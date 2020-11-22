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
#include "tree_node.hpp"
#include "utilitiesForTests.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <cstdint>
#include <utility>
#include <algorithm>

using namespace mewa;

static PseudoRandom g_random;
enum {NofPrimes=10};
static int g_primes[ NofPrimes] = {2,3,5,7,11,13,17,19,23,29};

struct NodeDef
{
	std::vector<int> elements;
	long product;

	NodeDef( const NodeDef& o)
		:elements(o.elements),product(o.product){}
	NodeDef( const std::vector<int>& elements_, long product_)
		:elements(elements_),product(product_)
	{
		std::sort( elements.begin(), elements.end());
	}
	NodeDef( const std::vector<int>& elements_)
		:elements(elements_),product(1)
	{
		long divproduct[2] = {1,1};
		int eidx = 0;
		for (auto elem : elements)
		{
			divproduct[ eidx++ & 1] *= elem;
			product *= elem;
			if (divproduct[0] * divproduct[1] != product) //... detect overflow
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
		}
		std::sort( elements.begin(), elements.end());
	}

	std::pair<NodeDef,NodeDef> split() const
	{
		if (elements.size() <= 1) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		std::vector<int> ar = elements;
		shuffle( ar, g_random);
		int ai = g_random.get( 1, ar.size()-1);
		return std::pair<NodeDef,NodeDef>( std::vector<int>( ar.begin(), ar.begin()+ai), std::vector<int>( ar.begin()+ai, ar.end()));
	}
	static NodeDef create()
	{
		long pp = 1;
		long divpp[2] = {1,1};
		std::vector<int> ar;
		int nn = g_random.get( 1, 20);
		for (int ii = 0; ii<nn; ++ii)
		{
			int prim = g_primes[ g_random.get( 0, NofPrimes)];
			divpp[ ii++ & 1] *= prim;
			pp *= prim;
			if (pp < 0 || divpp[0] * divpp[1] != pp) break; //... detect overflow
			ar.push_back( prim);
		}
		return NodeDef( ar);
	}
};

namespace std
{
	ostream& operator<<( ostream& os, const NodeDef& nd)
	{
		os << " {";
		int didx = 0;
		for (auto elem : nd.elements)
		{
			if (didx++) os << ", ";
			os << elem;
		}
		os << "} = " << nd.product;
		return os;
	}
}//namespace std

typedef test::TreeNode<NodeDef> NodeDefTree;

static void addChildren( NodeDefTree* nd)
{
	if (nd->item.elements.size() >= 2)
	{
		auto parts = nd->item.split();
		nd->addChild( parts.first);
		nd->addChild( parts.second);

		addChildren( nd->chld);
		addChildren( nd->chld->next);
	}
}

static NodeDefTree* createRandomNodeDefTree()
{
	NodeDefTree* rt = new NodeDefTree( NodeDef::create());
	addChildren( rt);
	return rt;
}

static uint64_t hibit( uint64_t n) {
    n |= (n >>  1u);
    n |= (n >>  2u);
    n |= (n >>  4u);
    n |= (n >>  8u);
    n |= (n >> 16u);
    n |= (n >> 32u);
    return n - (n >> 1);
}

static void collectPath( std::vector<NodeDefTree const*>& result, NodeDefTree const* nd, uint64_t in, uint64_t mask)
{
	result.push_back( nd);
	if (mask == 1) return;
	if (in & (mask>>1))
	{
		collectPath( result, nd->chld->next, in, mask>>1);
	}
	else
	{
		collectPath( result, nd->chld, in, mask>>1);
	}
}

static bool comparePath( const std::vector<NodeDefTree const*>& p1, const std::vector<NodeDefTree const*>& p2)
{
	if (p1.size() != p2.size()) return false;
	auto i1 = p1.begin();
	auto i2 = p2.begin();
	for (; i1 != p1.end() && i2 != p2.end(); ++i1,++i2)
	{
		if ((*i1)->item.product != (*i2)->item.product) return false;
	}
	return true;
}

static uint64_t getShortestPath( NodeDefTree const* nd, int elem, uint64_t in, bool ambiguity)
{
	if (nd->item.elements.size() == 1)
	{
		return (nd->item.elements[ 0] == elem) ? in : 0;
	}
	else
	{
		uint64_t left = getShortestPath( nd->chld, elem, in << 1, ambiguity);
		uint64_t right = getShortestPath( nd->chld->next, elem, (in << 1) | 1, ambiguity);
		if (left && right) 
		{
			if (hibit(left) == hibit(right) /*equal length*/)
			{
				uint64_t ll = getShortestPath( nd->chld, elem, 2, ambiguity);
				uint64_t rr = getShortestPath( nd->chld->next, elem, 2+1, ambiguity);

				std::vector<NodeDefTree const*> result_left;
				std::vector<NodeDefTree const*> result_right;
				collectPath( result_left, nd, ll, hibit(ll)/*mask*/);
				collectPath( result_right, nd, rr, hibit(rr)/*mask*/);

				if (comparePath( result_left, result_right))
				{
					right = 0;
				}
			}
			else if (hibit(left) < hibit(right))
			{
				right = 0;
			}
			else
			{
				left = 0;
			}
		}
		if (left && right) return ambiguity ? left : right;
		return left ? left:right;
	}
}

struct TypeDatabaseContext
{
	std::vector<std::string> constructors;
	std::map<std::string,int> typemap;
	std::map<int,std::string> typeinv;
	std::set<std::pair<long,long> > reduset;

	TypeDatabaseContext()
		:constructors(),typemap(){}
};

static int defineTypeInfo( TypeDatabase& typedb, TypeDatabaseContext& ctx, const Scope& scope, NodeDefTree const* nd)
{
	std::string name = string_format( "%ld", nd->item.product);
	int type = -1;
	auto ti = ctx.typemap.find( name);
	if (ti == ctx.typemap.end())
	{
		int constructor = ctx.constructors.size()+1;
		ctx.constructors.push_back( string_format( "type %ld", nd->item.product));
		type = typedb.defineType( scope, 0/*contextType*/, name, constructor, std::vector<TypeDatabase::Parameter>(), 0/*priority*/);
		if (type < 0) throw Error( Error::DuplicateDefinition, ctx.constructors.back());
		if (type > 0)
		{
			ctx.typemap[ name] = type;
			ctx.typeinv[ type] = name;
		}
		std::string proc = string_format( "!%ld", nd->item.product);
		constructor = ctx.constructors.size()+1;
		ctx.constructors.push_back( string_format( "call %ld", nd->item.product));
		int proctype = typedb.defineType( scope, type, proc, constructor, std::vector<TypeDatabase::Parameter>(), 0/*priority*/);
		if (proctype < 0) throw Error( Error::DuplicateDefinition, proc);
		if (proctype > 0)
		{
			ctx.typemap[ proc] = proctype;
			ctx.typeinv[ proctype] = proc;
		}
	}
	else
	{
		type = ti->second;
	}
	if (nd->chld)
	{
		int leftType = defineTypeInfo( typedb, ctx, scope, nd->chld);
		int rightType = defineTypeInfo( typedb, ctx, scope, nd->chld->next);

		if (ctx.reduset.insert( {nd->chld->item.product, nd->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size()+1;
			ctx.constructors.push_back( string_format( " -> %ld", nd->chld->item.product));
			typedb.defineReduction( scope, leftType, type, constructor, 1/*tag*/, 1.0);
		}
		if (ctx.reduset.insert( {nd->item.product, nd->chld->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size()+1;
			ctx.constructors.push_back( string_format( "inv %ld <- %ld", nd->item.product, nd->chld->item.product));
			typedb.defineReduction( scope, type, leftType, constructor, 1/*tag*/, 100.0/*bad reduction*/);
		}
		if (ctx.reduset.insert( {nd->chld->next->item.product, nd->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size()+1;
			ctx.constructors.push_back( string_format( " -> %ld", nd->chld->next->item.product));
			typedb.defineReduction( scope, rightType, type, constructor, 1/*tag*/, 1.0);
		}
		if (ctx.reduset.insert( {nd->item.product, nd->chld->next->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size()+1;
			ctx.constructors.push_back( string_format( "inv %ld <- %ld", nd->item.product, nd->chld->next->item.product));
			typedb.defineReduction( scope, type, rightType, constructor, 1/*tag*/, 100.0/*bad reduction*/);
		}
	}
	return type;
}

static std::string reductionsToString( TypeDatabase& typedb, TypeDatabaseContext& ctx, const std::pmr::vector<TypeDatabase::ReductionResult>& reductions)
{
	std::string rt;
	for (auto const& redu :reductions)
	{
		TypeDatabase::ResultBuffer resbuf;
		auto tn = typedb.typeToString( redu.type, " ", resbuf);
		if (string_format( " -> %s", tn.c_str()) != ctx.constructors[ redu.constructor-1]
		&&  string_format( "type %s", tn.c_str()) != ctx.constructors[ redu.constructor-1])
		{
			throw std::runtime_error( string_format( "expect 'type %s' or ' -> %s' instead of '%s'",
									tn.c_str(), tn.c_str(), ctx.constructors[ redu.constructor-1].c_str()));
		}
		rt.append( ctx.constructors[ redu.constructor-1]);
	}
	return rt;
}

static std::string deriveResultToString( TypeDatabase& typedb, TypeDatabaseContext& ctx, const TypeDatabase::DeriveResult& res)
{
	return reductionsToString( typedb, ctx, res.reductions);
}

static std::string resolveResultToString( TypeDatabase& typedb, TypeDatabaseContext& ctx, const TypeDatabase::ResolveResult& res)
{
	std::string rt = reductionsToString( typedb, ctx, res.reductions);
	rt.append( rt.empty() ? "{":" {");
	int iidx = 0;
	for (auto const& item :res.items)
	{
		TypeDatabase::ResultBuffer resbuf;
		if (iidx++) rt.append( ", ");
		rt.append( typedb.typeToString( item.type, " ", resbuf));
	}
	rt.append( "}");
	return rt;
}

static bool startsWith( const std::string& str, const std::string& prefix)
{
	return (prefix.size() <= str.size() && 0==std::memcmp( prefix.c_str(), str.c_str(), prefix.size()));
}

static void testRandomQuery( TypeDatabase& typedb, TypeDatabaseContext& ctx, const Scope::Step step, NodeDefTree const* tree, bool verbose)
{
	int randomSearch = g_primes[ g_random.get( 0, NofPrimes)];
	uint64_t expc_oracle1 = getShortestPath( tree, randomSearch/*elem*/, 1/*in*/, true/*ambiguity*/);
	uint64_t expc_oracle2 = getShortestPath( tree, randomSearch/*elem*/, 1/*in*/, false/*ambiguity*/);
	uint64_t expc_oracle = expc_oracle1 ? expc_oracle1 : expc_oracle2;
	std::string expc_str;

	std::string typenam = string_format("%ld", tree->item.product);
	std::string procnam = string_format("!%d", randomSearch);

	if (expc_oracle1 && expc_oracle2 && expc_oracle1 != expc_oracle2)
	{
		expc_str = "ambiguous";
	}
	else if (expc_oracle)
	{
		std::vector<NodeDefTree const*> resultnodes;
		collectPath( resultnodes, tree, expc_oracle, hibit(expc_oracle)/*mask*/);
		int ridx = 0;
		for (auto resultnode : resultnodes)
		{
			expc_str.append( ridx == 0 ? "type " : " -> ");
			expc_str.append( string_format( "%ld", resultnode->item.product));
			++ridx;
		}
		expc_str.append( expc_str.empty() ? "{" : " {");
		expc_str.append( string_format("%ld %s", resultnodes.back()->item.product, procnam.c_str()));
		expc_str.append( "}");
	}
	else
	{
		expc_str.append( "{}");
	}
	auto ti = ctx.typemap.find( typenam);
	if (ti == ctx.typemap.end())
	{
		throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
	}
	std::string resc_str;
	if (verbose)
	{
		std::cerr << "Query: " << typenam << " " << procnam << std::endl;
	}
	TypeDatabase::ResultBuffer resbuf_resolve;
	TypeDatabase::ResolveResult result = typedb.resolveType( step, ti->second, procnam, TagMask::matchAll(), resbuf_resolve);
	if (result.conflictType >= 0)
	{
		resc_str = "ambiguous";
		if (verbose)
		{
			TypeDatabase::ResultBuffer resbuf_typestr;
			std::cerr << "Conflicting type: " << typedb.typeToString( result.conflictType, " ", resbuf_typestr) << std::endl;
		}
	}
	else
	{
		resc_str = resolveResultToString( typedb, ctx, result);
		if (resc_str != "{}")
		{
			auto searchi = ctx.typemap.find( string_format( "%d", randomSearch));
			if (searchi == ctx.typemap.end())
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			TypeDatabase::ResultBuffer resbuf_derive;
			TypeDatabase::DeriveResult deriveres =
				typedb.deriveType( step, searchi->second/*toType*/, ti->second/*fromType*/,
							TagMask::matchAll(), TagMask::matchNothing(), -1/*maxPathLengthCount undefined*/, resbuf_derive);
			std::string redu_str;
			if (deriveres.defined)
			{
				TypeDatabase::ResultBuffer resbuf_tostring;
				redu_str.append( "type ");
				redu_str.append( typedb.typeToString( ti->second, " ", resbuf_tostring));
				redu_str.append( deriveResultToString( typedb, ctx, deriveres));
			}
			else
			{
				redu_str.append( "undefined");
			}
			if (verbose)
			{
				std::cerr << "Derive result: " << redu_str << std::endl;
			}
			char const* callstr = std::strchr( resc_str.c_str(), '{');
			if (!callstr)
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
			if (!startsWith( resc_str, redu_str) || (callstr-resc_str.c_str()) > (ptrdiff_t)redu_str.size()+2)
			{
				throw std::runtime_error( string_format( "derive result not as expected: '%s' prefix of '%s'", redu_str.c_str(), resc_str.c_str()));
			}
		}
	}
	if (verbose)
	{
		std::cerr << "Result: " << resc_str << std::endl;
		std::cerr << "Expected: " << expc_str << std::endl;
	}
	if (resc_str != expc_str)
	{
		throw std::runtime_error( string_format( "result not as expected: '%s' != '%s'", resc_str.c_str(), expc_str.c_str())); 
	}
}

int main( int argc, const char* argv[] )
{
	try
	{
		int nofTests = 200;
		int nofQueries = 5;
		bool verbose = false;
		int argi = 1;
		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-n"))
			{
				++argi;
				if (argi == argc || argv[argi][0] == '-' || argv[argi][0] < '0' || argv[argi][0] > '9')
				{
					std::cerr << "Usage: testRandomTypeDb [-h][-V][-n NOFTESTS]" << std::endl;
					throw std::runtime_error( "Option -n expects argument cardinal (number of tests)");
				}
				nofTests = atoi( argv[argi]);
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testRandomTypeDb [-h][-V][-n NOFTESTS]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testRandomTypeDb [-h][-V][-n NOFTESTS]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testRandomTypeDb [-h][-V][-n NOFTESTS]" << std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}
		for (int ti=0; ti < nofTests; ++ti)
		{
			Scope scope( 0, 1+g_random.get( 0, 1000));
			std::unique_ptr<TypeDatabase> typedb( new TypeDatabase());
			TypeDatabaseContext ctx;

			std::unique_ptr<NodeDefTree> tree( createRandomNodeDefTree());
			(void)defineTypeInfo( *typedb, ctx, scope, tree.get());
			if (verbose)
			{
				std::cerr << "Test [" << (ti+1) << "] random tree:" << std::endl;
				tree->print( std::cerr);
			}
			for (int qi=0; qi<nofQueries; ++qi)
			{
				testRandomQuery( *typedb, ctx, g_random.get( 0, scope.end()), tree.get(), verbose);
			}
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

