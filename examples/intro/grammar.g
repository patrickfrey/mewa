% LANGUAGE intro;
% TYPESYSTEM "intro/typesystem";
% CMDLINE "cmdlinearg";
% COMMENT "/*" "*/";
% COMMENT "//";

BOOLEAN : '((true)|(false))';
IDENT    : '[a-zA-Z_]+[a-zA-Z_0-9]*';
DQSTRING: '["]((([^\\"\n]+)|([\\][^"\n]))*)["]' 1;
UINTEGER: '[0123456789]+';
FLOAT    : '[0123456789]*[.][0123456789]+';
FLOAT    : '[0123456789]*[.][0123456789]+[Ee][+-]{0,1}[0123456789]+';

program           = extern_deflist free_deflist main_procedure (program)
		  ;
extern_deflist    = extern_def extern_deflist
		  |
		  ;
free_deflist      = free_def free_deflist
		  |
		  ;
inclass_deflist= inclass_def inclass_deflist
		  |
		  ;
extern_def        = "extern" "function" IDENT
		       typespec "(" extern_paramlist ")" ";"    (extern_funcdef)
		  ;
extern_paramdecl  = typespec IDENT    (extern_paramdef)
		  | typespec          (extern_paramdef)
		  ;
extern_parameters = extern_paramdecl "," extern_parameters
		  | extern_paramdecl
		  ;
extern_paramlist  = extern_parameters       (extern_paramdeflist)
		  |                         (extern_paramdeflist)
		  ;
inclass_def       = variabledef ";"         (definition 2)
		  | classdef                (definition 1)
		  | functiondef             (definition_2pass 4)
		  ;
free_def          = variabledef ";"         (definition 1)
		  | classdef                (definition 1)
		  | functiondef             (definition 1)
		  ;
typename/L1       = IDENT
		  | IDENT "::" typename
		  ;
typehdr/L1        = typename                                (typehdr)
		  ;
typegen/L1        = typehdr
		  | typegen "[" UINTEGER "]"                (arraytype)
		  ;
typespec/L1       = typegen                                 (typespec)
		  ;
classdef          = "class" IDENT "{" inclass_deflist "}"   (>>classdef)
		  ;
functiondef       = "function" IDENT typespec callablebody  (funcdef)
		  ;
callablebody      = "(" impl_paramlist ")"
			  "{" statementlist "}"             ({}callablebody)
		  ;
main_procedure    = "main" codeblock                        (main_procdef)
		  |
		  ;
impl_paramlist    = impl_parameters                         (paramdeflist)
		  |                                         (paramdeflist)
		  ;
impl_parameters   = impl_paramdecl "," impl_parameters
		  | impl_paramdecl
		  ;
impl_paramdecl    = typespec IDENT                          (paramdef)
		  ;
codeblock/L1      = "{" statementlist "}"                   ({}codeblock)
		  ;
statementlist/L1  = statement statementlist                 (>>)
		  |
		  ;
elseblock/L1      = "elseif" "(" expression ")"
			  codeblock elseblock               (conditional_elseif)
		  | "elseif" "(" expression ")" codeblock   (conditional_elseif)
		  | "else" codeblock                        (conditional_else)
		  ;
statement/L1      = classdef                                (definition 1)
		  | functiondef                             (definition 1)
		  | "var" variabledef ";"                   (>>definition 1)
		  | expression ";"                          (free_expression)
		  | "return" expression ";"                 (>>return_value)
		  | "return" ";"                            (>>return_void)
		  | "if" "(" expression ")"
			   codeblock elseblock		    (conditional_if)
		  | "if" "(" expression ")" codeblock       (conditional_if)
		  | "while" "(" expression ")" codeblock    (conditional_while)
		  | codeblock
		  ;
variabledef       = typespec IDENT                          (>>vardef)
		  | typespec IDENT "=" expression           (>>vardef)
		  ;
expression/L1     = "{" expressionlist "}"                  (>>structure)
		  | "{" "}"                                 (>>structure)
		  ;
expression/L2     = IDENT                                   (variable)
		  | BOOLEAN                                 (constant "constexpr bool")
		  | UINTEGER                                (constant "constexpr int")
		  | FLOAT                                   (constant "constexpr double")
		  | DQSTRING                                (constant "constexpr string")
		  | "(" expression ")"
		  ;
expression/L3     = expression  "="  expression             (>>binop "=")
		  ;
expression/L4     = expression  "||"  expression            (>>binop "||")
		  ;
expression/L5     = expression  "&&"  expression            (>>binop "&&")
		  ;
expression/L8     = expression  "=="  expression            (>>binop "==")
		  | expression  "!="  expression            (>>binop "!=")
		  | expression  "<="  expression            (>>binop "<=")
		  | expression  "<"  expression             (>>binop "<")
		  | expression  ">="  expression            (>>binop ">=")
		  | expression  ">"  expression             (>>binop ">")
		  ;
expression/L9     = expression  "+"  expression             (>>binop "+")
		  | expression  "-"  expression             (>>binop "-")
		  | "-"  expression                         (>>unop "-")
		  | "+"  expression                         (>>unop "+")
		  ;
expression/L10    = expression  "*"  expression             (>>binop "*")
		  | expression  "/"  expression             (>>binop "/")
		  ;
expression/L12    = expression "." IDENT                    (member)
		  ;
expression/L13    = expression  "(" expressionlist ")"      (>>operator "()")
		  | expression  "(" ")"                     (>>operator "()")
		  | expression  "[" expressionlist "]"      (>>operator_array "[]")
		  ;
expressionlist/L0 = expression "," expressionlist
		  | expression
		  ;

