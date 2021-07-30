## Ubuntu 20.04 on x86_64, i686

### Build system
Use Cmake with gcc or clang with C++17 support.

### LLVM Version
The examples of this version of _Mewa_ are based on LLVM version 10 and run also with version 11.
The examples pass with with LLVM versions 12 and 13 too, but the output of the IR differs slightly.
Some tests that compare the IR output will therefore fail with llvm-12 or llvm-13.

### Prerequisites
Install packages with 'apt-get'/aptitude.

#### Required packages
```Bash
lua5.2 liblua5.2-dev luarocks llvm llvm-runtime
```
or
```Bash
lua5.1 liblua5.1-0-dev luarocks llvm llvm-runtime
```
when building for Lua 5.1.

#### Install required luarocks packages
```Bash
luarocks install bit32
luarocks install penlight
luarocks install LuaBcd
```
### Build LuaBcd from sources (if luarocks install LuaBcd fails)
If the build of LuaBcd with luarocks fails, you can fetch the sources from github and build it:
```Bash
git clone https://github.com/patrickfrey/luabcd.git
cd LuaBcd
./configure
make
make PREFIX=/usr/local install
```

### Fetch sources of latest release version
```Bash
git clone https://github.com/patrickfrey/mewa
cd mewa
git checkout -b 0.5

```

### Configure to find Lua includes and to write the file Lua.inc included by make
```Bash
./configure
```

### Build with GNU C/C++
```Bash
make COMPILER=gcc RELEASE=YES
```

### Build with Clang C/C++
```Bash
make COMPILER=clang RELEASE=YES
```

### Run tests
```Bash
make test
```

### Install
```Bash
make PREFIX=/usr/local install
```

### Lua Environment
For running the examples by hand don't forget to set the environment variables needed by Lua (LUA_CPATH,LUA_PATH) correctly by sourcing the script luaenv.sh in tests:
```Bash
. tests/luaenv.sh
```

