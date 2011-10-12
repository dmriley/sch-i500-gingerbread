//-----------------------------------------------------------------------------
// menu-advanced.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Menus
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

#include "common.h"							// Include COMMON declarations
#include "ui.h"								// Include UI declarations
#include "menus.h"							// Include MENUS declarations

//-----------------------------------------------------------------------------
// GLOBAL VARIABLES
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// alloc_standard_header
//
// Allocates the standard header text for the recovery menu system.  Release
// the pointer returned by this function with ui::free_menu_list()
//
// Arguments:
//
//	subheader		- Subheader text (typically should be the menu location)

char** alloc_standard_header(const char* subheader)
{
	return alloc_menu_list("     -- GALAXY S SCH-I500 RECOVERY TOOLS --", "", subheader, "", NULL);
}

//-----------------------------------------------------------------------------