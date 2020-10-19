Ubuntu 20.04 on x86_64, i686
----------------------------

# Build system
Cmake with gcc or clang with C++17 support.

# Prerequisites
Install packages with 'apt-get'/aptitude.

## Required packages
        lua5.2 liblua5.2-dev luarocks llvm

## Required luarocks packages
	penlight filesystem

# Fetch sources
        git clone https://github.com/patrickfrey/mewa
        cd mewa

# Configure to find Lua includes and to write the file Lua.inc included by make
        ./configure

# Build with GNU C/C++
        make -DCOMPILER=gcc -DRELEASE=true

# Build with Clang C/C++
        make -DCOMPILER=clang -DRELEASE=true

# Run tests
        make test

# Install
        make PREFIX=/usr/local install

