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

void TypeDatabase::appendTypeToString( std::pmr::string& res, int type) const
{
	if (type == 0) return;
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.inv.contextType)
	{
		appendTypeToString( res, rec.inv.contextType);
		res.push_back( ' ');
	}
	res.append( m_identMap->inv( rec.inv.ident));
	if (rec.parameterlen)
	{
		res.append( "( ");
		for (int pi = 0; pi < rec.parameterlen; ++pi)
		{
			if (pi) res.append( ", "); 
			appendTypeToString( res, m_parameterMap[ rec.parameter + pi -1].type);
		}
		res.push_back( ')');
	}
}

std::pmr::string TypeDatabase::typeToString( int type, ResultBuffer& resbuf) const
{
	std::pmr::string rt( &resbuf.memrsc);
	rt.reserve( resbuf.buffersize()-1);
	appendTypeToString( rt, type);
	return rt;
}

std::string_view TypeDatabase::typeName( int type) const
{
	if (type == 0) return std::string_view("",0);
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return m_identMap->inv( rec.inv.ident);
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

TypeDatabase::ParameterList TypeDatabase::typeParameters( int type) const
{
	if (type == 0) return ParameterList( 0, nullptr);
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.parameter < 0) return ParameterList( 0, nullptr);
	return ParameterList( rec.parameterlen, m_parameterMap.data() + rec.parameter - 1);
}

int TypeDatabase::typeConstructor( int type) const
{
	if (type == 0) return 0;
	if (type < 0 || type > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle);

	const TypeRecord& rec = m_typerecMap[ type-1];
	return rec.constructor;
}

bool TypeDatabase::compareParameterSignature( int param1, short paramlen1, int param2, short paramlen2) const noexcept
{
	if (paramlen1 != paramlen2) return false;
	if (param1 == 0 || param2 == 0) return true/*param length is equal and one param length is 0*/;
	for (int pi = 0; pi < paramlen1; ++pi)
	{
		if (m_parameterMap[ param1+pi-1].type != m_parameterMap[ param2+pi-1].type) return false;
	}
	return true;
}

int TypeDatabase::defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, const ParameterList& parameter, int priority)
{
	if (parameter.size() >= MaxNofParameter) throw Error( Error::TooManyTypeArguments, string_format( "%zu", parameter.size()));
	if ((int)(m_parameterMap.size() + parameter.size()) >= std::numeric_limits<int>::max()) throw Error( Error::CompiledSourceTooComplex);
	if (priority > MaxPriority) throw Error( Error::PriorityOutOfRange, string_format( "%d", priority));
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
	m_typerecMap.push_back( TypeRecord( constructor, parameteridx, parameterlen, priority, typeDef));

	int lastIndex = 0;
	int prev_typerec = m_typeTable->getOrSet( scope, typeDef, typerec);
	if (prev_typerec/*already exists*/)
	{
		// Check if there is a conflict:
		int tri = prev_typerec;
		while (tri)
		{
			auto& tr = m_typerecMap[ tri-1];
			if (compareParameterSignature( tr.parameter, tr.parameterlen, m_typerecMap.back().parameter, m_typerecMap.back().parameterlen))
			{
				if (priority > tr.priority)
				{
					tr.priority = priority;
					tr.constructor = constructor;
					m_typerecMap.pop_back();
					m_parameterMap.resize( m_parameterMap.size()-parameter.size());
					typerec = tri;
				}
				else if (priority == tr.priority)
				{
					ResultBuffer resbuf;
					throw Error( Error::DuplicateDefinition, typeToString( tri, resbuf));
				}
				else
				{
					// .... ignore 2nd definition with lower priority
					m_typerecMap.pop_back();
					m_parameterMap.resize( m_parameterMap.size()-parameter.size());
					typerec = tri;
				}
				break;
			}
			lastIndex = tri;
			tri = tr.next;
		}
		if (!tri) //... loop passed completely, no break called because of compareParameterSignature
		{
			if (!lastIndex) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
			m_typerecMap[ lastIndex-1].next = typerec;
		}
	}
	return typerec;
}


