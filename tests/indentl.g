% LANGUAGE indentl;
% TYPESYSTEM "tests/indentl";
% COMMENT "/*" "*/";
% COMMENT "//";
% INDENTL IND END NL 4;

IDENT    : '[a-zA-Z_]+[a-zA-Z_0-9]*';
UINTEGER : '[0123456789]+';
ILLEGAL	 : '[0123456789]+[A-Za-z_]';

program  = proclist
	 ;
proclist = proc proclist
	 |
	 ;
proc     = NL "fn" IDENT IND stmlist END
	 ;
stmlist  = stm stmlist
	 | stm
	 ;
stm      = NL IDENT "=" UINTEGER
	 | NL "if" IDENT "==" UINTEGER IND stmlist END
	 ;
