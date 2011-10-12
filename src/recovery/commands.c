//-----------------------------------------------------------------------------
// commands.c
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

// Must #define this before including the YAFFS2 header files
#define CONFIG_YAFFS_UTIL 

#include <stdlib.h>						// Include STDLIB declarations
#include <string.h>						// Include STRING declarations
#include <sys/limits.h>					// Include LIMITS declarations
#include <sys/statfs.h>					// Include STATFS declarations
#include <sys/wait.h>					// Include WAIT declarations
#include <time.h>						// Include TIME declarations
#include <diskconfig/diskconfig.h>		// Include DISKCONFIG declarations
#include <zlib.h>						// Include ZLIB declarations
#include "common.h"						// Include COMMON declarations
#include "callbacks.h"					// Include CALLBACKS declarations
#include "ui.h"							// Include UI declarations
#include "exec.h"						// Include EXEC declarations
#include "volume.h"						// Include VOLUME declarations
#include "backup.h"						// Include BACKUP declarations
#include "restore.h"					// Include RESTORE declarations
#include "install.h"					// Include INSTALL declarations
#include "minzip/DirUtil.h"				// Include DIRUTIL declarations
#include "minzip/Zip.h"					// Include ZIP declarations
#include "ext4_utils/ext4.h"			// Include EXT4_UTILS support
#include "ext4_utils/ext4_utils.h"		// Include EXT4_UTILS support
#include "ext4_utils/sparse_format.h"	// Include EXT4_UTILS support
#include "yaffs2/yaffs2/yaffs_guts.h"	// Include YAFFS2 support
#include "commands.h"					// Include COMMANDS declarations

//-----------------------------------------------------------------------------
// CONSTANTS / MACROS
//-----------------------------------------------------------------------------

// YAFFS2 constants
#define YAFFS2_CHUNK_SIZE 				2048
#define YAFFS2_SPARE_SIZE 				64

// CONVERT_TEMP_PATH - Path to use when converting file systems
static char CONVERT_TEMP_FILE[]	= "/sdcard/__convert_temp.img";

//-----------------------------------------------------------------------------
// PRIVATE TYPE DECLARATIONS
//-----------------------------------------------------------------------------

// backup_extensions - Structure that provides a lookup between a backup method
// and the default/expected file extension
typedef struct {
	
	cmd_backup_method 	method;
	char*				name;
	char*				extension;
	char*				compressed_extension;

} cmd_backup_method_info;

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// Table of information for supported backup types
static const cmd_backup_method_info g_backup_method_info[] = {
	
	/* method		name					extension	compressed_extension */
	{ ext4, 		"ext4 image", 			"ximg",		"ximg.gz"	},
	{ ext4_sparse,	"ext4 sparse image", 	"simg",		"szimg"		},
	{ dump,			"raw dump",				"img",		"img.gz"	},
	{ yaffs2,		"yaffs2 image",			"yimg",		"yimg.gz" 	},
};

//-----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Generates the paths for backup operations
static int cmd_gen_device_backup_path(char* out, size_t cch);

// Attempts to determine the type of an image file
static int cmd_restore_divine_method(const char* srcpath, cmd_backup_method* method);

// YAFFS2 restores require the volume be formatted and mounted first
static void cmd_restore_volume_yaffs2(const char* srcpath, const Volume* destvol);

//-----------------------------------------------------------------------------
// cmd_backup_device
//
// Creates a backup of the entire device
//
// Arguments:
//
//	NONE

void cmd_backup_device(void)
{
	char 				destpath[PATH_MAX];	// Backup destination path
	const Volume*		destvol;			// Destination volume
	int 				destmounted = 0;	// Flag if dest volume was mounted
	int 				srcmounted = 0;		// Flag if source volume was mounted
	char 				imgfile[256];		// Image file name
	const Volume*		iterator;			// Volume iterator
	ui_callbacks		callbacks;			// UI callbacks for backup operations
	struct statfs 		volstats;			// Volume statistics
	unsigned long long	totalbytes = 0;		// Total bytes to be backed up
	int 				result;				// Result from function call
	
	// Clear any currently displayed UI text and initialize the callbacks 
	ui_clear_text();	
	init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);

	// Figure out where the backup is going to go
	result = cmd_gen_device_backup_path(destpath, PATH_MAX);
	if(result != 0) { LOGE("cmd_backup_device: Cannot generate device backup output path"); return; }
	
	// Determine what the destination volume will be and make sure it's not the
	// same as the source volume ...
	destvol = get_volume_for_path(destpath);
	if(destvol == NULL) { LOGE("cmd_backup_device: Cannot locate volume for path %s\n", destpath); return; }
	
	// Make a first pass over all of the volumes to be backed up to determine what % of the backup each volume
	// takes up, and also check to make sure that the destination volume isn't one of them
	iterator = foreach_volume(NULL);
	while(iterator != NULL) {
		
		if(*iterator->dump == '1') {
			
			// Check that DEST is not one of the SOURCE volumes
			if(iterator == destvol) { LOGE("cmd_backup_device: Volume %s cannot be both a source and destination volume", destvol->name); return; }
			
			// Get stats for the volume and add it's size to the total byte count
			result = volume_stats(iterator, &volstats);
			if(result != 0) { LOGE("cmd_backup_device: Cannot get stats for volume %s", iterator->name); return; }
			totalbytes += (volstats.f_bsize * (volstats.f_blocks - volstats.f_bfree));
		}
	
		iterator = foreach_volume(iterator);				// Move to the next volume
	}
	
	//
	// TODO: /sdcard/Android and /sdcard/.android_secure
	//
	
	// Mount the destination volume
	result = mount_volume(destvol, &destmounted);
	if(result != 0) { LOGE("cmd_backup_device: Cannot mount destination volume %s. EC = %d\n", destvol->name, result); return; }
	
	// Create the destination folder
	result = dirCreateHierarchy(destpath, 0777, NULL, 0);
	if(result != 0) { 
		
		LOGE("cmd_backup_device: Cannot create destination folder %s. EC = %d\n", destpath, errno);
		if(destmounted != 0) unmount_volume(destvol, NULL);
		return;
	}
	
	ui_print("Backing up device...\n\n");
	
	// Iterate over all of the FSTAB volumes and back up each one marked DUMP
	iterator = foreach_volume(NULL);
	while(iterator != NULL) {
		
		if(*iterator->dump == '1') {
			
			ui_print("    > Backing up %s\n", iterator->name);
				
			// Determine what % of the total operation this volume is and set the progress bar portion
			result = volume_stats(iterator, &volstats);
			if(result != 0) { LOGW("cmd_backup_device: Cannot get stats for volume %s; progress will be inaccurate", iterator->name); }
			else ui_show_progress((float)(volstats.f_bsize * (volstats.f_blocks - volstats.f_bfree)) / (float)totalbytes, 0); 
			
			// Mount the source volume
			result = mount_volume(iterator, &srcmounted);
			if(result == 0) {
				
				// Back the volume up
				sprintf(imgfile, "%s/%s.%s", destpath, iterator->name, g_backup_method_info[yaffs2].compressed_extension);
				result = backup_yaffs2_ui(iterator->mount_point, imgfile, 1, &callbacks);
				if(result != 0) { LOGE("cmd_backup_device: Unable to backup volume %s.  EC = %d\n", iterator->name, result); }
					
				// If the source volume was mounted, unmount it before continuing
				if(srcmounted != 0) unmount_volume(iterator, NULL);
			}
				
			else { LOGE("cmd_backup_device: Cannot mount volume %s for backup. EC = %d\n", iterator->name, result); }			
		}
		
		iterator = foreach_volume(iterator);		// Move to next volume
	}
	
	//
	// TODO: /sdcard/Android and /sdcard/.android_secure
	//
	
	// If the destination volume was mounted by this function, unmount it
	if(destmounted != 0) unmount_volume(destvol, NULL);
	
	ui_reset_progress();								// Remove the progress bar
	
	ui_print("\n");
	ui_print("> Device backed up successfully.\n");		// Done
}

