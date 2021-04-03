/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Type system data implementation
/// \file "typedb.cpp"
#include "typedb.hpp"
#include "error.hpp"
#include "identmap.hpp"
#include "memory_resource.hpp"
#include <utility>
#include <algorithm>
#include <queue>
#include <limits>
#include <memory>
#include <memory_resource>

using namespace mewa;

void TypeDatabase::appendTypeToString( std::pmr::string& res, int type, const char* sep) const
{
	if (type == 0) return;
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.inv.contextType)
	{
		appendTypeToString( res, rec.inv.contextType, sep);
		res.append( sep);
	}
	res.append( m_identMap->inv( rec.inv.ident));
	if (rec.parameterlen)
	{
		res.push_back( '(');
		for (int pi = 0; pi < rec.parameterlen; ++pi)
		{
			if (pi) res.push_back( ','); 
			appendTypeToString( res, m_parameterMap[ rec.parameter + pi -1].type, sep);
		}
		res.push_back( ')');
	}
}

std::pmr::string TypeDatabase::typeToString( int type, const char* sep, ResultBuffer& resbuf) const
{
	std::pmr::string rt( &resbuf.memrsc);
	rt.reserve( resbuf.buffersize()-1);
	appendTypeToString( rt, type, sep);
	return rt;
}

std::string TypeDatabase::debugTypeToString( int type, const char* sep) const
{
	ResultBuffer resbuf;
	std::pmr::string res = typeToString( type, sep, resbuf);
	return std::string( res.c_str(), res.size());
}

std::string_view TypeDatabase::typeName( int type) const
{
	if (type == 0) return std::string_view("",0);
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return m_identMap->inv( rec.inv.ident);
}

int TypeDatabase::typeContext( int type) const
{
	if (type == 0) return 0;
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return rec.inv.contextType;
}

void TypeDatabase::setObjectInstance( const std::string_view& name, const Scope& scope, int handle)
{
	if (handle <= 0) throw Error( Error::InvalidHandle, string_format( "%d", handle));

	int objid = m_identMap->get( name);
	auto ins = m_objidMap->insert( {objid, m_objAr.size()});
	if (ins.second == true/*insert took place*/)
	{
		m_objAr.push_back( ObjectAccess( m_memory->resource_objtab(), -1/*nullval*/, m_memory->nofObjectInstanceTableInitBuckets()));
	}
	m_objAr[ ins.first->second].set( scope, handle/*value*/);
}

int TypeDatabase::getObjectInstance( const std::string_view& name, const Scope::Step step) const
{
	auto oi = m_objidMap->find( m_identMap->lookup( name));
	if (oi == m_objidMap->end()) return -1/*undefined*/;
	return m_objAr[ oi->second].get( step);
}

int TypeDatabase::getObjectInstanceOfScope( const std::string_view& name, const Scope scope) const
{
	auto oi = m_objidMap->find( m_identMap->lookup( name));
	if (oi == m_objidMap->end()) return -1/*undefined*/;
	return m_objAr[ oi->second].getThis( scope);
}

TypeDatabase::TypeConstructorPairList TypeDatabase::typeParameters( int type) const
{
	if (type == 0) return TypeConstructorPairList( 0, nullptr);
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.parameter < 0) return TypeConstructorPairList( 0, nullptr);
	return TypeConstructorPairList( rec.parameterlen, m_parameterMap.data() + rec.parameter - 1);
}

int TypeDatabase::typeConstructor( int type) const
{
	if (type == 0) return 0;
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return rec.constructor;
}

Scope TypeDatabase::typeScope( int type) const
{
	if (type == 0) return Scope(0,-1);
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return rec.scope;
}

bool TypeDatabase::compareParameterSignature( const TypeDatabase::TypeConstructorPairList& parameter, int param2, int paramlen2) const noexcept
{
	if (parameter.arsize != paramlen2) return false;
	for (int pi = 0; pi < paramlen2; ++pi)
	{
		if (parameter[ pi].type != m_parameterMap[ param2+pi-1].type) return false;
	}
	return true;
}

