% LANGUAGE test;
% TYPESYSTEM "language1/typesystem";
% CMDLINE "cmdlinearg";
% COMMENT "/*" "*/";
% COMMENT "//";

BOOLEAN : '((true)|(false))';
IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*';
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1;
SQSTRING: "[']((([^\\'\n]+)|([\\][^'\n]))*)[']" 1;
CARDINAL: '[0123456789]+';
INTEGER : '[-][0123456789]+';
FLOAT	: '[-]{0,1}[0123456789]+[.][0123456789]+';
FLOAT	: '[-]{0,1}[0123456789]+[.][0123456789]+[Ee][+-]{0,1}[0123456789]+';

program		   	= externlist definitionlist mainproc						(program)
			;
externlist		= externdefinition externlist
			| ε
			;
definitionlist		= definition definitionlist
			| ε
			;
datadefinitionlist	= datadefinition datadefinitionlist
			| ε
			;
methoddefinitionlist 	= methoddefinition methoddefinitionlist
			| ε
			;
externdefinition	= "extern" DQSTRING "function" typespec IDENT "(" externparameterlist ")" ";"	(extern_funcdef)
			| "extern" DQSTRING "procedure" IDENT "(" externparameterlist ")" ";"		(extern_procdef)
			;
externparameterlist	= externparameters								(extern_paramdeflist)
			;
externparameters 	= typespec "," externparameters
			| typespec
			;
methoddefinition 	= "function" typespec IDENT "(" parameterlist ")" ";"				(interface_funcdef)
			| "procedure" IDENT "(" parameterlist ")" ";"					(interface_procdef)
			;
externparameterlist	= externparameters								(extern_paramdeflist)
			| ε
			;
mainproc		= "main" "{" codeblock "}"							({}main_procdef)
			| ε
			;
datadefinition		= typedefinition ";"								(definition)
			| variabledefinition ";"							(definition)
			| structdefinition ";"								(definition)
			;
definition		= datadefinition
			| functiondefinition								(definition)
			| classdefinition ";"								(definition)
			| interfacedefinition ";"							(definition)
			;
typename/L1		= IDENT
			| IDENT "::" typename
			;
typespec/L1		= typename									(typespec "")
			| "const" typename								(typespec "const ")
			| typename "&"									(typespec "&")
			| "const" typename "&"								(typespec "const&")
			| typename "^"									(typespec "^")
			| "const" typename "^"								(typespec "const^")
			| typename "^" "&"								(typespec "^&")
			| "const" typename "^" "&"							(typespec "const^&")
			| typename "^" "^"								(typespec "^^")
			| "const" typename "^" "^"							(typespec "const^^")
			| typename "^" "^" "&"								(typespec "^^&")
			| "const" typename  "^" "^" "&"							(typespec "const^^&")
			;
typedefinition		= "typedef" typename IDENT							(>>typedef)
			;
structdefinition	= "struct" IDENT "{" datadefinitionlist "}"					(>>structdef)
			;
interfacedefinition	= "interface" IDENT "{" methoddefinitionlist "}"				(>>interfacedef)
			;
classdefinition		= "class" IDENT "{" definitionlist "}"						(>>classdef)
			;
linkage			= "private"									(linkage {linkage="internal", attributes="#0 nounwind"})
			| "public"									(linkage {linkage="external", attributes="#0 noinline nounwind"})
			|										(linkage {linkage="internal", attributes="#0 nounwind"})
			;
functiondefinition	= linkage "function" typespec IDENT callablebody				(funcdef)
			| linkage "procedure" IDENT callablebody					(procdef)
			;
callablebody		= "(" parameterlist ")" "{" codeblock "}"					({}callablebody)
			;
parameterlist		= parameters									(paramdeflist)
			| ε
			;
parameters		= paramdecl "," parameters
			| paramdecl
			;
paramdecl		= typespec IDENT								(paramdef)
			;
codeblock		= statementlist									(codeblock)
			;
