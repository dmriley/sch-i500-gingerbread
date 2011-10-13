//-----------------------------------------------------------------------------
// menu-wipe.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Wipe Data Menu
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

#include "common.h"						// Include COMMON declarations
#include "ui.h"							// Include UI declarations
#include "menus.h"						// Include MENUS declarations
#include "commands.h"					// Include COMMANDS declarations

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// SUBHEADER_XXXX constants for menu headers
static char SUBHEADER_WIPEDATA[] = "> Wipe Device Data";

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Displays the Wipe All User Data confirmation and formats the device
static void submenu_wipedata_confirm();

//-----------------------------------------------------------------------------
// menu_wipedata
//
// Shows the WIPE DATA submenu to the user.  This menu allows the user to 
// perform varying levels of wipe
//
// Arguments:
//
//	NONE

int menu_wipedata(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	int 				nav;				// UI navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_WIPEDATA);
	if(headers == NULL) { LOGE("menu_wipedata: Cannot allocate menu headers"); return NAVIGATE_HOME; }
	
	// Allocate the menu item list
	items = alloc_menu_list(
		"- Wipe Cache",
		"- Wipe Dalvik Cache",
		"- Wipe Battery Statistics",
		"- Wipe all User Data (Factory Reset)",
		NULL);
	
	if(items == NULL) {
		LOGE("menu_wipedata: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_HOME;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {

		// Execute the selected operation or open child menu
		switch(selection) {
			
			// WIPE CACHE
			case 0: cmd_wipe_cache(); break;
			
			// WIPE DALVIK CACHE
			case 1: cmd_wipe_dalvik_cache(); break;
			
			// WIPE BATTERY STATISTICS
			case 2: cmd_wipe_battery_stats(); break;
			
			// WIPE ALL USER DATA
			case 3: submenu_wipedata_confirm(); break;
		}

		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------
// submenu_wipedata_confirm
//
// Confirms that the user really wants to factory reset the device, and if
// so issues the necessary format commands
//
// Arguments:
//
//	NONE

static void submenu_wipedata_confirm(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	const Volume*		iterator;			// Volume iterator
	int 				selection;			// Selected menu item
	int 				result;				// Result from function call
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_WIPEDATA);
	if(headers == NULL) { LOGE("submenu_wipedata_confirm: Cannot allocate menu headers\n"); return; }
	
	// Provide a warning message at the end of the header list
	headers = append_menu_list(headers, "WARNING: All user data on this device will");
	headers = append_menu_list(headers, "be permanently erased. Continue?");
	headers = append_menu_list(headers, "");
	
	// Allocate the confirmation menu
	items = alloc_menu_list("- No",
							"- No",
							"- No",
							"- No",
							"- Yes -- Erase all user data from device",		// <-- 4
							"- No",
							"- No",
							"- No",
							"- No",
							NULL);
	
	if(items == NULL) {
		
		LOGE("submenu_wipedata_confirm: Cannot allocate menu items");
		free_menu_list(headers);
		return;
	}
	
	// If the user selected YES, go ahead and format the volume
	if(get_menu_selection(headers, items, 1, 0) == 4) cmd_wipe_device();
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
}

//-----------------------------------------------------------------------------