---
title: "About"
date: 2021-04-26T08:47:11+01:00
draft: false
---

## Design Philosophy and Limitations
 - _Mewa_ is **not a framework**. It is not instrumented with configuration or plug-ins. The program logic is entirely implemented by the compiler front-end author in _Lua_. In other words: You write the compiler front-end with _Lua_ using _Mewa_ and not with _Mewa_ using _Lua_ as binding language.
 - _Mewa_ is **not optimized for collaborative work**, unlike many other compiler front-ends.
 - _Mewa_ provides no support for the evaluation of different paths of code generation. The idea is to do a one-to-one mapping of program structures to code and **leave all analytical optimization steps to the backend**.

## Status
I consider the software as ready for use in projects.

## Example
As **proof of concept**, I have implemented a compiler for a **general-purpose, statically typed programming language** with the following features:

 * Structures (plain data structures without methods, implicitly defined assignment operator)
 * Interfaces
 * Classes (custom defined constructors, destructors, operators, methods, inheritance of classes and interfaces)
 * Free procedures and functions
 * Polymorphism through interface inheritance
 * Generic programming (generic classes and functions)
 * Lambdas as arguments of generic classes or functions
 * Exception handling with a fixed exception structure (an error code and an optional string)

The example [language1](https://github.com/patrickfrey/mewa/blob/master/doc/example_language1.md) is built and run through some tests if you ```make test``` after the build.

## Portability
### Build
GNU Makefile support.

### Hardware
The tests currently use an LLVM IR target template file for the following platforms:

 * Linux x86_64 (Intel 64 bit)
 * Linux i686 (Intel 32 bit)

Contributions for other platforms and build systems are appreciated and welcome.


