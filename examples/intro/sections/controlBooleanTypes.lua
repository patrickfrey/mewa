local utils = require "typesystem_utils"

-- Initialize control boolean types used for implementing control structures
function initControlBooleanTypes()
    controlTrueType = typedb:def_type( 0, " controlTrueType")
    controlFalseType = typedb:def_type( 0, " controlFalseType")

    local function controlTrueTypeToBoolean( constructor)
        local env = getCallableEnvironment()
        local out = env.register()
        local subst = {falseExit=constructor.out,out=out}
        local fmt = llvmir.control.controlTrueTypeToBoolean
        return {code = constructor.code
                .. utils.constructor_format(fmt,subst,env.label),out=out}
    end
    local function controlFalseTypeToBoolean( constructor)
        local env = getCallableEnvironment()
        local out = env.register()
        local subst = {trueExit=constructor.out,out=out}
        local fmt = llvmir.control.controlFalseTypeToBoolean
        return {code = constructor.code
                .. utils.constructor_format( fmt,subst,env.label),out=out}
    end
    typedb:def_reduction( scalarBooleanType, controlTrueType,
                    controlTrueTypeToBoolean, tag_typeDeduction, rwd_control)
    typedb:def_reduction( scalarBooleanType, controlFalseType,
                    controlFalseTypeToBoolean, tag_typeDeduction, rwd_control)

    local function booleanToControlTrueType( constructor)
        local env = getCallableEnvironment()
        local out = env.label()
        local subst = {inp=constructor.out, out=out}
        local fmt = llvmir.control.booleanToControlTrueType
        return {code = constructor.code
                .. utils.constructor_format( fmt, subst, env.label),out=out}
    end
    local function booleanToControlFalseType( constructor)
        local env = getCallableEnvironment()
        local out = env.label()
        local subst = {inp=constructor.out, out=out}
        local fmt = llvmir.control.booleanToControlFalseType
        return {code = constructor.code
                .. utils.constructor_format( fmt, subst, env.label),out=out}
    end

    typedb:def_reduction( controlTrueType, scalarBooleanType,
                booleanToControlTrueType, tag_typeDeduction, rwd_control)
    typedb:def_reduction( controlFalseType, scalarBooleanType,
                booleanToControlFalseType, tag_typeDeduction, rwd_control)

    local function negateControlTrueType( this)
        return {type=controlFalseType, constructor=this.constructor}
    end
    local function negateControlFalseType( this)
        return {type=controlTrueType, constructor=this.constructor}
    end
    local function joinControlTrueTypeWithBool( this, arg)
        local out = this.out
        local subst = {inp=arg[1].out, out=out}
        local fmt = llvmir.control.booleanToControlTrueType
        local label_allocator = getCallableEnvironment().label
        local code2 = utils.constructor_format( fmt, subst, label_allocator)
        return {code=this.code .. arg[1].code .. code2, out=out}
    end
    local function joinControlFalseTypeWithBool( this, arg)
        local out = this.out
        local subst = {inp=arg[1].out, out=out}
        local fmt = llvmir.control.booleanToControlFalseType
        local label_allocator = getCallableEnvironment().label
        local code2 = utils.constructor_format( fmt, subst, label_allocator)
        return {code=this.code .. arg[1].code .. code2, out=out}
    end
    defineCall( controlTrueType, controlFalseType, "!", {}, nil)
    defineCall( controlFalseType, controlTrueType, "!", {}, nil)
    defineCall( controlTrueType, controlTrueType, "&&",
            {scalarBooleanType}, joinControlTrueTypeWithBool)
    defineCall( controlFalseType, controlFalseType, "||",
            {scalarBooleanType}, joinControlFalseTypeWithBool)

    local function joinControlFalseTypeWithConstexprBool( this, arg)
        if arg == false then
            return this
        else
            local env = getCallableEnvironment()
            local subst = {out=this.out}
            local fmt = llvmir.control.terminateTrueExit
            return {code = this.code
                    .. utils.constructor_format(fmt,subst,env.label), out=this.out}
        end
    end
    local function joinControlTrueTypeWithConstexprBool( this, arg)
        if arg == true then
            return this
        else
            local env = getCallableEnvironment()
            local subst = {out=this.out}
            local fmt = llvmir.control.terminateFalseExit
            return {code = this.code
                    .. utils.constructor_format(fmt,subst,env.label), out=this.out}
        end
    end
    defineCall( controlTrueType, controlTrueType, "&&",
            {constexprBooleanType}, joinControlTrueTypeWithConstexprBool)
    defineCall( controlFalseType, controlFalseType, "||",
            {constexprBooleanType}, joinControlFalseTypeWithConstexprBool)

    local function constexprBooleanToControlTrueType( value)
        local env = getCallableEnvironment()
        local out = label()
        local fmt = llvmir.control.terminateFalseExit
        local code = (value == true)
                and ""
                or utils.constructor_format( fmt, {out=out}, env.label)
        return {code=code, out=out}
    end
    local function constexprBooleanToControlFalseType( value)
        local env = getCallableEnvironment()
        local out = label()
        local fmt = llvmir.control.terminateFalseExit
        local code = (value == false)
                and ""
                or utils.constructor_format( fmt, {out=out}, env.label)
        return {code=code, out=out}
    end
    typedb:def_reduction( controlFalseType, constexprBooleanType,
                constexprBooleanToControlFalseType, tag_typeDeduction, rwd_control)
    typedb:def_reduction( controlTrueType, constexprBooleanType,
                constexprBooleanToControlTrueType, tag_typeDeduction, rwd_control)

    local function invertControlBooleanType( this)
        local out = getCallableEnvironment().label()
        local subst = {inp=this.out, out=out}
        local fmt = llvmir.control.invertedControlType
        return {code = this.code
                    .. utils.constructor_format( fmt, subst), out = out}
    end
    typedb:def_reduction( controlFalseType, controlTrueType,
                    invertControlBooleanType, tag_typeDeduction, rwd_control)
    typedb:def_reduction( controlTrueType, controlFalseType,
                    invertControlBooleanType, tag_typeDeduction, rwd_control)

    definePromoteCall( controlTrueType, constexprBooleanType, controlTrueType,
                    "&&", {scalarBooleanType}, constexprBooleanToControlTrueType)
    definePromoteCall( controlFalseType, constexprBooleanType, controlFalseType,
                    "||", {scalarBooleanType}, constexprBooleanToControlFalseType)
end

