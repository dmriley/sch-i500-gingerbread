//-----------------------------------------------------------------------------
// ui.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// User Interface
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

#ifndef __UI_H_
#define __UI_H_

//-----------------------------------------------------------------------------
// PUBLIC DATA TYPES
//-----------------------------------------------------------------------------

// Set the icon (normally the only thing visible besides the progress bar).
enum {
  BACKGROUND_ICON_NONE,
  BACKGROUND_ICON_INSTALLING,
  BACKGROUND_ICON_ERROR,
  NUM_BACKGROUND_ICONS
};

//-----------------------------------------------------------------------------
// PUBLIC CONSTANTS/MACROS
//-----------------------------------------------------------------------------

// Default allocation of progress bar segments to operations
static const int VERIFICATION_PROGRESS_TIME = 60;
static const float VERIFICATION_PROGRESS_FRACTION = 0.25;
static const float DEFAULT_FILES_PROGRESS_FRACTION = 0.4;
static const float DEFAULT_IMAGE_PROGRESS_FRACTION = 0.1;

static const int NAVIGATE_SELECT = 0;
static const int NAVIGATE_BACK = 1;
static const int NAVIGATE_HOME = 2;
static const int NAVIGATE_ERROR = 2; // <--- NAVIGATE_HOME

//-----------------------------------------------------------------------------
// PUBLIC FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Initialize the graphics system.
void ui_init();

// Use KEY_* codes from <linux/input.h> or KEY_DREAM_* from "minui/minui.h".
int ui_wait_key();            // waits for a key/button press, returns the code
int ui_key_pressed(int key);  // returns >0 if the code is currently pressed
int ui_text_visible();        // returns >0 if text log is currently visible
void ui_show_text(int visible);
void ui_clear_key_queue();

// Write a message to the on-screen log shown with Alt-L (also to stderr).
// The screen is small, and users may need to report these messages to support,
// so keep the output short and not too cryptic.
void ui_print(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// Clears all text from the UI
void ui_clear_text(void);

// Display some header text followed by a menu of items, which appears
// at the top of the screen (in place of any scrolling ui_print()
// output, if necessary).
void ui_start_menu(char** headers, char** items, int initial_selection);
// Set the menu highlight to the given index, and return it (capped to
// the range [0..numitems).
int ui_menu_select(int sel);
// End menu mode, resetting the text overlay so that ui_print()
// statements will be displayed.
void ui_end_menu();

void ui_set_background(int icon);

// Show a progress bar and define the scope of the next operation:
//   portion - fraction of the progress bar the next operation will use
//   seconds - expected time interval (progress bar moves at this minimum rate)
void ui_show_progress(float portion, int seconds);
void ui_set_progress(float fraction);  // 0.0 - 1.0 within the defined scope

// Show a rotating "barberpole" for ongoing operations.  Updates automatically.
void ui_show_indeterminate_progress();

// Hide and reset the progress bar.
void ui_reset_progress();

// Allocates a UI menu list.  Pass in NULL to allocate an empty list for use
// with append_menu_list()
char** alloc_menu_list(char* item, ...);

// Append a single string to an existing menu list
char** append_menu_list(char** list, char* item);

// Returns the length of a menu list
int len_menu_list(char** list);

// Replaces the text for a menu list item
void change_menu_list_item(char** list, int index, const char* item);

// Releases a UI menu list allocated with alloc_menu_list()
char** free_menu_list(char** list);

char**
prepend_title(const char** headers);

int
get_menu_selection(char** headers, char** items, int menu_only,
                   int initial_selection);

// Updated version of get_menu_selection that returns a navigation code
// rather than the selected item index and moves that index to an [OUT] var
int navigate_menu(char** headers, char** items, int* selection);

// 
int navigate_menu_browse(char** headers, const char* root, const char* filter,
	char* filename, int cch);

//-----------------------------------------------------------------------------

#endif	// __UI_H_
