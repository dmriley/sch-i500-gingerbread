//-----------------------------------------------------------------------------
// backup.c
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

#include <sys/stat.h>							// Include STAT declarations
#include <limits.h>								// Include LIMITS declarations
#include <fcntl.h>								// Include FCNTL declarations
#include <zlib.h> 								// Include ZLIB declarations
#include "dirent.h"								// Include DIRENT declarations
#include "ext4_utils/make_ext4fs.h"				// Include MAKE_EXT4FS decls
#include "yaffs2/yaffs2/utils/mkyaffs2image.h"	// Include MKYAFFS2IMAGE decls
#include "backup.h"								// Include BACKUP declarations

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// used by mkyaffs2_callback - see backup_yaffs2_internal
static int 					g_yaffs_filecount = 0;
static int 					g_yaffs_filesprocessed = 0;
static ui_callbacks* 		g_yaffs_uicallbacks = NULL;

//-----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// common helper that combines all the ext4 backup permutations
static int backup_ext4_internal(const Volume* volume, const char* imgfile, 
	ui_callbacks* callbacks, int gzip, int sparse);

// callback for mkyaffs2image
static void mkyaffs2_callback(char* filename);

// counts the total number of files in a directory for mkyaffs2_callback
static int mkyaffs2_countfiles(const char* directory, int* count);

//-----------------------------------------------------------------------------
// backup_dump
//
// Creates a dump file from a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP

int backup_dump(const Volume* volume, const char* imgfile, int gzip)
{
	// Invoke the UI version with a NULL callback structure
	return backup_dump_ui(volume, imgfile, gzip, NULL);
}

//-----------------------------------------------------------------------------
// backup_dump_ui
//
// Creates a dump file from a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP
//	callbacks		- Optional UI callbacks for progress and messages

int backup_dump_ui(const Volume* volume, const char* imgfile, int gzip, ui_callbacks* callbacks)
{
	unsigned char*		buffer;				// Data buffer
	int 				source;				// Source volume file descriptor
	unsigned long long	size;				// Size of input volume (for progress)
	gzFile 				dest;				// Destination file
	int 				cbread;				// Number of bytes read from source file
	unsigned long long	totalread = 0;		// Total number of bytes read from source
	int 				badthings = 0;		// Flag if bad things happened
	int 				result;				// Result from function call
	
	const int BUFFER_SIZE = 4096;			// Size of the read buffer
	
	USES_UI_CALLBACKS(callbacks);
	
	if(volume == NULL) return EINVAL;			// Invalid [in] argument
	if(imgfile == NULL) return EINVAL;			// Invalid [in] argument

	// Get the size of the input volume, but it's not the end of the world if we can't
	result = volume_size(volume, &size);
	if(result != 0) { UI_WARNING("Unable to determine input device size, progress indicator will not work\n"); }

	// Allocate the buffer memory
	buffer = malloc(BUFFER_SIZE);
	if(!buffer) { 
		
		UI_ERROR("Unable to allocate data buffer. EC = %d\n", errno); 
		return errno; 
	}
	
	// Attempt to open the input device first
	source = open(volume->device, O_RDONLY);
	if(source < 0) { 
		
		UI_ERROR("Cannot open input device %s for read. EC = %d\n", volume->device, errno);
		free(buffer);
		return errno;
	}
	
	// Now attempt to open the output file with gzip, specifying no compression as applicable
	dest = gzopen(imgfile, (gzip) ? "wb" : "wb0");
	if(dest == Z_NULL) {
		
		UI_ERROR("Cannot open output file %s for write. EC = %d\n", imgfile, errno);
		close(source);
		free(buffer);
		return errno;
	}
	
	// Move the data ....
	cbread = read(source, buffer, BUFFER_SIZE);
	while(cbread > 0) {
	
		totalread += cbread;				// Tack on the number of bytes read
		
		// Write the buffer to the output file, and break the loop on an error
		result = gzwrite(dest, buffer, cbread);
		if(result < cbread) { 
			
			UI_ERROR("Unable to write data to output file %s. EC = %d\n", imgfile, errno);
			badthings = -1;
			break;
		}
		
		// If we know the size of the output device, we can provide a progress status
		if(size > 0) UI_SETPROGRESS((float)(totalread * 100) / (float)size);
		
		cbread = read(source, buffer, BUFFER_SIZE);		// Read next block of data
	}
	
	if(cbread < 0) { 
		
		UI_ERROR("Unable to read data from input volume %s. EC = %d.\n", volume->device, errno); 
		badthings = -1;
	}
	
	gzclose(dest);								// Close the output file
	close(source);								// Close the input volume
	free(buffer);								// Release buffer memory
	
	return (badthings) ? -1 : 0;
}

