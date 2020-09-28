/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Type system implementation
/// \file "typesystem.hpp"
#ifndef _MEWA_TYPESYSTEM_HPP_INCLUDED
#define _MEWA_TYPESYSTEM_HPP_INCLUDED
#if __cplusplus >= 201703L
#include "scope.hpp"
#include "identmap.hpp"
#include <string>
#include <memory>

namespace mewa {

class TypeSystemMemory
{
public:
	enum {NofIdentInitBuckets = 1<<18};

	TypeSystemMemory()
		:m_identmap_memrsc( m_identmap_membuffer, sizeof(m_identmap_membuffer))
		,m_identstr_memrsc( m_identstr_membuffer, sizeof(m_identstr_membuffer))
		,m_typetab_memrsc( m_typetab_membuffer, sizeof(m_typetab_membuffer))
		,m_redutab_memrsc( m_redutab_membuffer, sizeof(m_redutab_membuffer)){}

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


class TypeSystem
{
public:
	TypeSystem( std::unique_ptr<TypeSystemMemory>&& memory_ = std::unique_ptr<TypeSystemMemory>( new TypeSystemMemory()))
		:m_typeTable( memory_->resource_typetab())
		,m_reduTable( memory_->resource_redutab())
		,m_identMap( memory_->resource_identmap(), memory_->resource_identstr(), TypeSystemMemory::NofIdentInitBuckets)
		,m_memory(std::move(memory_)){}

	std::string tostring() const
	{
		std::string rt;
		return rt;
	}

private:
	typedef std::pair<int,int> TypeDef;
	typedef ScopedMap<TypeDef,int> TypeTable;
	typedef ScopedMap<int,int> ReductionTable;

	TypeTable m_typeTable;
	ReductionTable m_reduTable;
	IdentMap m_identMap;
	std::unique_ptr<TypeSystemMemory> m_memory;
};

}//namespace

#else
#error Building mewa requires C++17
#endif
#endif


