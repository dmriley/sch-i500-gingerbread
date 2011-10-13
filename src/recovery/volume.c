//-----------------------------------------------------------------------------
// volume.c
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

#include "volume.h"						// Include module declarations

#include <stdio.h>						// Include STDIO declarations
#include <ctype.h>						// Include CTYPE declarations
#include <fcntl.h>						// Include FCNTL declarations
#include <unistd.h>						// Include UNISTD declarations
#include <errno.h>						// Include ERRNO declarations
#include <sys/mount.h>					// Include MOUNT declarations
#include <sys/stat.h>					// Include STAT declarations
#include <sys/types.h>					// Include TYPES declarations
#include <sys/wait.h>					// Include WAIT declarations
#include <sys/ioctl.h>					// Include IOCTL declarations
#include <zlib.h>						// Include ZLIB declarations
#include "exec.h"						// Include EXEC declarations
#include "mtdutils/mounts.h"			// Include MTDUTIL declarations
#include "ext4_utils/make_ext4fs.h"		// Include EXT4 declarations

//-----------------------------------------------------------------------------
// PRIVATE DATA TYPES
//-----------------------------------------------------------------------------

// mount_options - Stucture that defines mounting options.  Taken from AOSP mount.c
// Based on AOSP (system/core/toolbox/mount.c)
typedef struct {
	
	const char 		str[8];
	unsigned long 	mask;
	unsigned long 	set;
	unsigned long 	noset;
	
} mount_options;

// extra_mount_options - Structure that defines non-standard mount options.  From AOSP mount.c
// From AOSP (system/core/toolbox/mount.c); Unmodified
typedef struct {
	
	char*	str;
	char*	end;
	int 	used_size;
	int 	alloc_size;
	
} extra_mount_options;

//-----------------------------------------------------------------------------
// PRIVATE CONSTANTS / MACROS
//-----------------------------------------------------------------------------

// MS_TYPE - Combination of mounting flags to simplify options table
#define MS_TYPE			(MS_REMOUNT|MS_BIND|MS_MOVE)

// ARRAY_SIZE - determines size of a C array
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

// BLKGETSIZE64
//
// From <linux/fs.h>, that header has conflicts with <sys/mount.h>
#define BLKGETSIZE64 _IOR(0x12,114,size_t)  

//-----------------------------------------------------------------------------
// PRIVATE GLOBAL VARIABLES
//-----------------------------------------------------------------------------

static int 			g_num_volumes = 0;		// Number of fstab volumes
static Volume* 		g_volumes = NULL;		// Array of fstab volumes

// Table of string mounting options and their bitmask equivalents
// From AOSP (system/core/toolbox/mount.c); Unmodified
static const mount_options g_mount_options[] = {
	
	/* name			mask			set				noset		*/
	{ "async",		MS_SYNCHRONOUS,	0,				MS_SYNCHRONOUS	},
	{ "atime",		MS_NOATIME,		0,				MS_NOATIME		},
	{ "bind",		MS_TYPE,		MS_BIND,		0,				},
	{ "dev",		MS_NODEV,		0,				MS_NODEV		},
	{ "diratime",	MS_NODIRATIME,	0,				MS_NODIRATIME	},
	{ "dirsync",	MS_DIRSYNC,		MS_DIRSYNC,		0				},
	{ "exec",		MS_NOEXEC,		0,				MS_NOEXEC		},
	{ "move",		MS_TYPE,		MS_MOVE,		0				},
	{ "recurse",	MS_REC,			MS_REC,			0				},
	{ "remount",	MS_TYPE,		MS_REMOUNT,		0				},
	{ "ro",			MS_RDONLY,		MS_RDONLY,		0				},
	{ "rw",			MS_RDONLY,		0,				MS_RDONLY		},
	{ "suid",		MS_NOSUID,		0,				MS_NOSUID		},
	{ "sync",		MS_SYNCHRONOUS,	MS_SYNCHRONOUS,	0				},
	{ "verbose",	MS_VERBOSE,		MS_VERBOSE,		0				},
};

extern struct fs_info info;				// <--- For make_ext4fs

