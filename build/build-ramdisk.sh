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

# BUSYBOXBINDIR
BUSYBOXBINDIR=$BINDIR/busybox

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

# BUSYBOX --> RAMDISK
if [ ! -d "$RAMDISKBINDIR/xbin" ]; then
	mkdir $RAMDISKBINDIR/xbin
fi
cp $BUSYBOXBINDIR/busybox $RAMDISKBINDIR/xbin/

# ANIMATIONCHOOSER --> RAMDISK
cp $PLATFORMBINDIR/animationchooser $RAMDISKBINDIR/sbin/

# RECOVERY --> RAMDISK
cp $PLATFORMBINDIR/recovery $RAMDISKBINDIR/sbin/

# Create symbolic links for all of the busybox commands in /xbin
#
# NOTE: If the busybox commands change, you have to update this list *or*
# maybe it would be smarter and easier to also build the same version
# for the host, then "busybox --list" will provide the same information
pushd $RAMDISKBINDIR/xbin
ln -sfT busybox [
ln -sfT busybox [[
ln -sfT busybox acpid
ln -sfT busybox addgroup
ln -sfT busybox adduser
ln -sfT busybox adjtimex
ln -sfT busybox arp
ln -sfT busybox arping
ln -sfT busybox ash
ln -sfT busybox awk
ln -sfT busybox base64
ln -sfT busybox basename
ln -sfT busybox beep
ln -sfT busybox blkid
ln -sfT busybox blockdev
ln -sfT busybox brctl
ln -sfT busybox bunzip2
ln -sfT busybox bzcat
ln -sfT busybox bzip2
ln -sfT busybox cal
ln -sfT busybox cat
ln -sfT busybox catv
ln -sfT busybox chat
ln -sfT busybox chattr
ln -sfT busybox chgrp
ln -sfT busybox chmod
ln -sfT busybox chown
ln -sfT busybox chpasswd
ln -sfT busybox chpst
ln -sfT busybox chroot
ln -sfT busybox chrt
ln -sfT busybox chvt
ln -sfT busybox cksum
ln -sfT busybox clear
ln -sfT busybox cmp
ln -sfT busybox comm
ln -sfT busybox cp
ln -sfT busybox cpio
ln -sfT busybox crond
ln -sfT busybox crontab
ln -sfT busybox cryptpw
ln -sfT busybox cttyhack
ln -sfT busybox cut
ln -sfT busybox date
ln -sfT busybox dc
ln -sfT busybox dd
ln -sfT busybox deallocvt
ln -sfT busybox delgroup
ln -sfT busybox deluser
ln -sfT busybox depmod
ln -sfT busybox devmem
ln -sfT busybox df
ln -sfT busybox dhcprelay
ln -sfT busybox diff
ln -sfT busybox dirname
ln -sfT busybox dmesg
ln -sfT busybox dnsd
ln -sfT busybox dnsdomainname
ln -sfT busybox dos2unix
ln -sfT busybox du
ln -sfT busybox dumpkmap
ln -sfT busybox dumpleases
ln -sfT busybox echo
ln -sfT busybox ed
ln -sfT busybox egrep
ln -sfT busybox eject
ln -sfT busybox env
ln -sfT busybox envdir
ln -sfT busybox envuidgid
ln -sfT busybox ether-wake
ln -sfT busybox expand
ln -sfT busybox expr
ln -sfT busybox fakeidentd
ln -sfT busybox false
ln -sfT busybox fbset
ln -sfT busybox fbsplash
ln -sfT busybox fdflush
ln -sfT busybox fdformat
ln -sfT busybox fdisk
ln -sfT busybox fgconsole
ln -sfT busybox fgrep
ln -sfT busybox find
ln -sfT busybox findfs
ln -sfT busybox flock
ln -sfT busybox fold
ln -sfT busybox free
ln -sfT busybox freeramdisk
ln -sfT busybox fsck
ln -sfT busybox fsck.minix
ln -sfT busybox fsync
ln -sfT busybox ftpd
ln -sfT busybox ftpget
ln -sfT busybox ftpput
ln -sfT busybox fuser
ln -sfT busybox getopt
ln -sfT busybox getty
ln -sfT busybox grep
ln -sfT busybox groups
ln -sfT busybox gunzip
ln -sfT busybox gzip
ln -sfT busybox hd
ln -sfT busybox hdparm
ln -sfT busybox head
ln -sfT busybox hexdump
ln -sfT busybox hostid
ln -sfT busybox hostname
ln -sfT busybox httpd
ln -sfT busybox hush
ln -sfT busybox hwclock
ln -sfT busybox id
ln -sfT busybox ifconfig
ln -sfT busybox ifdown
ln -sfT busybox ifenslave
ln -sfT busybox ifplugd
ln -sfT busybox ifup
ln -sfT busybox inetd
ln -sfT busybox insmod
ln -sfT busybox install
ln -sfT busybox ionice
ln -sfT busybox iostat
ln -sfT busybox ip
ln -sfT busybox ipaddr
ln -sfT busybox ipcalc
ln -sfT busybox ipcrm
ln -sfT busybox ipcs
ln -sfT busybox iplink
ln -sfT busybox iproute
ln -sfT busybox iprule
ln -sfT busybox iptunnel
ln -sfT busybox kbd_mode
ln -sfT busybox kill
ln -sfT busybox killall
ln -sfT busybox killall5
ln -sfT busybox klogd
ln -sfT busybox last
ln -sfT busybox less
ln -sfT busybox linux32
ln -sfT busybox linux64
ln -sfT busybox ln
ln -sfT busybox loadfont
ln -sfT busybox loadkmap
ln -sfT busybox logger
ln -sfT busybox login
ln -sfT busybox logname
ln -sfT busybox logread
ln -sfT busybox losetup
ln -sfT busybox lpd
ln -sfT busybox lpq
ln -sfT busybox lpr
ln -sfT busybox ls
ln -sfT busybox lsattr
ln -sfT busybox lsmod
ln -sfT busybox lspci
ln -sfT busybox lsusb
ln -sfT busybox lzcat
ln -sfT busybox lzma
ln -sfT busybox lzop
ln -sfT busybox lzopcat
ln -sfT busybox makedevs
ln -sfT busybox makemime
ln -sfT busybox man
ln -sfT busybox md5sum
ln -sfT busybox mdev
ln -sfT busybox microcom
ln -sfT busybox mkdir
ln -sfT busybox mkdosfs
ln -sfT busybox mke2fs
ln -sfT busybox mkfifo
ln -sfT busybox mkfs.ext2
ln -sfT busybox mkfs.minix
ln -sfT busybox mkfs.vfat
ln -sfT busybox mknod
ln -sfT busybox mkpasswd
ln -sfT busybox mkswap
ln -sfT busybox mktemp
ln -sfT busybox modinfo
ln -sfT busybox modprobe
ln -sfT busybox more
ln -sfT busybox mount
ln -sfT busybox mountpoint
ln -sfT busybox mpstat
ln -sfT busybox mt
ln -sfT busybox mv
ln -sfT busybox nameif
ln -sfT busybox nbd-client
ln -sfT busybox nc
ln -sfT busybox netstat
ln -sfT busybox nice
ln -sfT busybox nmeter
ln -sfT busybox nohup
ln -sfT busybox nslookup
ln -sfT busybox ntpd
ln -sfT busybox od
ln -sfT busybox openvt
ln -sfT busybox passwd
ln -sfT busybox patch
ln -sfT busybox pgrep
ln -sfT busybox pidof
ln -sfT busybox ping
ln -sfT busybox ping6
ln -sfT busybox pipe_progress
ln -sfT busybox pivot_root
ln -sfT busybox pkill
ln -sfT busybox pmap
ln -sfT busybox popmaildir
ln -sfT busybox powertop
ln -sfT busybox printenv
ln -sfT busybox printf
ln -sfT busybox ps
ln -sfT busybox pstree
ln -sfT busybox pscan
ln -sfT busybox pwd
ln -sfT busybox pwdx
ln -sfT busybox raidautorun
ln -sfT busybox rdate
ln -sfT busybox rdev
ln -sfT busybox readahead
ln -sfT busybox readlink
ln -sfT busybox readprofile
ln -sfT busybox realpath
ln -sfT busybox reformime
ln -sfT busybox renice
ln -sfT busybox reset
ln -sfT busybox resize
ln -sfT busybox rev
ln -sfT busybox rm
ln -sfT busybox rmdir
ln -sfT busybox rmmod
ln -sfT busybox route
ln -sfT busybox rpm
ln -sfT busybox rpm2cpio
ln -sfT busybox rtcwake
ln -sfT busybox run-parts
ln -sfT busybox runlevel
ln -sfT busybox runsv
ln -sfT busybox runsvdir
ln -sfT busybox rx
ln -sfT busybox script
ln -sfT busybox scriptreplay
ln -sfT busybox sed
ln -sfT busybox sendmail
ln -sfT busybox seq
ln -sfT busybox setarch
ln -sfT busybox setconsole
ln -sfT busybox setfont
ln -sfT busybox setkeycodes
ln -sfT busybox setlogcons
ln -sfT busybox setsid
ln -sfT busybox setuidgid
ln -sfT busybox sh
ln -sfT busybox sha1sum
ln -sfT busybox sha256sum
ln -sfT busybox sha512sum
ln -sfT busybox showkey
ln -sfT busybox slattach
ln -sfT busybox sleep
ln -sfT busybox smemcap
ln -sfT busybox softlimit
ln -sfT busybox sort
ln -sfT busybox split
ln -sfT busybox start-stop-daemon
ln -sfT busybox stat
ln -sfT busybox strings
ln -sfT busybox stty
ln -sfT busybox sulogin
ln -sfT busybox sum
ln -sfT busybox sv
ln -sfT busybox svlogd
ln -sfT busybox swapoff
ln -sfT busybox swapon
ln -sfT busybox switch_root
ln -sfT busybox sync
ln -sfT busybox sysctl
ln -sfT busybox syslogd
ln -sfT busybox tac
ln -sfT busybox tail
ln -sfT busybox tar
ln -sfT busybox tcpsvd
ln -sfT busybox tee
ln -sfT busybox telnet
ln -sfT busybox telnetd
ln -sfT busybox test
ln -sfT busybox tftp
ln -sfT busybox tftpd
ln -sfT busybox time
ln -sfT busybox timeout
ln -sfT busybox top
ln -sfT busybox touch
ln -sfT busybox tr
ln -sfT busybox traceroute
ln -sfT busybox traceroute6
ln -sfT busybox true
ln -sfT busybox tty
ln -sfT busybox ttysize
ln -sfT busybox tunctl
ln -sfT busybox udhcpc
ln -sfT busybox udhcpd
ln -sfT busybox udpsvd
ln -sfT busybox umount
ln -sfT busybox uname
ln -sfT busybox unexpand
ln -sfT busybox uniq
ln -sfT busybox unix2dos
ln -sfT busybox unlzma
ln -sfT busybox unlzop
ln -sfT busybox unxz
ln -sfT busybox unzip
ln -sfT busybox uptime
ln -sfT busybox users
ln -sfT busybox usleep
ln -sfT busybox uudecode
ln -sfT busybox uuencode
ln -sfT busybox vconfig
ln -sfT busybox vi
ln -sfT busybox vlock
ln -sfT busybox volname
ln -sfT busybox wall
ln -sfT busybox watch
ln -sfT busybox watchdog
ln -sfT busybox wc
ln -sfT busybox wget
ln -sfT busybox which
ln -sfT busybox who
ln -sfT busybox whoami
ln -sfT busybox whois
ln -sfT busybox xargs
ln -sfT busybox xz
ln -sfT busybox xzcat
ln -sfT busybox yes
ln -sfT busybox zcat
ln -sfT busybox zcip
popd

exit

