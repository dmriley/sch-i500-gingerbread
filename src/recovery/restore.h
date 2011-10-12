//-----------------------------------------------------------------------------
// restore.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Restore Operations
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

#ifndef __RESTORE_H_
#define __RESTORE_H_

#include "callbacks.h"				// Include CALLBACKS declarations
#include "volume.h"					// Include VOLUME declarations

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Write the DUMP file to the specified volume
int restore_dump(const char* imgfile, const Volume* volume);

// Write the DUMP file to the specified volume, with a UI status callback
int restore_dump_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks);

// Write the EXT4 file to the specified volume
int restore_ext4(const char* imgfile, const Volume* volume);

// Write the EXT4 file to the specified volume, with a UI status callback
int restore_ext4_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks);

// Write the sparse EXT4 file to the specified volume
int restore_ext4_sparse(const char* imgfile, const Volume* volume);

// Write the sparse EXT4 file to the specified volume, with a UI status callback
int restore_ext4_sparse_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks);

// Write the YAFFS2 file to the specified directory
int restore_yaffs2(const char* imgfile, const char* directory);

// Write the YAFFS2 file to the specified directory, with a UI status callback
int restore_yaffs2_ui(const char* imgfile, const char* directory, ui_callbacks* callbacks);

//-----------------------------------------------------------------------------

#endif	// __RESTORE_H_