//-----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Appends a new option to an extra_opts structure
void add_extra_option(extra_mount_options* extra, char *s);

// Releases and reinitializes an extra_opts structure
extra_mount_options* free_extra_options(extra_mount_options* extra);

// Initializes an extra_opts structure
void init_extra_options(extra_mount_options* extra);

// Parses string-based mounting options into mounting flags
unsigned long parse_mount_options(const char* arglist, extra_mount_options* extra);

//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// add_extra_option (PRIVATE)
//
// Adds a non-standard option to the string data for mount()
// From AOSP (system/core/toolbox/mount.c); Unmodified

void add_extra_option(extra_mount_options* extra, char *s)
{
	int len = strlen(s);
	int newlen = extra->used_size + len;

	if (extra->str)
	       len++;			/* +1 for ',' */

	if (newlen >= extra->alloc_size) {
		char *new;

		new = realloc(extra->str, newlen + 1);	/* +1 for NUL */
		if (!new)
			return;

		extra->str = new;
		extra->end = extra->str + extra->used_size;
		extra->alloc_size = newlen;
	}

	if (extra->used_size) {
		*extra->end = ',';
		extra->end++;
	}
	strcpy(extra->end, s);
	extra->used_size += len;
}

//-----------------------------------------------------------------------------
// foreach_volume
//
// Iterates over the volume table.  Pass NULL for the initial call, and then
// pass the previously returned Volume* for successive calls.  Returns NULL
// when the end of the table has been reached
//
// Arguments:
//
//	previous	- Pointer previously returned, or NULL to start at the beginning

const Volume* foreach_volume(const Volume* previous)
{
    int 		index = 0;					// Loop index variable
	
	if(g_volumes == NULL) return NULL;		// Table has not been loaded
	if(g_num_volumes <= 0) return NULL;		// Table has not been loaded

	// If NULL has been provided as previous, return the first entry
	if(previous == NULL) return &g_volumes[0];

	// In retrospect, I should have added a NULL entry to the end of the table
	// to make this look a little less ridiculous ... it works.
    for (index = 0; index < g_num_volumes; index++) {
		
		if(previous == &g_volumes[index]) {
			
			if((index + 1) < g_num_volumes) return &g_volumes[index + 1];
			else break;
		}
	}
	
    return NULL;
}

//-----------------------------------------------------------------------------
// format_volume
//
// Formats the specified volume with the specified filesystem
//
// Arguments:
//
//	volume		- Volume to be formatted
//	fs			- Optional file system name; NULL for default from FSTAB

int format_volume(const Volume* volume, const char* fs) 
{
	int					unmounted = 0;		// Flag if volume was unmounted
	unsigned long long	volsize = 0;		// Size of an RFS target volume
	int					result;				// Result from function call
	
	// Volume must be non-NULL and not a RAMDISK
	if(volume == NULL) return EINVAL;
	if(strcasecmp(volume->fs_type, "ramdisk") == 0) return EINVAL;
	
	// Ensure that the volume isn't mounted
	result = unmount_volume(volume, &unmounted);
	if(result != 0) return result;
	
	// If no specific file system type was provided, use the default
	if(fs == NULL) fs = volume->fs_type;
    
    // >> FORMAT EXT4
    if (strcasecmp(fs, "ext4") == 0) {
		
        reset_ext4fs_info();
        result = make_ext4fs(volume->device, NULL, NULL, 0, 0, 0);
    }
    
    // >> FORMAT RFS
    else if(strcasecmp(fs, "rfs") == 0) {
		
		// RFS formatting is accomplished by an external application (fat.format).
		// If the volume is greater than 1GiB in size, a special flag (-F 32) must 
		// be passed to that application to properly format the device, otherwise
		// the format will silently fail and that's not a good thing		
		result = volume_size(volume, &volsize);
		if(result == 0) {
			
			if(volsize < (1024 * 1024 * 1024)) result = WEXITSTATUS(exec("fat.format %s", volume->device));
			else result = WEXITSTATUS(exec("fat.format -F 32 %s", volume->device));
		}
	}
	
	else if(strcasecmp(fs, "vfat") == 0) {
		
		// VFAT formatting is accomplished by an external application (mkfs.vfat) ...
		result = WEXITSTATUS(exec("mkfs.vfat %s", volume->device));
	}
    
	// UNKNOWN/UNSUPPORTED FILE SYSTEMS
	else result = EINVAL;
	
	// If the volume was unmounted, try to re-mount it regardless of success
	if(unmounted) mount_volume(volume, NULL);
	
    return result;						// Return result from format operation
}

