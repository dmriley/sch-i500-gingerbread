//-----------------------------------------------------------------------------
// MOUNTVOL
//
// MOUNTVOL is a simple boot-time utility that mounts or unmounts a volume
// listed in the ramdisk fstab file.  This makes life a little easier
// when dealing with the initialization scripts in that they need only
// specify the volume to mount and not the fs type or options
//
// Based on AOSP "Recovery" Utility
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
//
// Usage:
//
//	MOUNTVOL [-f fstab] [-R] [-u] volume [volume...]
//
//	-f			- Use a specific fstab file
//	-R			- Use /etc/recovery.fstab file
//	-u			- Unmount rather than mount the volumes
//	volume		- List of volumes to mount/unmount
//-----------------------------------------------------------------------------

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "mounts.h"				// Include MTDUTILS declarations

//-----------------------------------------------------------------------------
// DATA TYPES
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
// CONSTANTS / MACROS
//-----------------------------------------------------------------------------

// MS_TYPE - Combination of mounting flags to simplify options table
#define MS_TYPE			(MS_REMOUNT|MS_BIND|MS_MOVE)

// ARRAY_SIZE - determines size of a C array
#define ARRAY_SIZE(x)	(sizeof(x) / sizeof(x[0]))

// DEFAULT_FSTAB
// RECOVERY_FSTAB
//
// Default FSTAB file names
static char DEFAULT_FSTAB[] 	= "/etc/fstab";
static char RECOVERY_FSTAB[] 	= "/etc/recovery.fstab";

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
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

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Appends a new option to an extra_opts structure
void add_extra_option(extra_mount_options* extra, char *s);

// Releases and reinitializes an extra_opts structure
extra_mount_options* free_extra_options(extra_mount_options* extra);

// Gets a reference to a named volume
const Volume* get_volume(const char* name);

// Initializes an extra_opts structure
void init_extra_options(extra_mount_options* extra);

// Mounts a volume
int mount_volume(const Volume* volume, int* mounted);

// Parses string-based mounting options into mounting flags
unsigned long parse_mount_options(const char* arglist, extra_mount_options* extra);

// Unmounts a volume
int unmount_volume(const Volume* volume, int* unmounted);

// Initializes the global volume table from an FSTAB
int volumes_init(const char* fstab_file);

// Releases the global volume table
void volumes_term(void);

//-----------------------------------------------------------------------------
// IMPLEMENTATION
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// add_extra_option
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
// free_extra_options
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
// init_extra_options
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
// parse_mount_options
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
	
	// MTDUTILS: Unmount the volume and set the previously mounted flag
	result = unmount_mounted_volume(pmv);
	if((result == 0) && (unmounted)) *unmounted = -1;
    
	return result;							// Return result
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
// main
//
// Main application entry point
//
// Arguments:
//
//	argc 	- Number of command line arguments
//	argv	- Array of command line argument pointers

int main(int argc, char **argv)
{
	const char*		fstab = NULL;		// Pointer to the FSTAB file name
	int 			unmount = 0;		// Flag to unmount instead of mount
	int 			numvolumes = 0;		// Number of volumes to be mounted/unmounted
	char**			volumes = NULL;		// List of volumes to be mounted/unmounted
	int 			index;				// Loop index variable
	const Volume* 	volume = NULL;		// A single Volume entry
	int 			result = 0;			// Result from function call
	
	fstab = DEFAULT_FSTAB;				// Default to standard FSTAB file
	
	// Process command line arguments
	const char* optString = "f:Ru";
	result = getopt(argc, argv, optString);
	while(result != -1) {
		
		switch(result) {
			
			// --f: Use a specific fstab file
			case 'f':
				fstab = optarg;
				break;
				
			// --R: Use /etc/recovery.fstab file
			case 'r':
				fstab = RECOVERY_FSTAB;
				break;
			
			// --u: Unmount rather than mount the volume
			case 'u':
				unmount = 1;
				break;
		}
	
		result = getopt(argc, argv, optString);
	}
	
	numvolumes = argc - optind;			// Number of unswitched arguments
	volumes = argv + optind;			// Array of volume names

	// If no volume names were specified, bail out
	if(numvolumes <= 0) {
		
		printf("Error: no volume names were specified\n");
		return EINVAL;
	}
	
	// Load the volume table from the specified fstab file
	result = volumes_init(fstab);
	if(result != 0) { 
		
		printf("Error: cannot load volume table from fstab file %s [%s]\n", fstab, strerror(errno));
		return result;
	}
	
	// Iterate over all of the provided volume names and attempt to mount/unmount them
	for(index = 0; index < numvolumes; index++) {
		
		// Get a reference to the volume information in the FSTAB file
		volume = get_volume(volumes[index]);
		if(volume == NULL) {
			
			printf("Error: specified volume name [%s] does not exist in fstab file [%s]\n", volumes[index], fstab);
			result = EINVAL;
			break;
		}
		
		// Attempt to mount/unmount the specified volume
		result = (unmount == 0) ? mount_volume(volume, NULL) : unmount_volume(volume, NULL);
		if(result != 0) {
			
			printf("Error: unable to %s volume %s [%s]\n", (unmount == 0) ? "mount" : "unmount", volume->name, strerror(errno));
			break;
		}
	}
	
	volumes_term();						// Clean up the volume table
	return result;						// Return result from mount/unmount
}
