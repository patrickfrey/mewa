/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Exception free shared library interface for the Mewa TypeDatabase (typedb)
/// \note This interface is intended for users that want to use the type database library in a C++ context without using the mewa parser generator or the Lua module.
/// \file "mewa/typedb.hpp"
#ifndef _MEWA_LIBMEWA_TYPEDB_HPP_INCLUDED
#define _MEWA_LIBMEWA_TYPEDB_HPP_INCLUDED
#include <string_view>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstdint>
#include <memory>
#include <utility>
#include <limits>

namespace libmewa {

class Error
{
public:
	Error() noexcept :m_code(0),m_arg(){}
	Error( const Error& o) = default;
	Error( Error&& o) noexcept = default;
	Error& operator=( const Error& o) = default;
	Error& operator=( Error&& o) = default;

	explicit Error( int code_, std::string arg_="") noexcept :m_code(code_),m_arg(std::move(arg_)){}

	int code() const noexcept 			{return m_code;}
	const std::string& arg() const noexcept 	{return m_arg;}

private:
	int m_code;
	std::string m_arg;
};

class Scope
{
public:
	Scope( int start_,  int end_) :m_start(start_),m_end(end_){}
	Scope() noexcept :m_start(0),m_end(std::numeric_limits<int>::max()){}
	Scope( const Scope& o) = default;
	Scope( Scope&& o) noexcept = default;

	int start() const noexcept 			{return m_start;}
	int end() const noexcept 			{return m_end;}

private:
	int m_start;
	int m_end;
};

class TagMask
{
public:
	enum {MinTag=1,MaxTag=32};
	typedef std::uint32_t BitSet;

public:
	TagMask( const TagMask& o) = default;
	TagMask( TagMask&& o) = default;
	TagMask() :m_mask(0){}

	TagMask& operator |= (int tag_)		{add( tag_); return *this;}
	TagMask operator | (int tag_) const	{TagMask rt; rt.add( tag_); return rt;}
	static TagMask matchAll() noexcept	{TagMask rt; rt.m_mask = 0xffFFffFFU; return rt;}

private:
	void add( int tag_)
	{
		m_mask |= (1U << (tag_-1));
	}

private:
	friend class TypeDatabase;
	BitSet m_mask;
};


class ObjectInstanceTree
{
public:
	typedef std::size_t NodeIndex;

	ObjectInstanceTree() :m_impl(nullptr){}
	ObjectInstanceTree( const ObjectInstanceTree& o);
	ObjectInstanceTree( ObjectInstanceTree&& o);
	~ObjectInstanceTree();

	bool defined() const noexcept			{return m_impl;}
	NodeIndex root() const noexcept			{return m_impl ? 1:0;}
	NodeIndex chld( NodeIndex ni) const noexcept;
	NodeIndex next( NodeIndex ni) const noexcept;

	Scope scope( NodeIndex ni) const noexcept;
	int instance( NodeIndex ni) const noexcept;

private:
	friend class TypeDatabase;
	void* m_impl;
};

class TypeDefinitionTree
{
public:
	typedef std::size_t NodeIndex;

	TypeDefinitionTree() :m_impl(nullptr){}
	TypeDefinitionTree( const TypeDefinitionTree& o);
	TypeDefinitionTree( TypeDefinitionTree&& o);
	~TypeDefinitionTree();

	bool defined() const noexcept			{return m_impl;}
	NodeIndex root() const noexcept			{return m_impl ? 1:0;}
	NodeIndex chld( NodeIndex ni) const noexcept;
	NodeIndex next( NodeIndex ni) const noexcept;

	Scope scope( NodeIndex ni) const noexcept;
	std::vector<int> list( NodeIndex ni) const noexcept;

private:
	friend class TypeDatabase;
	void* m_impl;
};

class ReductionDefinitionTree
{
public:
	typedef std::size_t NodeIndex;

	ReductionDefinitionTree() :m_impl(nullptr){}
	ReductionDefinitionTree( const ReductionDefinitionTree& o);
	ReductionDefinitionTree( ReductionDefinitionTree&& o);
	~ReductionDefinitionTree();

	struct Data
	{
		int toType;
		int fromType;
		int constructor;
		int tag;
		float weight;

		Data( int toType_, int fromType_, int constructor_, int tag_, float weight_)
			:toType(toType_),fromType(fromType_),constructor(constructor_),tag(tag_),weight(weight_){}
	};

