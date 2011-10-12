//-----------------------------------------------------------------------------
// menus.h
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

#ifndef __MENUS_H_
#define __MENUS_H_

//-----------------------------------------------------------------------------
// PUBLIC FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Allocates the standard recovery header, with subheader as the text to
// display on the final line before the blank.  Release the returned string
// with ui::free_menu_list()
char** alloc_standard_header(const char* subheader);

// Handler for the MAIN menu
void menu_mainmenu(void);

// Handler for the TOOLS menu
int menu_tools(void);

// Handler for the VOLUME MANAGEMENT menu
int menu_volumemanagement(void);

// Handler for the WIPE DATA menu
int menu_wipedata(void);

//-----------------------------------------------------------------------------

#endif	// __MENUS_H_
