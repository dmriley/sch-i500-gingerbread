//-----------------------------------------------------------------------------
// menu-mainmenu.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Main Menu
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
#include "callbacks.h"					// Include CALLBACKS declarations
#include "volume.h"						// Include VOLUME declarations
#include "ui.h"							// Include UI declarations
#include "minzip/DirUtil.h"				// Include DIRUTIL declarations
#include "menus.h"						// Include MENUS declarations
#include "backup.h"						// Include BACKUP declarations
#include "restore.h"					// Include RESTORE declarations
#include "callbacks.h"					// Include CALLBACKS declarations
#include "commands.h"					// Include COMMANDS declarations

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// SUBHEADER_XXXX constants for menu headers
static char SUBHEADER_MAINMENU[] 				= "> Main Menu";
static char SUBHEADER_MAINMENU_BACKUP[]		 	= "> Backup Device";
static char SUBHEADER_MAINMENU_RESTORE[]		= "> Restore Device";
static char SUBHEADER_MAINMENU_APPLYUPDATE[]	= "> Install Updates";

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Restores the device from SD card
static void perform_restore_device();

// Displays the file browser to select (and apply) an update
static int submenu_selectupdate();

// Displays the install update confirmation
static int submenu_selectupdate_confirm(const char* zipfile);

//-----------------------------------------------------------------------------
// menu_mainmenu
//
// Shows the MAIN MENU to the user
//
// Arguments:
//
//	NONE

void menu_mainmenu(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				lastitem;			// BACK selection code
	int 				selection;			// Selected menu item

	// TODO: Original prompt_and_wait had a finish_recovery(NULL) call
	// in it for each iteration of the menu loop.  Do I need that?
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_MAINMENU);
	if(headers == NULL) { LOGE("menu_mainmenu: Cannot allocate menu headers"); return; }
	
	// Allocate the menu item list
	items = alloc_menu_list(
		"- Install Update Package",
		"- Create Device Backup",
		"- [OLD] Restore Device from SD Card",
		"- Wipe Data",
		"- Manage Volumes",
		"- [OLD] Advanced Tools",
		"- Options",
		"- Exit",
		NULL);
	
	if(items == NULL) {
		LOGE("menu_mainmenu: Cannot allocate menu items");
		free_menu_list(headers);
		return;
	}
	
	// Determine the index of the last item in the list (EXIT)
	lastitem = len_menu_list(items) - 1;
	
	// Keep displaying the menu until the user chooses the EXIT option
	do {
		
		// Prompt the user with the list of target volumes
		selection = get_menu_selection(headers, items, 1, 0);
		
		// Execute the selected operation or open child menu
		switch(selection) {
			
			// APPLY UPDATE FROM SD CARD
			case 0: submenu_selectupdate(); break;
			
			// BACKUP DEVICE TO SD CARD
			case 1: cmd_backup_device(); break;
			
			// RESTORE DEVICE FROM SD CARD
			case 2: perform_restore_device(); break;

			// WIPE DATA
			case 3: menu_wipedata(); break;
			
			// VOLUME MANAGEMENT
			case 4: menu_volumemanagement(); break;
			
			// TOOLS
			case 5: menu_tools(); break;
			
			// OPTIONS
			case 6: /* TODO */ break;
		}
				
	} while(selection < lastitem);
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
}

//-----------------------------------------------------------------------------
// perform_restore_device
//
// Restores the device from SDCARD
//
// Arguments:
//
//	NONE

