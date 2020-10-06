/*
  Copyright (c) 2020 Patrick P. Frey

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Scope test program using random generated scope hierarchies
/// \file "testRandomScope.cpp"

#if __cplusplus < 201703L
#error Building mewa requires at least C++17
#endif
#include "scope.hpp"
#include "error.hpp"
#include "fileio.hpp"
#include "strings.hpp"
#include "utilitiesForTests.hpp"
#include "tree.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <limits>
#include <cstring>

using namespace mewa;

static PseudoRandom g_random;
static int g_idCounter = 0;

struct NodeDef
{
	typedef std::pair<std::string,std::string> Relation;

	Scope scope;
	int id;
	std::vector<std::string> vars;
	std::vector<Relation> relations;

	NodeDef( const NodeDef& o)
		:scope(o.scope),id(o.id),vars(o.vars),relations(o.relations){}
	NodeDef( const Scope scope_, int id_, const std::vector<std::string>& vars_, const std::vector<Relation>& relations_)
		:scope(scope_),id(id_),vars(vars_),relations(relations_){}
};

namespace std
{
	ostream& operator<<( ostream& os, const NodeDef& sc)
	{
		os << "[" << sc.scope.start() << "," << sc.scope.end() << "] -> " << sc.id;
		os << " {";
		int didx = 0;
		for (auto const& def : sc.vars)
		{
			if (didx++) os << ", ";
			os << def;
		}
		for (auto const& rel : sc.relations)
		{
			if (didx++) os << ", ";
			os << rel.first << "->" << rel.second;
		}
		os << "}";
		return os;
	}
}//namespace std

typedef test::Tree<NodeDef> NodeDefTree;

static std::string randomVariable( int alphabetsize)
{
	std::string rt;
	char ch;
	while (alphabetsize > 26)
	{
		ch = 'A' + g_random.get( 0, alphabetsize % 26);
		alphabetsize /= 26;
		rt.push_back( ch);
	}
	ch = 'A' + g_random.get( 0, alphabetsize);
	rt.push_back( ch);
	return rt;
}

static std::vector<std::string> randomDefs( int nn, int alphabetsize)
{
	std::set<std::string> vars;
	int ii = 0, ee = g_random.get( 0, nn);
	for (; ii < ee; ++ii) vars.insert( randomVariable( alphabetsize));
	return std::vector<std::string>( vars.begin(), vars.end());
}

static std::vector<NodeDef::Relation> randomRelations( int nn, int alphabetsize)
{
	std::set<NodeDef::Relation> relations;
	int ii = 0, ee = g_random.get( 0, nn);
	for (; ii < ee; ++ii) relations.insert( NodeDef::Relation( randomVariable( alphabetsize), randomVariable( alphabetsize)));
	return std::vector<NodeDef::Relation>( relations.begin(), relations.end());
}

static NodeDefTree* createRandomTree( const Scope& scope, int depth, int maxwidth, int members, int alphabetsize)
{
	if (scope.start() == scope.end()) throw mewa::Error( mewa::Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));

	NodeDefTree* rt = new NodeDefTree( NodeDef( scope, ++g_idCounter, randomDefs( members, alphabetsize), randomRelations( members, alphabetsize)));

	if (g_random.get( 0, g_random.get( 0, depth)) > 0)
	{
		int si = 1, se = scope.end() - scope.start();
		int width = g_random.get( 0, maxwidth);
		std::vector<Scope> scopear;
		if (width >= 1)
		{
			int step = se / width;
			if (step == 0) ++step;
			for (; si + step > 0 && si + step <= se; si += step)
			{
				scopear.push_back( Scope( scope.start()+si, scope.start()+si+step));
			}
		}
		for (auto subscope : scopear)
		{
			try
			{
				rt->addChild( createRandomTree( subscope, depth-1, maxwidth, members, alphabetsize));
			}
			catch (const std::bad_alloc& err)
			{
				delete rt;
				throw std::bad_alloc();
			}
		}
	}
	return rt;
}

typedef ScopedSet<int> IdSet;
typedef ScopedMap<std::string,std::string> VarMap;
typedef ScopedRelationMap<std::string,int> RelMap;

static std::string varConstructor( const std::string& var, int nodeid)
{
	return mewa::string_format( "%s:%d", var.c_str(), nodeid);
}

static void insertDefinitions( VarMap& varmap, NodeDefTree const* nd)
{
	struct Insert
	{
		Scope scope;
		std::string key;
		std::string value;

		Insert()
			:scope(0,0),key(),value(){}
		Insert( const Scope scope_, const std::string& key_, const std::string& value_)
			:scope(scope_),key(key_),value(value_){}
		Insert( const Insert& o)
			:scope(o.scope),key(o.key),value(o.value){}
	};
	std::vector<Insert> inserts;

	std::vector<NodeDefTree const*> stk;
	stk.push_back( nd);
	std::size_t si = 0;
	for (; si < stk.size(); ++si)
	{
		NodeDefTree const* next = stk[ si]->next;
		NodeDefTree const* chld = stk[ si]->chld;
		if (next) stk.push_back( next);
		if (chld) stk.push_back( chld);

		NodeDefTree const& cur = *stk[ si];

		for (auto const& def : cur.item.vars)
		{
			inserts.push_back( {cur.item.scope, def, varConstructor( def, cur.item.id)} );
		}
	}
	shuffle( inserts, g_random);
	for (auto ins : inserts)
	{
		varmap.set( ins.scope, ins.key, ins.value);
	}
}

static void insertRelations( RelMap& relmap, NodeDefTree const* nd)
{
	struct Insert
	{
		Scope scope;
		std::string left;
		std::string right;
		int value;

		Insert()
			:scope(0,0),left(),right(),value(){}
		Insert( const Scope scope_, const std::string& left_, const std::string& right_, int value_)
			:scope(scope_),left(left_),right(right_),value(value_){}
		Insert( const Insert& o)
			:scope(o.scope),left(o.left),right(o.right),value(o.value){}
	};
	std::vector<Insert> inserts;
	std::vector<NodeDefTree const*> stk;
	stk.push_back( nd);
	std::size_t si = 0;
	for (; si < stk.size(); ++si)
	{
		NodeDefTree const* next = stk[ si]->next;
		NodeDefTree const* chld = stk[ si]->chld;
		if (next) stk.push_back( next);
		if (chld) stk.push_back( chld);

		NodeDefTree const& cur = *stk[ si];

		for (auto const& rel : cur.item.relations)
		{
			inserts.push_back( {cur.item.scope, rel.first, rel.second, cur.item.id});
		}
	}
	shuffle( inserts, g_random);
	for (auto ins : inserts)
	{
		relmap.set( ins.scope, ins.left, ins.right, ins.value);
	}
}

static void insertIds( IdSet& idset, NodeDefTree const* nd)
{
	std::vector<std::pair<Scope,int> > inserts;
	std::vector<NodeDefTree const*> stk;
	stk.push_back( nd);
	std::size_t si = 0;
	for (; si < stk.size(); ++si)
	{
		NodeDefTree const* next = stk[ si]->next;
		NodeDefTree const* chld = stk[ si]->chld;
		if (next) stk.push_back( next);
		if (chld) stk.push_back( chld);

		NodeDefTree const& cur = *stk[ si];
		inserts.push_back( {cur.item.scope,cur.item.id});
	}
	shuffle( inserts, g_random);
	for (auto kv : inserts)
	{
		idset.set( kv.first, kv.second);
	}
}

static std::pair<std::string,Scope::Step> selectVarQuery( const NodeDefTree* nd)
{
	std::pair<std::string,Scope::Step> rt;
	if (nd->chld && g_random.get( 0,4) > 0)
	{
		rt = selectVarQuery( nd->chld);
	}
	if (rt.first.empty() && nd->next && g_random.get( 0,3) > 0)
	{
		rt = selectVarQuery( nd->next);
	}
	if (rt.first.empty() && !nd->item.vars.empty())
	{
		rt.second = g_random.get( nd->item.scope.start(), nd->item.scope.end());
		rt.first = nd->item.vars[ g_random.get( 0, nd->item.vars.size())];
	}
	return rt;
}

static std::pair<std::string,Scope::Step> selectRelQuery( const NodeDefTree* nd)
{
	std::pair<std::string,Scope::Step> rt;
	if (nd->chld && g_random.get( 0,4) > 0)
	{
		rt = selectRelQuery( nd->chld);
	}
	if (rt.first.empty() && nd->next && g_random.get( 0,3) > 0)
	{
		rt = selectRelQuery( nd->next);
	}
	if (rt.first.empty() && !nd->item.relations.empty())
	{
		rt.second = g_random.get( nd->item.scope.start(), nd->item.scope.end());
		rt.first = nd->item.relations[ g_random.get( 0, nd->item.relations.size())].first;
	}
	return rt;
}

static std::string findVariable( const NodeDefTree* nd, const std::string& var, Scope::Step step)
{
	std::string rt;
	if (nd->item.scope.contains( step))
	{
		if (nd->chld)
		{
			rt = findVariable( nd->chld, var, step);
		}
		if (rt.empty())
		{
			for (auto const& def : nd->item.vars)
			{
				if (def == var)
				{
					rt = varConstructor( def, nd->item.id);
					break;
				}
			}
		}
	}
	else if (nd->next)
	{
		rt = findVariable( nd->next, var, step);
	}
	return rt;
}

static int findInnerScopeId( const NodeDefTree* nd, Scope::Step step)
{
	int rt = -1;
	if (nd->item.scope.contains( step))
	{
		if (nd->chld)
		{
			rt = findInnerScopeId( nd->chld, step);
		}
		if (rt == -1)
		{
			rt = nd->item.id;
		}
	}
	else if (nd->next)
	{
		rt = findInnerScopeId( nd->next, step);
	}
	return rt;
}

static void collectRelations( std::map<std::string,int>& result, const NodeDefTree* nd, const std::string& first, Scope::Step step)
{
	if (nd->item.scope.contains( step))
	{
		for (auto const& rel : nd->item.relations)
		{
			if (rel.first == first)
			{
				result[ rel.second] = nd->item.id;
			}
		}
		if (nd->chld)
		{
			collectRelations( result, nd->chld, first, step);
		}
	}
	else if (nd->next)
	{
		collectRelations( result, nd->next, first, step);
	}
}

static std::map<std::string,int> getRelations( const NodeDefTree* nd, const std::string& first, Scope::Step step)
{
	std::map<std::string,int> rt;
	collectRelations( rt, nd, first, step);
	return rt;
}

static void randomVarQuery( const VarMap& varmap, const NodeDefTree* nd, int alphabetsize, bool verbose)
{
	std::pair<std::string,Scope::Step> qry = selectVarQuery( nd);
	if (qry.first.empty())
	{
		qry.first = randomVariable( alphabetsize);
		qry.second = g_random.get( 0, std::numeric_limits<int>::max());
	}
	std::string expc = findVariable( nd, qry.first/*var*/, qry.second/*step*/);
	std::string resc = varmap.get( qry.second/*step*/, qry.first/*key*/);

	std::string expcstr = expc.empty() ? std::string("<no result>") : expc;
	std::string rescstr = resc.empty() ? std::string("<no result>") : resc;
	if (verbose)
	{
		std::cerr << mewa::string_format( "Query variable '%s' at step %d: Expected '%s', got '%s'",
							qry.first.c_str(), qry.second, expcstr.c_str(), rescstr.c_str())
				<< std::endl;
	}
	if (resc != expc)
	{
		throw std::runtime_error(
				mewa::string_format( "Random query variable '%s' [%d] result '%s' not as expected '%s'",
							qry.first.c_str(), qry.second, rescstr.c_str(), expcstr.c_str()));
	}
}

