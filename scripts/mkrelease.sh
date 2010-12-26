#!/bin/sh

# the script I use to prepare a release tree from the uhexen2 svn snapshot

if echo `pwd` | grep "\/scripts" > /dev/null 2>&1 ; then
cd ..
fi
# this relies on the name of this script file being "mkrelease.sh"
test -f scripts/mkrelease.sh || { echo "change into the cvs snapshot directory before running this script"; exit 1; }

# the current gamecode version
GAMECODE_VER=1.19b

# move the spec file to the root
mv rpm/hexen2.spec .
rm -rf rpm

# move the license file to the root
mv docs/LICENSE ./LICENSE

# rename the gamecode directory to include its version number
mv gamecode gamecode-$GAMECODE_VER