int TypeDatabase::findTypeWithSignature( int typerec, const TypeDatabase::TypeConstructorPairList& parameter, int& lastListIndex) const noexcept
{
	lastListIndex = 0;
	int tri = typerec;
	while (tri)
	{
		auto const& tr = m_typerecMap[ tri-1];
		if (compareParameterSignature( parameter, tr.parameter, tr.parameterlen)) break;

		lastListIndex = tri;
		tri = tr.next;
	}
	return tri;
}

bool TypeDatabase::compareParameterSignature( const TypeDatabase::TypeList& parameter, int param2, int paramlen2) const noexcept
{
	if (parameter.arsize != paramlen2) return false;
	for (int pi = 0; pi < paramlen2; ++pi)
	{
		if (parameter[ pi] != m_parameterMap[ param2+pi-1].type) return false;
	}
	return true;
}

int TypeDatabase::findTypeWithSignature( int typerec, const TypeDatabase::TypeList& parameter) const noexcept
{
	int tri = typerec;
	while (tri)
	{
		auto const& tr = m_typerecMap[ tri-1];
		if (compareParameterSignature( parameter, tr.parameter, tr.parameterlen)) break;

		tri = tr.next;
	}
	return tri;
}

int TypeDatabase::getType( const Scope& scope, int contextType, const std::string_view& name, const TypeList& parameter) const
{
	if (contextType < 0 || contextType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", contextType));
	TypeDef typeDef( contextType, m_identMap->get( name));

	int typerec = m_typeTable->getDef( scope, typeDef);
	return typerec ? findTypeWithSignature( typerec, parameter) : 0;
}

TypeDatabase::GetTypesResult TypeDatabase::getTypes( const Scope& scope, int contextType, const std::string_view& name, ResultBuffer& resbuf) const
{
	TypeDatabase::GetTypesResult rt( resbuf);
	if (contextType < 0 || contextType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", contextType));
	TypeDef typeDef( contextType, m_identMap->get( name));

	int tri = m_typeTable->getDef( scope, typeDef);
	while (tri)
	{
		rt.types.push_back( tri);
		tri = m_typerecMap[ tri-1].next;
	}
	return rt;
}

int TypeDatabase::defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, const TypeConstructorPairList& parameter)
{
	if (parameter.size() >= MaxNofParameter) throw Error( Error::TooManyTypeArguments, string_format( "%zu", parameter.size()));
	if ((int)(m_parameterMap.size() + parameter.size()) >= std::numeric_limits<int>::max()) throw Error( Error::CompiledSourceTooComplex);
	if (constructor < 0) throw Error( Error::InvalidHandle, string_format( "%d", constructor));
	if (contextType < 0 || contextType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", contextType));

	m_typerecMap.reserve( m_typerecMap.size()+1);

	int parameterlen = parameter.size();
	int parameteridx;
	if (parameter.empty())
	{
		parameteridx = 0;
	}
	else
	{
		parameteridx = (int)m_parameterMap.size()+1;
		m_parameterMap.insert( m_parameterMap.end(), parameter.begin(), parameter.end());
	}
	int typerec = m_typerecMap.size()+1;
	TypeDef typeDef( contextType, m_identMap->get( name));
	m_typerecMap.push_back( TypeRecord( constructor, scope, parameteridx, parameterlen, typeDef));

	int prev_typerec = m_typeTable->getOrSet( scope, typeDef, typerec);
	if (prev_typerec/*already exists*/)
	{
		// Check if there is a conflict, and find the end of the single linked list (-> lastListIndex):
		int lastListIndex = 0;
		int tri = findTypeWithSignature( prev_typerec, parameter, lastListIndex);
		if (tri) return -1;

		// ... the definition is new and we got the tail of the list in 'lastListIndex', we add the new created record:
		if (!lastListIndex) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		m_typerecMap[ lastListIndex-1].next = typerec;
	}
	return typerec;
}

int TypeDatabase::defineTypeAs( const Scope& scope, int contextType, const std::string_view& name, int type)
{
	if (contextType < 0 || contextType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", contextType));
	if (type <= 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", type));
	TypeDef typeDef( contextType, m_identMap->get( name));
	return m_typeTable->set( scope, typeDef, type) ? type : 0;
}

void TypeDatabase::defineReduction( const Scope& scope, int toType, int fromType, int constructor, int tag, float weight)
{
	if (constructor < 0) throw Error( Error::InvalidHandle, string_format( "%d", constructor));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));
	if (toType == fromType)
	{
		std::string nam = debugTypeToString( toType, " ");
		throw Error( Error::InvalidReductionDefinition, string_format( "%s <- %s", nam.c_str(), nam.c_str()));
	}
	m_reduTable->set( scope, fromType, toType, constructor, tag, weight);
}

struct ReduStackElem
{
	int type;
	int constructor;
	int prev;
	int pathlen;

	ReduStackElem( const ReduStackElem& o) noexcept
		:type(o.type),constructor(o.constructor),prev(o.prev),pathlen(o.pathlen){}
	ReduStackElem( int type_, int constructor_, int prev_, int pathlen_) noexcept
		:type(type_),constructor(constructor_),prev(prev_),pathlen(pathlen_){}
};

class ReduStack
{
public:
	ReduStack( std::pmr::memory_resource* memrsc, int buffersize)
		:m_ar( memrsc)
	{
		m_ar.reserve( buffersize / sizeof(ReduStackElem));
	}

	int pushNonDuplicateStart( int type, int constructor)
	{
		int rt = -1;
		std::size_t ai = 0;
		for (; ai < m_ar.size() && m_ar[ ai].type != type; ++ai){}
		if (ai == m_ar.size())
		{
			rt = ai;
			m_ar.push_back( {type,constructor,-1/*prev*/,0/*path len*/});
		}
		return rt;
	}
	int pushIfNew( int type, int constructor, int prev, int pathlen)
	{
		int rt = -1;
		int index = prev;
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			if (ee.type == type) return rt;
			index = ee.prev;
		}
		rt = m_ar.size();
		m_ar.push_back( {type,constructor,prev,pathlen});
		return rt;
	}
	int size() const noexcept
	{
		return m_ar.size();
	}
	const ReduStackElem& operator[]( int idx) const noexcept
	{
		return m_ar[ idx];
	}

	void collectResult( std::pmr::vector<TypeDatabase::TypeConstructorPair>& result, int index) const
	{
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			if (ee.prev != -1)
			{
				result.push_back( {ee.type, ee.constructor} );
			}
			index = ee.prev;
		}
		std::reverse( result.begin(), result.end());
	}

	void collectResultWithRootIndex( int& rootIndex, std::pmr::vector<TypeDatabase::TypeConstructorPair>& result, int index) const
	{
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			if (ee.prev != -1)
			{
				result.push_back( {ee.type, ee.constructor} );
			}
			else
			{
				rootIndex = ee.constructor;
			}
			index = ee.prev;
		}
		std::reverse( result.begin(), result.end());
	}

	std::vector<int> debugCollectConflictPath( int index) const
	{
		std::vector<int> rt;
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			if (ee.prev != -1)
			{
				rt.push_back( ee.type );
			}
			index = ee.prev;
		}
		std::reverse( rt.begin(), rt.end());
		return rt;
	}

	void collectConflictPath( std::pmr::vector<int>& result, int index) const
	{
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			if (ee.prev != -1)
			{
				result.push_back( ee.type );
			}
			index = ee.prev;
		}
		std::reverse( result.begin(), result.end());
	}