	bool defined() const noexcept			{return m_impl;}
	NodeIndex root() const noexcept			{return m_impl ? 1:0;}
	NodeIndex chld( NodeIndex ni) const noexcept;
	NodeIndex next( NodeIndex ni) const noexcept;

	Scope scope( NodeIndex ni) const noexcept;
	std::vector<Data> list( NodeIndex ni) const noexcept;

private:
	friend class TypeDatabase;
	void* m_impl;
};

struct Parameter
{
	int type;
	int constructor;

	Parameter() noexcept :type(0),constructor(0){}
	Parameter( const Parameter& o) noexcept :type(o.type),constructor(o.constructor){}
	Parameter( int type_, int constructor_) noexcept :type(type_),constructor(constructor_){}
};

struct ParameterList
{
	int arsize;
	Parameter const* ar;

	ParameterList() noexcept :arsize(0),ar(nullptr){}
	ParameterList( const std::vector<Parameter>& ar_) noexcept :arsize(ar_.size()),ar(ar_.data()){}
	ParameterList( int arsize_, const Parameter* ar_) noexcept :arsize(arsize_),ar(ar_){}
	ParameterList( const ParameterList& o) noexcept :arsize(o.arsize),ar(o.ar){}

	bool empty() const noexcept				{return !arsize;}
	std::size_t size() const noexcept			{return arsize;}
	const Parameter& operator[]( int idx) const noexcept 	{return ar[ idx];}
};

struct ReductionResult
{
	int type;
	int constructor;

	ReductionResult( const ReductionResult& o) noexcept :type(o.type),constructor(o.constructor){}
	ReductionResult( int type_, int constructor_) noexcept :type(type_),constructor(constructor_){}
};

struct ResolveResultItem
{
	int type;
	int constructor;

	ResolveResultItem( const ResolveResultItem& o) noexcept :type(o.type),constructor(o.constructor){}
	ResolveResultItem( int type_, int constructor_) noexcept :type(type_),constructor(constructor_){}
};

struct DeriveResult
{
	std::vector<ReductionResult> reductions;
	float weightsum;

	DeriveResult() noexcept :reductions(),weightsum(0.0){}
	DeriveResult( const DeriveResult&) = delete;
	DeriveResult( DeriveResult&& o) :reductions(std::move(o.reductions)),weightsum(o.weightsum){}
};

struct ResolveResult
{
	std::vector<ReductionResult> reductions;
	std::vector<ResolveResultItem> items;
	float weightsum;

	ResolveResult() noexcept :reductions(),items(),weightsum(0.0){}
	ResolveResult( const ResolveResult&) = delete;
	ResolveResult( ResolveResult&& o) :reductions(std::move(o.reductions)),items(std::move(o.items)),weightsum(o.weightsum){}
};

class TypeDatabase
{
	/// \brief Constructor
	TypeDatabase( std::size_t initmemsize = 1<<26)
	{
		if (!init( initmemsize)) throw std::bad_alloc();
	}

	TypeDatabase( const TypeDatabase&) = delete;
	TypeDatabase( TypeDatabase&&) = delete;

	/// \brief Destructor
	~TypeDatabase();

	/// \brief Define an object retrievable by its name within a scope
	/// \param[in] name the name of the object
	/// \param[in] scope the scope of this definition
	/// \param[in] handle the handle (non negative cardinal number, negative value -1 reserved by the getter for not found) for the object given by the caller
	/// \param[out] error error description in case of false returned
	/// \return true in case of success, false in case of error
	bool setObjectInstance( const std::string_view& name, const Scope& scope, int handle, Error& error) noexcept;

	/// \brief Retrieve an object by its name in the innermost scope covering the specified scope step
	/// \param[in] name the name of the object
	/// \param[in] step the scope step of the search defining what are valid definitions
	/// \param[out] error error description in case of -1 returned
	/// \return handle of the object defined by the caller in the given scope step with the name specified (setObject), or -1 if no object found or in case of error
	int getObjectInstance( const std::string_view& name, int step, Error& error) const noexcept;

	/// \brief Get the scope dependency tree of all objects with a defined name
	/// \param[in] name name of the elements to retrieve
	/// \param[out] error error description in case of empty tree returned
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	/// \remark the tree structure life time must be in the scope of this type database interface
	ObjectInstanceTree getObjectInstanceTree( const std::string_view& name, Error& error) const noexcept;

