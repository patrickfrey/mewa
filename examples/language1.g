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


program		   	= definitionlist								(program)
			;
definitionlist/L	= definition definitionlist
			| ε
			;
definition		= functiondefinition
			| typedefinition ";"
			| variabledefinition ";"
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
			| typename "&&"									(typespec "&&")
			| "const" typename "&&"								(typespec "const&&")
			;
typedefinition		= "typedef" typename IDENT							(>>typedef)
			;
functiondefinition	= "function" typespec IDENT
				"(" parameters ")"
				"{" statementlist "}"							({}funcdef)
			| "procedure" IDENT
				"(" parameters ")"
				"{" statementlist "}"							({}procdef)
			;
parameters/L		= paramdecl "," parameters
			| paramdecl
			| ε
			;
paramdecl		= typespec IDENT								(paramdef)
			;
statementlist/L		= statement statementlist
			| ε
			;
statement		= functiondefinition
			| typedefinition ";"
			| variabledefinition ";"
			| expression ";"								(>>stm_expression)
			| returnstatement ";"
			| "if" "(" expression ")" "{" statementlist "}"					({}conditional_if)
			| "if" "(" expression ")" "{" statementlist "}" "else" "{" statementlist "}"	({}conditional_if)
			| "while" "(" expression ")" "{" statementlist "}"				({}conditional_while)
			| "{" statementlist "}"								({})
			;
variabledefinition	= "var" typespec IDENT "=" expression						(>>vardef_assign)
			| "var" typespec IDENT								(>>vardef)
			| "var" typespec IDENT "[" "]" "=" expression					(>>vardef_array_assign)
			| "var" typespec IDENT "[" "]"							(>>vardef_array)
			;  
returnstatement	   	= "return" expression								(>>stm_return)
			;
expression/L1		= IDENT
			| CARDINAL
			| FLOAT
			| DQSTRING	
			| SQSTRING
			| "(" expression ")"
			;
expression/L2		= expression  "="  expression							(binary_operator "=")
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
expression/L3		= expression  "||"  expression							(binary_operator "||")
			;
expression/L4		= expression  "&&"  expression							(binary_operator "&&")
			;
expression/L5		= expression  "|"  expression							(binary_operator "|")
			;			
expression/L6		= expression  "^"  expression							(binary_operator "^")
			| expression  "&"  expression							(binary_operator "&")
			;			
expression/L7		= expression  "=="  expression							(binary_operator "==")
			| expression  "!="  expression							(binary_operator "!=")
			| expression  "<="  expression							(binary_operator "<=")
			| expression  "<"  expression							(binary_operator "<")
			| expression  ">="  expression							(binary_operator ">=")
			| expression  ">"  expression							(binary_operator ">")
			;
expression/L8		= expression  "+"  expression							(binary_operator "+")
			| expression  "-"  expression							(binary_operator "-") 
			| "-"  expression								(unary_operator "-")
			| "+"  expression								(unary_operator "+") 
			| "~"  expression								(unary_operator "~")
			| "!"  expression								(unary_operator "!") 
			;
expression/L9		= expression  "*"  expression							(binary_operator "*")
			| expression  "/"  expression							(binary_operator "/")
			| expression  "%"  expression							(binary_operator "%")
			;
expression/L10		= expression  "<<"  expression							(binary_operator "<<")
			| expression  ">>"  expression							(binary_operator ">>")
			;
expression/L11		= expression "->" IDENT								(binary_operator arrow)
			| expression "." IDENT								(binary_operator member)
			| "*" expression								(unary_operator ptrderef)
			;
expression/L12		= expression  "(" expressionlist ")"						(nary_operator "(")
			| expression  "(" ")"								(nary_operator "(")
			| expression  "[" expressionlist "]"						(nary_operator "[")
			;
expressionlist		= expression "," expressionlist
			| expression
			| ε
			;

