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
#include "tree.hpp"
#include "utilitiesForTests.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
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
		:elements(elements_),product(product_){}
	NodeDef( const std::vector<int>& elements_)
		:elements(elements_),product(1)
	{
		for (auto elem : elements)
		{
			product *= elem;
			if (product < 1)
			{
				throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			}
		}
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
		long prev_pp = 0;
		std::vector<int> ar;
		int nn = g_random.get( 1, 20);
		for (int ii = 0; ii<nn; ++ii)
		{
			int prim = g_primes[ g_random.get( 0, NofPrimes)];
			prev_pp = pp;
			pp *= prim;
			if (prev_pp >= pp) break;
			ar.push_back( prim);
		}
		std::sort( ar.begin(), ar.end());
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

typedef test::Tree<NodeDef> NodeDefTree;

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

struct TypeDatabaseContext
{
	std::vector<std::string> constructors;
	std::map<std::string,int> typemap;
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
		int constructor = ctx.constructors.size();
		ctx.constructors.push_back( string_format( "type %ld", nd->item.product));
		type = typedb.defineType( scope, 0/*contextType*/, name, constructor, std::vector<TypeDatabase::Parameter>(), 0/*priority*/);
		ctx.typemap[ name] = type;

		std::string proc = string_format( "!%ld", nd->item.product);
		constructor = ctx.constructors.size();
		ctx.constructors.push_back( string_format( "call %ld", nd->item.product));
		(void)typedb.defineType( scope, type, proc, constructor, std::vector<TypeDatabase::Parameter>(), 0/*priority*/);
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
			int constructor = ctx.constructors.size();
			ctx.constructors.push_back( string_format( "redu %ld <- %ld", nd->chld->item.product, nd->item.product));
			typedb.defineReduction( scope, leftType, type, constructor);
		}
		if (ctx.reduset.insert( {nd->item.product, nd->chld->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size();
			ctx.constructors.push_back( string_format( "inv %ld <- %ld", nd->item.product, nd->chld->item.product));
			typedb.defineReduction( scope, type, leftType, constructor);
		}
		if (ctx.reduset.insert( {nd->chld->next->item.product, nd->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size();
			ctx.constructors.push_back( string_format( "redu %ld <- %ld", nd->chld->next->item.product, nd->item.product));
			typedb.defineReduction( scope, rightType, type, constructor);
		}
		if (ctx.reduset.insert( {nd->item.product, nd->chld->next->item.product} ).second/*insert took place*/)
		{
			int constructor = ctx.constructors.size();
			ctx.constructors.push_back( string_format( "inv %ld <- %ld", nd->item.product, nd->chld->next->item.product));
			typedb.defineReduction( scope, type, rightType, constructor);
		}
	}
	return type;
}

int main( int argc, const char* argv[] )
{
	try
	{
		int nofTests = 100;
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
				std::cerr << "Usage: testRandomTypeDb [-h][-V]" << std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testRandomTypeDb [-h][-V]" << std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc)
		{
			std::cerr << "Usage: testRandomTypeDb [-h][-V]" << std::endl;
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
				std::cerr << "Test [" << ti << "] random tree:" << std::endl;
				tree->print( std::cerr);
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

