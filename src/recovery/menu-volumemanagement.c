//-----------------------------------------------------------------------------
// menu-volumemanagement.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Volume Management Menu
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

#include <stdio.h>						// Include STDIO declarations
#include <stdlib.h>						// Include STDLIB declarations
#include "common.h"						// Include COMMON declarations
#include "ui.h"							// Include UI declarations
#include "menus.h"						// Include MENUS declarations
#include "commands.h"					// Include COMMANDS declarations
#include "volume.h"						// Include VOLUME declarations

//-----------------------------------------------------------------------------
// VOLUME MANAGEMENT MENU
//
//	> Volume Management
//		> Mount Volumes
//			> Mount Volume N
//		> Unmount Volumes
//			> Unmount Volume N
//		> Format Volumes
//			> Format Volume N
//				[ > Format Volume N [FILESYSTEM] ] * OPTIONAL
//					> CONFIRMATION
//		> Backup Volumes
//			> Backup Volume N [METHOD]
//		> Restore Volumes
//		> Convert Volumes
//			> Convert Volume N
//				> Convert Volume N [FILESYSTEM1]
//				> Convert Volume N [FILESYSTEM2]
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// PRIVATE TYPE DECLARATIONS
//-----------------------------------------------------------------------------

// CREATE_VOLMENU_FLAGS
//
// Flags used in conjunction with create_volume_menuitems and 
typedef struct {
	
	const char* 	operation;			// Operation string
	const char*		ignore;				// Specific volume to ignore
	int 			dualfsonly;			// Flag to hide single FS volumes
	
} CREATE_VOLMENU_FLAGS;

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// SUBHEADER_XXXX constants for menu headers
static char SUBHEADER_VOLUMEMGMT[] 		= "> Volume Management";
static char SUBHEADER_MOUNTVOLUMES[] 	= "> Mount Volumes";
static char SUBHEADER_UNMOUNTVOLUMES[] 	= "> Unmount Volumes";
static char SUBHEADER_FORMATVOLUMES[] 	= "> Format Volumes";
static char SUBHEADER_BACKUPVOLUMES[]	= "> Backup Volumes";
static char SUBHEADER_RESTOREVOLUMES[]	= "> Restore Volumes";
static char SUBHEADER_CONVERTVOLUMES[]	= "> Convert Volumes";

// DISABLE_COMPRESSION_ITEM / ENABLE_COMPRESSION_ITEM 
//
// Used in the backup menu to toggle the global compression variable
static char DISABLE_COMPRESSION_ITEM[] = "- Disable Backup File Compression";
static char ENABLE_COMPRESSION_ITEM[] = "- Enable Backup File Compression";

// VOLIGNORE_SDCARD
//
// Used to ignore the SDCARD volume for backup/restore operations
static char VOLIGNORE_SDCARD[] = "SDCARD";

// g_backup_compression
//
// Used to enable/disable backup file compression
static int g_backup_compression = 1;

//-----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Helper function used to generate a menu list of volumes
static char** create_volume_menuitems(CREATE_VOLMENU_FLAGS* flags);

// Helper function used to select a Volume* from the list generated
// by create_volume_menuitems()
static const Volume* select_volume_menuitem(int selection, CREATE_VOLMENU_FLAGS* flags);

// Handler for the BACKUP VOLUMES menu
static int submenu_backupvolumes(void);

// Displays the Backup Volumes submenu for the selected volume
static int submenu_backuponevolume(const Volume* volume);

// Handler for the CONVERT VOLUMES menu
static int submenu_convertvolumes(void);

// Displays the Convert Volumes submenu for the selected volume
static int submenu_convertonevolume(const Volume* volume);

// Handler for the FORMAT VOLUMES menu
static int submenu_formatvolumes(void);

// Displays the Format Volumes submenu for the selected volume
static int submenu_formatonevolume(const Volume* volume);

