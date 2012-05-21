#!/bin/sh

UHEXEN2_TOP=../../..
. $UHEXEN2_TOP/scripts/cross_defs.dj

if test "$1" = "strip"; then
	$STRIPPER hwcl.exe
	exit 0
fi

HOST_OS=`uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]'`
case "$HOST_OS" in
freebsd|openbsd|netbsd)
	MAKE_CMD=gmake ;;
linux)	MAKE_CMD=make ;;
*)	MAKE_CMD=make ;;
esac

if test "$1" = "all"; then
	shift
fi
exec $MAKE_CMD hw $*