private:
	std::pmr::vector<ReduStackElem> m_ar;
};

struct ReduQueueElem
{
	float weight;
	int index;

	ReduQueueElem() noexcept
		:weight(0.0),index(-1){}
	ReduQueueElem( float weight_, int index_) noexcept
		:weight(weight_),index(index_){}
	ReduQueueElem( const ReduQueueElem& o) noexcept
		:weight(o.weight),index(o.index){}

	bool operator < (const ReduQueueElem& o) const noexcept
	{
		return weight == o.weight ? index < o.index : weight < o.weight;
	}
	bool operator > (const ReduQueueElem& o) const noexcept
	{
		return weight == o.weight ? index > o.index : weight > o.weight;
	}
};

std::string TypeDatabase::reductionsToString( const std::pmr::vector<TypeConstructorPair>& reductions) const
{
	std::string rt;

	int ridx = 0;
	for (auto const& redu :reductions)
	{
		ResultBuffer resbuf;
		if (ridx++) rt.append( " -> ");
		rt.append( typeToString( redu.type, " ", resbuf));
	}
	return rt;
}

std::string TypeDatabase::deriveResultToString( const DeriveResult& res) const
{
	return reductionsToString( res.reductions) + std::string(" [%.3f]", res.weightsum);
}

std::string TypeDatabase::resolveResultToString( const ResolveResult& res) const
{
	std::string rt = reductionsToString( res.reductions);
	rt.append( " {");

	int iidx = 0;
	for (auto const item :res.items)
	{
		ResultBuffer resbuf;
		if (iidx++) rt.append( ", ");
		rt.append( typeToString( item, " ", resbuf));
	}
	rt.append( "}");
	return rt;
}