//-----------------------------------------------------------------------------
// backup_ext4
//
// Creates an EXT4 image of a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP

int backup_ext4(const Volume* volume, const char* imgfile, int gzip)
{
	// Invoke the UI version with a NULL callback structure
	return backup_ext4_ui(volume, imgfile, gzip, NULL);
}

//-----------------------------------------------------------------------------
// backup_ext4_ui
//
// Creates an EXT4 image of a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP
//	callbacks		- Optional UI callbacks for progress and messages

int backup_ext4_ui(const Volume* volume, const char* imgfile, int gzip, 
	ui_callbacks* callbacks)
{
	// Invoke the common backup routine
	return backup_ext4_internal(volume, imgfile, callbacks, (gzip) ? 1 : 0, 0);
}

//-----------------------------------------------------------------------------
// backup_ext4_sparse
//
// Creates a sparse EXT4 image of a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP

int backup_ext4_sparse(const Volume* volume, const char* imgfile, int gzip)
{
	// Invoke the UI version with a NULL callback structure
	return backup_ext4_sparse_ui(volume, imgfile, gzip, NULL);
}

//-----------------------------------------------------------------------------
// backup_ext4_sparse_ui
//
// Creates a sparse EXT4 image of a volume
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP
//	callbacks		- Optional UI callbacks for progress and messages

int backup_ext4_sparse_ui(const Volume* volume, const char* imgfile, 
	int gzip, ui_callbacks* callbacks)
{
	// Invoke the common backup routine
	return backup_ext4_internal(volume, imgfile, callbacks, (gzip) ? 1 : 0, 1);
}

//-----------------------------------------------------------------------------
// backup_ext4_internal (private)
//
// Common function that actually calls make_ext4fs
//
// Arguments:
//
//	volume			- Volume to be backed up
//	imgfile			- Destination image file path
//	callbacks		- Optional UI callbacks for progress and messages
//	gzip			- Flag indicating if image file should be compressed
//	sparse			- Flag indicating that a sparse EXT4 image should be created

static int backup_ext4_internal(const Volume* volume, const char* imgfile, 
	ui_callbacks* callbacks, int gzip, int sparse)
{
	unsigned long long	size;				// Size of the source volume
	int 				result;				// Result from function call
	
	USES_UI_CALLBACKS(callbacks);
	
	if(volume == NULL) return EINVAL;			// Invalid [in] argument
	if(imgfile == NULL) return EINVAL;			// Invalid [in] argument

	// Get the size of the source volume
	result = volume_size(volume, &size);
	if(result != 0) { 
		
		UI_ERROR("Cannot determine size of source volume %s. EC = %d\n", volume->name, result); 
		return result;
	}

	// Initialize the EXT4 information structure with any relevant details
	// and the user interface callbacks so it can communicate back to us
	reset_ext4fs_info();
	info.len = size;
	info.ui_stderr = callbacks->uiprint;
	info.ui_stdout = callbacks->uiprint;
	info.ui_progress = callbacks->progress;
	
	// Create the EXT4 file system image
	result = make_ext4fs(imgfile, volume->mount_point, (char*)volume->mount_point, 1, gzip, sparse);
	
	reset_ext4fs_info();						// Unnecessary, but clean out info
	return result;								// Return result from make_ext4fs
}

