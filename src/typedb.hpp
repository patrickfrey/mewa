/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Type system database implementation, data structures to define types and reductions and to resolve them
/// \file "typedb.hpp"
#ifndef _MEWA_TYPEDB_HPP_INCLUDED
#define _MEWA_TYPEDB_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "scope.hpp"
#include "error.hpp"
#include "strings.hpp"
#include "identmap.hpp"
#include <string_view>
#include <vector>
#include <cstdlib>
#include <memory>
#include <algorithm>

namespace mewa {

class TypeDatabase
{
public:
	explicit TypeDatabase( std::size_t initmemsize = 1<<26)
		:m_memblock(std::min((initmemsize/8) *8,(std::size_t)1024)),m_memory()
		,m_objidMap(),m_objSets(),m_typeTable(),m_reduTable(),m_identMap(),m_parameterMap(),m_typerecMap()
	{
		m_memory.reset( new Memory( m_memblock.ptr, m_memblock.size));

		m_objidMap.reset( new ObjectIdMap( m_memory->resource_objtab()));
		m_typeTable.reset( new TypeTable( m_memory->resource_typetab(), 0/*nullval*/, m_memory->nofScopedMapInitBuckets()));
		m_reduTable.reset( new ReductionTable( m_memory->resource_redutab(), m_memory->nofReductionMapInitBuckets()));
		m_identMap.reset( new IdentMap( m_memory->resource_identmap(), m_memory->resource_identstr(), m_memory->nofIdentInitBuckets()));

		m_parameterMap.reserve( m_memory->nofParameterMapInitBuckets());
		m_typerecMap.reserve( m_memory->nofTypeRecordMapInitBuckets());
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
	/// \brief Define an object retrievable by its name within a scope
	/// \param[in] scope Scope the scope of this definition
	/// \param[in] name name of the object
	/// \param[in] handle handle (non negative cardinal number, negative value -1 reserved by the getter for not found) for the object given by the caller
	void setNamedObject( const Scope& scope, const std::string_view& name, int handle);

	/// \brief Retrieve an object by its name in the innermost scope covering the specified scope step
	/// \param[in] step the scope step of the search defining what are valid definitions
	/// \param[in] name name of the object
	/// \return handle of the object defined by the caller in the given scope step with the name specified (setObject), or -1 if no object found
	int getNamedObject( const Scope::Step step, const std::string_view& name) const;

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
	/// \param[in] weight weight given to the reduction in the search, the reduction path with the lowest sum of weights wins
	void defineReduction( const Scope& scope, int toType, int fromType, int constructor, float weight);

	/// \brief Get the reduction constructor of a type to another type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType target type of the reduction
	/// \param[in] fromType source type of the reduction
	/// \return the constructor of the reduction found, -1 if not found
	int reductionConstructor( const Scope::Step step, int toType, int fromType) const;

	/// \brief Get the list of reductions defined for a type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] fromType source type of the reduction
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the list of reductions found
	ReduceResult reductions( const Scope::Step step, int fromType, ResultBuffer& resbuf) const;

	/// \brief Perform a sequence reductions of a type to another type and return the smallest path of reductions found
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType target type of the reduction
	/// \param[in] fromType source type of the reduction
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the shortest path found, throws if two path of same length are found
	ReduceResult reduce( const Scope::Step step, int toType, int fromType, ResultBuffer& resbuf) const;

	/// \brief Resolve a type name in a context of a context reducible from the context passed
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] contextType the context (realm,namespace) of the query type. The searched item has a context type that is reachable via a path of type reductions.
	/// \param[in] name name of the type searched
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the shortest path found, throws if two path of same length are found	
	ResolveResult resolve( const Scope::Step step, int contextType, const std::string_view& name, ResultBuffer& resbuf) const;

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
	void collectResultItems( std::pmr::vector<ResolveResultItem>& items, int typerecidx) const;
	std::string reductionsToString( const std::pmr::vector<ReductionResult>& reductions) const;
	std::string reduceResultToString( const ReduceResult& res) const;
	std::string resolveResultToString( const ResolveResult& res) const;

private:
	struct MemoryBlock
	{
		void* ptr;
		std::size_t size;

		explicit MemoryBlock( std::size_t size_)
		{
			size = size_;
			ptr = std::aligned_alloc( 8/*alignment*/, size);
			if (!ptr) std::bad_alloc();
		}
		~MemoryBlock()
		{
			std::free(ptr);
		}
	};

