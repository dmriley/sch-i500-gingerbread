//-----------------------------------------------------------------------------
// backup.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Backup Operations
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
// Portions based on MAKE_EXT4FS utility, part of the Android Open Source Project
//
// Modifications: Copyright (C) 2011 Michael Brehm
//-----------------------------------------------------------------------------

#ifndef __BACKUP_H_
#define __BACKUP_H_

#include "callbacks.h"				// Include CALLBACKS declarations
#include "volume.h"					// Include VOLUME declarations

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Create a dump file from the specified volume
int backup_dump(const Volume* volume, const char* imgfile, int gzip);

// Create a dump file from the specified volume, with a UI status callback
int backup_dump_ui(const Volume* volume, const char* imgfile, int gzip, ui_callbacks* callbacks);

// Create an EXT4 image file from the specified volume
int backup_ext4(const Volume* volume, const char* imgfile, int gzip);

// Create an EXT4 image file from the specified volume, with a UI status callback
int backup_ext4_ui(const Volume* volume, const char* imgfile, int gzip, ui_callbacks* callbacks);

// Create a sparse EXT4 image file from the specified mountpoint
int backup_ext4_sparse(const Volume* volume, const char* imgfile, int gzip);

// Create a sparse EXT4 image file from the specified mountpoint, with a UI status callback
int backup_ext4_sparse_ui(const Volume* volume, const char* imgfile, int gzip, ui_callbacks* callbacks);

// Create a YAFFS2 image file from the specified directory
int backup_yaffs2(const char* directory, const char* imgfile, int gzip);

// Create a YAFFS2 image file from the specified directory, with a UI status callback
int backup_yaffs2_ui(const char* directory, const char* imgfile, int gzip, ui_callbacks* callbacks);

//-----------------------------------------------------------------------------

#endif	// __RESTORE_H_
