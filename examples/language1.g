
% LANGUAGE test ;
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
definition		: functiondefinition
			| variabledefinition
			;
typename		= IDENT
			;
variable		= IDENT
			;
functiondefinition	= "function" typename IDENT 
				"(" parameters ")"
				"{" statementlist "}"
			;
parameters/L		= paramdecl "," parameters
			| ε
			;
paramdecl		= typename IDENT
			;
statementlist/L		= statement statementlist
			| ε
			;
statement		= variabledefinition
			| assignement
			| expressionstatement
			| returnstatement
			| "{" statementlist "}"
			;
variabledefinition	= typename IDENT "=" expression ";"
			| typename IDENT ";"
			| typename IDENT "[" "]" "=" expression ";"
			| typename IDENT "[" "]" ";"
			;  
assignement		= variable "=" expression
			;
expressionstatement	= expression
			;
returnstatement	   	= "return" expression
			;
expression		= IDENT	  
			| CARDINAL
			| FLOAT
			| DQSTRING	
			| SQSTRING	
			| "(" expression ")"
			;
expression/L1		= expression  "+"  expression		(operator add)
			| expression  "-"  expression		(operator sub) 
			| "-"  expression			(operator minus)
			| "+"  expression			(operator plus) 
			;
expression/L2		= expression  "*"  expression		(operator mul)	
			| expression  "/"  expression		(operator div)
			| expression  "%"  expression		(operator mod)
			;
expression/L3		= elementaccess
			| arrayaccess
			| functioncall
			;	 
elementaccess/L		= IDENT "->" IDENT
			| elementaccess "->" IDENT 
			| IDENT "." IDENT
			| elementaccess "." IDENT 
			;
arrayaccess/L		= identifier "[" expression "]"
			| arrayaccess "[" expression "]" 
			;
callparamlist		= expression "," callparamlist
			| expression
			;
functioncall/L		= IDENT  "(" callparamlist ")"
			| IDENT "(" ")" 
			;



