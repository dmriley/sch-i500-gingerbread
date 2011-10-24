#!/bin/bash

echo
echo "---------------------------------------------------------------"
echo " BUILD OUTPUT"
echo "---------------------------------------------------------------"
echo

# KERNELBINDIR
KERNELBINDIR=$BINDIR/kernel

# Ensure output folders exist
if [ ! -d "$BINDIR" ]; then
	mkdir $BINDIR
fi
if [ ! -d "$OUTDIR" ]; then
	mkdir $OUTDIR
fi

# Clean edify templates from KERNELBINDIR
if [ -d "$KERNELBINDIR/kernel-update" ]; then
	rm -f -r $KERNELBINDIR/kernel-update
fi
if [ -d "$KERNELBINDIR/recovery-update" ]; then
	rm -f -r $KERNELBINDIR/recovery-update
fi

# Copy zImage/recovery.bin to output
cp $KERNELBINDIR/zImage $OUTDIR/
cp $KERNELBINDIR/recovery.bin $OUTDIR/

# Copy edify templates into KERNELBINDIR
cp -R $REFDIR/kernel-update/ $KERNELBINDIR/
cp -R $REFDIR/recovery-update/ $KERNELBINDIR/

# KERNEL Edify Update ZIP
cp $KERNELBINDIR/zImage $KERNELBINDIR/kernel-update/kernel_update/
pushd $KERNELBINDIR/kernel-update > /dev/null
zip -r -q sch-i500-kernel.zip .
mv sch-i500-kernel.zip $OUTDIR/
popd > /dev/null

# RECOVERY Edify Update ZIP
cp $KERNELBINDIR/recovery.bin $KERNELBINDIR/recovery-update/recovery_update/
pushd $KERNELBINDIR/recovery-update > /dev/null
zip -r -q sch-i500-recovery.zip .
mv sch-i500-recovery.zip $OUTDIR/
popd > /dev/null

# KERNEL ODIN tarball
pushd $OUTDIR > /dev/null
tar --format=ustar -cf sch-i500-kernel.tar zImage
md5sum -t sch-i500-kernel.tar >> sch-i500-kernel.tar
mv sch-i500-kernel.tar sch-i500-kernel.tar.md5
popd > /dev/null

# RECOVERY ODIN tarball
pushd $OUTDIR > /dev/null
tar --format=ustar -cf sch-i500-recovery.tar recovery.bin
md5sum -t sch-i500-recovery.tar >> sch-i500-recovery.tar
mv sch-i500-recovery.tar sch-i500-recovery.tar.md5
popd > /dev/null

pushd $OUTDIR > /dev/null
md5sum zImage
md5sum recovery.bin
md5sum sch-i500-kernel.zip
md5sum sch-i500-recovery.zip
md5sum sch-i500-kernel.tar.md5
md5sum sch-i500-recovery.tar.md5
popd > /dev/null
echo

