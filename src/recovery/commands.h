//-----------------------------------------------------------------------------
// commands.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Recovery Commands
//
// Copyright (C) 2007 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Modifications: Copyright (C) 2011 Michael Brehm
//-----------------------------------------------------------------------------

#ifndef __COMMANDS_H_
#define __COMMANDS_H_

#include "volume.h"					// Include VOLUME declarations

//-----------------------------------------------------------------------------
// TYPE DECLARATIONS
//-----------------------------------------------------------------------------

// cmd_backup_method
//
// Enumeration used to determine what method (EXT4, YAFFS2, etc) of imaging
// to use when backing up a volume
typedef enum {
	
	ext4			= 0,
	ext4_sparse,
	dump,
	yaffs2,
	
} cmd_backup_method;

//-----------------------------------------------------------------------------
// PUBLIC FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Creates a backup of the entire device
void cmd_backup_device(void);

// Creates a YAFFS2 image of a single directory
void cmd_backup_directory(const char* directory, const char* destpath, int compress);

// Creates an image of an entire volume
void cmd_backup_volume(const Volume* srcvol, const char* destpath, cmd_backup_method method, int compress);

// Converts a volume file system
void cmd_convert_volume(const Volume* volume, const char* fs);

// Formats a volume
void cmd_format_volume(const Volume* volume, const char* fs);

// Installs the busybox included in the recovery ramdisk to the system
void cmd_install_busybox(void);

// Installs the su included in the recovery ramdisk to the system
void cmd_install_su(void);

// Installs an update from a .zip file
void cmd_install_updatezip(const char* zipfile);

// Kills the ADBD process, which will force it to restart
void cmd_kill_adbd(void);

// Mounts a volume
void cmd_mount_volume(const Volume* volume);

// Removes busybox from the system (assumes it was installed by us)
void cmd_remove_busybox(void);

// Removes su from the system (assumes it was installed by us)
void cmd_remove_su(void);

// Restores a YAFFS2 image of a directory
void cmd_restore_directory(const char* srcpath, const char* directory);

// Restores an image of an entire volume
void cmd_restore_volume(const char* srcpath, const Volume* destvol);

// Shows the recovery key usage information
void cmd_show_usage(void);

// Unmounts a volume
void cmd_unmount_volume(const Volume* volume);

// Wipes the BATTERYSTATS.BIN file
void cmd_wipe_battery_stats(void);

// Wipes the CACHE partition
void cmd_wipe_cache(void);

// Wipes the Dalvik cache
void cmd_wipe_dalvik_cache(void);

// Wipes all user data from the device
void cmd_wipe_device(void);

//-----------------------------------------------------------------------------

#endif	// __COMMANDS_H_
