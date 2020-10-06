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
#include <utility>
#include <algorithm>
#include <queue>

using namespace mewa;

std::string TypeDatabase::typeToString( int type) const
{
	std::string rt;
	if (type == 0) return rt;
	const TypeDef& td = m_typerecMap[ type-1].inv;
	if (td.first)
	{
		rt.append( typeToString( td.first));
		rt.push_back( ' ');
	}
	rt.append( m_identMap->inv( td.second));
	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.parameterlen)
	{
		rt.append( "( ");
		for (int pi = 0; pi < rec.parameterlen; ++pi)
		{
			if (pi) rt.append( ", "); 
			rt.append( typeToString( m_parameterMap[ rec.parameter + pi -1].type));
		}
		rt.push_back( ')');
	}
	return rt;
}

TypeDatabase::ParameterList TypeDatabase::parameters( int type) const noexcept
{
	if (type == 0) return ParameterList( 0, nullptr);
	const TypeRecord& rec = m_typerecMap[ type-1];
	if (rec.parameter < 0) return ParameterList( 0, nullptr);
	return ParameterList( rec.parameterlen, m_parameterMap.data() + rec.parameter - 1);
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

int TypeDatabase::defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, const std::vector<Parameter>& parameter, int priority)
{
	if (parameter.size() >= MaxNofParameter) throw Error( Error::TooManyTypeArguments, string_format( "%zu", parameter.size()));
	if ((int)(m_parameterMap.size() + parameter.size()) >= std::numeric_limits<int>::max()) throw Error( Error::CompiledSourceTooComplex);
	if (priority > MaxPriority) throw Error( Error::PriorityOutOfRange, string_format( "%d", priority));
	int parameterlen = parameter.size();
	int parameteridx = parameter.empty() ? 0 : (int)(m_parameterMap.size()+1);
	m_parameterMap.insert( m_parameterMap.end(), parameter.begin(), parameter.end());

	m_typerecMap.reserve( m_typerecMap.size()+1);

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
					throw Error( Error::DuplicateDefinition, typeToString( tri));
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


void TypeDatabase::defineReduction( const Scope& scope, int toType, int fromType, int constructor)
{
	m_reduTable->set( scope, fromType, toType, constructor );
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
	int length;
	int index;

	ReduQueueElem() noexcept
		:length(0),index(-1){}
	ReduQueueElem( int length_, int index_) noexcept
		:length(length_),index(index_){}
	ReduQueueElem( const ReduQueueElem& o) noexcept
		:length(o.length),index(o.index){}

	bool operator < (const ReduQueueElem& o) const noexcept
	{
		return length == o.length ? index < o.index : length < o.length;
	}
};


TypeDatabase::ReduceResult TypeDatabase::reduce( Scope::Step step, int toType, int fromType, ResultBuffer& resbuf)
{
	ReduceResult rt( resbuf);
	
	int buffer[ 1024];
	std::pmr::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem> priorityQueue;
	//NOTE: no pmr variant found for priority_queue

	stack.pushIfNew( fromType, 0/*constructor*/, -1/*prev*/);
	priorityQueue.push( ReduQueueElem( 0/*length*/, 0/*index*/));

	while (!priorityQueue.empty())
	{
		auto qe = priorityQueue.top();
		priorityQueue.pop();
		auto const& elem = stack[ qe.index];

		if (elem.type == toType)
		{
			stack.collectResult( rt.reductions, qe.index);
			break;
		}
		int redu_buffer[ 512];
		std::pmr::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( elem.type, step, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int index = stack.pushIfNew( redu.type(), redu.value()/*constructor*/, qe.index/*prev*/);
			if (index >= 0)
			{
				priorityQueue.push( ReduQueueElem( qe.length+1/*length*/, index));
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

TypeDatabase::ResolveResult TypeDatabase::resolve( Scope::Step step, int contextType, const std::string_view& name, ResultBuffer& resbuf)
{
	ResolveResult rt( resbuf);
	int nameid = m_identMap->get( name);

	int buffer[ 1024];
	std::pmr::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	ReduStack stack( &stack_memrsc, sizeof buffer);
	std::priority_queue<ReduQueueElem> priorityQueue;
	//NOTE: no pmr variant found for priority_queue

	stack.pushIfNew( contextType, 0/*constructor*/, -1/*prev*/);
	priorityQueue.push( ReduQueueElem( 0/*length*/, 0/*index*/));

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
			break;
		}

		int redu_buffer[ 512];
		std::pmr::monotonic_buffer_resource redu_memrsc( redu_buffer, sizeof redu_buffer);
		auto redulist = m_reduTable->get( step, elem.type, &redu_memrsc);

		for (auto const& redu : redulist)
		{
			int index = stack.pushIfNew( redu.type(), redu.value()/*constructor*/, qe.index/*prev*/);
			if (index >= 0)
			{
				//... type not used yet in a previous search (avoid cycles)
				priorityQueue.push( ReduQueueElem( qe.length+1/*length*/, index));
			}
		}
	}
	return rt;
}


