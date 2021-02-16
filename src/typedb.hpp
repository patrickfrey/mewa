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
#include "iterator.hpp"
#include "identmap.hpp"
#include "tree.hpp"
#include "memory_resource.hpp"
#include <string_view>
#include <vector>
#include <cstdlib>
#include <memory>
#include <utility>
#include <algorithm>

namespace mewa {

class TypeDatabase
{
public:
	explicit TypeDatabase( std::size_t initmemsize = 1<<26)
		:m_memblock(std::max((initmemsize/8) *8,(std::size_t)1024)),m_memory()
		,m_objidMap(),m_objAr(),m_typeTable(),m_reduTable(),m_identMap(),m_parameterMap(),m_typerecMap()
	{
		m_memory.reset( new Memory( m_memblock.ptr, m_memblock.size));

		m_objidMap.reset( new ObjectIdMap( m_memory->resource_objtab()));
		m_objAr.push_back( ObjectAccess( m_memory->resource_objtab(), -1/*nullval*/, 0/*nof init buckets*/));
		m_typeTable.reset( new TypeTable( m_memory->resource_typetab(), 0/*nullval*/, m_memory->nofScopedMapInitBuckets()));
		m_reduTable.reset( new ReductionTable( m_memory->resource_redutab(), m_memory->nofReductionMapInitBuckets()));
		m_identMap.reset( new IdentMap( m_memory->resource_identmap(), m_memory->resource_identstr(), m_memory->nofIdentInitBuckets()));

		m_parameterMap.reserve( m_memory->nofParameterMapInitBuckets());
		m_typerecMap.reserve( m_memory->nofTypeRecordMapInitBuckets());
	}
	TypeDatabase( const TypeDatabase& ) = delete;

public:
	struct TypeList
	{
		int arsize;
		int const* ar;

		TypeList( const std::vector<int>& ar_) noexcept
			:arsize(ar_.size()),ar(ar_.data()){}
		TypeList( const std::pmr::vector<int>& ar_) noexcept
			:arsize(ar_.size()),ar(ar_.data()){}
		TypeList( int arsize_, int const* ar_) noexcept
			:arsize(arsize_),ar(ar_){}
		TypeList( const TypeList& o) noexcept
			:arsize(o.arsize),ar(o.ar){}

		bool empty() const noexcept
		{
			return !arsize;
		}
		std::size_t size() const noexcept
		{
			return arsize;
		}
		int operator[]( int idx) const noexcept
		{
			return ar[ idx];
		}
	};
	struct TypeConstructorPair
	{
		int type;
		int constructor;

		TypeConstructorPair() noexcept
			:type(0),constructor(0){}
		TypeConstructorPair( const TypeConstructorPair& o) noexcept
			:type(o.type),constructor(o.constructor){}
		TypeConstructorPair( int type_, int constructor_) noexcept
			:type(type_),constructor(constructor_){}
	};
	struct TypeConstructorPairList
	{
		int arsize;
		TypeConstructorPair const* ar;

		TypeConstructorPairList( const std::vector<TypeConstructorPair>& ar_) noexcept
			:arsize(ar_.size()),ar(ar_.data()){}
		TypeConstructorPairList( const std::pmr::vector<TypeConstructorPair>& ar_) noexcept
			:arsize(ar_.size()),ar(ar_.data()){}
		TypeConstructorPairList( int arsize_, const TypeConstructorPair* ar_) noexcept
			:arsize(arsize_),ar(ar_){}
		TypeConstructorPairList( const TypeConstructorPairList& o) noexcept
			:arsize(o.arsize),ar(o.ar){}

		bool empty() const noexcept
		{
			return !arsize;
		}
		std::size_t size() const noexcept
		{
			return arsize;
		}
		const TypeConstructorPair& operator[]( int idx) const noexcept
		{
			return ar[ idx];
		}

		typedef CArrayForwardIterator::const_iterator<TypeConstructorPair> const_iterator;

		const_iterator begin() const noexcept						{return const_iterator( ar);}
		const_iterator end() const noexcept						{return const_iterator( ar+arsize);}
	};
	struct ResultBuffer
	{
		enum {NofBufferElements = 1024, BufferSize = NofBufferElements*sizeof(int)};
		int buffer[ NofBufferElements];
		mewa::monotonic_buffer_resource memrsc;

