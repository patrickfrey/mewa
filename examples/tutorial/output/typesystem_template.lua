local mewa = require "mewa"
local typedb = mewa.typedb()
local typesystem = {}

function typesystem.program( node)
end
function typesystem.extern_funcdef( node)
end
function typesystem.extern_funcdef_vararg( node)
end
function typesystem.extern_paramdef( node)
end
function typesystem.extern_paramdef_collect( node)
end
function typesystem.extern_paramdeflist( node)
end
function typesystem.definition( node, decl)
end
function typesystem.definition_2pass( node, decl)
end
function typesystem.typehdr( node)
end
function typesystem.arraytype( node)
end
function typesystem.typespec( node)
end
function typesystem.inheritdef( node, decl)
end
function typesystem.classdef( node)
end
function typesystem.funcdef( node)
end
function typesystem.callablebody( node)
end
function typesystem.main_procdef( node)
end
function typesystem.paramdeflist( node)
end
function typesystem.paramdef( node)
end
function typesystem.codeblock( node)
end
function typesystem.conditional_elseif( node)
end
function typesystem.conditional_else( node)
end
function typesystem.free_expression( node)
end
function typesystem.return_value( node)
end
function typesystem.return_void( node)
end
function typesystem.conditional_if( node)
end
function typesystem.conditional_while( node)
end
function typesystem.vardef( node)
end
function typesystem.structure( node)
end
function typesystem.variable( node)
end
function typesystem.constant( node, decl)
end
function typesystem.string_constant( node)
end
function typesystem.binop( node, decl)
end
function typesystem.unop( node, decl)
end
function typesystem.member( node)
end
function typesystem.operator( node, decl)
end
function typesystem.operator_array( node, decl)
end

return typesystem