template <typename ELEMTYPE>
class LocalMemVector :public std::pmr::vector<ELEMTYPE>
{
public:
	typedef std::pmr::vector<ELEMTYPE> ParentClass;

public:
	LocalMemVector()
		:std::pmr::vector<ELEMTYPE>( &m_memrsc),m_memrsc(m_buffer,sizeof m_buffer){}
	LocalMemVector( const LocalMemVector& o)
		:std::pmr::vector<ELEMTYPE>( &m_memrsc),m_memrsc(m_buffer,sizeof m_buffer)
	{
		insert( ParentClass::end(), o.begin(), o.end());
	}

private:
	int m_buffer[ 1024];
	mewa::monotonic_buffer_resource m_memrsc;
};

TypeDatabase::GetReductionResult TypeDatabase::getReduction( const Scope::Step step, int toType, int fromType, const TagMask& selectTags) const
{
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));

	int redu_buffer[ 512];
	mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
	auto redulist = m_reduTable->get( step, fromType, selectTags, &redu_memrsc);

	TypeDatabase::GetReductionResult rt;
	for (auto const& redu : redulist)
	{
		if (toType == redu.right())
		{
			if (rt.defined())
			{
				if (rt.weight < redu.weight() - std::numeric_limits<float>::epsilon()) continue;
				if (rt.weight <= redu.weight() + std::numeric_limits<float>::epsilon())
				{
					//... weights are equal
					ResultBuffer resbuf_err;
					auto toTypeStr = typeToString( toType, " ", resbuf_err);
					auto fromTypeStr = typeToString( fromType, " ", resbuf_err);
					throw Error( Error::AmbiguousReductionDefinitions, string_format( "%s <- %s", toTypeStr.c_str(), fromTypeStr.c_str()));
				}
			}
			rt.constructor = redu.value()/*constructor*/;
			rt.weight = redu.weight()/*constructor*/;
		}
	}
	return rt;
}

TypeDatabase::GetReductionsResult TypeDatabase::getReductions(
	const Scope::Step step, int fromType,
	const TagMask& selectTags, const TagMask& selectTagsCount, ResultBuffer& resbuf) const
{
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));

	GetReductionsResult rt( resbuf);

	int redu_buffer[ 512];
	mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
	auto redulist = m_reduTable->get( step, fromType, selectTags, &redu_memrsc);

	for (auto const& redu : redulist)
	{
		bool countFlag = selectTagsCount.matches( redu.tagval());
		rt.reductions.push_back( {redu.right(), redu.value()/*constructor*/, redu.weight(), countFlag});
	}
	return rt;
}

