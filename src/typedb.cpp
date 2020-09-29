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

using namespace mewa;

std::string TypeDatabase::typeToString( int type) const
{
	std::string rt;
	if (type == 0) return rt;
	const TypeDef& td = m_typeInvTable[ type-1];
	if (td.first)
	{
		rt.append( typeToString( td.first));
		rt.push_back( ' ');
	}
	rt.append( m_identMap.inv( td.second));
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
	if (m_parameterMap.size() + parameter.size() >= std::numeric_limits<int>::max()) throw Error( Error::CompiledSourceTooComplex);
	if (priority > MaxPriority) throw Error( Error::PriorityOutOfRange, string_format( "%d", priority));
	int parameterlen = parameter.size();
	int parameteridx = parameter.empty() ? 0 : (int)(m_parameterMap.size()+1);
	m_parameterMap.insert( m_parameterMap.end(), parameter.begin(), parameter.end());

	m_typerecMap.reserve( m_typerecMap.size()+1);
	m_typeInvTable.reserve( m_typeInvTable.size()+1);

	int typerec = m_typerecMap.size()+1;
	m_typerecMap.push_back( TypeRecord( constructor, parameteridx, parameterlen, priority));
	m_typeInvTable.push_back( TypeDef( contextType, m_identMap.get( name)));

	int lastIndex = 0;
	auto ins = m_typeTable.insert( {{m_typeInvTable.back(), scope}, typerec});
	if (ins.second == false/*already exists*/)
	{
		int tri = ins.first->second;
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
					m_typeInvTable.pop_back();
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

struct ReduStackElem
{
	int type;
	int constructor;
	int prev;

	ReduStackElem( const ReduStackElem& o)
		:type(o.type),constructor(o.constructor),prev(o.prev){}
	ReduStackElem( int type_, int constructor_, int prev_)
		:type(type_),constructor(constructor_),prev(prev_){}
};

struct ReduStack
{
	std::pmr::vector<ReduStackElem> ar;

	ReduStack( std::pmr::memory_resource* memrsc)
		:ar( memrsc){}

	void pushIfNew( int type, int constructor, int prev)
	{
		while (prev < 0)
		{
			const ReduStackElem& ee = ar[ prev];
			if (ee.type == type) return;
			prev = ee.prev;
		}
		ar.push_back( {type,constructor,prev});
	}
};

void TypeDatabase::defineReduction( const Scope& scope, int toType, int fromType, int constructor)
{
	m_reduTable.insert( {{fromType, scope}, {toType, constructor}} );
}

std::pmr::vector<TypeDatabase::ReductionResult> TypeDatabase::reduce( Scope::Step step, int toType, int fromType, ResultBuffer& resbuf)
{
	int buffer[ 2048];
	std::pmr::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	std::pmr::vector<ReductionResult> rt( &resbuf.memrsc);
	
	ReduStack stack( &stack_memrsc);
	return rt;
}

std::pmr::vector<TypeDatabase::ResolveResult> TypeDatabase::resolve( Scope::Step step, int contextType, const std::string_view& name, ResultBuffer& resbuf)
{
	int buffer[ 2048];
	std::pmr::monotonic_buffer_resource stack_memrsc( buffer, sizeof buffer);

	std::pmr::vector<ResolveResult> rt( &resbuf.memrsc);	
	return rt;
}


