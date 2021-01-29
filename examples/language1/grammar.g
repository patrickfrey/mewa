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

program		   	= extern_definitionlist free_definitionlist main_procedure			(program)
			;
extern_definitionlist	= extern_definition extern_definitionlist
			| ε
			;
free_definitionlist	= free_definition free_definitionlist
			| ε
			;
struct_definitionlist	= struct_definition struct_definitionlist
			| ε
			;
class_definitionlist	= class_definition class_definitionlist
			| ε
			;
interface_definitionlist= interface_definition interface_definitionlist
			| ε
			;
extern_definition	= "extern" DQSTRING "function" IDENT typespec "(" extern_paramlist ")" ";"	(extern_funcdef)
			| "extern" DQSTRING "procedure" IDENT "(" extern_paramlist ")" ";"		(extern_procdef)
			| "extern" DQSTRING "function" IDENT typespec "(" extern_paramlist "..." ")" ";"(extern_funcdef_vararg)
			| "extern" DQSTRING "procedure" IDENT "(" extern_paramlist "..." ")" ";"	(extern_procdef_vararg)
			;
extern_parameters 	= typespec "," extern_parameters
			| typespec
			;
extern_paramlist	= extern_parameters								(extern_paramdeflist)
			| ε										(extern_paramdeflist)
			;
interface_definition 	= "function" IDENT typespec "(" parameterlist ")" ";"				(interface_funcdef "&")
			| "function" IDENT typespec "(" parameterlist ")" "const" ";"			(interface_funcdef "const&")
			| "procedure" IDENT "(" parameterlist ")" ";"					(interface_procdef "&")
			| "procedure" IDENT "(" parameterlist ")" "const" ";"				(interface_procdef "const&")
			| "operator" operatordecl typespec "(" parameterlist ")" ";"			(interface_operator_funcdef "&")
			| "operator" operatordecl typespec "(" parameterlist ")" "const" ";"		(interface_operator_funcdef "const&")
			| "operator" operatordecl "(" parameterlist ")" ";"				(interface_operator_procdef "&")
			| "operator" operatordecl "(" parameterlist ")" "const" ";"			(interface_operator_procdef "const&")
			;
struct_definition	= typedefinition ";"								(definition 1)
			| variabledefinition ";"							(definition 1)
			| structdefinition								(definition 1)
			;
class_definition	= typedefinition ";"								(definition 1)
			| variabledefinition ";"							(definition 1)
			| structdefinition								(definition 1)
			| classdefinition 								(definition 1)
			| interfacedefinition								(definition 1)
			| constructordefinition								(definition 2)
			| functiondefinition								(definition 2)
			| operatordefinition								(definition 2)
			;
free_definition		= struct_definition
			| functiondefinition								(definition 1)
			| classdefinition 								(definition 1)
			| interfacedefinition								(definition 1)
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
typepath/L1		= typename									(typespec "")
			;
typedefinition		= "typedef" typepath IDENT							(>>typedef)
			;
structdefinition	= "struct" IDENT "{" struct_definitionlist "}"					(>>structdef)
			;
interfacedefinition	= "interface" IDENT "{" interface_definitionlist "}"				(>>interfacedef)
			;
inheritlist		= typepath "," inheritlist							(>>inheritdef 1)
			| typepath									(>>inheritdef 1)
			;
classdefinition		= "class" IDENT "{" class_definitionlist "}"					(>>classdef)
			| "class" IDENT ":" inheritlist "{" class_definitionlist "}"			(>>classdef)
			;
linkage			= "private"									(linkage {name="private", linkage="internal"})
			| "public"									(linkage {name="public", linkage="external"})
			|										(linkage {name="private", linkage="internal"})
			;
functiondefinition	= linkage "function" IDENT typespec callablebody				(funcdef)
			| linkage "procedure" IDENT callablebody					(procdef)
			;
constructordefinition	= linkage "constructor" callablebody						(constructordef)
			| "destructor" "{" codeblock "}"						(destructordef)
			;
operatordefinition      = linkage "operator" operatordecl typespec callablebody				(operator_procdef)
			| linkage "operator" operatordecl callablebody					(operator_funcdef)
			;
operatordecl		= "->"										(operatordecl {name="->", symbol="__arrow"})
			| "="										(operatordecl {name="=", symbol="__assign"})
			| "+"										(operatordecl {name="+", symbol="__plus"})
			| "-"										(operatordecl {name="-", symbol="__minus"})
			| "*"										(operatordecl {name="*", symbol="__mul"})
			| "/"										(operatordecl {name="/", symbol="__div"})
			| "%"										(operatordecl {name="%", symbol="__mod"})
			| "&&"										(operatordecl {name="&&", symbol="__and"})
			| "||"										(operatordecl {name="||", symbol="__or"})
			| "&"										(operatordecl {name="&", symbol="__bitand"})
			| "|"										(operatordecl {name="|", symbol="__bitor"})
			| "<<"										(operatordecl {name="<<", symbol="__lsh"})
			| ">>"										(operatordecl {name=">>", symbol="__rsh"})
			| "~"										(operatordecl {name="~", symbol="__lneg"})
			| "!"										(operatordecl {name="!", symbol="__not"})
			| "(" ")"									(operatordecl {name="()", symbol="__call"})
			| "[" "]"									(operatordecl {name="[]", symbol="__get"})
			| "=="										(operatordecl {name="==", symbol="__eq"})
			| "!="										(operatordecl {name="!=", symbol="__ne"})
			| ">="										(operatordecl {name=">=", symbol="__ge"})
			| "<="										(operatordecl {name="<=", symbol="__le"})
			| ">"										(operatordecl {name=">", symbol="__gt"})
			| "<"										(operatordecl {name="<", symbol="__lt"})
			;
callablebody		= "(" parameterlist ")" "{" codeblock "}"					({}callablebody "&")
			|  "(" parameterlist ")" "const" "{" codeblock "}"				({}callablebody "const&")
			;
main_procedure		= "main" "{" codeblock "}"							({}main_procdef)
			| ε
			;
parameterlist		= parameters									(paramdeflist)
			| ε										(paramdeflist)
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
			| "cast" typespec ":" "{" expression "}"					(typecast)
			| "cast" typespec ":" "{" "}"							(typecast)
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
			| expression  "^="  expression							(assign_operator "^")
			| expression  "&="  expression							(assign_operator "&")
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

