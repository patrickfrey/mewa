# Examples
Currently there exist only one real example and an incomplete language for an article introducing _Mewa_.
I have split the files in non language specific and language specific part.

## Example Compiler Projects
 1. [language1](example_language1.md) General-purpose programming language (also used in tests).
 2. [intro](example_intro_language.md) A demo-language for the codeproject article introducing _Mewa_. A very small subset of language1, only static structures and first class scalar types.

## Language Independent Files
The language independent files for the example languages are located in [examples](../examples/):

 - [typesystem_utils.lua](../examples/typesystem_utils.lua) 	Utility functions for AST traversal, error reporting, symbol encoding, UTF-8 parsing, etc.
 - [cmdlinearg.lua](../examples/cmdlinearg.lua) 		Command line parser for the example language compilers.

## Language Independent Generator Script
There is a Perl script for generating the format templates organized in structures for the basic first class scalar types like integers and floating point numbers out of a description.
The Perl script has nearly the size of the generated code, but generating code helps here to avoid singular bugs hard to detect (typos etc.). The script is only called when the description changes.
The generated ouput is checked-in as part of the project.

 - [generateScalarLlvmir.pl](../examples/gen/generateScalarLlvmir.pl)




