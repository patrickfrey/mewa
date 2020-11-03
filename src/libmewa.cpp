/*
  Copyright (c) 2020 Patrick P. Frey
 
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/// \brief Exception free shared library interface for the Mewa TypeDatabase (typedb)
/// \note This interface is intended for users that want to use the type database library in a C++ context without using the mewa parser generator or the Lua module.
/// \file "libmewa.cpp"
#include "mewa/typedb.hpp"
#include "typedb.hpp"
#include "export.hpp"
#include "tree.hpp"
#include "scope.hpp"

/// NOTE There are some wild casts in this module. But this should not matter too much as this interface is thought to be stable.

DLL_PUBLIC libmewa::ObjectInstanceTree::ObjectInstanceTree( const libmewa::ObjectInstanceTree& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::ObjectInstanceTree( *(mewa::TypeDatabase::ObjectInstanceTree*)o.m_impl);
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::ObjectInstanceTree::ObjectInstanceTree( libmewa::ObjectInstanceTree&& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::ObjectInstanceTree( std::move( *(mewa::TypeDatabase::ObjectInstanceTree*)o.m_impl));
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::ObjectInstanceTree::~ObjectInstanceTree()
{
	if (m_impl) delete (mewa::TypeDatabase::ObjectInstanceTree*)m_impl;
}

DLL_PUBLIC libmewa::ObjectInstanceTree::NodeIndex libmewa::ObjectInstanceTree::chld( libmewa::ObjectInstanceTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::ObjectInstanceTree& tr = *(mewa::TypeDatabase::ObjectInstanceTree*)m_impl;
	return tr[ ni].chld();
}

DLL_PUBLIC libmewa::ObjectInstanceTree::NodeIndex libmewa::ObjectInstanceTree::next( libmewa::ObjectInstanceTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::ObjectInstanceTree& tr = *(mewa::TypeDatabase::ObjectInstanceTree*)m_impl;
	return tr[ ni].next();
}

DLL_PUBLIC libmewa::Scope libmewa::ObjectInstanceTree::scope( libmewa::ObjectInstanceTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return libmewa::Scope(0,0);
	mewa::TypeDatabase::ObjectInstanceTree& tr = *(mewa::TypeDatabase::ObjectInstanceTree*)m_impl;
	const mewa::Scope& scope = tr[ ni].item().scope;
	return libmewa::Scope( scope.start(), scope.end());
}

DLL_PUBLIC int libmewa::ObjectInstanceTree::instance( libmewa::ObjectInstanceTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::ObjectInstanceTree& tr = *(mewa::TypeDatabase::ObjectInstanceTree*)m_impl;
	return tr[ ni].item().value;
}


DLL_PUBLIC libmewa::TypeDefinitionTree::TypeDefinitionTree( const libmewa::TypeDefinitionTree& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::TypeDefinitionTree( *(mewa::TypeDatabase::TypeDefinitionTree*)o.m_impl);
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::TypeDefinitionTree::TypeDefinitionTree( libmewa::TypeDefinitionTree&& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::TypeDefinitionTree( std::move( *(mewa::TypeDatabase::TypeDefinitionTree*)o.m_impl));
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::TypeDefinitionTree::~TypeDefinitionTree()
{
	if (m_impl) delete (mewa::TypeDatabase::TypeDefinitionTree*)m_impl;
}

DLL_PUBLIC libmewa::TypeDefinitionTree::NodeIndex libmewa::TypeDefinitionTree::chld( libmewa::TypeDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::TypeDefinitionTree& tr = *(mewa::TypeDatabase::TypeDefinitionTree*)m_impl;
	return tr[ ni].chld();
}

DLL_PUBLIC libmewa::TypeDefinitionTree::NodeIndex libmewa::TypeDefinitionTree::next( libmewa::TypeDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::TypeDefinitionTree& tr = *(mewa::TypeDatabase::TypeDefinitionTree*)m_impl;
	return tr[ ni].next();
}

DLL_PUBLIC libmewa::Scope libmewa::TypeDefinitionTree::scope( libmewa::TypeDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return libmewa::Scope(0,0);
	mewa::TypeDatabase::TypeDefinitionTree& tr = *(mewa::TypeDatabase::TypeDefinitionTree*)m_impl;
	const mewa::Scope& scope = tr[ ni].item().scope;
	return libmewa::Scope( scope.start(), scope.end());
}

DLL_PUBLIC std::vector<int> libmewa::TypeDefinitionTree::list( libmewa::TypeDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return std::vector<int>();
	mewa::TypeDatabase::TypeDefinitionTree& tr = *(mewa::TypeDatabase::TypeDefinitionTree*)m_impl;
	return tr[ ni].item().value;
}


DLL_PUBLIC libmewa::ReductionDefinitionTree::ReductionDefinitionTree( const libmewa::ReductionDefinitionTree& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::ReductionDefinitionTree( *(mewa::TypeDatabase::ReductionDefinitionTree*)o.m_impl);
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::ReductionDefinitionTree::ReductionDefinitionTree( libmewa::ReductionDefinitionTree&& o)
{
	try
	{
		m_impl = new mewa::TypeDatabase::ReductionDefinitionTree( std::move( *(mewa::TypeDatabase::ReductionDefinitionTree*)o.m_impl));
	}
	catch (...)
	{
		m_impl = nullptr;
	}
}

DLL_PUBLIC libmewa::ReductionDefinitionTree::~ReductionDefinitionTree()
{
	if (m_impl) delete (mewa::TypeDatabase::ReductionDefinitionTree*)m_impl;
}

DLL_PUBLIC libmewa::ReductionDefinitionTree::NodeIndex libmewa::ReductionDefinitionTree::chld( libmewa::ReductionDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::ReductionDefinitionTree& tr = *(mewa::TypeDatabase::ReductionDefinitionTree*)m_impl;
	return tr[ ni].chld();
}

DLL_PUBLIC libmewa::ReductionDefinitionTree::NodeIndex libmewa::ReductionDefinitionTree::next( libmewa::ReductionDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return 0;
	mewa::TypeDatabase::ReductionDefinitionTree& tr = *(mewa::TypeDatabase::ReductionDefinitionTree*)m_impl;
	return tr[ ni].next();
}

DLL_PUBLIC libmewa::Scope libmewa::ReductionDefinitionTree::scope( libmewa::ReductionDefinitionTree::NodeIndex ni) const noexcept
{
	if (!m_impl || !ni) return libmewa::Scope(0,0);
	mewa::TypeDatabase::ReductionDefinitionTree& tr = *(mewa::TypeDatabase::ReductionDefinitionTree*)m_impl;
	const mewa::Scope& scope = tr[ ni].item().scope;
	return libmewa::Scope( scope.start(), scope.end());
}

DLL_PUBLIC std::vector<libmewa::ReductionDefinitionTree::Data> libmewa::ReductionDefinitionTree::list( libmewa::ReductionDefinitionTree::NodeIndex ni) const noexcept
{
	std::vector<libmewa::ReductionDefinitionTree::Data> rt;
	if (!m_impl || !ni) return rt;
	mewa::TypeDatabase::ReductionDefinitionTree& tr = *(mewa::TypeDatabase::ReductionDefinitionTree*)m_impl;
	for (auto const& elem : tr[ ni].item().value)
	{
		rt.emplace_back( elem.toType, elem.fromType, elem.constructor, elem.tag, elem.weight);
	}
	return rt;
}

DLL_PUBLIC bool libmewa::TypeDatabase::init( std::size_t initmemsize) noexcept
{
	try
	{
		m_impl = new mewa::TypeDatabase( initmemsize);
		return true;
	}
	catch (...)
	{
		m_impl = nullptr;
		return false;
	}
}

DLL_PUBLIC libmewa::TypeDatabase::~TypeDatabase()
{
	if (m_impl) delete (mewa::TypeDatabase*)m_impl;
}

static libmewa::Error lippincottFunction()
{
	try
	{
		throw;
	}
	catch (const mewa::Error& err)
	{
		return libmewa::Error( err.code(), err.arg());
	}
	catch (const std::runtime_error& err)
	{
		return libmewa::Error( mewa::Error::UnexpectedException, err.what());
	}
	catch (const std::bad_alloc&)
	{
		return libmewa::Error( mewa::Error::MemoryAllocationError);
	}
	catch (...)
	{
		return libmewa::Error( mewa::Error::UnexpectedException);
	}
}

DLL_PUBLIC bool libmewa::TypeDatabase::setObjectInstance( const std::string_view& name, const Scope& scope, int handle, Error& error) noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		typedb.setObjectInstance( name, mewa::Scope( scope.start(), scope.end()), handle);
		return true;
	}
	catch (...)
	{
		error = lippincottFunction();
		return false;
	}
}

DLL_PUBLIC int libmewa::TypeDatabase::getObjectInstance( const std::string_view& name, int step, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.getObjectInstance( name, step);
	}
	catch (...)
	{
		error = lippincottFunction();
		return -1;
	}
}

DLL_PUBLIC libmewa::ObjectInstanceTree libmewa::TypeDatabase::getObjectInstanceTree( const std::string_view& name, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		libmewa::ObjectInstanceTree rt;
		rt.m_impl = new mewa::TypeDatabase::ObjectInstanceTree( typedb.getObjectInstanceTree( name));
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::ObjectInstanceTree();
	}
}

DLL_PUBLIC int libmewa::TypeDatabase::defineType( const Scope& scope, int contextType, const std::string_view& name, int constructor, 
		const ParameterList& parameter, int priority, Error& error) noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.defineType( mewa::Scope( scope.start(), scope.end()), contextType, name, constructor, 
						mewa::TypeDatabase::ParameterList( parameter.arsize, (mewa::TypeDatabase::Parameter const*)parameter.ar),
						priority);
	}
	catch (...)
	{
		error = lippincottFunction();
		return -1;
	}
}

DLL_PUBLIC int libmewa::TypeDatabase::getType( const Scope& scope, int contextType, const std::string_view& name, const TypeList& parameter, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.getType( mewa::Scope( scope.start(), scope.end()), contextType, name, 
						mewa::TypeDatabase::TypeList( parameter.arsize, (int const*)parameter.ar));
	}
	catch (...)
	{
		error = lippincottFunction();
		return -1;
	}
}

DLL_PUBLIC libmewa::TypeDefinitionTree libmewa::TypeDatabase::getTypeDefinitionTree( Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		libmewa::TypeDefinitionTree rt;
		rt.m_impl = new mewa::TypeDatabase::TypeDefinitionTree( typedb.getTypeDefinitionTree());
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::TypeDefinitionTree();
	}
}

DLL_PUBLIC bool libmewa::TypeDatabase::defineReduction( const Scope& scope, int toType, int fromType, int constructor, int tag, float weight, Error& error) noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		typedb.defineReduction( mewa::Scope( scope.start(), scope.end()), toType, fromType, constructor, tag, weight);
		return true;
	}
	catch (...)
	{
		error = lippincottFunction();
		return false;
	}
}


DLL_PUBLIC libmewa::ReductionDefinitionTree libmewa::TypeDatabase::getReductionDefinitionTree( Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		libmewa::ReductionDefinitionTree rt;
		rt.m_impl = new mewa::TypeDatabase::ReductionDefinitionTree( typedb.getReductionDefinitionTree());
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::ReductionDefinitionTree();
	}
}

DLL_PUBLIC int libmewa::TypeDatabase::reduction( int step, int toType, int fromType, const TagMask& selectTags, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.reduction( step, toType, fromType, mewa::TagMask( selectTags.m_mask));
	}
	catch (...)
	{
		error = lippincottFunction();
		return -1;
	}
}

DLL_PUBLIC std::vector<libmewa::ReductionResult> libmewa::TypeDatabase::reductions( int step, int fromType, const TagMask& selectTags, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto redulist = typedb.reductions( step, fromType, mewa::TagMask( selectTags.m_mask), resbuf);
		std::vector<libmewa::ReductionResult> rt;
		for (auto const& redu : redulist)
		{
			rt.emplace_back( redu.type, redu.constructor);
		}
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return std::vector<libmewa::ReductionResult>();
	}
}

DLL_PUBLIC libmewa::DeriveResult libmewa::TypeDatabase::deriveType( int step, int toType, int fromType, const TagMask& selectTags, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto deriveres = typedb.deriveType( step, toType, fromType, mewa::TagMask( selectTags.m_mask), resbuf);
		libmewa::DeriveResult rt;
		rt.weightsum = deriveres.weightsum;
		rt.conflictPath.insert( rt.conflictPath.end(), deriveres.conflictPath.begin(), deriveres.conflictPath.end());
		for (auto const& redu : deriveres.reductions)
		{
			rt.reductions.emplace_back( redu.type, redu.constructor);
		}
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::DeriveResult();
	}
}

DLL_PUBLIC libmewa::ResolveResult libmewa::TypeDatabase::resolveType( 
				int step, int contextType, const std::string_view& name, const TagMask& selectTags, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		mewa::TypeDatabase::ResultBuffer resbuf;
		auto resolveres = typedb.resolveType( step, contextType, name, mewa::TagMask( selectTags.m_mask), resbuf);
		libmewa::ResolveResult rt;
		rt.weightsum = resolveres.weightsum;
		rt.contextType = resolveres.contextType;
		rt.conflictType = resolveres.conflictType;
		for (auto const& redu : resolveres.reductions)
		{
			rt.reductions.emplace_back( redu.type, redu.constructor);
		}
		for (auto const& item : resolveres.items)
		{
			rt.items.emplace_back( item.type, item.constructor);
		}
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::ResolveResult();
	}
}

DLL_PUBLIC std::string libmewa::TypeDatabase::typeToString( int type, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		mewa::TypeDatabase::ResultBuffer resbuf;
		std::string rt;
		rt.append( typedb.typeToString( type, resbuf));
		return rt;
	}
	catch (...)
	{
		error = lippincottFunction();
		return std::string();
	}
}

DLL_PUBLIC std::string_view libmewa::TypeDatabase::typeName( int type, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.typeName( type);
	}
	catch (...)
	{
		error = lippincottFunction();
		return std::string_view();
	}
}

DLL_PUBLIC libmewa::ParameterList libmewa::TypeDatabase::typeParameters( int type, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		auto params = typedb.typeParameters( type);
		return libmewa::ParameterList( params.arsize, (libmewa::Parameter*)params.ar);
	}
	catch (...)
	{
		error = lippincottFunction();
		return libmewa::ParameterList();
	}
}

DLL_PUBLIC int libmewa::TypeDatabase::typeConstructor( int type, Error& error) const noexcept
{
	try
	{
		if (!m_impl) throw mewa::Error( mewa::Error::MemoryAllocationError);
		mewa::TypeDatabase& typedb = *(mewa::TypeDatabase*)m_impl;
		return typedb.typeConstructor( type);
	}
	catch (...)
	{
		error = lippincottFunction();
		return -1;
	}
}