//-----------------------------------------------------------------------------
// cmd_backup_directory
//
// Creates a YAFFS2 backup of the specified directory
//
// Arguments:
//
//	directory	- Source Directory
//	destpath	- Destination path
//	compress	- Flag to compress the image file

void cmd_backup_directory(const char* directory, const char* destpath, int compress)
{
	const Volume* 	srcvol;				// Source volume
	const Volume*	destvol;			// Destination volume
	int 			srcmounted = 0;		// Flag if source volume was mounted
	int 			destmounted = 0;	// Flag if dest volume was mounted
	const char*		ext;				// Image file extension
	char			imgfile[256];		// Buffer for generated output file name
	ui_callbacks 	callbacks;			// UI callbacks for backup functions
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Unlike a volume backup, source and dest can be on the same volume, but the
	// destination file cannot be in a subdirectory of the source folder
	srcvol = get_volume_for_path(directory);
	if(srcvol == NULL) { LOGE("cmd_backup_directory: Cannot locate volume for path %s\n", directory); return; }
	
	destvol = get_volume_for_path(destpath);
	if(destvol == NULL) { LOGE("cmd_backup_directory: Cannot locate volume for path %s\n", destpath); return; }
	
	if(srcvol == destvol) {
		
		int len = strlen(directory);
		if (strncmp(destpath, directory, len) == 0 && (destpath[len] == '\0' || destpath[len] == '/')) {
		
			LOGE("cmd_backup_directory: Destination file cannot be a child of the target directory\n");
			return;
		}
	}
	
	// Mount the source volume
	result = mount_volume(srcvol, &srcmounted);
	if(result != 0) { LOGE("cmd_backup_directory: Unable to mount source volume %s. EC = %d\n", srcvol->name, result); return; }
	
	// Mount the destination volume
	result = mount_volume(destvol, &destmounted);
	if(result == 0) {
		
		// Generate the output file name, which is based on the directory name and backup method
		ext = (compress) ? g_backup_method_info[yaffs2].compressed_extension : g_backup_method_info[yaffs2].extension;
		sprintf(imgfile, "%s/%s.%s", destpath, "TODOTODO", ext);
		
		// Create the output image file directory on the destination volume
		result = dirCreateHierarchy(imgfile, 0777, NULL, 1);
		if(result == 0) {

			// Output some generally useless information about the backup operation
			ui_print("Backing up %s...\n\n", directory);
			ui_print("Method      : %s\n", g_backup_method_info[yaffs2].name);
			ui_print("Compression : %s\n", (compress) ? "Enabled" : "Disabled");
			ui_print("Output File : %s\n", imgfile);
			ui_print("\n");
			
			// Initialize UI callbacks and the progress bar
			init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);
			ui_show_progress(1.0, 0);
					
			// Create the backup image file
			backup_yaffs2_ui(directory, imgfile, compress, &callbacks);

			ui_print("> %s backed up successfully.\n", directory);
			ui_reset_progress();
		}
		
		else { LOGE("cmd_backup_directory: Cannot create destination path %s. EC = %d\n", destpath, result); }		
		
		// Unmount the destination volume if it was mounted by this function
		if(destmounted) unmount_volume(destvol, NULL);
	}
	
	else { LOGE("cmd_backup_directory: Unable to mount destination volume %s. EC = %d\n", destvol->name, result); }
	
	// Unmount the source volume if it was mounted by this function
	if(srcmounted) unmount_volume(srcvol, NULL);
}

//-----------------------------------------------------------------------------
// cmd_backup_volume
//
// Creates an image of the specified volume
//
// Arguments:
//
//	srcvol		- Source Volume
//	destpath	- Destination path
//	method		- Imaging method
//	compress	- Flag to compress the image file