static void perform_restore_device()
{
	const Volume*		iterator;		// Volume iterator
	char				temp[256];		// Temporary string buffer
	int 				mounted = 0;	// Flag if volume was mounted
	ui_callbacks 		callbacks;		// UI callbacks for restore operation
	int 				result;			// Result from function call
	int 				failed = 0;		// Flag if restore operation failed
	
	// TODO: hard-coded to use /sdcard/backup for now
	// TODO: this needs fail-safes like making sure SYSTEM.IMG is present
	// otherwise the user will end up with an unusable device
	
	// Mount the SDCARD volume
	result = mount_volume(get_volume("SDCARD"), &mounted);
	if(result != 0) { LOGE("perform_restore_device: Cannot mount SDCARD\n"); return; }
	
	// Clear any currently displayed UI text and initialize the callbacks 
	ui_clear_text();	
	init_ui_callbacks(&callbacks, &ui_print, &ui_set_progress);
		
	// Iterate over all of the FSTAB volumes and dump each one marked DUMP
	iterator = foreach_volume(NULL);
	while(iterator != NULL) {
		
		// If the volume is a dump volume, attempt to restore it
		if(*iterator->dump == '1') {

			result = unmount_volume(iterator, NULL);
			if(result == 0) {

				sprintf(temp, "/sdcard/backup/%s.szimg", iterator->name);
				ui_print("Restoring %s ...\n", iterator->name);
				ui_show_progress(1.0, 100);			
				
				result = restore_ext4_sparse_ui(temp, iterator, &callbacks);
				if(result != 0) {
					
					LOGE("Unable to restore volume %s. EC = %d\n", iterator->name, result);
					failed = 1;
				}

				ui_reset_progress();				// Reset the progress bar
			}
			
			else { LOGE("perform_restore_device: Cannot unmount %s\n", iterator->name); }
		}
		
		// Otherwise, if it's a wipe volume, format it
		else if(*iterator->wipe == '1') {
			
			ui_show_indeterminate_progress();
			ui_print("Formatting %s ...\n", iterator->name);
			
			result = format_volume(iterator, iterator->fs_type);
			if(result != 0) {
				
				LOGE("Unable to format volume %s. EC = %d\n", iterator->name, result);
				failed = 1;
			}
			
			ui_reset_progress();					// Reset the progress bar
		}
		
		iterator = foreach_volume(iterator);		// Move to next volume
	}
	
	// If the SDCARD volume was mounted by this function, unmount it before exiting
	if(mounted != 0) unmount_volume(get_volume("SDCARD"), NULL);
	
	if(failed == 0) { ui_print("Device restore complete.\n\n"); }
	else { ui_print("Device restore completed with errors; device may be unstable.\n\n"); }
		
	ui_reset_progress();
}

//-----------------------------------------------------------------------------
// submenu_selectupdate (private)
//
// Displays a sub-menu that allows the user to choose a zip file to apply
//
// Arguments:
//
//	NONE

static int submenu_selectupdate(void)
{
	char**			headers = NULL;		// UI menu headers
	const Volume*	sdvolume;			// SDCARD volume
	int 			sdmounted;			// Flag if SDCARD was mounted
	char 			zipfile[256];		// Selected zip file to install
	int 			nav;				// Menu navigation code
	int 			result;				// Result from function call
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_MAINMENU_APPLYUPDATE);
	if(headers == NULL) { LOGE("submenu_selectupdate: Cannot allocate menu headers"); return NAVIGATE_ERROR; }
	
	// Grab a reference to the SDCARD volume
	sdvolume = get_volume("SDCARD");
	if(sdvolume == NULL) { LOGE("submenu_selectupdate: Cannot locate SDCARD volume entry in fstab"); return NAVIGATE_ERROR; }
	
	// The SDCARD volume has to be mounted before we can browse it (obviously)
	result = mount_volume(sdvolume, &sdmounted);
	if(result != 0) { LOGE("submenu_selectupdate: Cannot mount SDCARD volume"); return NAVIGATE_ERROR; }
	
	// Invoke the directory browser against the root of the SDCARD ...
	nav = navigate_menu_browse(headers, sdvolume->mount_point, "*.zip", zipfile, 256);
	if(nav == NAVIGATE_SELECT) nav = submenu_selectupdate_confirm(zipfile);
	
	// Unmount the SDCARD volume if it was mounted by this function
	if(sdmounted) unmount_volume(sdvolume, NULL);
	
	free_menu_list(headers);			// Release the string array
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_selectupdate_confirm (private)
//
// Confirms that the user really wants to install the selected update
//
// Arguments:
//
//	zipfile		- Zip file to be installed

static int submenu_selectupdate_confirm(const char* zipfile)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	char 				temp[256];			// Temporary string space
	int 				selection;			// Selected menu item
	int 				result;				// Result from function call
	int 				nav;				// Menu navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_MAINMENU_APPLYUPDATE);
	if(headers == NULL) { LOGE("submenu_selectupdate_confirm: Cannot allocate menu headers\n"); return NAVIGATE_ERROR; }
	
	// Provide a warning message at the end of the header list
	sprintf(temp, "WARNING: Changes made by installation packages");
	headers = append_menu_list(headers, temp);
	headers = append_menu_list(headers, "cannot be undone without a restore. Continue?");
	headers = append_menu_list(headers, "");
	
	//
	// TODO: Get just the file name for display here; not the entire path
	//
	
	// Format the YES option string
	sprintf(temp, "- Yes -- Install %s", zipfile);

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
		
		LOGE("submenu_selectupdate_confirm: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_ERROR;
	}
	
	// If the user selected YES, go ahead and install the package
	nav = navigate_menu(headers, items, &selection);
	if((nav == NAVIGATE_SELECT) && (selection == 4)) cmd_install_updatezip(zipfile);

	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------