% LANGUAGE test ;
% TYPESYSTEM typesystem_language1 ;
% CMDLINE cmdlinearg ;
% COMMENT "/*" "*/" ;
% COMMENT "//" ;

IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*' ;
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1 ;
SQSTRING: "[']((([^\\'\n]+)|([\\][^'\n]))*)[']" 1 ;
CARDINAL: '[0123456789]+' ;
FLOAT	: '[0123456789]+[.][0123456789]+' ;
FLOAT	: '[0123456789]+[.][0123456789]+[Ee][+-]{0,1}[0123456789]+' ;
BOOLEAN : '((true)|(false))' ;

program		   	= definitionlist								(program)
			;
definitionlist		= definition definitionlist
			| ε
			;
definition		= functiondefinition								(definition 0)
			| typedefinition ";"								(definition 0)
			| variabledefinition ";"							(definition 0)
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
			;
typedefinition		= "typedef" typename IDENT							(>>typedef)
			;
linkage			= "static"									(linkage {linkage="internal", attributes="#0 nounwind"})
			| "extern"									(linkage {linkage="external", attributes="#0 noinline nounwind"})
			|										(linkage {linkage="internal", attributes="#0 nounwind"})
			;
functiondefinition	= linkage "function" typespec IDENT
				"(" parameterlist ")"
				"{" statementlist "}"							({}funcdef)
			| linkage "procedure" IDENT
				"(" parameterlist ")"
				"{" statementlist "}"							({}procdef)
			;
parameterlist		= parameters									(paramdeflist)
			;
parameters		= paramdecl "," parameters
			| paramdecl
			| ε
			;
paramdecl		= typespec IDENT								(paramdef)
			;
statementlist		= statement statementlist							(>>statement)
			| ε
			;
statement		= functiondefinition								(definition)
			| typedefinition ";"								(definition)
			| variabledefinition ";"							(definition)
			| expression ";"
			| returnstatement ";"
			| "if" "(" expression ")" "{" statementlist "}"					({}conditional_if)
			| "if" "(" expression ")" "{" statementlist "}" "else" "{" statementlist "}"	({}conditional_if)
			| "while" "(" expression ")" "{" statementlist "}"				({}conditional_while)
			| "{" statementlist "}"								({})
			;
variabledefinition	= "var" typespec IDENT								(>>vardef)
			| "var" typespec IDENT "=" expression						(>>vardef_assign)
			| "var" typespec IDENT "[" "]" "=" expression					(>>vardef_array_assign)
			| "var" typespec IDENT "[" "]"							(>>vardef_array)
			;
returnstatement	   	= "return" expression								(>>return_value)
			;
expression/L1		= IDENT										(variable)
			| BOOLEAN									(constant "constexpr bool")
			| CARDINAL									(constant "constexpr int")
			| FLOAT										(constant "constexpr float")
			| DQSTRING									(constant "constexpr dqstring")
			| SQSTRING									(constant "constexpr qstring")
			| "(" expression ")"
			;
expression/L2		= expression  "="  expression							(operator "=")
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
expression/L3		= expression  "||"  expression							(logic_operator_or)
			;
expression/L4		= expression  "&&"  expression							(logic_operator_and)
			;
expression/L5		= expression  "|"  expression							(operator "|")
			;			
expression/L6		= expression  "^"  expression							(operator "^")
			| expression  "&"  expression							(operator "&")
			;			
expression/L7		= expression  "=="  expression							(operator "==")
			| expression  "!="  expression							(operator "!=")
			| expression  "<="  expression							(operator "<=")
			| expression  "<"  expression							(operator "<")
			| expression  ">="  expression							(operator ">=")
			| expression  ">"  expression							(operator ">")
			;
expression/L8		= expression  "+"  expression							(operator "+")
			| expression  "-"  expression							(operator "-") 
			| "-"  expression								(operator "-")
			| "+"  expression								(operator "+") 
			| "~"  expression								(operator "~")
			| "!"  expression								(logic_operator_not) 
			;
expression/L9		= expression  "*"  expression							(operator "*")
			| expression  "/"  expression							(operator "/")
			| expression  "%"  expression							(operator "%")
			;
expression/L10		= expression  "<<"  expression							(operator "<<")
			| expression  ">>"  expression							(operator ">>")
			;
expression/L11		= iexpression
			| expression "." IDENT								(operator ".")
			| expression "::" IDENT								(operator "::")
			| "*" expression								(operator "->")
			;
expression/L12		= expression  "(" expressionlist ")"						(operator "()")
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
			| ε
			;

