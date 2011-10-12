//-----------------------------------------------------------------------------
// callbacks.c
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

#include <stdio.h>							// Include STDIO declarations
#include "callbacks.h"						// Include CALLBACKS declarations

//-----------------------------------------------------------------------------
// init_ui_callbacks
//
// Initializes a ui_callbacks structure before it's passed into an operation
//
// Arguments:
//
//	callbacks		- Pointer to the ui_callbacks structure to initialize
//	uiprint			- Optional pointer to the PRINTF-style output callback
//	progress		- Optional pointer to the PROGRESS callback

void init_ui_callbacks(ui_callbacks* callbacks, UIPRINT_CALLBACK uiprint, PROGRESS_CALLBACK progress)
{
	if(!callbacks) return;					// NULL input structure

	// Callbacks are optional by nature, update the structure with either a function
	// pointer or NULL depending on what the user passed in
	
	callbacks->uiprint = (uiprint) ? uiprint : NULL;
	callbacks->progress = (progress) ? progress : NULL;
}

//-----------------------------------------------------------------------------