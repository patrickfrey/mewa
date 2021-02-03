# Examples
Currently there exist only one example. I tried to split the files in non language specific and language specific. The former ones are described here, the later ones in the document of the correspoding example.

## Language Independent Files
The language independent files for the test languages (currently only one) are located in [examples](../examples/). These are the following:

 - [typesystem_utils.lua](../examples/typesystem_utils.lua) 	Utility functions for AST traversal, error reporting, symbol encoding, UTF-8 parsing, etc.
 - [cmdlinearg.lua](../examples/cmdlinearg.lua) 	Command line parser for the test language compilers.
 
## Language Independent Generator Script
There is a Perl script for generating the format templates organized in structures for the basic first class scalar types like integers and floating point numbers out of a description. The Perl script has nearly the size of the generated code, but generating code helps here to avoid singular bugs (typos etc.) The script is only called when the description changes. The generated ouput is checked-in as part of the project.

 - [generateScalarLlvmir.pl](../examples/gen/generateScalarLlvmir.pl)

## Example Compiler Projects
 1. [language1](example_language1.md) Multiparadigm programming language also used in tests.
 
 

 
