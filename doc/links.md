# Collection of Links

## LLVM
### Howto
The main source of documentation about how to use _LLVM IR_ was the _Clang_ C++ front-end:
```sh
clang -S -emit-llvm -o example.llr example.cpp
cat example.llr
```
C++ example translations with _Clang_ unravel how to map specific constructs into LLVM IR.

### Manuals
* [LLVM Language Reference Manual](https://llvm.org/docs/LangRef.html)
* [LLVM GetElementPtr](https://llvm.org/docs/GetElementPtr.html)

### Documentation with Examples
* [Mapping High Level Constructs to LLVM IR Documentation](https://readthedocs.org/projects/mapping-high-level-constructs-to-llvm-ir/downloads/pdf/latest)
* [Mapping High Level Constructs to LLVM IR](https://github.com/f0rki/mapping-high-level-constructs-to-llvm-ir)

### Optimization
* [Performance Tips](https://llvm.org/docs/Frontend/PerformanceTips.html)

### Some Deeper Digging
* [LLVMdev: The nsw story](https://lists.llvm.org/pipermail/llvm-dev/2011-November/045735.html)

## LuaRocks (packaging)
* [Rockspec-format](https://github.com/luarocks/luarocks/wiki/Rockspec-format)