static std::string relationsToString( const std::string& left, const std::map<std::string,int>& ar)
{
	std::string rt;
	rt.append( "{");
	int ridx = 0;
	for (auto const& elem : ar)
	{
		if (ridx++) rt.append( ", ");
		rt.append( string_format("%s->%s:%d", left.c_str(), elem.first.c_str(), elem.second));
	}
	rt.append( "}");
	return rt;
}

static void randomRelQuery( const RelMap& relmap, const NodeDefTree* nd, int alphabetsize, bool verbose)
{
	std::pair<std::string,Scope::Step> qry = selectRelQuery( nd);
	if (qry.first.empty())
	{
		qry.first = randomVariable( alphabetsize);
		qry.second = g_random.get( 0, std::numeric_limits<int>::max());
	}
	std::map<std::string,int> expc = getRelations( nd, qry.first/*first (relation)*/, qry.second/*step*/);

	int local_membuffer[ 512];
	std::pmr::monotonic_buffer_resource local_memrsc( local_membuffer, sizeof local_membuffer);

	std::map<std::string,int> resc;
	auto results = relmap.get( qry.second/*step*/, qry.first/*var*/, &local_memrsc);
	for (auto const& result : results)
	{
		resc.insert( {result.type(), result.value()});
	}
	std::string expcstr = relationsToString( qry.first, expc);
	std::string rescstr = relationsToString( qry.first, resc);
	if (verbose)
	{
		std::cerr << mewa::string_format( "Query relations of '%s' at step %d: Expected %s, got %s",
							qry.first.c_str(), qry.second, expcstr.c_str(), rescstr.c_str())
				<< std::endl;
	}
	if (resc != expc)
	{
		throw std::runtime_error(
				mewa::string_format( "Random query relations of '%s' [%d] result '%s' not as expected '%s'",
							qry.first.c_str(), qry.second, rescstr.c_str(), expcstr.c_str()));
	}
}

