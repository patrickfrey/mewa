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
#include "tree_node.hpp"
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

struct Relation
{
	std::string left;
	std::string right;
	int tag;

	Relation( const std::string& left_, const std::string& right_, int tag_)
		:left(left_),right(right_),tag(tag_){}
	Relation( const Relation& o)
		:left(o.left),right(o.right),tag(o.tag){}
	bool operator < (const Relation& o) const noexcept
	{
		return left == o.left
			? right == o.right
				? tag < o.tag
				: right < o.right
			: left < o.left;
	}
};

struct NodeDef
{
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
			os << rel.left << "->" << rel.right << " /" << rel.tag;
		}
		os << "}";
		return os;
	}
}//namespace std

typedef test::TreeNode<NodeDef> NodeDefTree;

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

static std::vector<Relation> randomRelations( int nn, int alphabetsize, int noftags)
{
	std::set<Relation> relations;
	int ii = 0, ee = g_random.get( 0, nn);
	for (; ii < ee; ++ii) relations.insert( Relation( randomVariable( alphabetsize), randomVariable( alphabetsize), g_random.get( 1, noftags)));
	return std::vector<Relation>( relations.begin(), relations.end());
}

static int countNodes( const NodeDefTree* nd)
{
	int rt = 1;
	if (nd->chld) rt += countNodes( nd->chld);
	if (nd->next) rt += countNodes( nd->next);
	return rt;
}

