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
			| paramdecl
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
variabledefinition	= "var" typespec IDENT "=" expression
			| "var" typespec IDENT
			| "var" typespec IDENT "[" "]" "=" expression
			| "var" typespec IDENT "[" "]"
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
			| expression  "+="  expression		(operator assign_add)
			| expression  "-="  expression		(operator assign_sub)
			| expression  "*="  expression		(operator assign_mul)
			| expression  "/="  expression		(operator assign_div)
			| expression  "%="  expression		(operator assign_mod)
			;
expression/L3		= expression  "||"  expression		(operator logical_or)
			;
expression/L4		= expression  "&&"  expression		(operator logical_and)
			;
expression/L5		= expression  "|"  expression		(operator bitwise_or)
			;			
expression/L6		= expression  "^"  expression		(operator bitwise_xor)
			| expression  "&"  expression		(operator bitwise_and)
			;			
expression/L7		= expression  "=="  expression		(operator cmpeq)
			| expression  "!="  expression		(operator cmpne)
			| expression  "<="  expression		(operator cmple)
			| expression  "<"  expression		(operator cmplt)
			| expression  ">="  expression		(operator cmpge)
			| expression  ">"  expression		(operator cmpgt)
			;
expression/L8		= expression  "+"  expression		(operator add)
			| expression  "-"  expression		(operator sub) 
			| "-"  expression			(operator minus)
			| "+"  expression			(operator plus) 
			| "~"  expression			(operator minus)
			| "!"  expression			(operator plus) 
			;
expression/L9		= expression  "*"  expression		(operator mul)
			| expression  "/"  expression		(operator div)
			| expression  "%"  expression		(operator mod)
			;
expression/L10		= expression "->" IDENT
			| expression "." IDENT
			| "*" expression
			;
expression/L11		= expression  "(" expressionlist ")"
			| expression  "(" ")"
			| expression  "[" expressionlist "]"
			;
expressionlist		= expression "," expressionlist
                        | expression
			| ε
			;