//-----------------------------------------------------------------------------
// free_extra_options (PRIVATE)
//
// Releases memory allocated by an extra_mount_options structure and reinitializes it
//
// Arguments:
//
//	extra		- extra_mount_options structure to be released and reinitialized

extra_mount_options* free_extra_options(extra_mount_options* extra)
{
	// TODO: This still seg faults for some reason; just let
	// the memory leak for now. examine add_extra_option() above more closely
	//if(extra->str) free(extra->str);
	init_extra_options(extra);
	return extra;
}

//-----------------------------------------------------------------------------
// get_volume
//
// Returns a pointer to a Volume structure for the specified volume name
//
// Arguments:
//
//	name	- Name to locate in the global volume table
//
// Return Values:
//
//	NULL	- Name could not be mapped to an FSTAB volume entry

const Volume* get_volume(const char* name) 
{
    int 		index = 0;				// Loop index variable
	Volume*		result = NULL;			// Located Volume structure pointer
	
    for (index = 0; index < g_num_volumes; index++) {
		
        result = &g_volumes[index];
		if(strcasecmp(name, result->name) == 0) return result;
    }
    
    return NULL;
}

//-----------------------------------------------------------------------------
// get_volume_for_path
//
// Returns a pointer to a Volume structure for the specified path
//
// Arguments:
//
//	path	- Path to locate in the global volume table
//
// Return Values:
//
//	NULL	- Could not map PATH to a volume in the fstab file

const Volume* get_volume_for_path(const char* path) 
{
    int 		index = 0;				// Loop index variable
	Volume*		result = NULL;			// Located Volume structure pointer
	
	// Iterate over the global device volume table and look for the first
	// entry that has the proper root for the specified path string
    for (index = 0; index < g_num_volumes; index++) {
		
		result = &g_volumes[index];
		
		int len = strlen(result->mount_point);
		if (strncmp(path, result->mount_point, len) == 0 && (path[len] == '\0' || path[len] == '/')) return result;
    }
    
    return NULL;						// No matching volume was located
}

//-----------------------------------------------------------------------------
// init_extra_options (PRIVATE)
//
// Initializes an extra_mount_options structure
//
// Arguments:
//
//	extra		- extra_opts structure to be initialized

void init_extra_options(extra_mount_options* extra)
{
	memset(extra, 0, sizeof(extra_mount_options));
}

//-----------------------------------------------------------------------------
// mount_volume
//
// Attempts to mount the specified volume
//
// Arguments:
//
//	volume		- Volume to be mounted (if unmounted)
//	mounted		- Optional flag to receive previously unmounted status

int mount_volume(const Volume* volume, int* mounted) 
{
	int						result;			// Result from function call
	const MountedVolume*	pmv = NULL;		// Pointer to MountedVolume info
	extra_mount_options		extra;			// Non-standard mounting options
	unsigned long 			mntflags;		// Mounting flags
	
	if(mounted) *mounted = 0;				// Initialize [OUT] variable
    if(volume == NULL) return EINVAL; 		// Should never happen

	// RAMDISK volumes are always mounted ...
	if(strcasecmp(volume->fs_type, "ramdisk") == 0) return 0;

	// MTDUTILS: Load the mounted volumes table
    result = scan_mounted_volumes();
    if (result < 0) return result;

	// MTDUTILS: check to see if the volume is already mounted
    pmv = find_mounted_volume_by_mount_point(volume->mount_point);
    
	// MTDUTILS: if the volume is mounted, we're done
	if (pmv) return 0; 
	
	// Create the mount point with default permissions in case it doesn't already exist
	mkdir(volume->mount_point, 0755);
	
	// Generate the standard and extra mounting options for primary fstype
	init_extra_options(&extra);
	mntflags = parse_mount_options(volume->fs_options, &extra);
	
	// Attempt to mount the volume using the primary file system
	result = mount(volume->device, volume->mount_point, volume->fs_type, mntflags, extra.str);
	if((result != 0) && (volume->fs_type2)) {
		
		// Generate the standard and extra mounting options for secondary fstype
		init_extra_options(free_extra_options(&extra));
		mntflags = parse_mount_options(volume->fs_options2, &extra);
		
		// Attempt to mount the volume using the secondary file system
		result = mount(volume->device, volume->mount_point, volume->fs_type2, mntflags, extra.str);
	}
	
	free_extra_options(&extra);		// Release extra options allocated memory
	
	if((result == 0) && (mounted)) *mounted = -1;		// Mounted the volume
	return result;										// Return result from mount()
}