statementlist		= statement statementlist							(>>)
			| ε
			;
statement/L1		= functiondefinition								(definition)
			| typedefinition ";"								(definition)
			| "var" variabledefinition ";"							(definition)
			| expression ";"								(free_expression)
			| "return" expression ";"							(>>return_value)
			| "delete" expression ";"							(delete)
			| "if" "(" expression ")" "{" codeblock "}"					({}conditional_if)
			| "if" "(" expression ")" "{" codeblock "}" "else" "{" codeblock "}"		({}conditional_if_else)
			| "while" "(" expression ")" "{" codeblock "}"					({}conditional_while)
			| "{" codeblock "}"								({})
			;
variabledefinition	= typespec IDENT								(>>vardef)
			| typespec IDENT "=" expression							(>>vardef_assign)
			| typespec IDENT "[" expression "]" "=" expression				(>>vardef_array_assign)
			| typespec IDENT "[" expression "]"						(>>vardef_array)
			;
expression/L1		= "{" expressionlist "}"							(structure)
			| "{" "}"									(structure)
			| "new" typespec ":" expression							(allocate)
			;
expression/L2		= IDENT										(variable)
			| BOOLEAN									(constant "constexpr bool")
			| CARDINAL									(constant "constexpr uint")
			| INTEGER									(constant "constexpr int")
			| FLOAT										(constant "constexpr float")
			| "null"									(null)
			| DQSTRING									(string_constant)
			| SQSTRING									(char_constant)
			| "(" expression ")"
			;
expression/L3		= expression  "="  expression							(operator "=")
			| expression  "+="  expression							(assign_operator "+")
			| expression  "-="  expression							(assign_operator "-")
			| expression  "*="  expression							(assign_operator "*")
			| expression  "/="  expression							(assign_operator "/")
			| expression  "%="  expression							(assign_operator "%")
			| expression  "&&="  expression							(assign_operator "&&")
			| expression  "||="  expression							(assign_operator "||")
			| expression  "&="  expression							(assign_operator "&")
			| expression  "|="  expression							(assign_operator "|")
			| expression  "<<="  expression							(assign_operator "<<")
			| expression  ">>="  expression							(assign_operator ">>")
			;
expression/L4		= expression  "||"  expression							(operator "||")
			;
expression/L5		= expression  "&&"  expression							(operator "&&")
			;
expression/L6		= expression  "|"  expression							(operator "|")
			;			
expression/L7		= expression  "^"  expression							(operator "^")
			| expression  "&"  expression							(operator "&")
			;			
expression/L8		= expression  "=="  expression							(operator "==")
			| expression  "!="  expression							(operator "!=")
			| expression  "<="  expression							(operator "<=")
			| expression  "<"  expression							(operator "<")
			| expression  ">="  expression							(operator ">=")
			| expression  ">"  expression							(operator ">")
			;
expression/L9		= expression  "+"  expression							(operator "+")
			| expression  "-"  expression							(operator "-")
			| "&"  expression								(operator "&")
			| "-"  expression								(operator "-")
			| "+"  expression								(operator "+") 
			| "~"  expression								(operator "~")
			| "!"  expression								(operator "!") 
			;
expression/L10		= expression  "*"  expression							(operator "*")
			| expression  "/"  expression							(operator "/")
			| expression  "%"  expression							(operator "%")
			;
expression/L11		= expression  "<<"  expression							(operator "<<")
			| expression  ">>"  expression							(operator ">>")
			;
expression/L12		= iexpression
			| expression "." IDENT								(member)
			| "*" expression								(operator "->")
			;
expression/L13		= expression  "(" expressionlist ")"						(operator "()")
			| expression  "(" ")"								(operator "()")
			| expression  "[" expressionlist "]"						(operator "[]")
			;
iexpression		= expression indirection IDENT							(rep_operator "->")
			;
indirection		= "->" indirection								(count)
			| "->"										(count)
			;
expressionlist		= expression "," expressionlist
			| expression
			;

