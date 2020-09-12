
% LANGUAGE test ;
% TYPESYSTEM typesystem1 ;
% COMMENT "/*" "*/" ;
% COMMENT "//" ;

IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*' ;
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1 ;
SQSTRING: "[']((([^\\'\n]+)|([\\][^'\n]))*)[']" 1 ;
CARDINAL: '[0123456789]+' ;
FLOAT	: '[0123456789]+[.][0123456789]+' ;
FLOAT	: '[0123456789]+[.][0123456789]+[Ee][+-]{0,1}[0123456789]+' ;


program		   	= definitionlist
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
typespec/L1		= typename
			| "const" typename
			| typename "&"
			| "const" typename "&"
			| typename "^"
			| "const" typename "^"
			| typename "^" "&"
			| "const" typename "^" "&"
			| typename "&&"
			;
typedefinition		= "typedef" typename IDENT
			;
functiondefinition	= "function" typespec IDENT
				"(" parameters ")"
				"{" statementlist "}"
			| "procedure" IDENT
				"(" parameters ")"
				"{" statementlist "}"
			;
parameters/L		= paramdecl "," parameters
			| ε
			;
paramdecl		= typespec IDENT
			;
statementlist/L		= statement statementlist
			| ε
			;
statement		= functiondefinition
			| typedefinition ";"
			| variabledefinition ";"
			| expression ";"
			| returnstatement ";"
			| "if" "(" expression ")" "{" statementlist "}"
			| "if" "(" expression ")" "{" statementlist "}" "else" "{" statementlist "}"
			| "while" "(" expression ")" "{" statementlist "}"
			| "{" statementlist "}"
			;
variabledefinition	= typespec IDENT "=" expression ";"
			| typespec IDENT ";"
			| typespec IDENT "[" "]" "=" expression ";"
			| typespec IDENT "[" "]" ";"
			;  
returnstatement	   	= "return" expression
			;
expression/L1		= IDENT
			| CARDINAL
			| FLOAT
			| DQSTRING	
			| SQSTRING
			| "(" expression ")"
			;
expression/L2		= expression  "="  expression		(operator assign)
			;
expression/L3		= expression  "+"  expression		(operator add)
			| expression  "-"  expression		(operator sub) 
			| "-"  expression			(operator minus)
			| "+"  expression			(operator plus) 
			;
expression/L4		= expression  "*"  expression		(operator mul)	
			| expression  "/"  expression		(operator div)
			| expression  "%"  expression		(operator mod)
			;
expression/L5		= expression "->" IDENT
			| expression "." IDENT
			;
expression/L6		= expression  "(" expressionlist ")"
			| expression  "(" ")"
			| expression  "[" expressionlist "]"
			;
expressionlist		= expression "," expressionlist
			| ε
			;