		int buffersize() const noexcept
		{
			return sizeof buffer;
		}
		ResultBuffer() noexcept
			:memrsc( buffer, BufferSize){}
		ResultBuffer( const ResultBuffer&) = delete;
	};
	struct GetReductionResult
	{
		float weight;
		int constructor;

		bool defined() const noexcept	{return constructor >= 0;}

		GetReductionResult() noexcept
			:weight(0.0),constructor(-1){}
		GetReductionResult( const GetReductionResult& o) noexcept
			:weight(o.weight),constructor(o.constructor){}
		GetReductionResult( float weight_, int constructor_) noexcept
			:weight(weight_),constructor(constructor_){}
	};
	struct WeightedReduction
	{
		int type;
		int constructor;
		float weight;

		WeightedReduction( const WeightedReduction& o) noexcept
			:type(o.type),constructor(o.constructor),weight(o.weight){}
		WeightedReduction( int type_, int constructor_, float weight_) noexcept
			:type(type_),constructor(constructor_),weight(weight_){}
	};
	struct GetTypesResult
	{
		std::pmr::vector<int> types;

		GetTypesResult( ResultBuffer& resbuf) noexcept
			:types(&resbuf.memrsc)
		{
			types.reserve( (resbuf.buffersize() / sizeof(int)));
		}
		GetTypesResult( const GetTypesResult&) = delete;
		GetTypesResult( GetTypesResult&& o)
			:types(std::move(o.types)){}
	};
	struct GetReductionsResult
	{
		std::pmr::vector<WeightedReduction> reductions;

		GetReductionsResult( ResultBuffer& resbuf) noexcept
			:reductions(&resbuf.memrsc)
		{
			reductions.reserve( (resbuf.buffersize() / sizeof(WeightedReduction)));
		}
		GetReductionsResult( const GetReductionsResult&) = delete;
		GetReductionsResult( GetReductionsResult&& o)
			:reductions(std::move(o.reductions)){}
	};
	struct DeriveResult
	{
		std::pmr::vector<TypeConstructorPair> reductions;
		std::pmr::vector<int> conflictPath;
		float weightsum;
		bool defined;

		DeriveResult( ResultBuffer& resbuf) noexcept
			:reductions(&resbuf.memrsc),conflictPath(&resbuf.memrsc),weightsum(0.0),defined(false)
		{
			reductions.reserve(   ((resbuf.buffersize() / 4) * 3) / sizeof(TypeConstructorPair));
			conflictPath.reserve( ((resbuf.buffersize() / 4) * 1) / sizeof(int));
		}
		DeriveResult( const DeriveResult&) = delete;
		DeriveResult( DeriveResult&& o)
			:reductions(std::move(o.reductions)),conflictPath(std::move(o.conflictPath)),weightsum(o.weightsum),defined(o.defined){}
	};
	struct ResolveResult
	{
		std::pmr::vector<TypeConstructorPair> reductions;
		std::pmr::vector<int> items;
		int rootIndex;
		int contextType;
		int conflictType;
		float weightsum;

		ResolveResult( ResultBuffer& resbuf) noexcept
			:reductions(&resbuf.memrsc),items(&resbuf.memrsc),rootIndex(-1),contextType(-1),conflictType(-1),weightsum(0.0)
		{
			reductions.reserve( resbuf.buffersize() / 2 / sizeof(TypeConstructorPair));
			items.reserve( resbuf.buffersize() / 2 / sizeof(int));
		}
		ResolveResult( const ResolveResult&) = delete;
		ResolveResult( ResolveResult&& o)
			:reductions(std::move(o.reductions)),items(std::move(o.items))
			,rootIndex(o.rootIndex),contextType(o.contextType),conflictType(o.conflictType),weightsum(o.weightsum){}
	};
	struct ReductionDefinition
	{
		int toType;
		int fromType;
		int constructor;
		int tag;
		float weight;

