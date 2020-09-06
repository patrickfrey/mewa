
% COMMENT "/*" "*/"
% COMMENT "//"

IDENT	: '[a-zA-Z_]+[a-zA-Z_0-9]*'
DQSTRING: "[\"]((([^\\\"\n]+)|([\\][^\"\n]))*)[\"]" 1 ;
SQSTRING: "[\']((([^\\\'\n]+)|([\\][^\'\n]))*)[\']" 1 ;
CARDINAL: '[0123456789]+'
FLOAT	:'[0123456789]+[.][0123456789]+'
FLOAT	:'[0123456789]+[.][0123456789]+[Ee][+-]{0,1}[0123456789]+'


program		   	= definition program   
			|
			;

definition		= functiondefinition
			| constdefinition
			;

paramdecl		= identifier identifier
			;

paramdecllist/R       	= paramdecl "," paramdecllist 
			| paramdecl
			;
parameters		= paramdecllist
			|
			;
functiondefinition	= "function" identifier
				identifier 
				"(" parameters ")" ":" identifier
				"{" localdeclarationlist code returnstatement "}"
			;
localdeclarationlist    = constdefinition
			| variabledefinition
			| arrayvariabledefinition
			;

code			= assignement
			| "{" localdeclarationlist code "}"
			;  

constdefinition	   	= "const" identifier identifier "=" expression ";" ;

variabledefinition	= identifier identifier ";" ;  
arrayvariabledefinition   = identifier identifier "[" "]" ";" ;  

assignement		= identifier "=" expression ;

returnstatement	   	= "return" expression ;

expression		= identifier	  
			| number
			| float 
			| dqstring	
			| char
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

elementaccess/L		= identifier "->" identifier
			| elementaccess "->" identifier 
			;

arrayaccess/L		= identifier "[" expression "]"
			| arrayaccess "[" expression "]" 
			;

callparamlist		= expression "," callparamlist
			| expression
			;

functioncall/L		= identifier  "(" callparamlist ")"
			| identifier "(" ")" 
			;



