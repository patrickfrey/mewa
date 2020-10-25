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