// Displays the Format Volume confirmation and formats the volume
static int submenu_formatonevolume_confirm(const Volume* volume, const char* fs);

// Handler for the MOUNT VOLUMES menu
static int submenu_mountvolumes(void);

// Displays the Restore Volumes submenu for the selected volume
static int submenu_restoreonevolume(const Volume* volume);

// Displays the Restore Volume confirmation and restores the volume
static int submenu_restoreonevolume_confirm(const Volume* volume, const char* imgfile);

// Handler for the RESTORE VOLUMES menu
static int submenu_restorevolumes(void);

// Handler for the UNMOUNT VOLUMES menu
static int submenu_unmountvolumes(void);

//-----------------------------------------------------------------------------
// create_volume_menuitems (private)
//
// Helper function used to create the common menu list of volume names
// used by most of the submenus in here
//
// Arguments:
//
//	flags		- Menu creation flags (hackish at best)

static char** create_volume_menuitems(CREATE_VOLMENU_FLAGS* flags)
{
	const Volume*		volit;				// Volume iterator
	char**	 			items = NULL;		// UI menu items
	int					volnamelen = 0;		// Volume name length
	char 				padding[256];		// String padding
	char				temp[256];			// Temporary string space
	int 				index = 0;			// Loop index variable
	
	// Allocate an empty menu list that we will append all the volumes to
	items = alloc_menu_list(NULL);
	if(items == NULL) return NULL;
	
	// Iterate over all the volumes to find the longest volume name, which
	// will be used to do nothing more than align the text shown to the user
	volit = foreach_volume(NULL);
	while(volit != NULL) {
	
		// Don't look at virtual FSTAB items, as they are ignored below
		if(volit->virtual == 0) {
			
			int len = strlen(volit->name);
			if(len > volnamelen) volnamelen = len;
		}
		
		volit = foreach_volume(volit);
	}

	// Iterate over all the volumes and add each as a new menu item
	volit = foreach_volume(NULL);
	while(volit != NULL) { 
		
		// Don't list items that were added virtually to the FSTAB information
		if(volit->virtual == 0) {

			// SPECIAL CASE HANDLER: Ignore the volume specified by ignore
			if((flags->ignore == NULL) || (strcasecmp(volit->name, flags->ignore) != 0)) {
				
				// SPECIAL CASE HANDLER: Ignore single filesystem volumes
				if((flags->dualfsonly == 0) || (volit->fs_type2 != NULL)) {
				
					// It looks a lot nicer if we pad the device names so they line up
					memset(padding, 0, 256 * sizeof(char));
					int pad = volnamelen - strlen(volit->name);
					for(index = 0; index < pad; index++) { padding[index] = ' '; }
				
					sprintf(temp, "- %s %s %s[%s]", flags->operation, volit->name, padding, volit->device);
					items = append_menu_list(items, temp);
				}
			}
		}
		
		volit = foreach_volume(volit);
	}
	
	return items;					// Return generated list of menu items
}

//-----------------------------------------------------------------------------
// select_volume_menuitem (private)
//
// Helper function used to get the selected Volume* from the menu list created
// by the create_volume_menuitems() function
//
// Arguments:
//
//	selection		- Selection code returned from the menu system
//	flags			- Flags for filtering unwanted menu items

static const Volume* select_volume_menuitem(int selection, CREATE_VOLMENU_FLAGS* flags)
{
	const Volume*		volit;				// Volume iterator
	int 				index = 0;			// Loop index variable
	
	// Figure out what volume the user selected
	volit = foreach_volume(NULL);
	while((volit != NULL) && (index <= selection)) {
		
		// Virtual (ramdisk) volumes were not presented as valid options
		if(volit->virtual == 0) {
			
			// SPECIAL CASE HANDLER: Ignore the volume specified by ignore
			if((flags->ignore == NULL) || (strcasecmp(volit->name, flags->ignore) != 0)) {
				
				// SPECIAL CASE HANDLER: Ignore single filesystem volumes
				if((flags->dualfsonly == 0) || (volit->fs_type2 != NULL)) {
					
					// If we've matched the selection to the volume, call the submenu
					if(index == selection) return volit;
					index++;
				}
			}
		}
		
		volit = foreach_volume(volit);		// Move to next volume
	}
	
	return NULL;				// Could not match selection to volume
}

