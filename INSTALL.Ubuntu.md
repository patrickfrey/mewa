## Ubuntu 20.04 on x86_64, i686

### Build system
Use Cmake with gcc or clang with C++17 support.

### Prerequisites
Install packages with 'apt-get'/aptitude.

#### Required packages
```Bash
lua5.2 liblua5.2-dev luarocks llvm
```

#### Install required luarocks packages
```Bash
luarocks install penlight 
luarocks install LuaBcd
```

### Fetch sources
```Bash
git clone https://github.com/patrickfrey/mewa
cd mewa
```

### Configure to find Lua includes and to write the file Lua.inc included by make
```Bash
./configure
```

### Build with GNU C/C++
```Bash
make -DCOMPILER=gcc -DRELEASE=true
```

### Build with Clang C/C++
```Bash
make -DCOMPILER=clang -DRELEASE=true
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