//-----------------------------------------------------------------------------
// parse_mount_options (PRIVATE)
//
// Parses string-based mounting options into standard flags and nonstandard data
// Based on AOSP (system/core/toolbox/mount.c)
//
// Arguments:
//
//	arglist		- Comma-delimited list of mounting options
//	extra		- extra_mount_options structure to receive extra mounting options

unsigned long parse_mount_options(const char* arglist, extra_mount_options* extra)
{
	char*				args;			// Non-const copy of arglist
	char*				arg;			// Individual argument from the list
	char*				opt;			// Copy of pointer to arg for extra
	unsigned long		mntflags = 0;	// Generated mounting flags
	unsigned int		index;			// Loop index variable
	int					fno;			// Flag indicating a "no" option
	int					result;			// Result from string comparison
	
	// If there is no input string, just return the default flags
	if(arglist == NULL) return mntflags;
	
	// strsep modifies the original string, make a copy of it first
	args = malloc(sizeof(char) * (strlen(arglist) + 1));
	strcpy(args, arglist);
    
	// Break up the argument string into individual arguments
	while ((arg = strsep(&args, ",")) != NULL) {
		
		opt = arg;				// Save the original pointer
		
		// All options can be preceeded with "no"
		fno = ((arg[0] == 'n') && (arg[1] == 'o'));
		if(fno) arg += 2;

		// Iterate over the options table looking for this option
		for (index = 0, result = 1; index < ARRAY_SIZE(g_mount_options); index++) {
			
			result = strcasecmp(arg, g_mount_options[index].str);
			if (result == 0) {
				
				// Found the option; apply the set or noset to the flags
				mntflags &= ~g_mount_options[index].mask;
				if (fno) mntflags |= g_mount_options[index].noset;
				else mntflags |= g_mount_options[index].set;
			}

			if(result <= 0) break;		// Options are alphabetical
		}

		if (result != 0 && arg[0]) add_extra_option(extra, opt);
	}

	free(args);					// Release allocated memory
	return mntflags;			// Return generated mount flags
}

//-----------------------------------------------------------------------------
// restore_volume
//
// Restores a previously generated EXT4 file system image to the specified
// volume.  The volume will be formatted as EXT4 prior to the restore regardless
// of what existing file system is in place 
//
// Arguments:
//
//	volume			- Volume to be restored
//	backup_file		- Source backup file; cannot be located on destination volume
//
// Return Values:
//
//	0				- Success
//	EINVAL			- An invalid (NULL) argument was passed
//	ENOENT			- Cannot map backup_file to a valid VOLUME
//	EBUSY			- backup_file maps to the VOLUME that is to be restored

