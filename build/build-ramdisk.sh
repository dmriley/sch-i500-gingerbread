#!/bin/bash

echo
echo "---------------------------------------------------------------"
echo " BUILD RAMDISK IMAGE"
echo "---------------------------------------------------------------"
echo

# RAMDISKREFDIR
RAMDISKREFDIR=$REFDIR/ramdisk

# RAMDISKSRCDIR
RAMDISKSRCDIR=$SRCDIR/ramdisk

# RAMDISKBINDIR
export RAMDISKBINDIR=$BINDIR/ramdisk

# PLATFORMBINDIR
PLATFORMBINDIR=$BINDIR/platform

if [ ! -d "$BINDIR" ]; then
	mkdir $BINDIR
fi
if [ ! -d "$RAMDISKBINDIR" ]; then
	mkdir $RAMDISKBINDIR
fi

# Copy the reference ramdisk into the output directory
cp -R $RAMDISKREFDIR/* $RAMDISKBINDIR/

# Remove unwanted files from the ramdisk image
rm -f $RAMDISKBINDIR/lib/modules/hotspot_event_monitoring.ko

# Copy modified versions of the ramdisk files into the output directory
cp -R $RAMDISKSRCDIR/* $RAMDISKBINDIR/

# ANIMATIONCHOOSER --> RAMDISK
cp $PLATFORMBINDIR/animationchooser $RAMDISKBINDIR/sbin/

exit