//-----------------------------------------------------------------------------
// backup_yaffs2
//
// Creates a YAFFS2 image of a volume
//
// Arguments:
//
//	directory		- Directory to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP

int backup_yaffs2(const char* directory, const char* imgfile, int gzip)
{
	// Invoke the private version with a NULL callback structure
	return backup_yaffs2_ui(directory, imgfile, gzip, NULL);
}

//-----------------------------------------------------------------------------
// backup_yaffs2_ui
//
// Creates a YAFFS2 image of a volume
//
// Arguments:
//
//	directory		- Directory to be backed up
//	imgfile			- Destination image file path
//	gzip			- Flag to compress the output file with GZIP
//	callbacks		- Optional UI callbacks for progress and messages

int backup_yaffs2_ui(const char* directory, const char* imgfile, int gzip, 
	ui_callbacks* callbacks)
{
	int 				files = 0;			// Number of files on volume
	int 				result;				// Result from function call
	
	USES_UI_CALLBACKS(callbacks);
	
	if(directory == NULL) return EINVAL;		// Invalid [in] argument
	if(imgfile == NULL) return EINVAL;			// Invalid [in] argument

	// mkyaffs2image has a callback that indicates the file name being processed,
	// but this recovery uses a percent complete indicator.  Rather than modify
	// mkyaffs2image like I did with ext4_utils, since I have nothing else to change
	// in there, I'm opting to just count the files up front and deal with it 
	// via a simple custom callback here (For now, at least - I may change my mind,
	// especially since I want to add native gzip functionality to it)
	
	result = mkyaffs2_countfiles(directory, &files);
	if(result != 0) { UI_WARNING("Unable to determine file count for directory %s, progress will not be shown\n", directory); }
	
	g_yaffs_filecount = files;						// Set global callback variable
	g_yaffs_filesprocessed = 0;						// Set global callback variable
	g_yaffs_uicallbacks = callbacks;				// Set global callback variable
	
	// Create the YAFFS2 file system image
	result = mkyaffs2image((char*)directory, (char*)imgfile, 0, mkyaffs2_callback, gzip);
	
	g_yaffs_filecount = 0;							// Reset global callback variable
	g_yaffs_filesprocessed = 0;						// Reset global callback variable
	g_yaffs_uicallbacks = NULL;						// Reset global callback variable

	return result;
}

// this might stay; part of the callback solution for mkyaffs2image
static void mkyaffs2_callback(char* filename)
{
	(filename);						// UNREFERENCED VARIABLE
	
	USES_UI_CALLBACKS(g_yaffs_uicallbacks);

	if(g_yaffs_filecount <= 0) return;		// Avoid division by zero
	
	g_yaffs_filesprocessed++;
	UI_SETPROGRESS((float)(g_yaffs_filesprocessed * 100) / (float)g_yaffs_filecount);
}

// this might stay; part of the callback solution for mkyaffs2image
static int mkyaffs2_countfiles(const char* directory, int* count)
{
	DIR 			*dir;				// Directory to count files from
	struct dirent 	*entry;				// dirent file information
	
	if(!count) return -1;

	dir = opendir(directory);
	if(dir)
	{
		while((entry = readdir(dir)) != NULL)
		{
			// Ignore . and ..
			if(strcmp(entry->d_name, ".") && strcmp(entry->d_name, ".."))
 			{
 				char full_name[PATH_MAX];
				struct stat stats;
				
				sprintf(full_name, "%s/%s", directory, entry->d_name);
				lstat(full_name, &stats);
				
				if(S_ISLNK(stats.st_mode) || S_ISREG(stats.st_mode) || S_ISDIR(stats.st_mode) || S_ISFIFO(stats.st_mode) || 
					S_ISBLK(stats.st_mode) || S_ISCHR(stats.st_mode) || S_ISSOCK(stats.st_mode)) {

					(*count)++;
					if(S_ISDIR(stats.st_mode)) mkyaffs2_countfiles(full_name, count);
				}
			}
		}

		closedir(dir);
	}
	
	return 0;
}

//-----------------------------------------------------------------------------