int restore_volume(const Volume* volume, const char* backup_file)
{
	const Volume*		sourcevol = NULL;		// SOURCE volume
	int					srcmounted = 0;			// Flag if SOURCE was mounted
	int					destunmounted = 0;		// Flag if DESTINATION was unmounted
	gzFile				source;					// SOURCE file (GZIP)
	int					cb;						// Byte counter (source file read)
	int					dest_fd;				// DESTINATION file descriptor
	u8					buffer[4096];			// 4K data buffer
	int					result;					// Result from function call
	
	// Check arguments for NULL pointers or zero-length strings
	if((volume == NULL) || (backup_file == NULL) || (*backup_file == '\0')) return EINVAL;	
	
	// Locate the Volume information for the source path.  It must exist and cannot
	// cannot be located on the same volume as the destination volume
	sourcevol = get_volume_for_path(backup_file);
	if(sourcevol == NULL) return ENOENT;
	if(sourcevol == volume) return EBUSY;
	
	// Mount the SOURCE volume and try to open the source file
	result = mount_volume(sourcevol, &srcmounted);
	if(result != 0) return result;
	
	// Open the SOURCE file (ZLIB)
	source = gzopen(backup_file, "rb");
	if(source != Z_NULL) {
	
		//
		// TODO: There really needs to be some form of validation here, just
		// allowing a blind restore of any old image is a little reckless
		//
		
		// Unmount the DESTINATION volume
		result = unmount_volume(volume, &destunmounted);
		if(result == 0) {
			
			// Open the DESTINATION device for write access
			dest_fd = open(volume->device, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(dest_fd >= 0) {
				
				// Decompress the SOURCE image to the DESTINATION device ...
				cb = gzread(source, &buffer, 4096);
				while(cb > 0) {
					
					write(dest_fd, &buffer, cb);
					cb = gzread(source, &buffer, 4096);
				}

				close(dest_fd);				// Close the DESTINATION device
			}
			
			else result = errno;			// <-- Could not open DESTINATION device

			// If the DESTINATION volume was unmounted by us, remount it
			if(destunmounted) mount_volume(volume, NULL);
		}
		
		gzclose(source);					// Close the SOURCE file
	}
	
	else result = errno;					// <-- Could not open SOURCE device
	
	// Unmount the SOURCE volume if it wasn't already mounted on entry
	if(srcmounted) unmount_volume(sourcevol, NULL);
	
	return result;
}

//-----------------------------------------------------------------------------
// unmount_volume
//
// Attempts to unmount the specified volume
//
// Arguments:
//
//	volume		- Volume to be unmounted (if mounted)
//	unmounted	- Option pointer to receive previously mounted flag

int unmount_volume(const Volume* volume, int* unmounted) 
{
	int						result;			// Result from function call
	const MountedVolume*	pmv = NULL;		// Pointer to MountedVolume info
	
	if(unmounted) *unmounted = 0;			// Initialize [OUT] variable
    if(volume == NULL) return EINVAL; 		// Should never happen

	// RAMDISK volumes cannot be unmounted
	if(strcasecmp(volume->fs_type, "ramdisk") == 0) return EINVAL;
	
	// MTDUTILS: Load the mounted volumes table
    result = scan_mounted_volumes();
    if (result < 0) return result;

	// MTDUTILS: check to see if the volume is already mounted
    pmv = find_mounted_volume_by_mount_point(volume->mount_point);
    
	// MTDUTILS: if the volume isn't mounted, we're done
	if (pmv == NULL) return 0;
	
	sync();			// <--- Just in case; don't think this is useful here

	// MTDUTILS: Unmount the volume and set the previously mounted flag
	result = unmount_mounted_volume(pmv);
	if((result == 0) && (unmounted)) *unmounted = -1;
    
	return result;							// Return result
}

//-----------------------------------------------------------------------------
// volume_size
//
// Retrieves the size of the underlying volume device in bytes
//
// Arguments:
//
//	volume		- Target volume
//	size		- On success, contains the size of the volume in bytes

int volume_size(const Volume* volume, unsigned long long* size)
{
	int 			fd;					// Device file
	int 			result;				// Result from function call
	
	if(!volume) return EINVAL;			// NULL pointer
	if(!size) return EINVAL;			// NULL pointer

	*size = 0;							// Initialize [out] variable
	
	// Open the device read-only so we can ioctl() it
	fd = open(volume->device, O_RDONLY);
	if(fd == -1) return errno;
	
	// Issue an ioctl() to get the device size and close the device
	result = ioctl(fd, BLKGETSIZE64, size);
	close(fd);
	
	return result;						// Return result from ioctl()
}

//-----------------------------------------------------------------------------
// volume_stats
//
// Attempts to mount the specified volume and get statistical info for it
//
// Arguments:
//
//	volume		- Volume to be mounted (if unmounted)
//	pstats		- On success, receives volume statistics

int volume_stats(const Volume* volume, struct statfs* stats)
{
	int				mounted = 0;		// Flag if volume was mounted
	int 			result;				// Result from function call
	
	if(!volume) return EINVAL;			// NULL pointer
	if(!stats) return EINVAL;			// NULL pointer
	
	// Initialize output data structure
	memset(stats, 0, sizeof(struct statfs));
	
	// Mount the volume so we can get the stats
	result = mount_volume(volume, &mounted);
	if(result != 0) return result;
	
	// After mounting the volume, get the statistical information
	result = statfs(volume->mount_point, stats);
	
	// If the volume was mounted for this function, unmount it
	if(mounted) unmount_volume(volume, NULL);
	
	return result;						// Return result from statfs()
}

//-----------------------------------------------------------------------------
// volumes_init
//
// Loads the global volume table from specified fstab file
//
// Arguments:
//
//	fstab		- Path to the fstab file (custom format)

int volumes_init(const char* fstab_file) 
{
	int		alloc = 2;					// Array allocation size tracker
    char 	line_buffer[1024];			// .fstab line buffer
    FILE*	fstab = NULL;				// .fstab file handle
    int 	index;						// Loop index variable

	// Pre-allocate enough space for 2 entries in the global array
	g_volumes = malloc(alloc * sizeof(Volume));
	memset(g_volumes, 0, alloc * sizeof(Volume));

	// Insert a static entry for /tmp, which is the ramdisk and is
	// always mounted on the device (RECOVERY SPECIFIC)
	g_volumes[0].device 		= NULL;
	g_volumes[0].mount_point 	= strdup("/tmp");
	g_volumes[0].fs_type 		= strdup("ramdisk");
	g_volumes[0].fs_options 	= strdup("rw");
	g_volumes[0].dump 			= strdup("0");
	g_volumes[0].fsck_order 	= strdup("0");
	g_volumes[0].name 			= strdup("TEMP");
	g_volumes[0].wipe 			= strdup("no");
	g_volumes[0].fs_type2 		= NULL;
	g_volumes[0].fs_options2 	= NULL;
	g_volumes[0].virtual		= 1;
	g_num_volumes = 1;
	
	// Open fstab
    fstab = fopen(fstab_file, "r");
    if (fstab == NULL) return errno;

	// Read in and process each line of the .fstab file ...
    while (fgets(line_buffer, sizeof(line_buffer) - 1, fstab)) {
    	
		for (index = 0; line_buffer[index] && isspace(line_buffer[index]); ++index);
		if (line_buffer[index] == '\0' || line_buffer[index] == '#') continue;

		char* original 		= strdup(line_buffer);
		char* device		= strtok(line_buffer + index, " \t\n");
		char* mount_point	= strtok(NULL, " \t\n");
		char* fs_type		= strtok(NULL, " \t\n");
		char* fs_options	= strtok(NULL, " \t\n");
		char* dump			= strtok(NULL, " \t\n");
		char* fsck_order	= strtok(NULL, " \t\n");
		char* name			= strtok(NULL, " \t\n");
		char* wipe			= strtok(NULL, " \t\n");
		char* fs_type2		= strtok(NULL, " \t\n");
		char* fs_options2 	= strtok(NULL, " \t\n");

		// If the .fstab line is valid, add the entry to the global volume table
		if (name && mount_point && fs_type && device && wipe) {
		
			// Ensure enough space in the global array
            while (g_num_volumes >= alloc) {
				
				// Reallocate the array and zero out the newly allocated data
				int newalloc = alloc * 2;
                g_volumes = realloc(g_volumes, newalloc * sizeof(Volume));
				memset(&g_volumes[alloc], 0, alloc * sizeof(Volume));
				alloc = newalloc;
            }
            
            // Duplicate the string data from the .fstab into the global array
            g_volumes[g_num_volumes].device 		= strdup(device);
            g_volumes[g_num_volumes].mount_point 	= strdup(mount_point);
            g_volumes[g_num_volumes].fs_type 		= strdup(fs_type);
            g_volumes[g_num_volumes].fs_options 	= strdup(fs_options);
            g_volumes[g_num_volumes].dump 			= strdup(dump);
            g_volumes[g_num_volumes].fsck_order 	= strdup(fsck_order);
            g_volumes[g_num_volumes].name 			= strdup(name);
            g_volumes[g_num_volumes].wipe 			= strdup(wipe);
            g_volumes[g_num_volumes].fs_type2 		= NULL;			// Optional
            g_volumes[g_num_volumes].fs_options2 	= NULL;			// Optional
            g_volumes[g_num_volumes].virtual 		= 0;
            
            // The optional columns in the fstab may include the string "NULL" as a 
			// placeholder.  Check for that before assigning these values ...
			
			if((fs_type2) && (strcasecmp(fs_type2, "NULL") != 0))
				g_volumes[g_num_volumes].fs_type2 = strdup(fs_type2);
			
			if((fs_options2) && (strcasecmp(fs_options2, "NULL") != 0))
				g_volumes[g_num_volumes].fs_options2 = strdup(fs_options2);
			
            g_num_volumes++;
        } 
        
        free(original);				// Release original line buffer
    }

	fclose(fstab);					// Close fstab file
	return 0;						// Completed successfully
}

//-----------------------------------------------------------------------------
// volumes_term
//
// Unloads (releases) the global volume table
//
// Arguments:
//
//	NONE

void volumes_term(void)
{
	int index = 0;					// Loop index variable
	
	if(g_volumes) {
		
		// Chase the embedded string pointers generated by strdup()
		for(index = 0; index < g_num_volumes; index++) {
			
			if(g_volumes[index].device) 		free((char*)g_volumes[index].device);
			if(g_volumes[index].mount_point) 	free((char*)g_volumes[index].mount_point);
			if(g_volumes[index].fs_type) 		free((char*)g_volumes[index].fs_type);
			if(g_volumes[index].fs_options) 	free((char*)g_volumes[index].fs_options);
			if(g_volumes[index].dump) 			free((char*)g_volumes[index].dump);
			if(g_volumes[index].fsck_order) 	free((char*)g_volumes[index].fsck_order);
			if(g_volumes[index].name) 			free((char*)g_volumes[index].name);
			if(g_volumes[index].wipe) 			free((char*)g_volumes[index].wipe);
			if(g_volumes[index].fs_type2) 		free((char*)g_volumes[index].fs_type2);
			if(g_volumes[index].fs_options2) 	free((char*)g_volumes[index].fs_options2);
		}
		
		free(g_volumes); 			// Release global array
		g_volumes = NULL;			// Reset pointer to NULL
	}

	g_num_volumes = 0;						// Reset volume count back to zero
}

//-----------------------------------------------------------------------------
// wipe_volume
//
// Formats the specified volume with its current filesystem
//
// Arguments:
//
//	volume		- Volume to be formatted

int wipe_volume(const Volume* volume) 
{
	struct statfs	stats;					// Volume stats
	int				result;					// Result from function call
	
	// Volume must be non-NULL
	if(volume == NULL) return EINVAL;
	
	// Get the volume stats to determine what the filesystem is
	result = volume_stats(volume, &stats);
	if(result != 0) return result;
	
	// Invoke format_volume with the proper filesystem type
	switch(stats.f_type) {
		
		// EXT4
		case EXT4_SUPER_MAGIC: 
			return format_volume(volume, "ext4");
			
		// RFS seems to be both 2 (everything but /system) and 4 (/system).
		// It might be a good idea to do a little more research into this
		case 2: 
		case 4:
			return format_volume(volume, "rfs");
			
		// VFAT
		case MSDOS_SUPER_MAGIC: 
			return format_volume(volume, "vfat");
	}
	
	return EINVAL;					// Invalid filesystem type code ...
}

//-----------------------------------------------------------------------------