	class Memory
	{
	public:
		Memory( void* buffer, std::size_t buffersize)
			:m_identmap_memrsc( (char*)buffer+(0*buffersize/4), buffersize/4)
			,m_identstr_memrsc( (char*)buffer+(1*buffersize/4), buffersize/4)
			,m_typetab_memrsc(  (char*)buffer+(2*buffersize/4), buffersize/4)
			,m_redutab_memrsc(  (char*)buffer+(6*buffersize/8), buffersize/8)
			,m_objtab_memrsc(   (char*)buffer+(7*buffersize/8), buffersize/8)
			,m_nofInitBuckets( std::min( buffersize / (1<<6), (std::size_t)1024))
		{
			if (buffersize % 8 != 0) throw Error( Error::LogicError, string_format( "%s line %d", __FILE__, (int)__LINE__));
		}

		std::pmr::memory_resource* resource_identmap() noexcept		{return &m_identmap_memrsc;}
		std::pmr::memory_resource* resource_identstr() noexcept		{return &m_identstr_memrsc;}
		std::pmr::memory_resource* resource_typetab() noexcept		{return &m_typetab_memrsc;}
		std::pmr::memory_resource* resource_redutab() noexcept		{return &m_redutab_memrsc;}
		std::pmr::memory_resource* resource_objtab() noexcept		{return &m_objtab_memrsc;}

		std::size_t nofIdentInitBuckets() const noexcept		{return m_nofInitBuckets;}
		std::size_t nofScopedMapInitBuckets() const noexcept		{return m_nofInitBuckets;}
		std::size_t nofReductionMapInitBuckets() const noexcept		{return std::min( m_nofInitBuckets/2, (std::size_t)1024);}
		std::size_t nofParameterMapInitBuckets() const noexcept		{return m_nofInitBuckets;}
		std::size_t nofTypeRecordMapInitBuckets() const noexcept	{return m_nofInitBuckets;}
		std::size_t nofNamedObjectTableInitBuckets() const noexcept	{return std::min( m_nofInitBuckets/8, (std::size_t)1024);}

	private:
		std::pmr::monotonic_buffer_resource m_identmap_memrsc;
		std::pmr::monotonic_buffer_resource m_identstr_memrsc;
		std::pmr::monotonic_buffer_resource m_typetab_memrsc;
		std::pmr::monotonic_buffer_resource m_redutab_memrsc;
		std::pmr::monotonic_buffer_resource m_objtab_memrsc;
		std::size_t m_nofInitBuckets;
	};

private:
	enum {
		MaxNofParameter = 1U<<15,
		MaxPriority = 1U<<15
	};
	struct TypeDef
	{
		int contextType;
		int ident;

		TypeDef( int contextType_, int ident_) noexcept
			:contextType(contextType_),ident(ident_){}
		TypeDef( const TypeDef& o) noexcept
			:contextType(o.contextType),ident(o.ident){}

		bool operator == (const TypeDef& o) const noexcept		{return contextType==o.contextType && ident == o.ident;}
		bool operator != (const TypeDef& o) const noexcept		{return contextType!=o.contextType || ident != o.ident;}
		bool operator < (const TypeDef& o) const noexcept		{return contextType==o.contextType ? ident < o.ident : contextType < o.contextType;}
	};

	typedef std::pmr::map<int,int> ObjectIdMap;
	typedef ScopedSet<int> ObjectSet;
	typedef ScopedMap<TypeDef,int> TypeTable;
	typedef ScopedRelationMap<int,int> ReductionTable;

	struct TypeRecord
	{
		int constructor;
		short priority;
		short parameterlen;
		int parameter;
		int next;
		TypeDef inv;

		TypeRecord( const TypeRecord& o)
			:constructor(o.constructor),priority(o.priority),parameterlen(o.parameterlen),parameter(o.parameter),next(o.next),inv(o.inv){}
		TypeRecord( int constructor_, int parameter_, short parameterlen_, short priority_, const TypeDef& inv_)
			:constructor(constructor_),priority(priority_),parameterlen(parameterlen_),parameter(parameter_),next(0),inv(inv_){}
	};

	MemoryBlock m_memblock;				//< memory block used first by m_memory
	std::unique_ptr<Memory> m_memory;		//< memory resource used by maps (has to be defined before tables and maps as these depend on it!)
	std::unique_ptr<ObjectIdMap> m_objidMap;	//< map of object identifiers to index into object sets
	std::vector<ObjectSet> m_objSets;		//< object sets
	std::unique_ptr<TypeTable> m_typeTable;		//< table of types
	std::unique_ptr<ReductionTable> m_reduTable;	//< table of reductions
	std::unique_ptr<IdentMap> m_identMap;		//< identifier string to integer map
	std::vector<Parameter> m_parameterMap;		//< map cardinal to parameter arrays
	std::vector<TypeRecord> m_typerecMap;		//< type definition structures
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


