/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <linux/input.h>

#include "deviceui.h"
#include "common.h"

// djp952: Changed header text
char* MENU_HEADERS[] = { "Galaxy S SCH-I500 Recovery Tools",
                         "",
                         NULL };

char* MENU_ITEMS[] = { "reboot system now",
                       "apply update from sdcard",
                       "wipe data/factory reset",
                       "wipe cache partition",
					   "test backup code",
					   "test restore code",
					   "Format Volumes",
                       NULL };

int device_recovery_start() {
    return 0;
}

int device_toggle_display(volatile char* key_pressed, int key_code) {
	return 0;
    //return key_code == I500KEY_MENU;
}

int device_reboot_now(volatile char* key_pressed, int key_code) {
    return 0;
}

// djp952: Replaced generic Linux key codes with ones specific to
// SCH-I500 device (#defines in recovery_ui.h)
int device_handle_key(int key_code, int visible)
{
	if (visible) {
		
   	switch (key_code) {
   		
   		case I500KEY_VOLUMEDOWN:
   			return HIGHLIGHT_DOWN;

			case I500KEY_VOLUMEUP:
         	return HIGHLIGHT_UP;

			case I500KEY_BACK:
         	return SELECT_ITEM;
			
			case I500KEY_HOME:
				return SELECT_HOME;
				
			case I500KEY_MENU:
				return SELECT_BACK;
		}
	}

	return NO_ACTION;
}

int device_perform_action(int which) {
    return which;
}

int device_wipe_data() {
    return 0;
}
