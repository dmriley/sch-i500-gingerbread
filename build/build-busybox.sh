#!/bin/bash

echo
echo "---------------------------------------------------------------"
echo " BUILD BUSYBOX"
echo "---------------------------------------------------------------"
echo

# BUSYBOXBINDIR
BUSYBOXBINDIR=$BINDIR/busybox

if [ ! -d "$BINDIR" ]; then
	mkdir $BINDIR
fi
if [ ! -d "$BUSYBOXBINDIR" ]; then
	mkdir $BUSYBOXBINDIR
fi

# MAKE must be executed from the src/busybox directory
pushd $SRCDIR/busybox > /dev/null
make CROSS_COMPILE=$TOOLCHAIN/$TOOLCHAIN_PREFIX sch-i500.config
make CROSS_COMPILE=$TOOLCHAIN/$TOOLCHAIN_PREFIX
 
cp $SRCDIR/busybox/busybox $BUSYBOXBINDIR

if [[ $? -ne 0 ]]; then
	exit -1
fi

popd > /dev/null

exit

