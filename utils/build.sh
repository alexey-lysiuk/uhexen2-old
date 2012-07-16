#!/bin/sh

if test "$1" = "strip"; then
	exe_ext=
	if env | grep -i windir > /dev/null; then
		exe_ext=".exe"
	fi
	strip hcc/hcc$exe_ext			\
		dcc/dhcc$exe_ext		\
		vis/vis$exe_ext			\
		light/light$exe_ext		\
		qbsp/qbsp$exe_ext		\
		bspinfo/bspinfo$exe_ext		\
		qfiles/qfiles$exe_ext		\
		pak/pakx$exe_ext		\
		pak/paklist$exe_ext		\
		genmodel/genmodel$exe_ext	\
		jsh2color/jsh2colour$exe_ext	\
		texutils/bsp2wal/bsp2wal$exe_ext	\
		texutils/lmp2pcx/lmp2pcx$exe_ext
	exit 0
fi

HOST_OS=`uname|sed -e s/_.*//|tr '[:upper:]' '[:lower:]'`
case "$HOST_OS" in
freebsd|openbsd|netbsd)
	MAKE_CMD=gmake ;;
linux)	MAKE_CMD=make ;;
*)	MAKE_CMD=make ;;
esac

if test "$1" = "clean"; then
	$MAKE_CMD -s -C hcc clean
	$MAKE_CMD -s -C bspinfo clean
	$MAKE_CMD -s -C qbsp clean
	$MAKE_CMD -s -C light clean
	$MAKE_CMD -s -C vis clean
	$MAKE_CMD -s -C genmodel clean
	$MAKE_CMD -s -C qfiles clean
	$MAKE_CMD -s -C pak clean
	$MAKE_CMD -s -C dcc clean
	$MAKE_CMD -s -C jsh2color clean
	$MAKE_CMD -s -C texutils/bsp2wal clean
	$MAKE_CMD -s -C texutils/lmp2pcx clean
	exit 0
fi

echo "Building hcc, the HexenC compiler.."
$MAKE_CMD -C hcc $* || exit 1
echo "" && echo "Now building qfiles.."
$MAKE_CMD -C qfiles $* || exit 1
echo "" && echo "Now building pak tools.."
$MAKE_CMD -C pak $* || exit 1
echo "" && echo "Now building genmodel.."
$MAKE_CMD -C genmodel $* || exit 1
echo "" && echo "Now building qbsp.."
$MAKE_CMD -C qbsp $* || exit 1
echo "" && echo "Now building light.."
$MAKE_CMD -C light $* || exit 1
echo "" && echo "Now building vis.."
$MAKE_CMD -C vis $* || exit 1
echo "" && echo "Now building bspinfo.."
$MAKE_CMD -C bspinfo $* || exit 1
echo "" && echo "Now building dhcc, a progs.dat decompiler.."
$MAKE_CMD -C dcc $* || exit 1
echo "" && echo "Now building jsh2colour, a lit file generator.."
$MAKE_CMD -C jsh2color $* || exit 1
echo "" && echo "Now building the texutils.."
$MAKE_CMD -C texutils/bsp2wal $* || exit 1
$MAKE_CMD -C texutils/lmp2pcx $* || exit 1

