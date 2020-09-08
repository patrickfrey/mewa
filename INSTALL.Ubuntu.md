Ubuntu 20.04 on x86_64, i686
----------------------------

# Build system
Cmake with gcc or clang with C++17 support.

# Prerequisites
Install packages with 'apt-get'/aptitude.

## Required packages
        lua >= 5.2

# Fetch sources
        git clone https://github.com/patrickfrey/mewa
        cd mewa

# Build with GNU C/C++
        make -DCOMPILER=gcc -DRELEASE=true

# Build with Clang C/C++
        make -DCOMPILER=clang -DRELEASE=true

# Run tests
        make test

# Install
        make install

