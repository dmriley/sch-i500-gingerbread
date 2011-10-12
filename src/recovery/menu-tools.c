//-----------------------------------------------------------------------------
// menu-tools.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Tools Menu
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

//#include <stdio.h>						// Include STDIO declarations
//#include <stdlib.h>						// Include STDLIB declarations
#include "common.h"						// Include COMMON declarations
#include "ui.h"							// Include UI declarations
#include "menus.h"						// Include MENUS declarations
#include "commands.h"					// Include COMMANDS declarations

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// SUBHEADER_XXXX constants for menu headers
static char SUBHEADER_TOOLS[] = "> Tools";

//-----------------------------------------------------------------------------
// menu_tools
//
// Shows the TOOSL submenu to the user.  This menu allows the user to 
// install, remove or execute various (and hopefully useful) tools
//
// Arguments:
//
//	NONE

int menu_tools(void)
{
	char**				headers = NULL;		// UI menu headers
	char**	 			items = NULL;		// UI menu items
	int 				selection;			// Selected menu item
	int 				nav;				// UI navigation code
	
	// Allocate a standard header
	headers = alloc_standard_header(SUBHEADER_TOOLS);
	if(headers == NULL) { LOGE("menu_tools: Cannot allocate menu headers"); return NAVIGATE_HOME; }
	
	// Allocate the menu item list
	items = alloc_menu_list(
		"- Restart ADBD Service",
		//"- Install BusyBox",
		//"- Install su",
		//"- Remove BusyBox",
		//"- Remove su",
		NULL);
	
	if(items == NULL) {
		LOGE("menu_tools: Cannot allocate menu items");
		free_menu_list(headers);
		return NAVIGATE_HOME;
	}
	
	// Navigate the generated menu
	nav = navigate_menu(headers, items, &selection);
	while(nav == NAVIGATE_SELECT) {

		// Execute the selected operation or open child menu
		switch(selection) {
			
			// RESTART ADBD
			case 0: cmd_kill_adbd(); break;
			
			// INSTALL BUSYBOX
			//case 1: cmd_install_busybox(); break;
			
			// INSTALL SU
			//case 2: cmd_install_su(); break;
			
			// REMOVE BUSYBOX
			//case 3: cmd_remove_busybox(); break;
			
			// REMOVE SU
			//case 4: cmd_remove_su(); break;
		}

		nav = navigate_menu(headers, items, &selection);
	}
	
	free_menu_list(items);				// Release the string array
	free_menu_list(headers);			// Release the string array
	
	return nav;							// Return navigation code
}

//-----------------------------------------------------------------------------