#!/bin/bash

echo
echo "---------------------------------------------------------------"
echo " CLEAN"
echo "---------------------------------------------------------------"
echo

# Clean Intermediate files
if [ -d "$BINDIR" ]; then
	rm -f -r $BINDIR
fi

# Clean Output files
if [ -d "$OUTDIR" ]; then
	rm -f -r $OUTDIR
fi

# Clean BUSYBOX
pushd $SRCDIR/busybox > /dev/null
make clean
popd > /dev/null

if [[ $? -ne 0 ]]; then
	exit -1
fi

# Clean KERNEL
pushd $SRCDIR/kernel > /dev/null
make CROSS_COMPILE=$TOOLCHAIN/$TOOLCHAIN_PREFIX clean mrproper
popd > /dev/null

if [[ $? -ne 0 ]]; then
	exit -1
fi

exit

