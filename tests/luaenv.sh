#!/bin/sh
# Initialize environment variables for Lua (paths etc)
#
set +x

. ./LuaPath.inc

export LUA_CPATH="build/?.so;$LUA_CPATH"
export LUA_PATH="examples/?.lua;examples/intro/?.lua;tests/?.lua;build/?.lua;$LUA_PATH";

echo "LUA_CPATH=\"$LUA_CPATH\""
echo "LUA_PATH=\"$LUA_PATH\""