void cmd_backup_volume(const Volume* srcvol, const char* destpath, 
	cmd_backup_method method, int compress)
{
	const Volume*	destvol;			// Destination volume
	int 			destmounted = 0;	// Flag if dest volume was mounted
	int 			srcmounted = 0;		// Flag if source volume was mounted
	char			imgfile[256];		// Buffer for generated output file name
	const char*		ext;				// Image file extension
	ui_callbacks 	callbacks;			// UI callbacks for backup functions
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Determine what the destination volume will be and make sure it's not the
	// same as the source volume ...
	destvol = get_volume_for_path(destpath);
	if(destvol == NULL) { LOGE("cmd_backup_volume: Cannot locate volume for path %s\n", destpath); return; }
	if(destvol == srcvol) { LOGE("cmd_backup_volume: Destination volume cannot be the same as the source volume\n"); return; }
	
	// Mount the destination volume
	result = mount_volume(destvol, &destmounted);
	if(result != 0) { LOGE("cmd_backup_volume: Cannot mount destination volume %s\n", destvol->name); return; }
	
	// Generate the output file name, which is based on the volume name and backup method
	ext = (compress) ? g_backup_method_info[method].compressed_extension : g_backup_method_info[method].extension;
	sprintf(imgfile, "%s/%s.%s", destpath, srcvol->name, ext);

	// Create the output image file directory on the destination volume
	result = dirCreateHierarchy(imgfile, 0777, NULL, 1);
	if(result == 0) {
		
		// Mount the source volume if necessary
		result = mount_volume(srcvol, &srcmounted);
		if(result == 0) {
		
			// Output some generally useless information about the backup operation
			ui_print("Backing up volume %s...\n\n", srcvol->name);
			ui_print("Method      : %s\n", g_backup_method_info[method].name);
			ui_print("Compression : %s\n", (compress) ? "Enabled" : "Disabled");
			ui_print("Output File : %s\n", imgfile);
			ui_print("\n");
			
			// Initialize UI callbacks for operations that support them
			init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);

			// Execute the backup operation
			switch(method) {
				
				// EXT4 Image
				case ext4: 
					ui_show_progress(1.0, 0);
					result = backup_ext4_ui(srcvol, imgfile, compress, &callbacks); 
					break;
				
				// EXT4 Sparse Image
				case ext4_sparse: 
					ui_show_progress(1.0, 0);
					result = backup_ext4_sparse_ui(srcvol, imgfile, compress, &callbacks); 
					break;
				
				// RAW DUMP
				case dump:
					ui_show_progress(1.0, 0);
					result = backup_dump_ui(srcvol, imgfile, compress, &callbacks);
					break;
				
				// YAFFS2
				case yaffs2:
					ui_show_progress(1.0, 0);
					result = backup_yaffs2_ui(srcvol->mount_point, imgfile, compress, &callbacks);
					break;
					
				default: { LOGE("cmd_backup_volume: Unknown backup method code\n"); result = -1; }
			}
			
			if(result == 0) ui_print("> Volume %s backed up successfully.\n", srcvol->name);
			else ui_print("> Failed to back up volume %s.\n", srcvol->name);
			
			ui_reset_progress();
			
			// Unmount the source volume if it was mounted by this function
			if(srcmounted != 0) unmount_volume(srcvol, NULL);
		}
		
		else { LOGE("cmd_backup_volume: Cannot mount source volume %s\n", srcvol->name); }
	}
	
	else { LOGE("cmd_backup_volume: Cannot create destination path %s. EC = %d\n", destpath, result); }
	
	//
	// TODO: Unmounting the destination volume seems to take quite a bit of
	// time after writing so much data to it.  Make sure that's the case
	// and refactor this function such that we unmount it first, then remount it,
	// then unmount it here with a progress indicator.  It will look like this
	// function has locked up or failed since it says "complete" and then stalls
	//
	
	// Unmount the destination volume if it was mounted by this function
	if(destmounted != 0) unmount_volume(destvol, NULL);
}

//-----------------------------------------------------------------------------
// cmd_convert_volume
//
// Converts the filesystem of a volume
//
// Arguments:
//
//	volume		- Volume to be converted
//	fs			- Target filesystem

void cmd_convert_volume(const Volume* volume, const char* fs)
{
	const Volume* 	destvol;			// Destination volume (the SDCARD)
	int 			destmounted;		// Flag if destination volume was mounted
	int 			srcmounted;			// Flag if source volume was mounted
	ui_callbacks 	callbacks;			// UI callbacks for backup functions
	struct statfs	stats;				// Volume statistics
	int 			result;				// Result from function call
	
	ui_clear_text();
	ui_print("Converting volume %s to %s ...\n\n[%s]\n\n", volume->name, fs, volume->device);
	
	// Get the destination volume for the temporary image file
	destvol = get_volume_for_path(CONVERT_TEMP_FILE);
	if(destvol == NULL) { LOGE("cmd_convert_volume: Cannot locate %s volume entry\n", destvol->name); return; }
	
	// Mount the destination volume
	result = mount_volume(destvol, &destmounted);
	if(result != 0) { LOGE("cmd_convert_volume: Unable to mount volume %s. EC = %d\n", destvol->name, result); return; }
	
	// Initialize UI callbacks
	init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);
	
	// Mount the source volume for backup
	result = mount_volume(volume, &srcmounted);
	if(result == 0) {
	
		// Backup the source volume
		ui_show_progress(0.5, 0);
		result = backup_yaffs2_ui(volume->mount_point, CONVERT_TEMP_FILE, 1, &callbacks);
		if(result == 0) {
			
			// Format the source volume
			result = format_volume(volume, fs);
			if(result == 0) {
			
				// Now restore the image
				ui_show_progress(0.5, 0);
				result = restore_yaffs2_ui(CONVERT_TEMP_FILE, volume->mount_point, &callbacks);
				if(result != 0) { LOGE("cmd_convert_volume: Unable to restore backup image to volume %s\n", volume->name); }
				
				// Delete the temporary image file 		
				result = remove(CONVERT_TEMP_FILE);
				if(result != 0) { LOGW("cmd_convert_volume: Unable to remove temporary file %s. EC = %d\n", CONVERT_TEMP_FILE, result); }
			}
			
			else { LOGE("cmd_convert_volume: Unable to format source volume %s. EC = %d\n", volume->name, result); }
		}
		
		else { LOGE("cmd_convert_volume: Unable to generate YAFFS2 backup image. EC = %d\n", result); }
		
		// If the source volume was not mounted on the way in, unmount it
		if(srcmounted) unmount_volume(volume, NULL);
	}
	
	else { LOGE("cmd_convert_volume: Unable to mount source volume %s. EC = %d\n", volume->name, result); }
	
	// Unmount the destination volume if it was mounted by this function
	if(destmounted) unmount_volume(destvol, NULL);
	
	// If everything went well, we're good to go ...
	if(result == 0) {
		
		ui_print("> Volume %s converted to %s.\n\n", volume->name, fs);
	
		// Attempt to get some stats for the mounted volume and display them.
		// This isn't exactly important, so just skip it if it doesn't work for
		// some reason.  Talk about a useless feature :)
		result = volume_stats(volume, &stats);
		if(result == 0) {
			
			ui_print("    Size       : %12llu bytes\n", stats.f_bsize * stats.f_blocks);
			ui_print("    Free Space : %12llu bytes\n", stats.f_bsize * stats.f_bfree);
			ui_print("    Available  : %12llu bytes\n", stats.f_bsize * stats.f_bavail);
			ui_print("\n");
		}		
	}
	
	ui_reset_progress();				// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------
