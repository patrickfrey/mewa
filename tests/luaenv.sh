#!/bin/sh
# Initialize environment variables for Lua (paths etc)
#
set +x

TMPFILE=`mktemp`
luarocks path > $TMPFILE
. $TMPFILE
rm $TMPFILE

export LUA_CPATH="build/?.so;$LUA_CPATH"
export LUA_PATH="examples/?.lua;tests/?.lua;$LUA_PATH";

echo "LUA_CPATH=\"$LUA_CPATH\""
echo "LUA_PATH=\"$LUA_PATH\""

