//-----------------------------------------------------------------------------
// callbacks.h
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Callbacks
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
// Based on SIMG2IMG utility, part of the Android Open Source Project
//
// Modifications: Copyright (C) 2011 Michael Brehm
//-----------------------------------------------------------------------------

#ifndef __CALLBACKS_H_
#define __CALLBACKS_H_

//-----------------------------------------------------------------------------
// CALLBACKS
//-----------------------------------------------------------------------------

// UIPRINT_CALLBACK - Defines a callback used to write a string to the UI
typedef void(*UIPRINT_CALLBACK)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// PROGRESS_CALLBACK - Defines a callback used to indicate progress (based on 100%)
typedef void(*PROGRESS_CALLBACK)(float percent);

//-----------------------------------------------------------------------------
// DATA TYPES
//-----------------------------------------------------------------------------

// ui_callbacks
//
// Defines the generic callbacks into a single structure that makes life easier
typedef struct {
	
	UIPRINT_CALLBACK 		uiprint;
	PROGRESS_CALLBACK 		progress;
	
} ui_callbacks;

//-----------------------------------------------------------------------------
// CONSTANTS / MACROS
//-----------------------------------------------------------------------------

// UI CALLBACKS
//
// A somewhat inelegant set of macros that are used with ui-enabled functions to
// provide messages and progress back to the recovery user interface
//
// Pass an initialized ui_callbacks* pointer to the function, then declare
// USES_UI_CALLBACKS(variable_name) somewhere near the top of that function.
// The remaining macros can then be used within that function.

// USES_UI_CALLBACKS - Macro used to initialize a local ui_callbacks pointer
#define USES_UI_CALLBACKS(callbacks) ui_callbacks* __callbacks = callbacks;

// UI_PRINT - Macro used to invoke the pseudo-STDOUT 
#define UI_PRINT(fmt, ...) if(__callbacks != NULL) { if(((ui_callbacks*)__callbacks)->uiprint != NULL) \
	{ ((ui_callbacks*)__callbacks)->uiprint(fmt, ##__VA_ARGS__); } }

// UI_WARN - Macro used to invoke the pseudo-STDERR with a warning
#define UI_WARNING(fmt, ...) if(__callbacks != NULL) { if(((ui_callbacks*)__callbacks)->uiprint != NULL) \
	{ ((ui_callbacks*)__callbacks)->uiprint("W:" fmt, ##__VA_ARGS__); } }

// UI_ERROR - Macro used to invoke the pseudo-STDERR
#define UI_ERROR(fmt, ...) if(__callbacks != NULL) { if(((ui_callbacks*)__callbacks)->uiprint != NULL) \
	{ ((ui_callbacks*)__callbacks)->uiprint("E:" fmt, ##__VA_ARGS__); } }

// UI_SETPROGRESS - Macro used to invoke the progress indicator callback
#define UI_SETPROGRESS(percent) if(__callbacks != NULL) { if(((ui_callbacks*)__callbacks)->progress != NULL) \
	{ ((ui_callbacks*)__callbacks)->progress(percent / 100); } }

//-----------------------------------------------------------------------------
// FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// Initializes a set of UI callbacks before executing an operation
void init_ui_callbacks(ui_callbacks* callbacks, UIPRINT_CALLBACK uiprint, PROGRESS_CALLBACK progress);

//-----------------------------------------------------------------------------

#endif	// __CALLBACKS_H_
