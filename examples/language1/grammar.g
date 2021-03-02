% LANGUAGE test;
% TYPESYSTEM "language1/typesystem";
% CMDLINE "cmdlinearg";
% COMMENT "/*" "*/";
% COMMENT "//";

BOOLEAN : '((true)|(false))';
IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*';
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1;
SQSTRING: "[']((([^\\'\n]+)|([\\][^'\n]))*)[']" 1;
UINTEGER: '[0123456789]+';
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
namespace_definitionlist= namespace_definition namespace_definitionlist
			| ε
			;
instruct_definitionlist	= instruct_definition instruct_definitionlist
			| ε
			;
inclass_definitionlist	= inclass_definition inclass_definitionlist
			| ε
			;
ininterf_definitionlist	= ininterf_definition ininterf_definitionlist
			| ε
			;
extern_definition	= "extern" DQSTRING "function" IDENT typespec "(" extern_paramlist ")" ";"	(extern_funcdef)
			| "extern" DQSTRING "procedure" IDENT "(" extern_paramlist ")" ";"		(extern_procdef)
			| "extern" DQSTRING "function" IDENT typespec "(" extern_paramlist "..." ")" ";"(extern_funcdef_vararg)
			| "extern" DQSTRING "procedure" IDENT "(" extern_paramlist "..." ")" ";"	(extern_procdef_vararg)
			;
extern_parameters 	= typespec "," extern_parameters						(extern_paramdef)
			| typespec IDENT "," extern_parameters						(extern_paramdef)
			| typespec									(extern_paramdef)
			| typespec IDENT								(extern_paramdef)
			;
extern_paramlist	= extern_parameters								(extern_paramdeflist)
			| ε										(extern_paramdeflist)
			;
ininterf_definition 	= "function" IDENT typespec "(" extern_paramlist ")" ";"			(interface_funcdef {const=false})
			| "function" IDENT typespec "(" extern_paramlist ")" "const" ";"		(interface_funcdef {const=true})
			| "procedure" IDENT "(" extern_paramlist ")" ";"				(interface_procdef {const=false})
			| "procedure" IDENT "(" extern_paramlist ")" "const" ";"			(interface_procdef {const=true})
			| "operator" operatordecl typespec "(" extern_paramlist ")" ";"			(interface_operator_funcdef {const=false})
			| "operator" operatordecl typespec "(" extern_paramlist ")" "const" ";"		(interface_operator_funcdef {const=true})
			| "operator" operatordecl "(" extern_paramlist ")" ";"				(interface_operator_procdef {const=false})
			| "operator" operatordecl "(" extern_paramlist ")" "const" ";"			(interface_operator_procdef {const=true})
			;
instruct_definition	= typedefinition ";"								(definition 1)
			| variabledefinition ";"							(definition 2)
			| structdefinition								(definition 1)
			;
inclass_definition	= typedefinition ";"								(definition 1)
			| variabledefinition ";"							(definition 2)
			| structdefinition								(definition 1)
			| classdefinition 								(definition 1)
			| interfacedefinition								(definition 1)
			| functiondefinition								(definition 3)
			| operatordefinition								(definition 3)
			| constructordefinition								(definition 3)
			;
free_definition		= namespacedefinition								
			| typedefinition ";"								(definition 1)
			| variabledefinition ";"							(definition 1)
			| structdefinition								(definition 1)
			| classdefinition 								(definition 1)
			| interfacedefinition								(definition 1)
			| functiondefinition								(definition 1)
			;
namespace_definition	= namespacedefinition								(definition 1)
			| typedefinition ";"								(definition 1)
			| structdefinition								(definition 1)
			| classdefinition 								(definition 1)
			| interfacedefinition								(definition 1)
			| functiondefinition								(definition 1)
			;
typename/L1		= IDENT
			| IDENT "::" typename
			;