		ReductionDefinition( const ReductionDefinition& o) noexcept
			:toType(o.toType),fromType(o.fromType),constructor(o.constructor),tag(o.tag),weight(o.weight){}
		ReductionDefinition( int toType_, int fromType_, int constructor_, int tag_, float weight_) noexcept
			:toType(toType_),fromType(fromType_),constructor(constructor_),tag(tag_),weight(weight_){}
	};
	typedef std::vector<ReductionDefinition> ReductionDefinitionList;
	typedef std::vector<int> TypeDefinitionList;

	typedef Tree<ScopeHierarchyTreeNode<int> > ObjectInstanceTree;
	typedef Tree<ScopeHierarchyTreeNode<ReductionDefinitionList> > ReductionDefinitionTree;
	typedef Tree<ScopeHierarchyTreeNode<TypeDefinitionList> > TypeDefinitionTree;

public:
	/// \brief Define an object retrievable by its name within a scope
	/// \param[in] name the name of the object
	/// \param[in] scope the scope of this definition
	/// \param[in] handle the handle (non negative integer number, negative value -1 reserved by the getter for not found) for the object given by the caller
	void setObjectInstance( const std::string_view& name, const Scope& scope, int handle);

	/// \brief Retrieve an object by its name in the innermost scope covering the specified scope step
	/// \param[in] name the name of the object
	/// \param[in] step the scope step of the search defining what are valid definitions
	/// \return handle of the object defined by the caller in the given scope step with the name specified (setObject), or -1 if no object found
	int getObjectInstance( const std::string_view& name, const Scope::Step step) const;

	/// \brief Retrieve an object by its name assigned to the scope specified as parameter
	/// \param[in] name the name of the object
	/// \param[in] scope the scope of the definition we are looking for
	/// \return handle of the object defined by the caller in the given scope with the name specified (setObject), or -1 if no object defined in that scope
	int getObjectInstanceOfScope( const std::string_view& name, const Scope scope) const;

	/// \brief Get the scope dependency tree of all objects with a defined name
	/// \param[in] name name of the elements to retrieve
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	ObjectInstanceTree getObjectInstanceTree( const std::string_view& name) const;

	/// \brief Define a new type and get its handle
	/// \param[in] scope the scope of this definition
	/// \param[in] contextType the context of this type.
	/// \param[in] name name of the type
	/// \param[in] constructor handle for the constructor of the type or 0 if not defined
	/// \param[in] parameter list of parameters as part of the function signature
	/// \param[in] priority the priority resolves conflicts of definitions with the same signature in the same scope. The higher priority value wins.
	/// \return the handle assigned to the new created type
	///		or -1 if the type is already defined in the same scope with the same signature (duplicate definition)
	///		or 0 if the type is already defined in the same scope with the same signature but with a higher priority (second definition siletly discarded)
	int defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, const TypeConstructorPairList& parameter, int priority);

	/// \brief Define a new type representing another type already defined (a kind of synonym)
	/// \param[in] scope the scope of this definition
	/// \param[in] contextType the context of this type.
	/// \param[in] name name of the type
	/// \param[in] type handle of the type defined
	/// \return true in case of success, false in case of already defined (duplicate definition)
	bool defineTypeAs( const Scope& scope, int contextType, const std::string_view& name, int type);

	/// \brief Get a type with exact signature defined in a specified scope (does not search other valid definitions in enclosing scopes)
	/// \param[in] scope the scope of this definition
	/// \param[in] contextType the context of this type. The context type of the type definition retrieved is reachable via a path of type reductions.
	/// \param[in] name name of the type
	/// \param[in] parameter list of parameter types as part of the function signature
	/// \return the type handle or 0 if not defined
	int getType( const Scope& scope, int contextType, const std::string_view& name, const TypeList& parameter) const;

	/// \brief Get the list of all types defined in a specified scope differing in the list of parameters
	/// \remark does not search other valid definitions in enclosing scopes
	/// \param[in] scope the scope of this definition
	/// \param[in] contextType the context of this type. The context type of the type definitions retrieved is reachable via a path of type reductions.
	/// \param[in] name name of the type
	/// \param[in,out] resbuf the buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the list of types found
	GetTypesResult getTypes( const Scope& scope, int contextType, const std::string_view& name, ResultBuffer& resbuf) const;

	/// \brief Get the scope dependency tree of all types defined
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	TypeDefinitionTree getTypeDefinitionTree() const;