static void randomIdQueries( const IdSet& idset, const NodeDefTree* nd, int nofqueries, bool verbose)
{
	std::vector<Scope::Step> queries;
	std::vector<NodeDefTree const*> stk;
	std::map<int,Scope> id2ScopeMap;
	stk.push_back( nd);
	std::size_t si = 0;
	for (; si < stk.size(); ++si)
	{
		NodeDefTree const* next = stk[ si]->next;
		NodeDefTree const* chld = stk[ si]->chld;
		if (next) stk.push_back( next);
		if (chld) stk.push_back( chld);

		NodeDefTree const& cur = *stk[ si];
		id2ScopeMap.insert( {cur.item.id, cur.item.scope} );

		for (int ii=0; ii<10; ++ii)
		{
			int queryid = g_random.get( cur.item.scope.start(), cur.item.scope.end());
			queries.push_back( queryid);
		}
	}
	shuffle( queries, g_random);
	if ((int)queries.size() > nofqueries)
	{
		queries.resize( nofqueries);
	}
	for (auto query : queries)
	{
		int result = idset.get( query);
		int expect = findInnerScopeId( nd, query);

		if (verbose)
		{
			auto const& resultScope = id2ScopeMap.at( result);
			std::cerr << mewa::string_format( "Query scope step [%d] object query results to [%d] [%d,%d]", 
								query, result, resultScope.start(), resultScope.end())
					<< std::endl;
		}
		if (result != expect)
		{
			auto const& resultScope = id2ScopeMap.at( result);
			auto const& expectScope = id2ScopeMap.at( expect);
			throw std::runtime_error(
				mewa::string_format( "Random scope step [%d] object query result [%d] in [%d,%d] not as expected [%d] in [%d,%d]",
							query, result, resultScope.start(), resultScope.end(),
							expect, expectScope.start(), expectScope.end()));
		}
	}
}

