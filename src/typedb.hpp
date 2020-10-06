/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Type system data implementation
/// \file "typedb.hpp"
#ifndef _MEWA_TYPEDB_HPP_INCLUDED
#define _MEWA_TYPEDB_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "scope.hpp"
#include "error.hpp"
#include "strings.hpp"
#include "identmap.hpp"
#include <string_view>
#include <memory>

namespace mewa {

class TypeDatabaseMemory
{
public:
	enum {
		NofIdentInitBuckets = 1<<18,
		NofScopedMapInitBuckets = 1<<18,
		NofReductionMapInitBuckets = 1<<18
	};

	TypeDatabaseMemory() noexcept
		:m_identmap_memrsc( m_identmap_membuffer, sizeof m_identmap_membuffer)
		,m_identstr_memrsc( m_identstr_membuffer, sizeof m_identstr_membuffer)
		,m_typetab_memrsc( m_typetab_membuffer, sizeof m_typetab_membuffer)
		,m_redutab_memrsc( m_redutab_membuffer, sizeof m_redutab_membuffer){}

	std::pmr::memory_resource* resource_identmap() noexcept		{return &m_identmap_memrsc;}
	std::pmr::memory_resource* resource_identstr() noexcept		{return &m_identstr_memrsc;}
	std::pmr::memory_resource* resource_typetab() noexcept		{return &m_typetab_memrsc;}
	std::pmr::memory_resource* resource_redutab() noexcept		{return &m_redutab_memrsc;}

private:
	int m_identmap_membuffer[ 1<<22];
	int m_identstr_membuffer[ 1<<22];
	int m_typetab_membuffer[ 1<<22];
	int m_redutab_membuffer[ 1<<22];

	std::pmr::monotonic_buffer_resource m_identmap_memrsc;
	std::pmr::monotonic_buffer_resource m_identstr_memrsc;
	std::pmr::monotonic_buffer_resource m_typetab_memrsc;
	std::pmr::monotonic_buffer_resource m_redutab_memrsc;
};


class TypeDatabase
{
public:
	TypeDatabase( std::unique_ptr<TypeDatabaseMemory>&& memory_ = std::unique_ptr<TypeDatabaseMemory>( new TypeDatabaseMemory()))
		:m_typeTable( memory_->resource_typetab(), 0/*nullval*/, TypeDatabaseMemory::NofScopedMapInitBuckets)
		,m_reduTable( memory_->resource_redutab(), TypeDatabaseMemory::NofReductionMapInitBuckets)
		,m_identMap( memory_->resource_identmap(), memory_->resource_identstr(), TypeDatabaseMemory::NofIdentInitBuckets)
		,m_parameterMap()
		,m_typerecMap()
		,m_typeInvTable()
		,m_memory(std::move(memory_))
	{
		m_parameterMap.reserve( TypeDatabaseMemory::NofIdentInitBuckets);
		m_typerecMap.reserve( TypeDatabaseMemory::NofIdentInitBuckets);
		m_typeInvTable.reserve( TypeDatabaseMemory::NofIdentInitBuckets);
	}

public:
	struct Parameter
	{
		int type;
		int constructor;

		Parameter() noexcept
			:type(0),constructor(0){}
		Parameter( const Parameter& o) noexcept
			:type(o.type),constructor(o.constructor){}
		Parameter( int type_, int constructor_) noexcept
			:type(type_),constructor(constructor_){}
	};
	struct ParameterList
	{
		int arsize;
		const Parameter* ar;

		ParameterList( int arsize_, const Parameter* ar_)
			:arsize(arsize_),ar(ar_){}
		ParameterList( const ParameterList& o)
			:arsize(o.arsize),ar(o.ar){}

		int size() const noexcept
		{
			return arsize;
		}

		const Parameter& operator[]( int idx) const noexcept
		{
			return ar[ idx];
		}
	};
	struct ResultBuffer
	{
		int buffer[ 1024];
		std::pmr::monotonic_buffer_resource memrsc;

		int buffersize() const noexcept
		{
			return sizeof buffer;
		}
		ResultBuffer() noexcept
			:memrsc( buffer, sizeof buffer){}
	};
	struct ReductionResult
	{
		int type;
		int constructor;

		ReductionResult( const ReductionResult& o) noexcept
			:type(o.type),constructor(o.constructor){}
		ReductionResult( int type_, int constructor_) noexcept
			:type(type_),constructor(constructor_){}
	};
	struct ResolveResultItem
	{
		int type;
		int constructor;