static NodeDefTree* createRandomTree( const Scope& scope, int depth, int maxwidth, int members, int alphabetsize, int noftags, int maxnodes, int& nodecnt)
{
	if (scope.start() == scope.end()) throw mewa::Error( mewa::Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));

	NodeDefTree* rt = new NodeDefTree( NodeDef( scope, ++g_idCounter, randomDefs( members, alphabetsize), randomRelations( members, alphabetsize, noftags)));

	if (nodecnt < g_random.get( 0, maxnodes) && g_random.get( 0, depth) > 0)
	{
		++nodecnt;
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
				rt->addChild( createRandomTree( subscope, depth-1, maxwidth, members, alphabetsize, noftags, maxnodes, nodecnt));
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

typedef ScopedInstance<int> IdSet;
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
		int tag;

		Insert()
			:scope(0,0),left(),right(),value(){}
		Insert( const Scope scope_, const std::string& left_, const std::string& right_, int value_, int tag_)
			:scope(scope_),left(left_),right(right_),value(value_),tag(tag_){}
		Insert( const Insert& o)
			:scope(o.scope),left(o.left),right(o.right),value(o.value),tag(o.tag){}
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
			inserts.push_back( {cur.item.scope, rel.left, rel.right, cur.item.id, rel.tag});
		}
	}
	shuffle( inserts, g_random);
	for (auto ins : inserts)
	{
		relmap.set( ins.scope, ins.left, ins.right, ins.value, ins.tag, 1.0/*weight*/);
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

struct RelationQuery
{
	Scope::Step step;
	std::string left;
	TagMask mask;

	RelationQuery()
		:step(0),left(),mask(){}
	RelationQuery( const Scope::Step& step_, const std::string& left_, const TagMask& mask_)
		:step(step_),left(left_),mask(mask_){}
	RelationQuery( const RelationQuery& o)
		:step(o.step),left(o.left),mask(o.mask){}
	bool empty() const noexcept
	{
		return left.empty();
	}
};

static RelationQuery selectRelQuery( const NodeDefTree* nd)
{
	RelationQuery rt;
	if (nd->chld && g_random.get( 0,4) > 0)
	{
		rt = selectRelQuery( nd->chld);
	}
	if (rt.empty() && nd->next && g_random.get( 0,3) > 0)
	{
		rt = selectRelQuery( nd->next);
	}
	if (rt.empty() && !nd->item.relations.empty())
	{
		const Relation& rel = nd->item.relations[ g_random.get( 0, nd->item.relations.size())];
		rt.step = g_random.get( nd->item.scope.start(), nd->item.scope.end());
		rt.left = rel.left;
		rt.mask |= rel.tag;
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

static void collectRelations( std::map<std::string,int>& result, const NodeDefTree* nd, const std::string& left, Scope::Step step, const TagMask& mask)
{
	if (nd->item.scope.contains( step))
	{
		for (auto const& rel : nd->item.relations)
		{
			if (mask.matches(rel.tag-1) && rel.left == left)
			{
				result[ rel.right] = nd->item.id;
			}
		}
		if (nd->chld)
		{
			collectRelations( result, nd->chld, left, step, mask);
		}
	}
	else if (nd->next)
	{
		collectRelations( result, nd->next, left, step, mask);
	}
}

static std::map<std::string,int> getRelations( const NodeDefTree* nd, const std::string& left, Scope::Step step, const TagMask& mask)
{
	std::map<std::string,int> rt;
	collectRelations( rt, nd, left, step, mask);
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

static void randomRelQuery( const RelMap& relmap, const NodeDefTree* nd, int alphabetsize, int noftags, bool verbose)
{
	RelationQuery qry = selectRelQuery( nd);
	if (qry.empty())
	{
		qry.left = randomVariable( alphabetsize);
		qry.step = g_random.get( 0, std::numeric_limits<Scope::Step>::max());
		qry.mask |= g_random.get( 1, noftags+1);
	}
	while (g_random.get( 1, 10) > 3)
	{
		qry.mask |= g_random.get( 1, noftags+1);
	}
	std::map<std::string,int> expc = getRelations( nd, qry.left, qry.step, qry.mask);

	int local_membuffer[ 512];
	std::pmr::monotonic_buffer_resource local_memrsc( local_membuffer, sizeof local_membuffer);

	std::map<std::string,int> resc;
	auto results = relmap.get( qry.step, qry.left, qry.mask, &local_memrsc);
	for (auto const& result : results)
	{
		resc.insert( {result.right(), result.value()});
	}
	std::string expcstr = relationsToString( qry.left, expc);
	std::string rescstr = relationsToString( qry.left, resc);
	std::string maskstr = qry.mask.tostring(); 
	if (verbose)
	{
		std::cerr << mewa::string_format( "Query relations of '%s' at step %d select={%s}: Expected %s, got %s",
							qry.left.c_str(), qry.step, maskstr.c_str(), expcstr.c_str(), rescstr.c_str())
				<< std::endl;
	}
	if (resc != expc)
	{
		throw std::runtime_error(
			mewa::string_format( "Random query relations of '%s' at step %d select {%s} result '%s' not as expected '%s'",
						qry.left.c_str(), qry.step, maskstr.c_str(), rescstr.c_str(), expcstr.c_str()));
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

static std::string idSetTraversalElementToString( int value)
{
	return string_format( "%d", value);
}

static std::string expectIdSetTreeTraversal( NodeDefTree const* nd, int depth=0)
{
	std::string rt;
	while (nd)
	{
		std::string elem = std::string( depth, '\t') + nd->item.scope.tostring() + " " + idSetTraversalElementToString( nd->item.id);
		rt.append( elem);
		rt.push_back( '\n');
		if (nd->chld)
		{
			rt.append( expectIdSetTreeTraversal( nd->chld, depth+1));
		}
		nd = nd->next;
	}
	return rt;
}

static void checkIdSetTreeTraversal( const IdSet& idset, const NodeDefTree* nd, bool verbose)
{
	std::string output;
	auto tree = idset.getTree();
	if (verbose) std::cerr << "Traversal of IdSet tree:" << std::endl;
	for (auto ti = tree.begin(), te = tree.end(); ti != te; ++ti)
	{
		if (ti.depth() == 0) continue; 
		// ... do not print root node

		std::string elem = std::string( ti.depth()-1, '\t') + ti->item().scope.tostring()
				+ " " + idSetTraversalElementToString( ti->item().value);
		if (verbose) std::cerr << elem << std::endl;
		output.append( elem);
		output.push_back( '\n');
	}
	std::string expect = expectIdSetTreeTraversal( nd);
	if (output != expect)
	{
		if (verbose)
		{
			std::cerr << "Expect traversal of IdSet tree:" << std::endl;
			std::cerr << expect << std::endl;
		}
		writeFile( "build/testRandomScope.out", output);
		writeFile( "build/testRandomScope.exp", expect);
		throw std::runtime_error( "IdSet traversal not as expected, diff build/testRandomScope.out build/testRandomScope.exp");
	}
}

static std::string varMapTraversalElementToString( const std::string& key, const std::string& value)
{
	return string_format( "%s = '%s'", key.c_str(), value.c_str());
}

static std::string expectVarMapTreeTraversal( NodeDefTree const* nd, int depth=0)
{
	std::string rt;
	while (nd)
	{
		if (nd->item.vars.empty())
		{
			/// [PF:HACK] We follow here an anomaly of the implementation a scope without definitions is attached to the parent scope
			rt.append( expectVarMapTreeTraversal( nd->chld, depth/*!!! no +1*/));
		}
		else
		{
			std::string elem = std::string( depth, '\t') + nd->item.scope.tostring() + " {";
			int kidx = 0;
			for (auto const& var : nd->item.vars)
			{
				if (kidx++) elem.append( ", ");
				elem.append( varMapTraversalElementToString( var, varConstructor( var, nd->item.id)));
			}
			elem.append( "}");

			rt.append( elem);
			rt.push_back( '\n');

			if (nd->chld)
			{
				rt.append( expectVarMapTreeTraversal( nd->chld, depth+1));
			}
		}
		nd = nd->next;
	}
	return rt;
}

static void checkVarMapTreeTraversal( const VarMap& varmap, const NodeDefTree* nd, bool verbose)
{
	std::string output;
	auto tree = varmap.getTree();
	if (verbose) std::cerr << "Traversal of VarMap tree:" << std::endl;
	for (auto ti = tree.begin(), te = tree.end(); ti != te; ++ti)
	{
		if (ti.depth() == 0) continue; 
		// ... do not print root node

		std::string elem = std::string( ti.depth()-1, '\t') + ti->item().scope.tostring() + " {";
		int kidx = 0;
		for (auto const& kv : ti->item().value)
		{
			if (kidx++) elem.append( ", ");
			elem.append( varMapTraversalElementToString( kv.first, kv.second));
		}
		elem.append( "}");
		if (verbose) std::cerr << elem << std::endl;
		output.append( elem);
		output.push_back( '\n');
	}
	std::string expect = expectVarMapTreeTraversal( nd);
	if (output != expect)
	{
		if (verbose)
		{
			std::cerr << "Expect traversal of VarMap tree:" << std::endl;
			std::cerr << expect << std::endl;
		}
		writeFile( "build/testRandomScope.out", output);
		writeFile( "build/testRandomScope.exp", expect);
		throw std::runtime_error( "VarMap traversal not as expected, diff build/testRandomScope.out build/testRandomScope.exp");
	}
}

static std::string relMapTraversalElementToString( const std::string& key, const std::string& related, int value, int tag, float weight)
{
	return string_format( "(%s,%s) = %d /%d %.3f", key.c_str(), related.c_str(), value, tag, weight);
}

static std::string expectRelMapTreeTraversal( NodeDefTree const* nd, int depth=0)
{
	std::string rt;
	while (nd)
	{
		if (nd->item.relations.empty())
		{
			/// [PF:HACK] We follow here an anomaly of the implementation a scope without definitions is attached to the parent scope
			rt.append( expectRelMapTreeTraversal( nd->chld, depth/*!!! no +1*/));
		}
		else
		{
			/// [PF:HACK] Because the elements are grouped by the key in the implementation, they appear in different order, we sort both representations
			std::vector<std::string> members;
			for (auto const& rel : nd->item.relations)
			{
				members.push_back( relMapTraversalElementToString( rel.left, rel.right, nd->item.id, rel.tag, 1.0/*weight*/));
			}
			std::string elem = std::string( depth, '\t') + nd->item.scope.tostring() + " {";
			std::sort( members.begin(), members.end());
			int kidx = 0;
			for (auto const& member :members)
			{
				if (kidx++) elem.append( ", ");
				elem.append( member);
			}
			elem.append( "}");

			rt.append( elem);
			rt.push_back( '\n');

			if (nd->chld)
			{
				rt.append( expectRelMapTreeTraversal( nd->chld, depth+1));
			}
		}
		nd = nd->next;
	}
	return rt;
}

static void checkRelMapTreeTraversal( const RelMap& relmap, const NodeDefTree* nd, bool verbose)
{
	std::string output;
	auto tree = relmap.getTree();
	if (verbose) std::cerr << "Output traversal of RelMap tree:" << std::endl;
	for (auto ti = tree.begin(), te = tree.end(); ti != te; ++ti)
	{
		if (ti.depth() == 0) continue; 
		// ... do not print root node

		/// [PF:HACK] Because the elements are grouped by the key in the implementation, they appear in different order, we sort both representations
		std::vector<std::string> members;
		for (auto const& rel : ti->item().value)
		{
			members.push_back( relMapTraversalElementToString( rel.relation.first, rel.relation.second, rel.value, rel.tag, rel.weight));
		}
		std::string elem = std::string( ti.depth()-1, '\t') + ti->item().scope.tostring() + " {";
		std::sort( members.begin(), members.end());
		int kidx = 0;
		for (auto const& member :members)
		{
			if (kidx++) elem.append( ", ");
			elem.append( member);
		}
		elem.append( "}");

		if (verbose) std::cerr << elem << std::endl;
		output.append( elem);
		output.push_back( '\n');
	}
	std::string expect = expectRelMapTreeTraversal( nd);
	if (output != expect)
	{
		if (verbose)
		{
			std::cerr << "Expect traversal of RelMap tree:" << std::endl;
			std::cerr << expect << std::endl;
		}
		writeFile( "build/testRandomScope.out", output);
		writeFile( "build/testRandomScope.exp", expect);
		throw std::runtime_error( "RelMap traversal not as expected, diff build/testRandomScope.out build/testRandomScope.exp");
	}
}

static void testRandomScope( int maxdepth, int maxwidth, int maxnofnodes, int maxnofmembers, int alphabetsize, int noftags, int nofqueries, bool verbose)
{
	int buffer[ 4096];
	std::pmr::monotonic_buffer_resource memrsc( buffer, sizeof buffer);

	g_idCounter = 0;
	VarMap varmap( &memrsc, ""/*nullval*/, 1024/*initsize*/);
	RelMap relmap( &memrsc, 1024/*initsize*/);
	IdSet idset( &memrsc, -1/*nullval*/, 1024/*initsize*/);

	int nofTagsUsed = g_random.get( 1, g_random.get( 1, noftags));
	int depth = g_random.get( 1, maxdepth+1);
	int members = g_random.get( 1, maxnofmembers+1);
	Scope scope( 0, g_random.get( 1, std::numeric_limits<int>::max()));
	std::unique_ptr<NodeDefTree> tree;
	int nodecnt = 0;
	do
	{
		tree.reset( createRandomTree( scope, depth, maxwidth, members, g_random.get( 1, alphabetsize), nofTagsUsed, maxnofnodes, nodecnt));
	}
	while (countNodes(tree.get()) == 1 && g_random.get( 0,10) > 2);

	if (verbose)
	{
		std::cerr << string_format( "Random Tree (%d nodes):", countNodes( tree.get())) << std::endl;
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
		randomRelQuery( relmap, tree.get(), alphabetsize, nofTagsUsed, verbose);
	}
	randomIdQueries( idset, tree.get(), nofqueries, verbose);

	checkIdSetTreeTraversal( idset, tree.get(), verbose);
	checkVarMapTreeTraversal( varmap, tree.get(), verbose);
	checkRelMapTreeTraversal( relmap, tree.get(), verbose);
}

int main( int argc, const char* argv[] )
{
	try
	{
		bool verbose = false;
		int nofTests = 100;
		int maxDepth = 7;
		int maxWidth = 15;
		int maxNofNodes = 50;
		int maxMembers = 12;
		int alphabetSize = 26;
		int nofTags = 10;
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
				std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max nodes>] "
									"[<max members>] [<alphabet size>] [<nof tags>] [<queries>]"
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
				std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max nodes>] "
									"[<max members>] [<alphabet size>] [<nof tags>] [<queries>]"
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
		if (argi < argc) maxNofNodes = ArgParser::getCardinalNumberArgument( argv[ argi++], "maximum number of nodes");
		if (argi < argc) maxMembers = ArgParser::getCardinalNumberArgument( argv[ argi++], "maximum number of variable definitions per node");
		if (argi < argc) alphabetSize = ArgParser::getCardinalNumberArgument( argv[ argi++], "alphabet size");
		if (argi < argc) nofTags = ArgParser::getCardinalNumberArgument( argv[ argi++], "nof tags");
		if (argi < argc) nofQueries = ArgParser::getCardinalNumberArgument( argv[ argi++], "number of queries per random tree");

		if (argi < argc)
		{
			std::cerr << "Usage: testRandomScope [-h][-V] [<nof tests>] [<max depth>] [<max width>] [<max nodes>] "
									"[<max members>] [<alphabet size>] [<nof tags>] [<queries>]"
					<< std::endl;
			throw std::runtime_error( "no arguments except options expected");
		}
		int ti = 1;
		for (; ti <= nofTests; ++ti)
		{
			if (verbose) std::cerr << "Random scope tree test [" << ti << "]" << std::endl;
			testRandomScope( maxDepth, maxWidth, maxNofNodes, maxMembers, alphabetSize, nofTags, nofQueries, verbose);
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