	/// \brief Define a reduction of a type to another type
	/// \param[in] scope the scope of this definition
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[in] constructor the handle for the constructor that implements the construction of the target type from the source type
	///				or 0 for the identity (no constructor needed)
	/// \param[in] tag value inbetween 1 and 32 attached to the reduction that classifies it.
	/// \note	A search involving type reductions selects the reductions considered by their tags with a set of tags (TagMask).
	/// \param[in] weight the weight given to the reduction in the search, the reduction path with the lowest sum of weights wins
	void defineReduction( const Scope& scope, int toType, int fromType, int constructor, int tag, float weight);

	/// \brief Get the scope dependency tree of all reductions defined
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	ReductionDefinitionTree getReductionDefinitionTree() const;

	/// \brief Get the weight and the constructor of a reduction of a type to another type or {0.0,0} if undefined
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[in] selectTags set of tags selecting the reduction classes to use in this search
	/// \return the constructor of the reduction found, -1 if not found, 0 if undefined
	/// \note throws Error::AmbiguousTypeReference if more than one reduction is found
	GetReductionResult getReduction( const Scope::Step step, int toType, int fromType, const TagMask& selectTags) const;

	/// \brief Get the list of reductions defined for a type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] fromType the source type of the reduction
	/// \param[in] selectTags set of tags selecting the reduction classes to use in this search
	/// \param[in,out] resbuf the buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the list of reductions found
	GetReductionsResult getReductions( const Scope::Step step, int fromType, const TagMask& selectTags, ResultBuffer& resbuf) const;

	/// \brief Search for the sequence of reductions with the minimal sum of weights from one type to another type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[in] selectTags set of tags selecting the reduction classes to use in this search
	/// \param[in] selectTagsPathLength set of tags (subset of selectTags) that contribute to the path length count of the search
	/// \param[in] maxPathLengthCount maximum path length count (number of reductions matching selectTagsPathLength) of an accepted result, -1 for undefined
	/// \param[in,out] resbuf the buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the path found and its weight sum that is minimal and a conflict path if the solution is not unique
	DeriveResult deriveType( const Scope::Step step, int toType, int fromType, 
				 const TagMask& selectTags, const TagMask& selectTagsPathLength, int maxPathLengthCount, ResultBuffer& resbuf) const;