void TypeDatabase::defineReduction( const Scope& scope, int toType, int fromType, int constructor, int tag, float weight)
{
	if (constructor < 0) throw Error( Error::InvalidHandle, string_format( "%d", constructor));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));

	m_reduTable->set( scope, fromType, toType, constructor, tag, weight);
}


struct ReduStackElem
{
	int type;
	int constructor;
	int prev;

	ReduStackElem( const ReduStackElem& o) noexcept
		:type(o.type),constructor(o.constructor),prev(o.prev){}
	ReduStackElem( int type_, int constructor_, int prev_) noexcept
		:type(type_),constructor(constructor_),prev(prev_){}
};

class ReduStack
{
public:
	ReduStack( std::pmr::memory_resource* memrsc, int buffersize)
		:m_ar( memrsc)
	{
		m_ar.reserve( buffersize / sizeof(ReduStackElem));
	}

	int pushIfNew( int type, int constructor, int prev)
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
		m_ar.push_back( {type,constructor,prev});
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

	void collectResult( std::pmr::vector<TypeDatabase::ReductionResult>& result, int index)
	{
		while (index >= 0)
		{
			const ReduStackElem& ee = m_ar[ index];
			result.push_back( {ee.type, ee.constructor} );
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

std::string TypeDatabase::reductionsToString( const std::pmr::vector<ReductionResult>& reductions) const
{
	std::string rt;

	int ridx = 0;
	for (auto const& redu :reductions)
	{
		ResultBuffer resbuf;
		if (ridx++) rt.append( " -> ");
		rt.append( typeToString( redu.type, resbuf));
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
	for (auto const& item :res.items)
	{
		ResultBuffer resbuf;
		if (iidx++) rt.append( ", ");
		rt.append( typeToString( item.type, resbuf));
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

int TypeDatabase::reduction( const Scope::Step step, int toType, int fromType, const TagMask& selectTags) const
{
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));

	int redu_buffer[ 512];
	mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
	auto redulist = m_reduTable->get( step, fromType, selectTags, &redu_memrsc);

	int rt = -1;
	for (auto const& redu : redulist)
	{
		if (toType == redu.right())
		{
			if (rt >= 0)
			{
				ResultBuffer resbuf_err;
				auto toTypeStr = typeToString( toType, resbuf_err);
				auto fromTypeStr = typeToString( fromType, resbuf_err);
				throw Error( Error::AmbiguousTypeReference, string_format( "%s <- %s", toTypeStr.c_str(), fromTypeStr.c_str()));
			}
			rt = redu.value()/*constructor*/;
		}
	}
	return rt;
}

std::pmr::vector<TypeDatabase::ReductionResult> TypeDatabase::reductions( const Scope::Step step, int fromType, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));

	std::pmr::vector<ReductionResult> rt( &resbuf.memrsc);

	int redu_buffer[ 512];
	mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
	auto redulist = m_reduTable->get( step, fromType, selectTags, &redu_memrsc);

	for (auto const& redu : redulist)
	{
		rt.push_back( {redu.right(), redu.value()/*constructor*/});
	}
	return rt;
}

TypeDatabase::DeriveResult TypeDatabase::deriveType( const Scope::Step step, int toType, int fromType, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	DeriveResult rt( resbuf);

	if (fromType <= 0 || fromType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", fromType));
	if (toType < 0 || toType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", toType));

	int buffer[ 1024];
	mewa::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem,LocalMemVector<ReduQueueElem>,std::greater<ReduQueueElem> > priorityQueue;

	stack.pushIfNew( fromType, 0/*constructor*/, -1/*prev*/);
	priorityQueue.push( ReduQueueElem( 0.0/*weight*/, 0/*index*/));

	while (!priorityQueue.empty())
	{
		auto qe = priorityQueue.top();
		priorityQueue.pop();
		auto const& elem = stack[ qe.index];

		if (elem.type == toType)
		{
			//... we found a match and because of Dikstra we are finished
			rt.weightsum = qe.weight;
			stack.collectResult( rt.reductions, qe.index);

			// Search for an alternative solution to report an ambiguous reference error:
			while (!priorityQueue.empty())
			{
				auto alt_qe = priorityQueue.top();
				if (alt_qe.weight > qe.weight + std::numeric_limits<float>::epsilon()) break;
				priorityQueue.pop();

				auto const& alt_elem = stack[ alt_qe.index];

				if (alt_elem.type == toType)
				{
					DeriveResult alt_rt( resbuf);
					alt_rt.weightsum = alt_qe.weight;
					stack.collectResult( alt_rt.reductions, alt_qe.index);

					throw Error( Error::AmbiguousTypeReference, deriveResultToString( rt) + ", " + deriveResultToString( alt_rt));
				}
			}
			break;
		}
		int redu_buffer[ 512];
		mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( step, elem.type, selectTags, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int index = stack.pushIfNew( redu.right()/*type*/, redu.value()/*constructor*/, qe.index/*prev*/);
			if (index >= 0)
			{
				priorityQueue.push( ReduQueueElem( qe.weight + redu.weight(), index));
			}
		}
	}
	return rt;
}

void TypeDatabase::collectResultItems( std::pmr::vector<ResolveResultItem>& items, int typerecidx) const
{
	while (typerecidx > 0)
	{
		const TypeRecord& rec = m_typerecMap[ typerecidx-1];
		items.push_back( ResolveResultItem( typerecidx, rec.constructor));
		typerecidx = rec.next;
	}
}

TypeDatabase::ResolveResult TypeDatabase::resolveType( 
		const Scope::Step step, int contextType, const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const
{
	ResolveResult rt( resbuf);

	if (contextType < 0 || contextType > (int)m_typerecMap.size()) throw Error( Error::InvalidHandle, string_format( "%d", contextType));

	int nameid = m_identMap->lookup( name);
	if (!nameid) return rt;

	int buffer[ 1024];
	mewa::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem,LocalMemVector<ReduQueueElem>,std::greater<ReduQueueElem> > priorityQueue;

	stack.pushIfNew( contextType, 0/*constructor*/, -1/*prev*/);
	priorityQueue.push( ReduQueueElem( 0.0/*weight*/, 0/*index*/));

	while (!priorityQueue.empty())
	{
		auto qe = priorityQueue.top();
		priorityQueue.pop();
		auto const& elem = stack[ qe.index];

		int typerecidx = m_typeTable->get( step, TypeDef( elem.type, nameid));
		if (typerecidx)
		{
			//... we found a match and because of Dikstra we are finished
			stack.collectResult( rt.reductions, qe.index);
			collectResultItems( rt.items, typerecidx);

			// Search for an alternative solution to report an ambiguous reference error:
			while (!priorityQueue.empty())
			{
				auto alt_qe = priorityQueue.top();
				if (alt_qe.weight > qe.weight + std::numeric_limits<float>::epsilon()) break;
				priorityQueue.pop();

				auto const& alt_elem = stack[ alt_qe.index];
				int alt_typerecidx = m_typeTable->get( step, TypeDef( alt_elem.type, nameid));

				if (alt_typerecidx)
				{
					ResultBuffer resbuf_alt;
					ResolveResult alt_rt( resbuf_alt);
					stack.collectResult( alt_rt.reductions, alt_qe.index);
					collectResultItems( alt_rt.items, alt_typerecidx);

					throw Error( Error::AmbiguousTypeReference, resolveResultToString( rt) + ", " + resolveResultToString( alt_rt));
				}
			}
			break;
		}

		int redu_buffer[ 512];
		mewa::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( step, elem.type, selectTags, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int index = stack.pushIfNew( redu.right()/*type*/, redu.value()/*constructor*/, qe.index/*prev*/);
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
				rt.value.emplace_back( elem.relation.second/*toType*/, elem.relation.first/*fromType*/, elem.value/*constructor*/, elem.weight);
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