static int countNodes( const NodeDefTree* nd)
{
	int rt = 1;
	if (nd->chld) rt += countNodes( nd->chld);
	if (nd->next) rt += countNodes( nd->next);
	return rt;
}

static void testRandomScope( int maxdepth, int maxwidth, int members, int alphabetsize, int nofqueries, bool verbose)
{
	int buffer[ 4096];
	std::pmr::monotonic_buffer_resource memrsc( buffer, sizeof buffer);

	g_idCounter = 0;
	VarMap varmap( &memrsc, ""/*nullval*/, 1024/*initsize*/);
	RelMap relmap( &memrsc, 1024/*initsize*/);
	IdSet idset( &memrsc, -1/*nullval*/, 1024/*initsize*/);

	int depth = g_random.get( 1, maxdepth+1);
	Scope scope( 0, g_random.get( 1, std::numeric_limits<int>::max()));
	std::unique_ptr<NodeDefTree> tree;
	int minNofNodes = 10;
	do {
		tree.reset( createRandomTree( scope, depth, maxwidth, members, g_random.get( 1, alphabetsize)));
		minNofNodes -= 1;
	}
	while (countNodes( tree.get()) <= minNofNodes);

	if (verbose)
	{
		std::cerr << "Random Tree:" << std::endl;
		tree->print( std::cerr);
	}
	insertDefinitions( varmap, tree.get());
	insertRelations( relmap, tree.get());
	insertIds( idset, tree.get());

	for (int qi = 0; qi < nofqueries; ++qi)
	{
		randomVarQuery( varmap, tree.get(), alphabetsize, verbose);
	}
	for (int qi = 0; qi < nofqueries; ++qi)
	{
		randomRelQuery( relmap, tree.get(), alphabetsize, verbose);
	}
	randomIdQueries( idset, tree.get(), nofqueries, verbose);
}

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int nofTests = 100;
		int maxDepth = 7;
		int maxWidth = 15;
		int maxMembers = 12;
		int alphabetSize = 26;
		int nofQueries = 1000;
		int argi = 1;

		for (; argi < argc; ++argi)
		{
			if (0==std::strcmp( argv[argi], "-V"))
			{
				verbose = true;
			}
			else if (0==std::strcmp( argv[argi], "-h"))
			{
				std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max members>] [<alphabet size>] [<queries>]"
						<< std::endl;
				return 0;
			}
			else if (0==std::strcmp( argv[argi], "--"))
			{
				++argi;
				break;
			}
			else if (argv[argi][0] == '-')
			{
				std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max members>] [<alphabet size>] [<queries>]"
						<< std::endl;
				throw std::runtime_error( string_format( "unknown option '%s'", argv[argi]));
			}
			else
			{
				break;
			}
		}
		if (argi < argc) nofTests = ArgParser::getCardinalNumberArgument( argv[ argi++], "number of tests");
		if (argi < argc) maxDepth = ArgParser::getCardinalNumberArgument( argv[ argi++], "maximum tree depth");
		if (argi < argc) maxWidth = ArgParser::getCardinalNumberArgument( argv[ argi++], "maximum tree width");
		if (argi < argc) maxMembers = ArgParser::getCardinalNumberArgument( argv[ argi++], "maximum number of variable definitions per node");
		if (argi < argc) alphabetSize = ArgParser::getCardinalNumberArgument( argv[ argi++], "alphabet size");
		if (argi < argc) nofQueries = ArgParser::getCardinalNumberArgument( argv[ argi++], "number of queries per random tree");

		if (argi < argc)
		{
			std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max members>] [<alphabet size>] [<queries>]"
					<< std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}
		int ti = 1;
		for (; ti <= nofTests; ++ti)
		{
			if (verbose) std::cerr << "Random scope tree test [" << ti << "]" << std::endl;
			testRandomScope( maxDepth, maxWidth, maxMembers, alphabetSize, nofQueries, verbose);
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