TypeDatabase::DeriveResult TypeDatabase::deriveType( 
	const Scope::Step step, int toType, int fromType,
	const TagMask& selectTags, const TagMask& selectTagsCount, int maxCount, ResultBuffer& resbuf) const
{
	DeriveResult rt( resbuf);
	bool alt_searchstate = false;

	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));
	if (maxCount == -1) maxCount = std::numeric_limits<int>::max();
	if (maxCount < 0) throw Error( Error::InvalidBoundary, string_format( "%d", maxCount));

	int buffer[ 4096];
	mewa::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem,LocalMemVector<ReduQueueElem>,std::greater<ReduQueueElem> > priorityQueue;
	{
		int index = stack.pushNonDuplicateStart( fromType, 0/*constructor*/);
		priorityQueue.push( ReduQueueElem( 0.0/*weight*/, index));
	}
	while (!priorityQueue.empty())
	{
		auto qe = priorityQueue.top();
		priorityQueue.pop();

		if (alt_searchstate && qe.weight > rt.weightsum + std::numeric_limits<float>::epsilon()) break;
		// ... weight bigger than best match, according Dijkstra we are done

		auto const& elem = stack[ qe.index];
		if (elem.type == toType)
		{
			// ... we found a result
			if (!alt_searchstate)
			{
				rt.defined = true;
				rt.weightsum = qe.weight;
				stack.collectResult( rt.reductions, qe.index);
				// Set alternative search state, continue search for an alternative solution to report an ambiguous reference error:
				alt_searchstate = true;
			}
			else
			{
				// We have foud an alternative match with the same weight, report an ambiguous reference error:
				stack.collectConflictPath( rt.conflictPath, qe.index);
				break;
			}
		}
		// Put follow elements into priority queue:
		int redu_buffer[ 512];
		mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( step, elem.type, selectTags, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int pathlen = selectTagsCount.matches( redu.tagval()) ? (elem.pathlen+1) : elem.pathlen;
			if (pathlen <= maxCount)
			{
				int index = stack.pushIfNew( redu.right()/*type*/, redu.value()/*constructor*/, qe.index/*prev*/, pathlen);
				if (index >= 0)
				{
					priorityQueue.push( ReduQueueElem( qe.weight + redu.weight(), index));
				}
			}
		}
	}
	return rt;
}

void TypeDatabase::collectResultItems( std::pmr::vector<int>& items, int typerecidx) const
{
	while (typerecidx > 0)
	{
		items.push_back( typerecidx);
		typerecidx = m_typerecMap[ typerecidx-1].next;
	}
}

TypeDatabase::ResolveResult TypeDatabase::resolveType(
		const Scope::Step step, const std::pmr::vector<int>& contextTypes,
		const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	return resolveType_( step, contextTypes.data(), contextTypes.size(), name, selectTags, resbuf);
}

TypeDatabase::ResolveResult TypeDatabase::resolveType(
		const Scope::Step step, int contextType,
		const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	return resolveType_( step, &contextType, 1, name, selectTags, resbuf);
}