typehdr/L1		= typename									(typehdr {const=false})
			| "const" typename								(typehdr {const=true})
			| "any" "class" "^"								(typehdr_any "any class^")
			| "any" "const" "class" "^"							(typehdr_any "any const class^")
			| "any" "struct" "^"								(typehdr_any "any struct^")
			| "any" "const" "struct" "^"							(typehdr_any "any const struct^")
			;
typegen/L1		= typehdr
			| typehdr "[" generic_instance "]"						(typegen_generic)
			| typehdr "^"									(typegen_pointer {const=false})
			| typehdr "const" "^"								(typegen_pointer {const=true})
			;
typespec/L1		= typegen
			| typegen "&"									(typespec_ref)
			;
typedefinition		= "typedef" typegen IDENT							(>>typedef)
			| "typedef" "function" IDENT typespec "(" extern_paramlist ")" 			(>>typedef_functype)
			| "typedef" "procedure" IDENT "(" extern_paramlist ")" 				(>>typedef_proctype)
			;
structdefinition	= "struct" IDENT "{" instruct_definitionlist "}"				(>>structdef)
			| "generic" "struct" IDENT "[" generic_header "]"
				"{" instruct_definitionlist "}"						(>>generic_structdef)
			;
interfacedefinition	= "interface" IDENT "{" ininterf_definitionlist "}"				(>>interfacedef)
			;
inheritlist		= typegen "," inheritlist							(>>inheritdef 1)
			| typegen									(>>inheritdef 1)
			;
namespacedefinition	= "namespace" IDENT  "{" namespace_definitionlist "}"				(>>namespacedef)
			;
classdefinition		= "class" IDENT "{" inclass_definitionlist "}"					(>>classdef)
			| "class" IDENT ":" inheritlist "{" inclass_definitionlist "}"			(>>classdef)
			| "generic" "class" IDENT "[" generic_header "]"
				"{" inclass_definitionlist "}"						(>>generic_classdef)
			| "generic" "class" IDENT "[" generic_header "]" 
				":" inheritlist "{" inclass_definitionlist "}"				(>>generic_classdef)
			;
linkage			= "private"									(linkage {private=true, linkage="internal", explicit=true})
			| "public"									(linkage {private=false, linkage="external", explicit=true})
			| ε										(linkage {private=false, linkage="external", explicit=false})
			;
functiondefinition	= linkage "function" IDENT typespec callablebody				(funcdef {const=false})
			| linkage "function" IDENT typespec callablebody_const				(funcdef {const=true})
			| linkage "procedure" IDENT callablebody					(procdef {const=false})
			| linkage "procedure" IDENT callablebody_const					(procdef {const=true})
			| "generic" "function" IDENT "[" generic_header "]" typespec callablebody	(generic_funcdef {const=false})
			| "generic" "function" IDENT "[" generic_header "]" typespec callablebody_const (generic_funcdef {const=true})
			| "generic" "procedure" IDENT "[" generic_header "]" callablebody		(generic_procdef {const=false})
			| "generic" "procedure" IDENT "[" generic_header "]" callablebody_const 	(generic_procdef {const=true})
			;
constructordefinition	= linkage "constructor" callablebody						(constructordef)
			| "destructor" "{" codeblock "}"						(destructordef)
			;
operatordefinition      = linkage "operator" operatordecl typespec callablebody				(operator_funcdef {const=false})
			| linkage "operator" operatordecl typespec callablebody_const			(operator_funcdef {const=true})
			| linkage "operator" operatordecl callablebody					(operator_procdef {const=false})
			| linkage "operator" operatordecl callablebody_const				(operator_procdef {const=true})
			;
