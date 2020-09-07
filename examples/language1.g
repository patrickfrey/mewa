
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
definition		= functiondefinition
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
			| expression
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
returnstatement	   	= "return" expression
			;
expression/L1		= variable
			| CARDINAL
			| FLOAT
			| DQSTRING	
			| SQSTRING	
			| "(" expression ")"
			;
expression/L2		= expression  "+"  expression		(operator add)
			| expression  "-"  expression		(operator sub) 
			| "-"  expression			(operator minus)
			| "+"  expression			(operator plus) 
			;
expression/L3		= expression  "*"  expression		(operator mul)	
			| expression  "/"  expression		(operator div)
			| expression  "%"  expression		(operator mod)
			;
expression/L4		= expression "->" IDENT
			| expression "." IDENT
			;
expression/L5		= expression  "(" callparamlist ")"
			| expression  "(" ")"
			| expression  "[" callparamlist "]"
			;
callparamlist		= expression "," callparamlist
			| ε
			;

