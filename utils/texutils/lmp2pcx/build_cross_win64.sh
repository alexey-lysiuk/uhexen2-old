#!/bin/sh

UHEXEN2_TOP=../../..
. $UHEXEN2_TOP/scripts/cross_defs_w64

BIN_DIR=../../bin

if test "$1" = "strip"; then
	$STRIPPER $BIN_DIR/lmp2pcx.exe
	exit 0
fi

HOST_OS=`uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]'`

case "$HOST_OS" in
linux)
	MAKE_CMD=make
	;;
freebsd|openbsd|netbsd)
	MAKE_CMD=gmake
	;;
*)
	MAKE_CMD=make
	;;
esac

if test "$1" = "clean"; then
	$MAKE_CMD -s clean
	exit 0
fi

exec $MAKE_CMD $*