operatordecl		= "->"										(operatordecl {name="->", symbol="arrow"})
			| "="										(operatordecl {name="=", symbol="assign"})
			| "+"										(operatordecl {name="+", symbol="plus"})
			| "-"										(operatordecl {name="-", symbol="minus"})
			| "*"										(operatordecl {name="*", symbol="mul"})
			| "/"										(operatordecl {name="/", symbol="div"})
			| "%"										(operatordecl {name="%", symbol="mod"})
			| "&&"										(operatordecl {name="&&", symbol="and"})
			| "||"										(operatordecl {name="||", symbol="or"})
			| "&"										(operatordecl {name="&", symbol="bitand"})
			| "|"										(operatordecl {name="|", symbol="bitor"})
			| "<<"										(operatordecl {name="<<", symbol="lsh"})
			| ">>"										(operatordecl {name=">>", symbol="rsh"})
			| "~"										(operatordecl {name="~", symbol="lneg"})
			| "!"										(operatordecl {name="!", symbol="not"})
			| "(" ")"									(operatordecl {name="()", symbol="call"})
			| "[" "]"									(operatordecl {name="[]", symbol="get"})
			| "=="										(operatordecl {name="==", symbol="eq"})
			| "!="										(operatordecl {name="!=", symbol="ne"})
			| ">="										(operatordecl {name=">=", symbol="ge"})
			| "<="										(operatordecl {name="<=", symbol="le"})
			| ">"										(operatordecl {name=">", symbol="gt"})
			| "<"										(operatordecl {name="<", symbol="lt"})
			;
generic_instance_deflist= typegen									(generic_instance_type)
			| typegen "," generic_instance_deflist						(generic_instance_type)
			| UINTEGER									(generic_instance_dimension)
			| UINTEGER "," generic_instance_deflist						(generic_instance_dimension)
			;
generic_instance	= generic_instance_deflist							(generic_instance)
			;
generic_defaultlist	= IDENT "=" typegen "," generic_defaultlist					(generic_header_ident_type)
			| IDENT "=" typegen								(generic_header_ident_type)
			;
generic_identlist	= IDENT "," generic_identlist							(generic_header_ident)
			| IDENT "," generic_defaultlist							(generic_header_ident)
			| IDENT										(generic_header_ident)
			;
generic_header		= generic_identlist								(generic_header)
			| generic_defaultlist								(generic_header)
			;
callablebody		= "(" parameterlist ")" "{" codeblock "}"					({}callablebody)
			;
callablebody_const	=  "(" parameterlist ")" "const" "{" codeblock "}"				({}callablebody)
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
elseblock/L1		= "elseif" "(" expression ")" "{" codeblock "}"	elseblock			({}conditional_elseif)
			| "elseif" "(" expression ")" "{" codeblock "}" 				({}conditional_elseif)
			| "else" "{" codeblock "}" 							({}conditional_else)
			;
statement/L1		= structdefinition								(definition 1)
			| classdefinition 								(definition 1)
			| functiondefinition								(definition 1)
			| typedefinition ";"								(definition 1)
			| "var" variabledefinition ";"							(definition 1)
			| expression ";"								(free_expression)
			| "return" expression ";"							(>>return_value)
			| "return" ";"									(>>return_void)
			| "delete" expression ";"							(delete)
			| "if" "(" expression ")" "{" codeblock "}" elseblock				({}conditional_if)
			| "if" "(" expression ")" "{" codeblock "}" 					({}conditional_if)
			| "while" "(" expression ")" "{" codeblock "}"					({}conditional_while)
			| "with" "(" expression ")" "{" codeblock "}"					({}with_do)
			| "with" "(" expression ")" ";"							({}with_do)
			| "{" codeblock "}"								({})
			;
variabledefinition	= typespec IDENT								(>>vardef)
			| typespec IDENT "=" expression							(>>vardef_assign)
			;
expression/L1		= "{" expressionlist "}"							(structure)
			| "{" "}"									(structure)
			| "new" typespec ":" expression							(allocate)
			| "cast" typespec ":" expression						(typecast)
			;
expression/L2		= IDENT										(variable)
			| BOOLEAN									(constant "constexpr bool")
			| UINTEGER									(constant "constexpr uint")
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
			| "&"  expression								(operator_address "&")
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

