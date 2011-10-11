#!/bin/bash

echo
echo "---------------------------------------------------------------"
echo " BUILD PLATFORM"
echo "---------------------------------------------------------------"
echo

# PLATFORMBINDIR
PLATFORMBINDIR=$BINDIR/platform

# ZIPALIGN
#
# Path to the zipalign binary
ZIPALIGN=$ANDROIDROOTDIR/out/host/linux-x86/bin/zipalign

if [ ! -d "$BINDIR" ]; then
	mkdir $BINDIR
fi
if [ ! -d "$PLATFORMBINDIR" ]; then
	mkdir $PLATFORMBINDIR
fi

if [ ! -d "$ANDROIDROOTDIR/out/target/product/generic" ]; then
	echo "Android platform has not been built; output folder is missing"
	exit -1
fi

# MAKE must be executed from the platform root directory
pushd $ANDROIDROOTDIR > /dev/null

# Initialize
source build/envsetup.sh
lunch generic-eng

# ANIMATIONCHOOSER
mmm $SRCDIR/animationchooser/
if [[ $? -ne 0 ]]; then
	exit -1
fi

cp $ANDROIDROOTDIR/out/target/product/generic/system/bin/animationchooser $PLATFORMBINDIR/animationchooser

# SPAREPARTS
mmm $SRCDIR/platform/SpareParts/
if [[ $? -ne 0 ]]; then
	exit -1
fi

if [ -e "$PLATFORMBINDIR/SpareParts.apk" ]; then rm -f $PLATFORMBINDIR/SpareParts.apk; fi
cp $ANDROIDROOTDIR/out/target/product/generic/system/app/SpareParts.apk $PLATFORMBINDIR/SpareParts.zip
$ZIPALIGN 4 $PLATFORMBINDIR/SpareParts.zip $PLATFORMBINDIR/SpareParts.apk
rm $PLATFORMBINDIR/SpareParts.zip

popd > /dev/null

exit
