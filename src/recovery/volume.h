//-----------------------------------------------------------------------------
// volume.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Volume Utilities
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

#ifndef __VOLUME_H_
#define __VOLUME_H_

#include <sys/statfs.h>					// Include STATFS declarations

//-----------------------------------------------------------------------------
// PUBLIC DATA TYPES
//-----------------------------------------------------------------------------

// Volume - Volume mounting information from fstab file
//
//	device			- Device path
// 	mount_point		- Mounting point
//	fs_type			- Primary file system type
//	fs_options		- Primary FSTYPE mounting options
//  dump			- Flag if volume should be dumped during backup
//	fsck_order		- Order in which volume should be checked (or zero)
// 	name			- Display (friendly) volume name
//	wipe			- Flag to wipe this device on a factory reset
//	fs_type2		- Secondary FSTYPE to try if fs_type fails
//	fs_options2		- Secondary FSTYPE mounting options
//	virtual			- Flag if the entry was added virtually (RAMDISK)
//
typedef struct {
	
	const char* 	device;
	const char* 	mount_point;
	const char* 	fs_type;
	const char* 	fs_options;
	const char* 	dump;
	const char* 	fsck_order;
	const char* 	name;
	const char* 	wipe;
	const char* 	fs_type2;
	const char* 	fs_options2;
	int 			virtual;

} Volume;

//-----------------------------------------------------------------------------
// PUBLIC FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Iterates over all of the entries in the volume table
const Volume* foreach_volume(const Volume* previous);

// Formats the specified volume with the primary file system
int format_volume(const Volume* volume, const char* fs);

// Return the Volume record for the specified name
const Volume* get_volume(const char* name);

// Locates the Volume that the specified path would be contained in
const Volume* get_volume_for_path(const char* path);

// Mounts a volume
int mount_volume(const Volume* volume, int* mounted);

// Restores a gzipped EXT4 backup of the specified volume created with backup_volume
int restore_volume(const Volume* volume, const char* backup_file);

// Unmounts a volume
int unmount_volume(const Volume* volume, int* unmounted);

// Gets the raw size of a volume device
int volume_size(const Volume* volume, unsigned long long* size);

// Retrieves STATFS information for a volume
int volume_stats(const Volume* volume, struct statfs* stats);

// Load and parse volume data from fstab file.
int volumes_init(const char* fstab_file);

// Unload and release the global volume table data
void volumes_term(void);

// Formats the specified volume with whatever filesystem it already has
int wipe_volume(const Volume* volume);

//-----------------------------------------------------------------------------

#endif	// __VOLUME_H_
