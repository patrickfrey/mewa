#!/bin/sh
# Initialize environment variables for Lua (paths etc)
#
set +x

TMPFILE=`mktemp`
luarocks path > $TMPFILE
. $TMPFILE
rm $TMPFILE

addToLUA_CPATH () {
  case ":$LUA_CPATH:" in
    *";$1"*) :;; # already there
    *) export LUA_CPATH="$LUA_CPATH;$1";; 
  esac
}

addToLUA_PATH () {
  case ":$LUA_PATH:" in
    *";$1"*) :;; # already there
    *) export LUA_PATH="$LUA_PATH;$1";; 
  esac
}

addToLUA_CPATH "build/?.so"
addToLUA_PATH  "examples/?.lua"

echo "LUA_CPATH=\"$LUA_CPATH\""
echo "LUA_PATH=\"$LUA_PATH\""