//-----------------------------------------------------------------------------
// menu_volumemanagement
//
// Shows the VOLUME MANAGEMENT submenu to the user.  This menu allows the user to 
// execute advanced options against the volumes present in this device
//
// Arguments:
//
//	NONE

int menu_volumemanagement(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	int 				nav;				// Return navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_VOLUMEMGMT);
	if(headers == NULL) { LOGE("menu_volumemanagement: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Allocate the menu item list
	items = alloc_menu_list(
		"- Mount Volumes",
		"- Unmount Volumes",
		"- Backup Volumes",
		"- Restore Volumes",
		"- Convert Volumes",
		"- Format Volumes",
		NULL);
	
	if(items == NULL) {
		LOGE("menu_volumemanagement: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {

		switch(selection) {
			
			// MOUNT VOLUMES
			case 0: nav = submenu_mountvolumes(); break;
				
			// UNMOUNT VOLUMES
			case 1: nav = submenu_unmountvolumes(); break;
			
			// BACKUP VOLUMES
			case 2: nav = submenu_backupvolumes(); break;
				
			// RESTORE VOLUMES
			case 3: nav = submenu_restorevolumes(); break;
			
			// CONVERT VOLUMES
			case 4: nav = submenu_convertvolumes(); break;

			// FORMAT VOLUMES
			case 5: nav = submenu_formatvolumes(); break;
		}
		
		// NAVIGATE_HOME after a command breaks the loop so that
		// the menu system will unwind all the way back to the main menu
		if(nav == NAVIGATE_HOME) break;
		else nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_backuponevolume (private)
//
// Displays a sub-menu that allows the user to choose what method to use when
// generating the voluem backup image
//
// Arguments:
//
//	volume		- Volume selected by the user for backup

static int submenu_backuponevolume(const Volume* volume)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	cmd_backup_method 	method;				// Selected backup method
	int 				compress;			// Flag to use compression or not
	int 				selection;			// Selected menu item
	int 				nav;				// Menu navigation code

	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_BACKUPVOLUMES);
	if(headers == NULL) { LOGE("submenu_backuponevolume: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Allocate an empty menu list that we will append all the backup methods to
	items = alloc_menu_list(NULL);
	if(items == NULL) {
		
		LOGE("submenu_backuponevolume: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Provide the user with a choice of what backup imaging method to use
	sprintf(temp, "- Backup %s [ext4 image]", volume->name);
	items = append_menu_list(items, temp);
	
	sprintf(temp, "- Backup %s [ext4 sparse image]", volume->name);
	items = append_menu_list(items, temp);
	
	sprintf(temp, "- Backup %s [raw dump]", volume->name);
	items = append_menu_list(items, temp);
	
	sprintf(temp, "- Backup %s [yaffs2 image]", volume->name);
	items = append_menu_list(items, temp);

	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		switch(selection) {
			
			// TODO: Hard-coded paths
			case 0: cmd_backup_volume(volume, "/sdcard/backup", ext4, g_backup_compression); break;
			case 1: cmd_backup_volume(volume, "/sdcard/backup", ext4_sparse, g_backup_compression); break;
			case 2: cmd_backup_volume(volume, "/sdcard/backup", dump, g_backup_compression); break;
			case 3: cmd_backup_volume(volume, "/sdcard/backup", yaffs2, g_backup_compression); break;
		}
		
		nav = NAVIGATE_BACK;
	}

	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_backupvolumes (private)
//
// Shows the BACKUP VOLUMES submenu to the user.  This menu allows the user to
// back up any volume listed in the FSTAB file with a number of different methods
//
// Arguments:
//
//	NONE

static int submenu_backupvolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_BACKUPVOLUMES);
	if(headers == NULL) { LOGE("submenu_backupvolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation, excluding SDCARD
	CREATE_VOLMENU_FLAGS flags = { "Backup", VOLIGNORE_SDCARD, 0 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_backupvolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) nav = submenu_backuponevolume(selvol);
		
		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_convertonevolume (private)
//
// Displays a sub-menu that allows the user to choose what filesystem to use
// when converting a volume
//
// Arguments:
//
//	volume		- Volume selected by the user for backup

static int submenu_convertonevolume(const Volume* volume)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	cmd_backup_method 	method;				// Selected backup method
	int 				selection;			// Selected menu item
	int 				nav;				// Menu navigation code

	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_CONVERTVOLUMES);
	if(headers == NULL) { LOGE("submenu_convertonevolume: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Allocate an empty menu list that we will append all the backup methods to
	items = alloc_menu_list(NULL);
	if(items == NULL) {
		
		LOGE("submenu_convertonevolume: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Provide the user with a choice of what backup imaging method to use
	sprintf(temp, "- Convert %s [%s]", volume->name, volume->fs_type);
	items = append_menu_list(items, temp);
	
	sprintf(temp, "- Convert %s [%s]", volume->name, volume->fs_type2);
	items = append_menu_list(items, temp);
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		switch(selection) {
			
			case 0: cmd_convert_volume(volume, volume->fs_type); break;
			case 1: cmd_convert_volume(volume, volume->fs_type2); break;
		}
		
		nav = NAVIGATE_BACK;
	}

	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_convertvolumes (private)
//
// Shows the CONVERT VOLUMES submenu to the user.  This menu allows the user to
// switch a volume between it's primary and secondary file systems
//
// Arguments:
//
//	NONE

static int submenu_convertvolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_CONVERTVOLUMES);
	if(headers == NULL) { LOGE("submenu_convertvolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation, excluding single FS volumes
	CREATE_VOLMENU_FLAGS flags = { "Convert", NULL, 1 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_convertvolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) nav = submenu_convertonevolume(selvol);
		
		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_formatonevolume (private)
//
// Displays a sub-menu that allows the user to choose what file system to use
// when formatting the selected volume.  If no secondary file system is listed
// in the FSTAB for this volume, this will jump right to confirmation
//
// Arguments:
//
//	volume		- Volume selected by the user for formatting

static int submenu_formatonevolume(const Volume* volume)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	int 				selection;			// Selected menu item
	int 				nav;				// Menu navigation code

	// If there is no secondary filesystem listed in the FSTAB for this volume,
	// jump right into confirmation ...
	if(volume->fs_type2 == NULL) 
		return submenu_formatonevolume_confirm(volume, volume->fs_type);

	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_FORMATVOLUMES);
	if(headers == NULL) { LOGE("submenu_formatonevolume: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Allocate an empty menu list that we will append all the volumes to
	items = alloc_menu_list(NULL);
	if(items == NULL) {
		
		LOGE("submenu_formatonevolume: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Provide the user with a choice of formatting with either the primary or secondary
	// file system (or getting out of the operation altogether)
	sprintf(temp, "- Format %s [%s]", volume->name, volume->fs_type);
	items = append_menu_list(items, temp);
	
	sprintf(temp, "- Format %s [%s]", volume->name, volume->fs_type2);
	items = append_menu_list(items, temp);
	
	// Determine which option the user chose and continue on into confirmation
	nav = navigate_menu(headers, items, &selection);
	if(nav == NAVIGATE_SELECT) {
		
		switch(selection) {
		
			case 0 : nav = submenu_formatonevolume_confirm(volume, volume->fs_type); break;
			case 1 : nav = submenu_formatonevolume_confirm(volume, volume->fs_type2); break;
		}
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_formatonevolume_confirm (private)
//
// Confirms that the user really wants to format the selected volume with the
// selected file system and if so, initiates the format operation
//
// Arguments:
//
//	volume		- Volume selected by the user for formatting
//	fs			- File system selected by the user

static int submenu_formatonevolume_confirm(const Volume* volume, const char* fs)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	int 				selection;			// Selected menu item
	int 				result;				// Result from function call
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_FORMATVOLUMES);
	if(headers == NULL) { LOGE("submenu_formatonevolume_confirm: Cannot allocate menu headers\n"); return NAVIGATE_ERROR; }
	
	// Provide a warning message at the end of the header list
	sprintf(temp, "WARNING: All data on volume %s will", volume->name);
	headers = append_menu_list(headers, temp);
	headers = append_menu_list(headers, "be permanently erased. Continue?");
	headers = append_menu_list(headers, "");
	
	// Format the YES option string
	sprintf(temp, "- Yes -- Format %s [%s]", volume->name, fs);

	// Allocate the confirmation menu
	items = alloc_menu_list("- No",
							"- No",
							"- No",
							"- No",
							temp,			// <--- 4
							"- No",
							"- No",
							"- No",
							"- No",
							NULL);
	
	if(items == NULL) {
		
		LOGE("submenu_formatonevolume_confirm: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// If the user selected YES, go ahead and format the volume
	nav = navigate_menu(headers, items, &selection);
	if((nav == NAVIGATE_SELECT) && (selection == 4)) cmd_format_volume(volume, fs);

	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_formatvolumes (private)
//
// Shows the FORMAT VOLUMES submenu to the user.  This menu allows the user to
// format any volume listed in the FSTAB file with either the primary or
// secondary file system
//
// Arguments:
//
//	NONE

static int submenu_formatvolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_FORMATVOLUMES);
	if(headers == NULL) { LOGE("submenu_formatvolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation
	CREATE_VOLMENU_FLAGS flags= { "Format", NULL, 0 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_formatvolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) nav = submenu_formatonevolume(selvol);

		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_mountvolumes (private)
//
// Shows the MOUNT VOLUMES submenu to the user.  This menu allows the user to
// mount any volume listed in the FSTAB file
//
// Arguments:
//
//	NONE

static int submenu_mountvolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_MOUNTVOLUMES);
	if(headers == NULL) { LOGE("submenu_mountvolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation
	CREATE_VOLMENU_FLAGS flags = { "Mount", NULL, 0 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_mountvolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) cmd_mount_volume(selvol);

		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_restoreonevolume (private)
//
// Displays a sub-menu that allows the user to choose an image file to restore
// to the specified volume.  The format of the image file will be auto-detected
// if possible, and if not the restore process cannot continue (it would be absurd
// to even attempt this if we don't know what we're doing)
//
// Arguments:
//
//	volume		- Volume selected by the user for restore

static int submenu_restoreonevolume(const Volume* volume)
{
	char**			headers = NULL;		// UI menu headers
	const Volume*	sdvolume;			// SDCARD volume
	int 			sdmounted;			// Flag if SDCARD was mounted
	char 			filter[256];		// Directory browse filter string
	char 			imgfile[256];		// Selected image file to restore
	int 			nav;				// Menu navigation code
	int 			result;				// Result from function call
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_RESTOREVOLUMES);
	if(headers == NULL) { LOGE("submenu_restoreonevolume: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Grab a reference to the SDCARD volume
	sdvolume = get_volume("SDCARD");
	if(sdvolume == NULL) { LOGE("submenu_restoreonevolume: Cannot locate SDCARD volume entry in fstab"); return NAVIGATE_ERROR; }
	
	// The SDCARD volume has to be mounted before we can browse it (obviously)
	result = mount_volume(sdvolume, &sdmounted);
	if(result != 0) { LOGE("submenu_restoreonevolume: Cannot mount SDCARD volume"); return NAVIGATE_ERROR; }
	
	// Generate the directory browse filter string.  Backup files can have a number of different
	// extensions, but if they were created by this recovery they will have the volume name in
	// ALL CAPS as the base file name
	snprintf(filter, 256, "%s.*", volume->name);
	
	// Invoke the directory browser against the root of the SDCARD ...
	nav = navigate_menu_browse(headers, sdvolume->mount_point, filter, imgfile, 256);
	if(nav == NAVIGATE_SELECT) nav = submenu_restoreonevolume_confirm(volume, imgfile);
	
	// Unmount the SDCARD volume if it was mounted by this function
	if(sdmounted) unmount_volume(sdvolume, NULL);
	
	free_menu_list(headers);			// Release the string array
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_restoreonevolume_confirm (private)
//
// Confirms that the user really wants to restore the selected volume with the
// selected image file and if so, initiates the restore operation
//
// Arguments:
//
//	volume		- Volume selected by the user for formatting
//	imgfile		- Image File to be used to restore the volume

static int submenu_restoreonevolume_confirm(const Volume* volume, const char* imgfile)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	int 				selection;			// Selected menu item
	int 				result;				// Result from function call
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_RESTOREVOLUMES);
	if(headers == NULL) { LOGE("submenu_restoreonevolume_confirm: Cannot allocate menu headers\n"); return NAVIGATE_ERROR; }
	
	// Provide a warning message at the end of the header list
	sprintf(temp, "WARNING: All data on volume %s will be", volume->name);
	headers = append_menu_list(headers, temp);
	headers = append_menu_list(headers, "replaced by the backup image. Continue?");
	headers = append_menu_list(headers, "");
	
	// Format the YES option string
	sprintf(temp, "- Yes -- Restore %s", volume->name);

	// Allocate the confirmation menu
	items = alloc_menu_list("- No",
							"- No",
							"- No",
							"- No",
							temp,			// <--- 4
							"- No",
							"- No",
							"- No",
							"- No",
							NULL);
	
	if(items == NULL) {
		
		LOGE("submenu_restoreonevolume_confirm: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// If the user selected YES, go ahead and format the volume
	nav = navigate_menu(headers, items, &selection);
	if((nav == NAVIGATE_SELECT) && (selection == 4)) cmd_restore_volume(imgfile, volume);

	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_restorevolumes (private)
//
// Shows the RESTORE VOLUMES submenu to the user.  This menu allows the user to
// select a volume to restore from an image file
//
// Arguments:
//
//	NONE

static int submenu_restorevolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_RESTOREVOLUMES);
	if(headers == NULL) { LOGE("submenu_restorevolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation, excluding SDCARD
	CREATE_VOLMENU_FLAGS flags = { "Restore", VOLIGNORE_SDCARD, 0 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_restorevolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) nav = submenu_restoreonevolume(selvol);

		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_unmountvolumes (private)
//
// Shows the UNMOUNT VOLUMES submenu to the user.  This menu allows the user to
// unmount any volume listed in the FSTAB file
//
// Arguments:
//
//	NONE

static int submenu_unmountvolumes(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	const Volume*		selvol;				// Selected Volume item
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_UNMOUNTVOLUMES);
	if(headers == NULL) { LOGE("submenu_unmountvolumes: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Create the list of volume menu items for this operation
	CREATE_VOLMENU_FLAGS flags = { "Unmount", NULL, 0 };
	items = create_volume_menuitems(&flags);
	if(items == NULL) {
		
		LOGE("submenu_unmountvolumes: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {
		
		// Convert the selection code into a Volume pointer we can use
		selvol = select_volume_menuitem(selection, &flags);
		if(selvol != NULL) cmd_unmount_volume(selvol);

		if(nav == NAVIGATE_HOME) break;
		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------