		ResolveResultItem( const ResolveResultItem& o) noexcept
			:type(o.type),constructor(o.constructor){}
		ResolveResultItem( int type_, int constructor_) noexcept
			:type(type_),constructor(constructor_){}
	};
	struct ReduceResult
	{
		std::pmr::vector<ReductionResult> reductions;

		ReduceResult( ResultBuffer& resbuf) noexcept
			:reductions(&resbuf.memrsc)
		{
			reductions.reserve( resbuf.buffersize() / sizeof(ReductionResult));
		}
	};
	struct ResolveResult
	{
		std::pmr::vector<ReductionResult> reductions;
		std::pmr::vector<ResolveResultItem> items;

		ResolveResult( ResultBuffer& resbuf) noexcept
			:reductions(&resbuf.memrsc),items(&resbuf.memrsc)
		{
			reductions.reserve( resbuf.buffersize() / 2 / sizeof(ReductionResult));
			items.reserve( resbuf.buffersize() / 2 / sizeof(ResolveResultItem));
		}
	};

public:
	/// \brief Define a new type, throws if already defined in the same scope with same the signature and the same priority
	/// \param[in] scope Scope the scope of this definition
	/// \param[in] contextType the context (realm,namespace) of this type. A context is reachable via a path of type reductions.
	/// \param[in] name name of the type
	/// \param[in] constructor handle for the constructor of the type
	/// \param[in] parameter list of parameters as part of the function signature
	/// \param[in] priority The priority resolves conflicts of definitions with the same signature in the same scope. The higher priority value wins.
	/// \return the handle assigned to the new created type
	int defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, const std::vector<Parameter>& parameter, int priority);

	/// \brief Define a reduction of a type to another type with a constructor that implements the construction of the target type from the source type
	/// \param[in] scope Scope the scope of this definition
	/// \param[in] toType target type of the reduction
	/// \param[in] fromType source type of the reduction
	/// \param[in] constructor handle for the constructor that implements the construction of the target type from the source type
	void defineReduction( const Scope& scope, int toType, int fromType, int constructor);

	/// \brief Perform a reduction of a type to another type and return the smallest path of reductions
	/// \param[in] scope step the scope step of the search defining what are valid reductions
	/// \param[in] toType target type of the reduction
	/// \param[in] fromType source type of the reduction
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the shortest path found, throws if two path of same length are found
	ReduceResult reduce( Scope::Step step, int toType, int fromType, ResultBuffer& resbuf);

	/// \brief Resolve a type name in a context of a context reducible from the context passed
	/// \param[in] scope step the scope step of the search defining what are valid reductions
	/// \param[in] contextType the context (realm,namespace) of the query type. The searched item has a context type that is reachable via a path of type reductions.
	/// \param[in] name name of the type searched
	/// \param[in] nofParameters expected number of parameters or -1 if not specified
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the shortest path found, throws if two path of same length are found	
	ResolveResult resolve( Scope::Step step, int contextType, const std::string_view& name, int nofParameters, ResultBuffer& resbuf);

	/// \brief Get the string representation of a type
	/// \param[in] type handle of the type (return value of defineType)
	/// \return the complete string assigned to a type
	std::string typeToString( int type) const;

	/// \brief Get the parameters attached to a type
	/// \param[in] type handle of the type (return value of defineType)
	/// \return a view on the list of parameters
	ParameterList parameters( int type) const noexcept;

private:
	bool compareParameterSignature( int param1, short paramlen1, int param2, short paramlen2) const noexcept;

private:
	enum {
		MaxNofParameter = 1U<<15,
		MaxPriority = 1U<<15
	};
	typedef std::pair<int,int> TypeDef;
	typedef ScopedMap<TypeDef,int> TypeTable;
	typedef ScopedRelationMap<int,int> ReductionTable;

	struct TypeRecord
	{
		int constructor;
		short priority;
		short parameterlen;
		int parameter;
		int next;

		TypeRecord( const TypeRecord& o)
			:constructor(o.constructor),priority(o.priority),parameter(o.parameter),next(o.next){}
		TypeRecord( int constructor_, int parameter_, short parameterlen_, short priority_)
			:constructor(constructor_),priority(priority_),parameter(parameter_),next(0){}
	};

	TypeTable m_typeTable;
	ReductionTable m_reduTable;
	IdentMap m_identMap;
	std::vector<Parameter> m_parameterMap;
	std::vector<TypeRecord> m_typerecMap;
	std::vector<TypeDef> m_typeInvTable;

	std::unique_ptr<TypeDatabaseMemory> m_memory;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