TypeDatabase::ResolveResult TypeDatabase::resolveType_(
		const Scope::Step step, int const* contextTypeAr, std::size_t contextTypeSize,
		const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	ResolveResult rt( resbuf);
	bool alt_searchstate = false;

	int nameid = m_identMap->lookup( name);
	if (!nameid) return rt;

	int buffer[ 1024];
	mewa::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem,LocalMemVector<ReduQueueElem>,std::greater<ReduQueueElem> > priorityQueue;

	std::size_t ci = 0;
	for (; ci < contextTypeSize; ++ci)
	{
		if (contextTypeAr[ci] < 0 || contextTypeAr[ci] > (int)m_typerecMap.size())
		{
			throw Error( Error::InvalidHandle, string_format( "%d", contextTypeAr[ci]));
		}
		int index = stack.pushNonDuplicateStart( contextTypeAr[ci], (int)ci+1/*root index stored as constructor*/);
		if (index >= 0) priorityQueue.push( ReduQueueElem( 0.0/*weight*/, index));
	}
	while (!priorityQueue.empty())
	{
		auto qe = priorityQueue.top();
		priorityQueue.pop();

		if (alt_searchstate && qe.weight > rt.weightsum + std::numeric_limits<float>::epsilon()) break;
		// ... weight bigger than best match, according Dijkstra we are done

		auto const& elem = stack[ qe.index];

		int typerecidx = m_typeTable->get( step, TypeDef( elem.type, nameid));
		if (typerecidx)
		{
			//... we found a match
			if (!alt_searchstate)
			{
				//... we found the first match
				rt.weightsum = qe.weight;
				rt.contextType = elem.type;
				stack.collectResultWithRootIndex( rt.rootIndex, rt.reductions, qe.index);
				collectResultItems( rt.items, typerecidx);
				// Set alternative search state, continue search for an alternative solution to report an ambiguous reference error:
				alt_searchstate = true;
			}
			else
			{
				// We have foud an alternative match with the same weight, report an ambiguous reference error:
				rt.conflictType = elem.type;
				break;
			}
		}

		int redu_buffer[ 512];
		mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( step, elem.type, selectTags, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int index = stack.pushIfNew( redu.right()/*type*/, redu.value()/*constructor*/, qe.index/*prev*/, elem.pathlen+1);
			if (index >= 0)
			{
				//... type not used yet in a previous search (avoid cycles)
				priorityQueue.push( ReduQueueElem( qe.weight + redu.weight(), index));
			}
		}
	}
	return rt;
}

TypeDatabase::ObjectInstanceTree TypeDatabase::getObjectInstanceTree( const std::string_view& name) const
{
	auto oi = m_objidMap->find( m_identMap->lookup( name));
	int oidx = (oi == m_objidMap->end()) ? 0 : oi->second;
	return m_objAr[ oidx].getTree();
}

TypeDatabase::ReductionDefinitionTree TypeDatabase::getReductionDefinitionTree() const
{
	struct TranslateItem
	{
		TranslateItem(){}
		ScopeHierarchyTreeNode<ReductionDefinitionList> operator()( const ReductionTable::TreeNode& node)
		{
			ScopeHierarchyTreeNode<ReductionDefinitionList> rt( node.scope, {});
			for (auto const& elem : node.value)
			{
				rt.value.emplace_back( elem.relation.second/*toType*/, elem.relation.first/*fromType*/, elem.value/*constructor*/, elem.tag, elem.weight);
			}
			return rt;
		}
	};
	Tree<ReductionTable::TreeNode> tree = m_reduTable->getTree();
	TranslateItem translateItem;
	return tree.translate< ScopeHierarchyTreeNode<ReductionDefinitionList> >( translateItem);
}

TypeDatabase::TypeDefinitionTree TypeDatabase::getTypeDefinitionTree() const
{
	struct TranslateItem
	{
		TranslateItem(){}
		ScopeHierarchyTreeNode<TypeDefinitionList> operator()( const TypeTable::TreeNode& node)
		{
			ScopeHierarchyTreeNode<TypeDefinitionList> rt( node.scope, {});
			for (auto const& elem : node.value)
			{
				rt.value.push_back( elem.second);
			}
			return rt;
		}
	};
	Tree<TypeTable::TreeNode> tree = m_typeTable->getTree();
	TranslateItem translateItem;
	return tree.translate< ScopeHierarchyTreeNode<TypeDefinitionList> >( translateItem);
}