	/// \brief Define a new type, returns an error if already defined in the same scope with same the signature and the same priority
	/// \param[in] scope the scope of this definition
	/// \param[in] contextType the context (realm,namespace) of this type. A context is reachable via a path of type reductions.
	/// \param[in] name name of the type
	/// \param[in] constructor handle for the constructor of the type
	/// \param[in] parameter list of parameters as part of the function signature
	/// \param[in] priority the priority resolves conflicts of definitions with the same signature in the same scope. The higher priority value wins.
	/// \param[out] error error description in case of -1 returned
	/// \return the handle assigned to the new created type, or -1 in case of an error
	int defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, 
			const ParameterList& parameter, int priority, Error& error) noexcept;

	/// \brief Get the scope dependency tree of all types defined
	/// \param[out] error error description in case of empty tree returned
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	/// \remark the tree structure life time must be in the scope of this type database interface
	TypeDefinitionTree getTypeDefinitionTree( Error& error) const noexcept;

	/// \brief Define a reduction of a type to another type with a constructor that implements the construction of the target type from the source type
	/// \param[in] scope the scope of this definition
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[in] constructor the handle for the constructor that implements the construction of the target type from the source type
	/// \param[in] tag value inbetween 1 and 32 attached to the reduction that classifies it.
	/// \note	A search involving type reductions selects the reductions considered by their tags with a set of tags (TagMask).
	/// \param[in] weight the weight given to the reduction in the search, the reduction path with the lowest sum of weights wins
	/// \param[out] error error description in case of false returned
	/// \return true in case of success, false in case of an error
	bool defineReduction( const Scope& scope, int toType, int fromType, int constructor, int tag, float weight, Error& error) noexcept;

	/// \brief Get the scope dependency tree of all reductions defined
	/// \param[out] error error description in case of empty tree returned
	/// \note Building the tree is an expensive operation. The purpose of this method is mainly for introspection for debugging
	/// \return the tree built
	/// \remark the tree structure life time must be in the scope of this type database interface
	ReductionDefinitionTree getReductionDefinitionTree( Error& error) const noexcept;

	/// \brief Get the constructor of a reduction of a type to another type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[out] error error description in case of -1 returned
	/// \return the constructor of the reduction found, -1 if not found or in case of error
	int reduction( int step, int toType, int fromType, const TagMask& selectTags, Error& error) const noexcept;

	/// \brief Get the list of reductions defined for a type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] fromType the source type of the reduction
	/// \param[out] error error description in case of empty list returned
	/// \return the list of reductions found
	std::vector<ReductionResult> reductions( int step, int fromType, const TagMask& selectTags, Error& error) const noexcept;

	/// \brief Search for the sequence of reductions with the minimal sum of weights from one type to another type
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] toType the target type of the reduction
	/// \param[in] fromType the source type of the reduction
	/// \param[out] error error description in case of empty result returned
	/// \return the path found that has the minimal weight sum, returns an error if two path of same length are found
	DeriveResult deriveType( int step, int toType, int fromType, const TagMask& selectTags, Error& error) const noexcept;

	/// \brief Resolve a type name in a context of a context reducible from the context passed
	/// \param[in] step the scope step of the search defining what are valid reductions
	/// \param[in] contextType the context (realm,namespace) of the query type. The searched item has a context type that is reachable via a path of type reductions.
	/// \param[in] name name of the type searched
	/// \param[out] error error description in case of empty result returned
	/// \return the shortest path found, returns an error if two path of same length are found
	ResolveResult resolveType( int step, int contextType, const std::string_view& name, const TagMask& selectTags, Error& error) const noexcept;

	/// \brief Get the string representation of a type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \param[out] error error description in case of empty result returned
	/// \return the complete string assigned to a type or an empty string in case of an error
	std::string typeToString( int type, Error& error) const noexcept;

	/// \brief Get the name of a type (without context info and parameters)
	/// \param[in] type the handle of the type (return value of defineType)
	/// \param[out] error error description in case of empty result returned
	/// \return the name string of a type (name parameter of define type) or an empty string in case of an error
	std::string_view typeName( int type, Error& error) const noexcept;

	/// \brief Get the parameters attached to a type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \param[out] error error description in case of empty result returned
	/// \return a view on the list of parameters
	ParameterList typeParameters( int type, Error& error) const noexcept;

	/// \brief Get the constructor of a type
	/// \param[in] type the handle of the type (return value of defineType)
	/// \param[out] error error description in case of -1 returned
	/// \return the handle of the constructor of the type or -1 in case of an error
	int typeConstructor( int type, Error& error) const noexcept;

private:
	bool init( std::size_t initmemsize) noexcept;

	void* m_impl;
};

}//namespace
#endif

