/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for reading and writing files
/// \file "fileio.cpp"
#include "fileio.hpp"
#include "error.hpp"
#include <cstdio>
#include <cstring>
#include <string>

using namespace mewa;

void mewa::writeFile( const std::string& filename, const std::string& content)
{
	FILE* ff = std::fopen( filename.c_str(), "w");
	if (!ff) throw Error( (Error::Code)errno, filename);
	if (content.size() != std::fwrite( content.c_str(), 1, content.size(), ff))
	{
		std::fclose( ff);
		throw Error( (Error::Code)std::ferror( ff), filename);
	}
	std::fclose( ff);
}

std::string mewa::readFile( const std::string& filename)
{
	std::string rt;
	FILE* ff = std::fopen( filename.c_str(), "r");
	if (!ff) throw Error( (Error::Code)errno, filename);

	enum {RBufsize = 1<<12};
	char rbuf[ RBufsize];
	std::size_t nn = std::fread( rbuf, 1, RBufsize, ff);
	for (; nn == RBufsize; nn = std::fread( rbuf, 1, RBufsize, ff))
	{
		rt.append( rbuf, RBufsize);
	}
	int eno = std::ferror( ff);
	if (eno)
	{
		std::fclose( ff);
		throw Error( (Error::Code)eno, filename);
	}
	if (feof( ff))
	{
		rt.append( rbuf, nn);
	}
	else
	{
		std::fclose( ff);
		throw Error( Error::FileReadError, filename);
	}
	std::fclose( ff);
	return rt;
}