	/// \brief Resolve a type name in a context reducible from one of the contexts passed
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] contextTypes the context of the candidate query types.
	/// 		The searched item has a context type that is reachable via a path of type reductions from an element of this list.
	/// \param[in] name name of the type searched
	/// \param[in] selectTags set of tags selecting the reduction classes to use in this search
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the context type found and the shortest path to it, a list of all candidate items and a conflicting context type if there is one
	ResolveResult resolveType( const Scope::Step step, const std::pmr::vector<int>& contextTypes,
				   const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const;

	/// \brief Resolve a type name in a context reducible from the context passed
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] contextType the context of the candidate query type.
	/// 		The searched item has a context type that is reachable via a path of type reductions from this context type.
	/// \param[in] name name of the type searched
	/// \param[in] selectTags set of tags selecting the reduction classes to use in this search
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the context type found and the shortest path to it, a list of all candidate items and a conflicting context type if there is one
	ResolveResult resolveType( const Scope::Step step, int contextType,
				   const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const;

	/// \brief Get the string representation of a type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \param[in] sep separator between type elements
	/// \param[in,out] resbuf buffer used for memory allocation when building the result (allocate memory on the stack instead of the heap)
	/// \return the complete string assigned to a type
	std::pmr::string typeToString( int type, const char* sep, ResultBuffer& resbuf) const;

	/// \brief Get the name of a type (without context info and parameters)
	/// \param[in] type the handle of the type (return value of defineType)
	/// \return the name string of a type (name parameter of define type)
	std::string_view typeName( int type) const;

	/// \brief Get the parameters attached to a type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \return a view on the list of parameters
	TypeConstructorPairList typeParameters( int type) const;

	/// \brief Get the constructor of a type or 0 if undefined
	/// \param[in] type the handle of the type (return value of defineType)
	/// \return the handle of the constructor of the type
	int typeConstructor( int type) const;

	/// \brief Get the scope where the type has been defined
	/// \note Used to enforce some stricter rules for the visibility of certain objects, e.g. access variables in locally declared functions
	/// \param[in] type the handle of the type (return value of defineType)
	/// \return the scope where type has been defined
	Scope typeScope( int type) const;

	/// \brief Get the context type of this type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \return the parameter contextType passed to defineType when defining this type
	int typeContext( int type) const;

private:
	bool compareParameterSignature( const TypeConstructorPairList& parameter, int param2, int paramlen2) const noexcept;
	int findTypeWithSignature( int typerecidx, const TypeConstructorPairList& parameter, int& lastIndex) const noexcept;
	bool compareParameterSignature( const TypeList& parameter, int param2, int paramlen2) const noexcept;
	int findTypeWithSignature( int typerecidx, const TypeList& parameter) const noexcept;
	void collectResultItems( std::pmr::vector<int>& items, int typerecidx) const;
	std::string reductionsToString( const std::pmr::vector<TypeConstructorPair>& reductions) const;
	std::string deriveResultToString( const DeriveResult& res) const;
	std::string resolveResultToString( const ResolveResult& res) const;
	void appendTypeToString( std::pmr::string& res, int type, const char* sep) const;
	std::string debugTypeToString( int type, const char* sep) const;
	ResolveResult resolveType_( const Scope::Step step, int const* contextTypeAr, std::size_t contextTypeSize,
				   const std::string_view& name, const TagMask& selectTags, ResultBuffer& resbuf) const;

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
		MemoryBlock( const MemoryBlock&) = delete;
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
			,m_nofInitBuckets( std::max( buffersize / (1<<6), (std::size_t)1024))
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
		std::size_t nofReductionMapInitBuckets() const noexcept		{return std::max( m_nofInitBuckets/2, (std::size_t)1024);}
		std::size_t nofParameterMapInitBuckets() const noexcept		{return m_nofInitBuckets;}
		std::size_t nofTypeRecordMapInitBuckets() const noexcept	{return m_nofInitBuckets;}
		std::size_t nofObjectInstanceTableInitBuckets() const noexcept	{return std::max( m_nofInitBuckets/8, (std::size_t)1024);}

	private:
		mewa::monotonic_buffer_resource m_identmap_memrsc;
		mewa::monotonic_buffer_resource m_identstr_memrsc;
		mewa::monotonic_buffer_resource m_typetab_memrsc;
		mewa::monotonic_buffer_resource m_redutab_memrsc;
		mewa::monotonic_buffer_resource m_objtab_memrsc;
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
	typedef ScopedInstance<int> ObjectAccess;
	typedef ScopedMap<TypeDef,int> TypeTable;
	typedef ScopedRelationMap<int,int> ReductionTable;

	struct TypeRecord
	{
		Scope scope;
		int constructor;
		short priority;
		short parameterlen;
		int parameter;
		int next;
		TypeDef inv;

		TypeRecord( const TypeRecord& o)
			:scope(o.scope),constructor(o.constructor),priority(o.priority),parameterlen(o.parameterlen)
			,parameter(o.parameter),next(o.next),inv(o.inv){}
		TypeRecord( int constructor_, const Scope scope_, int parameter_, short parameterlen_, short priority_, const TypeDef& inv_)
			:scope(scope_),constructor(constructor_),priority(priority_),parameterlen(parameterlen_)
			,parameter(parameter_),next(0),inv(inv_){}
	};

	MemoryBlock m_memblock;				//< memory block used first by m_memory
	std::unique_ptr<Memory> m_memory;		//< memory resource used by maps (has to be defined before tables and maps as these depend on it!)
	std::unique_ptr<ObjectIdMap> m_objidMap;	//< map of object identifiers to index into object sets
	std::vector<ObjectAccess> m_objAr;		//< object access array
	std::unique_ptr<TypeTable> m_typeTable;		//< table of types
	std::unique_ptr<ReductionTable> m_reduTable;	//< table of reductions
	std::unique_ptr<IdentMap> m_identMap;		//< identifier string to integer map
	std::vector<TypeConstructorPair> m_parameterMap;//< map index to parameter arrays
	std::vector<TypeRecord> m_typerecMap;		//< type definition structures
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