// cmd_format_volume
//
// Formats the specified volume with the specified file system
//
// Arguments:
//
//	volume		- Volume to be formatted
//	fs			- Target file system

void cmd_format_volume(const Volume* volume, const char* fs)
{
	int 			unmounted = 0;		// Flag if volume was unmounted
	struct statfs	stats;				// Volume statistics
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	ui_show_indeterminate_progress();
	ui_print("Formatting volume %s (%s) ...\n\n[%s]\n\n", volume->name, fs, volume->device);
	
	// Unmount the target volume
	result = unmount_volume(volume, &unmounted);
	if(result != 0) { LOGE("cmd_format_volume: Cannot unmount volume %s\n", volume->name); return; }
	
	// Execute the format operation against the target volume
	result = format_volume(volume, fs);
	if(result != 0) {
	
		// Not too concerned with remounting the volume if the format fails, just exit early
		LOGE("cmd_format_volume: Unable to format volume %s with filesystem %s. EC = %d\n", volume->name, fs, result);
		return;
	}

	// If the destination volume was mounted on the way in, remount it
	if(unmounted) mount_volume(volume, NULL);
	
	// Attempt to get some stats for the mounted volume and display them.
	// This isn't exactly important, so just skip it if it doesn't work for
	// some reason.  Talk about a useless feature :)
	result = volume_stats(volume, &stats);
	if(result == 0) {
		
		ui_print("    Size       : %12llu bytes\n", stats.f_bsize * stats.f_blocks);
		ui_print("    Free Space : %12llu bytes\n", stats.f_bsize * stats.f_bfree);
		ui_print("    Available  : %12llu bytes\n", stats.f_bsize * stats.f_bavail);
		ui_print("\n");
	}		
	
	ui_print("> Volume %s formatted successfully.\n", volume->name);

	ui_reset_progress();					// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------
// cmd_gen_device_backup_path (private)
//
// Generates the path for a device backup operation
// Format: /sdcard/backup/device/YYYYMMDD[.x]
//
// Arguments:
//
//	out 		- Output string
//	cch			- Length of output buffer, in characters

int cmd_gen_device_backup_path(char* out, size_t cch)
{
	const Volume* 		volume;				// Destination volume (SDCARD)
	int 				mounted;			// Flag if volume was mounted
	time_t 				epoch;				// Current epoch time
	struct tm*			local;				// Coverted epoch time
	char 				basepath[PATH_MAX];	// Generated base path string
	struct stat 		dirstat;			// STAT information about a directory
	int 				index = 1;			// Path uniqueifier index
	int 				result;				// Result from function call
	
	if((out == NULL) || (cch <= 0)) return EINVAL;
	
	// Get a reference to the SDCARD volume
	volume = get_volume("SDCARD");
	if(volume == NULL) { LOGE("cmd_gen_device_backup_path: Cannot locate SDCARD volume entry in fstab.\n"); return ENOENT; }
	
	// Mount the SYSTEM volume
	result = mount_volume(volume, &mounted);
	if(result != 0) { LOGE("cmd_gen_device_backup_path: Cannot mount SDCARD, EC = %d\n", result); return ENOENT; }

	// Get the current local system time with which to format the string
	epoch = time(NULL);
	local = localtime(&epoch);
	
	// Start with /sdcard/backup/device/YYYYMMDD
	strftime(basepath, PATH_MAX, "/sdcard/backup/device/%Y%m%d", local);
	strncpy(out, basepath, cch);
	
	// Check to see if the generated directory exists, and if so append a .x
	// uniqueifier to the end of the string until we find one that doesn't
	while(stat(out, &dirstat) == 0) snprintf(out, cch, "%s.%d", basepath, index++);
	
	// TODO: It's kind of lame to just assume that stat() returning zero is a 
	// completely reliable method of determining what we want to determine here
	
	if(mounted) unmount_volume(volume, NULL);		// Unmount SDCARD if mounted
	return 0;
}

//-----------------------------------------------------------------------------
// cmd_install_busybox
//
// Installs the busybox included in the recovery ramdisk to the system
//
// Arguments:
//
//	NONE

void cmd_install_busybox(void)
{
	const Volume*		dest;				// Destination VOLUME
	int 				destmounted = 0;	// Flag if volume was mounted
	int 				result;				// Result from function call
	
	ui_clear_text();						// Clear the UI
	
	// Get a reference to the SYSTEM volume
	dest = get_volume("SYSTEM");
	if(dest == NULL) { LOGE("cmd_install_busybox: Cannot locate SYSTEM volume entry in fstab.\n"); return; }
	
	// Mount the SYSTEM volume
	result = mount_volume(dest, &destmounted);
	if(result != 0) { LOGE("cmd_install_busybox: Cannot mount SYSTEM, EC = %d\n", result); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Installing BusyBox to SYSTEM ...\n\n");
	
	// Install BUSYBOX using the script built into the ramdisk
	result = WEXITSTATUS(exec("sh /sbin/scripts/install-busybox.sh"));
	if(result != 0) LOGE("Unable to install BusyBox. EC = %d\n", result);
	else ui_print("> BusyBox installed successfully.\n");
	
	// If the SYSTEM volume was mounted by this function, unmount it before exit
	if(destmounted != 0) unmount_volume(dest, NULL);
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_install_su
//
// Installs the su binary included in the recovery ramdisk to the system
//
// Arguments:
//
//	NONE

void cmd_install_su(void)
{
	const Volume*		dest;				// Destination VOLUME
	int 				destmounted = 0;	// Flag if volume was mounted
	int 				result;				// Result from function call
	
	ui_clear_text();						// Clear the UI
	
	// Get a reference to the SYSTEM volume
	dest = get_volume("SYSTEM");
	if(dest == NULL) { LOGE("cmd_install_su: Cannot locate SYSTEM volume entry\n"); return; }
	
	// Mount the SYSTEM volume
	result = mount_volume(dest, &destmounted);
	if(result != 0) { LOGE("cmd_install_su: Cannot mount SYSTEM, EC = %d\n", result); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Installing su to SYSTEM ...\n\n");
	
	// Install SU using the script built into the ramdisk
	result = WEXITSTATUS(exec("sh /sbin/scripts/install-su.sh"));
	if(result != 0) LOGE("Unable to install su binary. EC = %d\n", result);
	else ui_print("> su binary installed successfully.\n");
	
	// If the SYSTEM volume was mounted by this function, unmount it before exit
	if(destmounted != 0) unmount_volume(dest, NULL);
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_install_updatezip
//
// Installs an update from a .zip file
//
// Arguments:
//
//	zipfile		- .ZIP file containing the update package

void cmd_install_updatezip(const char* zipfile)
{
	const Volume*		src;				// Source VOLUME
	int 				srcmounted = 0;		// Flag if volume was mounted
	ZipArchive 			zip;				// Installation .zip file
	int 				result;				// Result from function call

	ui_clear_text();						// Clear the UI
	
	// Locate the Volume entry for the source path in the FSTAB file
	src = get_volume_for_path(zipfile);
	if(src == NULL) { LOGE("cmd_install_updatezip: Cannot locate source volume entry for path %s in fstab.\n", zipfile); return; }
	
	// Mount the source volume
	result = mount_volume(src, &srcmounted);
	if(result != 0) { LOGE("cmd_install_updatezip: Cannot mount volume %s, EC = %d\n", src->name, result); return; }

	// Open the specified zip file archive
    result = mzOpenZipArchive(zipfile, &zip);
	if(result == 0) {
		
		ui_show_indeterminate_progress();
		ui_print("Installing update ...\n\n[%s]\n\n", zipfile);

		// Attempt to execute the update package ....
		result = try_update_binary(zipfile, &zip);
		if(result == INSTALL_SUCCESS) ui_print("\n> Installation complete.\n\n");
	}
	
	else { LOGE("cmd_install_updatezip: Unable to open zip archive %s, EC = %d\n", zipfile, result); }
	
	// If the source volume was mounted by this function, unmount it before exit
	if(srcmounted != 0) unmount_volume(src, NULL);

	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_kill_adbd
//
// Kills the ADBD process, which should cause it to restart as long as it's 
// been listed properly in the active .rc boot script
//
// Arguments:
//
//	NONE

void cmd_kill_adbd(void)
{
	int 				result;				// Result from function call
	
	ui_clear_text();						// Clear the UI
	
	ui_show_indeterminate_progress();
	ui_print("Killing Android Debug Bridge Daemon (ADBD) ...\n\n");
	
	// Find and kill the ADBD service process ...
	result = WEXITSTATUS(exec("pkill adbd"));
	if(result != 0) LOGE("Unable to kill ADBD process. EC = %d\n", result);
	else { ui_print("> ADBD process killed and should restart automatically.\n\n"); }
	
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_mount_volume
//
// Mounts the specified volume
//
// Arguments:
//
//	volume		- Volume to be mounted

void cmd_mount_volume(const Volume* volume)
{
	int 			mounted = 0;		// Flag if volume was mounted
	struct statfs	stats;				// Volume statistics
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Indicate the operation to the user
	ui_print("Mounting volume %s ...\n\n[%s]\n\n", volume->name, volume->device);
	
	// Attempt to mount the volume
	result = mount_volume(volume, &mounted);
	if(result == 0) {
	
		if(mounted) ui_print("> Volume %s mounted successfully.\n", volume->name);
		else ui_print("> Volume %s was already mounted.\n", volume->name);
		
		// Attempt to get some stats for the mounted volume and display them.
		// This isn't exactly important, so just skip it if it doesn't work for
		// some reason.  Talk about a useless feature :)
		result = volume_stats(volume, &stats);
		if(result == 0) {
			
			ui_print("\n");
			ui_print("    Size       : %12llu bytes\n", stats.f_bsize * stats.f_blocks);
			ui_print("    Free Space : %12llu bytes\n", stats.f_bsize * stats.f_bfree);
			ui_print("    Available  : %12llu bytes\n", stats.f_bsize * stats.f_bavail);
		}		
	}
	
	else LOGE("cmd_mount_volume: Unable to mount %s. EC = %d\n", volume->name, result);
}

//-----------------------------------------------------------------------------
// cmd_remove_busybox
//
// Removes busybox from the system.  Assumes that the busybox was either installed
// by this recovery, or at least had all the symlinks installed into /system/xbin
//
// Arguments:
//
//	NONE

void cmd_remove_busybox(void)
{
	const Volume*		dest;				// Destination VOLUME
	int 				destmounted = 0;	// Flag if volume was mounted
	int 				result;				// Result from function call
	
	ui_clear_text();						// Clear the UI
	
	// Get a reference to the SYSTEM volume
	dest = get_volume("SYSTEM");
	if(dest == NULL) { LOGE("cmd_remove_busybox: Cannot locate SYSTEM volume entry\n"); return; }
	
	// Mount the SYSTEM volume
	result = mount_volume(dest, &destmounted);
	if(result != 0) { LOGE("cmd_remove_busybox: Cannot mount SYSTEM, EC = %d\n", result); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Removing BusyBox from SYSTEM ...\n\n");
	
	// Remove BUSYBOX using the script built into the ramdisk
	result = WEXITSTATUS(exec("sh /sbin/scripts/remove-busybox.sh"));
	if(result != 0) LOGE("Unable to remove BusyBox. EC = %d\n", result);
	else { ui_print("> BusyBox removed successfully.\n\n"); }
	
	// If the SYSTEM volume was mounted by this function, unmount it before exit
	if(destmounted != 0) unmount_volume(dest, NULL);
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_remove_su
//
// Removes the su binary from the system volume
//
// Arguments:
//
//	NONE

void cmd_remove_su(void)
{
	const Volume*		dest;				// Destination VOLUME
	int 				destmounted = 0;	// Flag if volume was mounted
	int 				result;				// Result from function call
	
	ui_clear_text();						// Clear the UI
	
	// Get a reference to the SYSTEM volume
	dest = get_volume("SYSTEM");
	if(dest == NULL) { LOGE("cmd_remove_su: Cannot locate SYSTEM volume entry\n"); return; }
	
	// Mount the SYSTEM volume
	result = mount_volume(dest, &destmounted);
	if(result != 0) { LOGE("cmd_remove_su: Cannot mount SYSTEM, EC = %d\n", result); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Removing su from SYSTEM ...\n\n");
	
	// Remove SU using the script built into the ramdisk
	result = WEXITSTATUS(exec("sh /sbin/scripts/remove-su.sh"));
	if(result != 0) LOGE("Unable to remove su binary. EC = %d\n", result);
	else { ui_print("> su binary removed successfully.\n\n"); }
	
	// If the SYSTEM volume was mounted by this function, unmount it before exit
	if(destmounted != 0) unmount_volume(dest, NULL);
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// cmd_restore_divine_method (private)
//
// Attempts to determine what type of image file is being used
//
// Arguments:
//
//	srcpath			- Source image file path
//	method			- On success, contains the type of the image file

static int cmd_restore_divine_method(const char* srcpath, cmd_backup_method* method)
{
	unsigned char*		buffer;				// Data buffer
	gzFile 				source;				// Source file
	int 				cb;					// Bytes read from input file
	int 				result = -1;		// Return value from this function
	
	if(!srcpath) return EINVAL;				// Invalid [in] pointer
	if(!method) return EINVAL;				// Invalid [out] pointer

	*method = dump;							// Assume it's a DUMP

	// Allocate and initialize a buffer to hold the first 8KB of the file data
	buffer = (unsigned char*)malloc(8192);
	if(buffer) memset(buffer, 0, 8192);
	else return ENOMEM;

	// The input file may be compressed, so open it with gzopen()
	source = gzopen(srcpath, "rb");
	if(source != Z_NULL) { 
		
		// Read the first 8KB of data from the source file and have a look at it
		cb = gzread(source, buffer, 8192);
		if(cb > 0) {
			
			// EXT4 SPARSE is the easiest of them all, since it has a magic number at a
			// predictable location in the file (this even applies to my custom "szimg" files)
			if(result == -1) {
				
				sparse_header_t* sparse_header = (sparse_header_t*)buffer;
				if(sparse_header->magic == SPARSE_HEADER_MAGIC) { *method = ext4_sparse; result = 0; }
			}
			
			// RFS files can be determined by locating the MSDOS Master Boot Record
			// Note: Do this *before* EXT4 images as they have the same stupid magic number
			// in the stupid same place
			if(result == -1) {
				
				struct pc_boot_record* boot_record = (struct pc_boot_record*)buffer;
				if(boot_record->mbr_sig == PC_BIOS_BOOT_SIG) { *method = dump; result = 0; }	
			}
		
			// EXT4 files can be determined by locating the EXT4 superblock and checking
			// the magic number (superblock is 1K into the file, or 0x400 bytes)
			if(result == -1) {
				
				struct ext4_super_block* super_block = (struct ext4_super_block*)(buffer + 0x0400);
				if(super_block->s_magic == EXT4_SUPER_MAGIC) { *method = ext4; result = 0; }
			}
			
			// YAFFS2 images have to examined in a bit more detail. The first chunk of data should be a
			// directory with a parent object_id of 1 and a NULL name.  The second chunk of data should 
			// have a parent object_id of 1 and a non-NULL name.
			if(result == -1) {
				
				yaffs_ObjectHeader* chunk_0 = (yaffs_ObjectHeader*)buffer;
				yaffs_ObjectHeader* chunk_1 = (yaffs_ObjectHeader*)(buffer + YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE);
				
				if((chunk_0->type == YAFFS_OBJECT_TYPE_DIRECTORY) && (chunk_0->parentObjectId == 1) && (chunk_0->name[0] == '\0')) {
					if((chunk_1->parentObjectId == 1) && (chunk_1->name[0] != '\0')) { *method = yaffs2; result = 0; }
				}
			}
		}
		
		else if(cb == -1) { LOGE("cmd_restore_divine_method: Unable to read from source file %s. EC = %d\n", srcpath, errno); }

		gzclose(source);					// Close the input file
	}
	
	else { LOGE("cmd_restore_divine_method: Unable to open source file %s. EC = %d\n", srcpath, errno); }

	free(buffer);							// Release allocated memory
	return result;							// Done!
}

//-----------------------------------------------------------------------------
// cmd_restore_volume
//
// Restores an image file to the specified volume
//
// Arguments:
//
//	srcpath		- Source path
//	destvol		- Destination volume

void cmd_restore_volume(const char* srcpath, const Volume* destvol)
{
	const Volume*		srcvol;				// Source volume	
	int 				srcmounted = 0;		// Flag if source volume was mounted
	cmd_backup_method 	method;				// Restore method (source file type)
	int 				destunmounted = 0;	// Flag if dest volume was unmounted
	ui_callbacks 		callbacks;			// UI callbacks for backup functions
	int 				result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Determine what the source volume is and make sure it's not the same as the destination
	srcvol = get_volume_for_path(srcpath);
	if(srcvol == NULL) { LOGE("cmd_restore_volume: Cannot locate volume for path %s\n", srcpath); return; }
	if(srcvol == destvol) { LOGE("cmd_restore_volume: Destination volume cannot be the same as the source volume\n"); return; }
	
	// Mount the source volume
	result = mount_volume(srcvol, &srcmounted);
	if(result != 0) { LOGE("cmd_restore_volume: Cannot mount source volume %s\n", srcvol->name); return; }
	
	// Automatically determine what type of image file this is from direct binary evidence
	result = cmd_restore_divine_method(srcpath, &method);
	if(result != 0) {
		
		LOGE("Unable to determine the image type of source file %s\n", srcpath);
		if(srcmounted) unmount_volume(srcvol, NULL);
		return;
	}
	
	// YAFFS2 restores of an entire volume requires a different method
	if(method == yaffs2) { 
		
		if(srcmounted) unmount_volume(srcvol, NULL);
		cmd_restore_volume_yaffs2(srcpath, destvol); 
		return;
	}

	// Unmount the destination volume if necessary
	result = unmount_volume(destvol, &destunmounted);
	if(result == 0) {
	
		// Output some generally useless information about the restore operation
		ui_print("Restoring volume %s...\n\n", destvol->name);
		ui_print("Source File : %s\n", srcpath);
		ui_print("Method      : %s\n", g_backup_method_info[method].name);
		ui_print("\n");
		
		// Initialize UI callbacks for operations that support them
		init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);

		// Execute the backup operation
		switch(method) {
			
			// EXT4 Image
			case ext4: 
				ui_show_progress(1.0, 0);
				restore_ext4_ui(srcpath, destvol, &callbacks);
				break;
			
			// EXT4 Sparse Image
			case ext4_sparse: 
				ui_show_progress(1.0, 0);
				restore_ext4_sparse_ui(srcpath, destvol, &callbacks); 
				break;
			
			// DUMP
			case dump:
				ui_show_progress(1.0, 0);
				restore_dump_ui(srcpath, destvol, &callbacks);
				break;
				
			default: LOGE("cmd_restore_volume: Unknown restore method code\n");
		}
		
		ui_print("> Volume %s restored successfully.\n", destvol->name);
		ui_reset_progress();
		
		// Mount the destination volume if it was unmounted by this function
		if(destunmounted != 0) mount_volume(destvol, NULL);
	}
	
	// Unmount the source volume if it was mounted by this function
	if(srcmounted != 0) unmount_volume(srcvol, NULL);
}

//-----------------------------------------------------------------------------
// cmd_restore_volume_yaffs2 (private)
//
// Restores an image file to the specified volume
//
// Arguments:
//
//	srcpath		- Source path
//	destvol		- Destination volume

static void cmd_restore_volume_yaffs2(const char* srcpath, const Volume* destvol)
{
	const Volume*	srcvol;				// Source volume
	int 			srcmounted = 0;		// Flag if source volume was mounted
	int 			destmounted = 0;	// Flag if dest volume was mounted
	ui_callbacks 	callbacks;			// UI callbacks for backup functions
	int 			result;				// Result from function call

	ui_clear_text();					// Clear the UI
	
	// Determine what the source volume is and make sure it's not the same as the destination
	srcvol = get_volume_for_path(srcpath);
	if(srcvol == NULL) { LOGE("cmd_restore_volume_yaffs2: Cannot locate volume for path %s\n", srcpath); return; }
	if(srcvol == destvol) { LOGE("cmd_restore_volume_yaffs2: Destination volume cannot be the same as the source volume\n"); return; }
	
	// Mount the source volume
	result = mount_volume(srcvol, &srcmounted);
	if(result != 0) { LOGE("cmd_restore_volume_yaffs2: Cannot mount source volume %s\n", srcvol->name); return; }
	
	// The destination volume must be wiped before the restore
	result = wipe_volume(destvol);
	if(result == 0) {
		
		// YAFFS2 restore writes files to a mounted filesystem, so mount it
		result = mount_volume(destvol, &destmounted);
		if(result == 0) {
			
			// Output some generally useless information about the restore operation
			ui_print("Restoring volume %s...\n\n", destvol->name);
			ui_print("Source File : %s\n", srcpath);
			ui_print("Method      : %s\n", g_backup_method_info[yaffs2].name);
			ui_print("\n");
			
			// Initialize UI callbacks and the UI progress bar
			init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);
			ui_show_progress(1.0, 0);
			
			// Restore the volume data from the YAFFS2 image file ..
			restore_yaffs2_ui(srcpath, destvol->mount_point, &callbacks);

			ui_print("> Volume %s restored successfully.\n", destvol->name);
			ui_reset_progress();
			
			// If the destination volume was mounted by this function, unmount it
			if(destmounted) unmount_volume(destvol, NULL);
		}
		
		else { LOGE("cmd_restore_volume_yaffs2: Cannot mount destination volume %s. EC = %d\n", destvol->name, result); }
	}
	
	else { LOGE("cmd_restore_volume_yaffs2: Cannot wipe destination volume %s. EC = %d\n", destvol->name, result); }

	// Unmount the source volume if it was mounted by this function
	if(srcmounted != 0) unmount_volume(srcvol, NULL);
}

//-----------------------------------------------------------------------------
// cmd_show_usage
//
// Displays the recovery key usage information
//
// Arguments:
//
//	NONE

void cmd_show_usage(void)
{
	ui_clear_text();
	ui_print("GALAXY S SCH-I500 RECOVERY NAVIGATION\n");
	ui_print("=====================================\n\n");
	ui_print("VOLUME UP   : Move menu item selection bar up\n");
	ui_print("VOLUME DOWN : Move menu item selection bar down\n");
	ui_print("MENU        : Go back to previous menu\n");
	ui_print("HOME        : Go back to main menu\n");
	ui_print("BACK        : Select highlighted menu item\n");
	ui_print("\n\n\n\n\n");
}

//-----------------------------------------------------------------------------
// cmd_unmount_volume
//
// Unmounts the specified volume
//
// Arguments:
//
//	volume		- Volume to be unmounted

void cmd_unmount_volume(const Volume* volume)
{
	int 		unmounted = 0;			// Flag if volume was unmounted
	int 		result;					// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Indicate the operation to the user
	ui_print("Unmounting volume %s ...\n\n[%s]\n\n", volume->name, volume->device);
	
	// Attempt to unmount the volume
	result = unmount_volume(volume, &unmounted);
	if(result == 0) {
		
		if(unmounted) ui_print("> Volume %s unmounted successfully.\n", volume->name);
		else ui_print("> Volume %s was not mounted.\n", volume->name);
	}
	
	else LOGE("cmd_unmount_volume: Unable to unmount %s. EC = %d\n", volume->name, result);
}

//-----------------------------------------------------------------------------
// cmd_wipe_battery_stats
//
// Wipes the BATTERYSTATS.BIN file from the device
//
// Arguments:
//
//	NONE

void cmd_wipe_battery_stats(void)
{
	const Volume*	volume;				// CACHE volume pointer
	int 			mounted = 0;		// Flag if volume was mounted
	char 			temp[256];			// Temporary string space
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Get a reference to the DATA volume
	volume = get_volume("DATA");
	if(volume == NULL) { LOGE("cmd_wipe_battery_stats: Unable to locate DATA volume entry in fstab.\n"); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Wiping Battery statistics ...\n\n");
	
	// Unmount the target volume
	result = mount_volume(volume, &mounted);
	if(result != 0) { LOGE("cmd_wipe_battery_stats: Cannot mount volume %s\n", volume->name); return; }
	
	// Generate the path to batterystats.bin and remove it
	sprintf(temp, "%s/system/batterystats.bin", volume->mount_point);
	result = remove(temp);
	if(result != 0) LOGE("cmd_wipe_battery_stats: Unable to remove file %s. EC = %d\n", temp, result);
	else ui_print("> Battery statistics wiped successfully.\n");

	// If the cache volume was unmounted on the way in, unmount it
	if(mounted) unmount_volume(volume, NULL);
	
	ui_reset_progress();					// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------
// cmd_wipe_cache
//
// Wipes the CACHE volume using whatever file system it happens to already be
//
// Arguments:
//
//	NONE

void cmd_wipe_cache(void)
{
	const Volume*	volume;				// CACHE volume pointer
	int 			unmounted = 0;		// Flag if volume was unmounted
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Get a reference to the CACHE volume
	volume = get_volume("CACHE");
	if(volume == NULL) { LOGE("cmd_wipe_cache: Unable to locate CACHE volume entry in fstab.\n"); return; }
	
	// This is a high-level recovery operation that doesn't provide a lot of feedback to 
	// the user by the way of ui_print() statements.  Just report the basic facts ...	
	ui_show_indeterminate_progress();
	ui_print("Wiping Cache ...\n\n");
	
	// Unmount the target volume
	result = unmount_volume(volume, &unmounted);
	if(result != 0) { LOGE("cmd_wipe_cache: Cannot unmount volume %s\n", volume->name); return; }
	
	// Use wipe_volume() to format the volume with its current filesystem type
	result = wipe_volume(volume);
	if(result != 0) LOGE("cmd_wipe_cache: Unable to wipe volume %s. EC = %d\n", volume->name, result);
	else ui_print("> Cache wiped successfully.\n");

	// If the cache volume was mounted on the way in, remount it
	if(unmounted) {
		
		result = mount_volume(volume, NULL);
		if(result != 0) LOGW("cmd_wipe_cache: Unable to remount volume %s. EC = %d\n", volume->name, result);
	}
	
	ui_reset_progress();					// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------
// cmd_wipe_dalvik_cache
//
// Wipes the Dalvik cache
//
// Arguments:
//
//	NONE

void cmd_wipe_dalvik_cache(void)
{
	const Volume*	volume;				// DATA volume pointer
	int 			mounted = 0;		// Flag if volume was mounted
	int 			result;				// Result from function call
	
	ui_clear_text();					// Clear the UI
	
	// Get a reference to the DATA volume
	volume = get_volume("DATA");
	if(volume == NULL) { LOGE("cmd_wipe_dalvik_cache: Unable to locate DATA volume entry in fstab.\n"); return; }
	
	ui_show_indeterminate_progress();
	ui_print("Wiping Dalvik Cache ...\n\n");
	
	// Mount the target volume
	result = mount_volume(volume, &mounted);
	if(result != 0) { LOGE("cmd_wipe_dalvik_cache: Cannot mount volume %s\n", volume->name); return; }
	
	// Use dirUnlinkHierarchy() to recursively delete the Dalvik Cache folder ...
	result = dirUnlinkHierarchy("/data/dalvik-cache");
	if(result != 0) LOGE("cmd_wipe_dalvik_cache: Unable to remove directory /data/dalvik-cache. EC = %d\n", errno);
	else ui_print("> Dalvik Cache wiped successfully.\n");

	// If the data volume was mounted on the way in, unmount it
	if(mounted) {
		
		result = unmount_volume(volume, NULL);
		if(result != 0) LOGW("cmd_wipe_dalvik_cache: Unable to unmount volume %s. EC = %d\n", volume->name, result);
	}
	
	ui_reset_progress();					// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------
// cmd_wipe_device
//
// Wipes all volumes marked as 'wipe' in the FSTAB file with the same filesystem
// that is already in place on them
//
// Arguments:
//
//	NONE

void cmd_wipe_device(void)
{
	const Volume*	iterator;			// FSTAB volume iterator
	int 			mounted = 0;		// Flag if volume was mounted
	struct statfs	stats;				// Volume statistics	
	char 			temp[256];			// Temporary string space
	int 			result;				// Result from function call
	int 			failed = 0;			// Flag if wipe operation failed
	
	ui_clear_text();					// Clear the UI
	
	// This is a high-level recovery operation that doesn't provide a lot of feedback to 
	// the user by the way of ui_print() statements.  Just report the basic facts ...	
	ui_show_indeterminate_progress();
	ui_print("Wiping device ...\n\n");

	// Iterate over all of the FSTAB volumes and format each one marked as WIPE
	iterator = foreach_volume(NULL);
	while(iterator != NULL) {
		
		if(*iterator->wipe == '1') {
			
			// Mount the target volume (wipe_volume needs it mounted to figure out the file system!!!)
			result = mount_volume(iterator, &mounted);
			if(result != 0) { LOGW("cmd_wipe_device: Cannot mount volume %s\n", iterator->name); failed = 1; continue; }
			
			// Use wipe_volume() to format the volume with its current filesystem type
			result = wipe_volume(iterator);
			if(result != 0) {
				
				LOGW("cmd_wipe_device: Unable to wipe volume %s. EC = %d\n", iterator->name, result);
				failed = 1;
			}
			
			// If the volume was mounted before we unmounted it, try to remount it
			if(mounted) unmount_volume(iterator, NULL);
		}
		
		iterator = foreach_volume(iterator);		// Move to next volume
	}
	
	// Once the main volume wipes are complete the SDCARD needs to be cleaned off
	iterator = get_volume("SDCARD");
	if(iterator != NULL) {
		
		result = mount_volume(iterator, &mounted);
		if(result == 0) {
			
			// Remove the /.android_secure directory
			sprintf(temp, "%s/.android_secure", iterator->mount_point);
			dirUnlinkHierarchy(temp);
			
			// Remove the /Android directory
			sprintf(temp, "%s/Android", iterator->mount_point);
			dirUnlinkHierarchy(temp);

			if(mounted) unmount_volume(iterator, NULL);			// Unmount if we mounted it
		}
		
		else { LOGW("cmd_wipe_device: Cannot mount volume SDCARD\n"); failed = 1; }
	}
	
	else { LOGW("cmd_wipe_device:  Unable to locate SDCARD volume entry in fstab\n"); failed = 1; }
	
	// If any of the volumes failed to format, the wipe is incomplete and needs to be reported	
	if(failed) ui_print("> Data partially wiped. Some user data may still be present on the device.\n");
	else ui_print("> Data wiped successfully.\n");
	
	ui_reset_progress();					// Reset/hide the progress bar
}

//-----------------------------------------------------------------------------