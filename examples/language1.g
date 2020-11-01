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
expression/L2		= expression  "="  expression							(operator assign)
			| expression  "+="  expression							(operator assign_add)
			| expression  "-="  expression							(operator assign_sub)
			| expression  "*="  expression							(operator assign_mul)
			| expression  "/="  expression							(operator assign_div)
			| expression  "%="  expression							(operator assign_mod)
			;
expression/L3		= expression  "||"  expression							(operator logical_or)
			;
expression/L4		= expression  "&&"  expression							(operator logical_and)
			;
expression/L5		= expression  "|"  expression							(operator bitwise_or)
			;			
expression/L6		= expression  "^"  expression							(operator bitwise_xor)
			| expression  "&"  expression							(operator bitwise_and)
			;			
expression/L7		= expression  "=="  expression							(operator cmpeq)
			| expression  "!="  expression							(operator cmpne)
			| expression  "<="  expression							(operator cmple)
			| expression  "<"  expression							(operator cmplt)
			| expression  ">="  expression							(operator cmpge)
			| expression  ">"  expression							(operator cmpgt)
			;
expression/L8		= expression  "+"  expression							(operator add)
			| expression  "-"  expression							(operator sub) 
			| "-"  expression								(operator minus)
			| "+"  expression								(operator plus) 
			| "~"  expression								(operator minus)
			| "!"  expression								(operator plus) 
			;
expression/L9		= expression  "*"  expression							(operator mul)
			| expression  "/"  expression							(operator div)
			| expression  "%"  expression							(operator mod)
			;
expression/L10		= expression "->" IDENT								(operator arrow)
			| expression "." IDENT								(operator member)
			| "*" expression								(operator ptrderef)
			;
expression/L11		= expression  "(" expressionlist ")"						(operator call)
			| expression  "(" ")"								(operator call)
			| expression  "[" expressionlist "]"						(operator arrayaccess)
			;
expressionlist		= expression "," expressionlist
			| expression
			| ε
			;

