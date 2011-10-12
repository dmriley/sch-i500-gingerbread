//-----------------------------------------------------------------------------
// ui.c
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

#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>						// Include STDLIB declarations
#include <string.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>						// Include DIRENT declarations
#include <fnmatch.h>					// Include FNMATCH declarations
#include <string.h>						// Include STRING declarations

#include "common.h"
#include "minui/minui.h"
#include "deviceui.h"

#include "ui.h"							// Include UI declarations

#define MAX_COLS 96
#define MAX_ROWS 32

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

#define PROGRESSBAR_INDETERMINATE_STATES 6
#define PROGRESSBAR_INDETERMINATE_FPS 15

static pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
static gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static gr_surface gProgressBarIndeterminate[PROGRESSBAR_INDETERMINATE_STATES];
static gr_surface gProgressBarEmpty;
static gr_surface gProgressBarFill;

static const struct { gr_surface* surface; const char *name; } BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_ICON_INSTALLING], "icon_installing" },
    //{ &gBackgroundIcon[BACKGROUND_ICON_ERROR],      "icon_error" },
    { &gBackgroundIcon[BACKGROUND_ICON_ERROR],      "galaxy-s" },
    { &gProgressBarIndeterminate[0],    "indeterminate1" },
    { &gProgressBarIndeterminate[1],    "indeterminate2" },
    { &gProgressBarIndeterminate[2],    "indeterminate3" },
    { &gProgressBarIndeterminate[3],    "indeterminate4" },
    { &gProgressBarIndeterminate[4],    "indeterminate5" },
    { &gProgressBarIndeterminate[5],    "indeterminate6" },
    { &gProgressBarEmpty,               "progress_empty" },
    { &gProgressBarFill,                "progress_fill" },
    { NULL,                             NULL },
};

static gr_surface gCurrentIcon = NULL;

static enum ProgressBarType {
    PROGRESSBAR_TYPE_NONE,
    PROGRESSBAR_TYPE_INDETERMINATE,
    PROGRESSBAR_TYPE_NORMAL,
} gProgressBarType = PROGRESSBAR_TYPE_NONE;

// Progress bar scope of current operation
static float gProgressScopeStart = 0, gProgressScopeSize = 0, gProgress = 0;
static time_t gProgressScopeTime, gProgressScopeDuration;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

// Log text overlay, displayed when a magic key is pressed
static char text[MAX_ROWS][MAX_COLS];
static int text_cols = 0, text_rows = 0;
static int text_col = 0, text_row = 0, text_top = 0;
static int show_text = 0;

static char menu[MAX_ROWS][MAX_COLS];
static int show_menu = 0;
static int menu_top = 0, menu_items = 0, menu_sel = 0;

// Key event input queue
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
static int key_queue[256], key_queue_len = 0;
static volatile char key_pressed[KEY_MAX + 1];

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(gr_surface icon)
{
    gPagesIdentical = 0;
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        int iconWidth = gr_get_width(icon);
        int iconHeight = gr_get_height(icon);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(icon, 0, 0, iconWidth, iconHeight, iconX, iconY);
    }
}

// Draw the progress bar (if any) on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_progress_locked()
{
    if (gProgressBarType == PROGRESSBAR_TYPE_NONE) return;

    int iconHeight = gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]);
    int width = gr_get_width(gProgressBarEmpty);
    int height = gr_get_height(gProgressBarEmpty);

    int dx = (gr_fb_width() - width)/2;
    int dy = (3*gr_fb_height() + iconHeight - 2*height)/4;

    // Erase behind the progress bar (in case this was a progress-only update)
    gr_color(0, 0, 0, 255);
    gr_fill(dx, dy, width, height);

    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL) {
        float progress = gProgressScopeStart + gProgress * gProgressScopeSize;
        int pos = (int) (progress * width);

        if (pos > 0) {
          gr_blit(gProgressBarFill, 0, 0, pos, height, dx, dy);
        }
        if (pos < width-1) {
          gr_blit(gProgressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
        }
    }

    if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
        static int frame = 0;
        gr_blit(gProgressBarIndeterminate[frame], 0, 0, width, height, dx, dy);
        frame = (frame + 1) % PROGRESSBAR_INDETERMINATE_STATES;
    }
}

static void draw_text_line(int row, const char* t) {
  if (t[0] != '\0') {
    gr_text(0, (row+1)*CHAR_HEIGHT-1, t);
  }
}

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_screen_locked(void)
{
    draw_background_locked(gCurrentIcon);
    draw_progress_locked();

    if (show_text) {
        gr_color(0, 0, 0, 160);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int i = 0;
        if (show_menu) {
            //gr_color(64, 96, 255, 255);		// <-- MENU TEXT COLOR
            gr_color(0, 128, 0, 255);		// <-- MENU TEXT COLOR
            gr_fill(0, (menu_top+menu_sel) * CHAR_HEIGHT,
                    gr_fb_width(), (menu_top+menu_sel+1)*CHAR_HEIGHT+1);

            for (; i < menu_top + menu_items; ++i) {
                if (i == menu_top + menu_sel) {
                    gr_color(255, 255, 255, 255);
                    draw_text_line(i, menu[i]);
                    //gr_color(64, 96, 255, 255);	// <-- MENU TEXT COLOR
                    gr_color(0, 128, 0, 255);	// <-- MENU TEXT COLOR
                } else {
                    draw_text_line(i, menu[i]);
                }
            }
            gr_fill(0, i*CHAR_HEIGHT+CHAR_HEIGHT/2-1,
                    gr_fb_width(), i*CHAR_HEIGHT+CHAR_HEIGHT/2+1);
            ++i;
        }

        //gr_color(255, 255, 0, 255);		// <-- OUTPUT TEXT COLOR
        gr_color(255, 255, 255, 255);		// <-- OUTPUT TEXT COLOR

        for (; i < text_rows; ++i) {
            draw_text_line(i, text[(i+text_top) % text_rows]);
        }
    }
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
static void update_screen_locked(void)
{
    draw_screen_locked();
    gr_flip();
}

// Updates only the progress bar, if possible, otherwise redraws the screen.
// Should only be called with gUpdateMutex locked.
static void update_progress_locked(void)
{
    if (show_text || !gPagesIdentical) {
        draw_screen_locked();    // Must redraw the whole screen
        gPagesIdentical = 1;
    } else {
        draw_progress_locked();  // Draw only the progress bar
    }
    gr_flip();
}

// Keeps the progress bar updated, even when the process is otherwise busy.
static void *progress_thread(void *cookie)
{
    for (;;) {
        usleep(1000000 / PROGRESSBAR_INDETERMINATE_FPS);
        pthread_mutex_lock(&gUpdateMutex);

        // update the progress bar animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE && !show_text) {
            update_progress_locked();
        }

        // move the progress bar forward on timed intervals, if configured
        int duration = gProgressScopeDuration;
        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && duration > 0) {
            int elapsed = time(NULL) - gProgressScopeTime;
            float progress = 1.0 * elapsed / duration;
            if (progress > 1.0) progress = 1.0;
            if (progress > gProgress) {
                gProgress = progress;
                update_progress_locked();
            }
        }

        pthread_mutex_unlock(&gUpdateMutex);
    }
    return NULL;
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void *input_thread(void *cookie)
{
    int rel_sum = 0;
    int fake_key = 0;
    for (;;) {
        // wait for the next key event
        struct input_event ev;
        do {
            ev_get(&ev, 0);

            if (ev.type == EV_SYN) {
                continue;
            } else if (ev.type == EV_REL) {
                if (ev.code == REL_Y) {
                    // accumulate the up or down motion reported by
                    // the trackball.  When it exceeds a threshold
                    // (positive or negative), fake an up/down
                    // key event.
                    rel_sum += ev.value;
                    if (rel_sum > 3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_DOWN;
                        ev.value = 1;
                        rel_sum = 0;
                    } else if (rel_sum < -3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_UP;
                        ev.value = 1;
                        rel_sum = 0;
                    }
                }
            } else {
                rel_sum = 0;
            }
        } while (ev.type != EV_KEY || ev.code > KEY_MAX);

        pthread_mutex_lock(&key_queue_mutex);
        if (!fake_key) {
            // our "fake" keys only report a key-down event (no
            // key-up), so don't record them in the key_pressed
            // table.
            key_pressed[ev.code] = ev.value;
        }
        fake_key = 0;
        const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
        if (ev.value > 0 && key_queue_len < queue_max) {
            key_queue[key_queue_len++] = ev.code;
            pthread_cond_signal(&key_queue_cond);
        }
        pthread_mutex_unlock(&key_queue_mutex);

        if (ev.value > 0 && device_toggle_display(key_pressed, ev.code)) {
            pthread_mutex_lock(&gUpdateMutex);
            show_text = !show_text;
            update_screen_locked();
            pthread_mutex_unlock(&gUpdateMutex);
        }

        if (ev.value > 0 && device_reboot_now(key_pressed, ev.code)) {
            reboot(RB_AUTOBOOT);
        }
    }
    return NULL;
}

void ui_init(void)
{
    gr_init();
    ev_init();

    text_col = text_row = 0;
    text_rows = gr_fb_height() / CHAR_HEIGHT;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    text_top = 1;

    text_cols = gr_fb_width() / CHAR_WIDTH;
    if (text_cols > MAX_COLS - 1) text_cols = MAX_COLS - 1;

    int i;
    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            if (result == -2) {
                LOGI("Bitmap %s missing header\n", BITMAPS[i].name);
            } else {
                LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
            }
            *BITMAPS[i].surface = NULL;
        }
    }

    pthread_t t;
    pthread_create(&t, NULL, progress_thread, NULL);
    pthread_create(&t, NULL, input_thread, NULL);
}

void ui_set_background(int icon)
{
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = gBackgroundIcon[icon];
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_indeterminate_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    if (gProgressBarType != PROGRESSBAR_TYPE_INDETERMINATE) {
        gProgressBarType = PROGRESSBAR_TYPE_INDETERMINATE;
        update_progress_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_progress(float portion, int seconds)
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart += gProgressScopeSize;
    gProgressScopeSize = portion;
    gProgressScopeTime = time(NULL);
    gProgressScopeDuration = seconds;
    gProgress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_progress(float fraction)
{
    pthread_mutex_lock(&gUpdateMutex);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && fraction > gProgress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(gProgressBarIndeterminate[0]);
        float scale = width * gProgressScopeSize;
        if ((int) (gProgress * scale) != (int) (fraction * scale)) {
            gProgress = fraction;
            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_reset_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NONE;
    gProgressScopeStart = gProgressScopeSize = 0;
    gProgressScopeTime = gProgressScopeDuration = 0;
    gProgress = 0;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_clear_text(void)
{
	int row = 0;
	int col = 0;
	
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {

		for(row = 0; row < text_rows; row++) {
			for(col = 0; col < text_cols; col++) {
				text[row][col] = '\0';
			}
		}
		
		update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 256, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);

    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_start_menu(char** headers, char** items, int initial_selection) {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        for (i = 0; i < text_rows; ++i) {
            if (headers[i] == NULL) break;
            strncpy(menu[i], headers[i], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_top = i;
        for (; i < text_rows; ++i) {
            if (items[i-menu_top] == NULL) break;
            strncpy(menu[i], items[i-menu_top], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_items = i - menu_top;
        show_menu = 1;
        menu_sel = initial_selection;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_menu_select(int sel) {
    int old_sel;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;
        if (menu_sel < 0) menu_sel = 0;
        if (menu_sel >= menu_items) menu_sel = menu_items-1;
        sel = menu_sel;
        if (menu_sel != old_sel) update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return sel;
}

void ui_end_menu() {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0 && text_rows > 0 && text_cols > 0) {
        show_menu = 0;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_text_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

void ui_show_text(int visible)
{
    pthread_mutex_lock(&gUpdateMutex);
    show_text = visible;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_wait_key()
{
    pthread_mutex_lock(&key_queue_mutex);
    while (key_queue_len == 0) {
        pthread_cond_wait(&key_queue_cond, &key_queue_mutex);
    }

    int key = key_queue[0];
    memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

int ui_key_pressed(int key)
{
    // This is a volatile static array, don't bother locking
    return key_pressed[key];
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}

//-----------------------------------------------------------------------------
// alloc_menu_list
//
// Allocates a char*[] array of strings for use with the recovery menu system.
// Caller responsible for releasing the returned pointer with free_menu_list()
//
// Arguments:
//
//	item	- First menu item string
//	...		- Variable length list of char* items to add to the list

char** alloc_menu_list(char* item, ...)
{
	va_list				marker;				// Variable argument marker
	char*				arg;				// Argument pointer
	size_t				cArgs = 0;			// Number of arguments
	char**			result = NULL;		// Allocated array of strings
	char**	resit;				// Result array iterator

	// Count the number of arguments provided so we can allocate the array
	arg = item;
	va_start(marker, item);

	while(arg != NULL) { cArgs++; arg = va_arg(marker, char*); }
	va_end(marker);

	// Allocate and initialize the array of string pointers
	result = (char**)calloc(cArgs + 1, sizeof(char*));
	if(result == NULL) return NULL;

	// Iterate over the arguments again, this time allocating the strings
	arg = item;
	resit = result;
	va_start(marker, item);

	while(arg != NULL) {

		// Allocate the string buffer and copy the data into it ...
		*resit = (char*)calloc(strlen(arg) + 1, sizeof(char));
		strcpy(*resit, arg);
		arg = va_arg(marker, char*);	// Move to the next argument
		resit++;
	}

	va_end(marker);				// Done processing varargs
	return result;				// Return allocated string array
}

//-----------------------------------------------------------------------------
// append_menu_list
//
// Appends a new item to an existing MenuList.  Original list will be
// released and cannot be used after calling this function.  This isn't
// done very efficiently, but it will suffice for current purposes
//
// Arguments:
//
//	list		- MenuList to append the new item to
//	item		- New string to append to the list

char** append_menu_list(char** list, char* item)
{
	char** 	iterator;		// List iterator
	int 				numitems = 0;	// New number of items
	char** 			result = NULL;	// Resultant MenuList
	char** 	resit;			// Result iterator
	
	if(list == NULL) return NULL;
	
	// Count the number of items in the existing list
	iterator = list;
	while(*iterator != NULL) { numitems++; iterator++; }
	
	// Allocate and initialize the new array of string pointers
	// +2 = one for the new string, one for the trailing NULL
	result = (char**)calloc(numitems + 2, sizeof(char*));
	if(result == NULL) return list;
	
	// Copy over all of the original string pointers first
	iterator = list;
	resit = result;
	while(*iterator != NULL) {
		
		*resit = *iterator;		// Copy the string pointer
		iterator++;				// Move to next source string
		resit++;				// Move to next destination string
	}
	
	// Allocate and copy the new item into the end of the array
	*resit = (char*)calloc(strlen(item) + 1, sizeof(char));
	strcpy(*resit, item);
	
	free(list);				// Release the old char* array	
	return result;			// Return the new list
}

//-----------------------------------------------------------------------------
// free_menu_list
//
// Releases a recovery menu list allocated with alloc_menu_list()
//
// Arguments:
//
//	list		- Pointer to list allocated by alloc_menu_list()

char** free_menu_list(char** list)
{
	char**	iterator = NULL;		// List iterator

	if(list == NULL) return NULL;		// List wasn't allocated

	// Walk the array and chase all the string pointers until the NULL
	// terminator is located ...
	iterator = list;
	while(*iterator != NULL) { free(*iterator); iterator++; }

	free(list);					// Release the array
	return NULL;				// Always return NULL
}

char** prepend_title(const char** headers) {
    char* title[] = { "Android system recovery <"
                          EXPAND(RECOVERY_API_VERSION) "e>",
                      "",
                      NULL };

    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 0;
    char** p;
    for (p = title; *p; ++p, ++count);
    for (p = headers; *p; ++p, ++count);

    char** new_headers = malloc((count+1) * sizeof(char*));
    char** h = new_headers;
    for (p = title; *p; ++p, ++h) *h = *p;
    for (p = headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int get_menu_selection(char** headers, char** items, int menu_only,
                   int initial_selection) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui_clear_key_queue();

    ui_start_menu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1;

    while (chosen_item < 0) {
        int key = ui_wait_key();
        int visible = ui_text_visible();

        int action = device_handle_key(key, visible);

        if (action < 0) {
            switch (action) {
                case HIGHLIGHT_UP:
                    --selected;
                    selected = ui_menu_select(selected);
                    break;
                case HIGHLIGHT_DOWN:
                    ++selected;
                    selected = ui_menu_select(selected);
                    break;
                case SELECT_ITEM:
                    chosen_item = selected;
                    break;
                case NO_ACTION:
                    break;
            }
        } else if (!menu_only) {
            chosen_item = action;
        }
    }

    ui_end_menu();
    return chosen_item;
}

// djp952: Different version of get_menu_selection that returns a navigation
// code rather than the selected index directly, and omits the menu_only
// and initial_selection variables
int navigate_menu(char** headers, char** items, int* selection)
{
    int 	selected = 0;		// Currently selected item
    int 	nav = -1;			// Result from this function
    
    *selection = -1;			// Initialize [OUT] variable
    
    ui_clear_key_queue();				// Clear the input queue
    ui_start_menu(headers, items, 0);	// Start the menu

	do {
		
		// Grab the next action by reading/translating the keystoke
		// An action key was pressed, translate/handle it
		switch(device_handle_key(ui_wait_key(), ui_text_visible())) {
			
			// HIGHLIGHT_UP - Move to the previous menu item
			case HIGHLIGHT_UP:
				selected = ui_menu_select(--selected);
				break;
				
			// HIGHLIGHT_DOWN - Move to the next menu item
			case HIGHLIGHT_DOWN:
				selected = ui_menu_select(++selected);
				break;
				
			// SELECT_ITEM - Select the currently highlighted item
			case SELECT_ITEM:
				*selection = selected;
				nav = NAVIGATE_SELECT;
				break;
				
			// SELECT_BACK - Go BACK to the previous menu
			case SELECT_BACK:
				nav = NAVIGATE_BACK;
				break;
				
			// SELECT_HOME - Go to the HOME menu
			case SELECT_HOME:
				nav = NAVIGATE_HOME;
				break;
		}

	} while(nav < 0);
	
	ui_end_menu();					// Clean up the menu; done
	return nav;						// Return navigation code
}

static int compare_string_insensitive(const void* a, const void* b) {
    return strcasecmp(*(const char**)a, *(const char**)b);
}

//-----------------------------------------------------------------------------
// browse_include_subdirectory
//
// Determines if a subdirectory should be included in the browser menu.  The
// directory must contain file(s) that match the specified filter or have
// additional subdirectories underneath it to be included
//
// Arguments:
//
//	path		- Path to the directory to check for inclusion
//	filter		- Browser filter string

static int browse_include_subdirectory(const char* path, const char* filter)
{
	DIR*				dir;			// Current directory
	struct dirent*		dirent;			// Current directory entries
	char 				temp[256];		// Temporary string data
	int 				include = 0;	// Flag to include this directory
	
	// Attempt to open the specified directory
	dir = opendir(path);
	if(dir == NULL) return 0;

	// Iterate over the items in this directory ...
	while((dirent = readdir(dir)) != NULL) {
		
		int name_len = strlen(dirent->d_name);
		
		// DIRECTORY
		if(dirent->d_type == DT_DIR) {
			
			// Skip "." and ".." entries
			if((name_len == 1) && (dirent->d_name[0] == '.')) continue;
			if((name_len == 2) && (dirent->d_name[0] == '.') && (dirent->d_name[1] == '.')) continue;
			
			// Show this directory since at least one subdirectory exists
			include = 1;
			break;
		}
		
		// REGULAR FILE
		else if(dirent->d_type == DT_REG) {
			
			// Check the file against the filter if one was specified, and if
			// a match is found, include this subdirectory
			if((!filter) || (fnmatch(filter, dirent->d_name, FNM_FILE_NAME | FNM_IGNORECASE) == 0))
			{
				include = 1;
				break;
			}
		}		
	}

	closedir(dir);						// Close the directory
	return include;						// Return inclusion status
}

//-----------------------------------------------------------------------------
// navigate_menu_browse
//
// Specialized navigation menu used to browse for a file
//
// Arguments:
//
//	headers		- Menu headers
//	root		- Optional root directory (NULL for /)
//	filter		- Optional file filter (NULL for all files)
//	filename	- Buffer to receive the selected file name
//	cchfilename	- Length of the filename buffer in characters
//
// Return Values:
//
//	NAVIGATE_SELECT : A file was selected and the path/name is in filename
//  NAVIGATE_BACK   : The BACK button was pressed to exit the browser
//  NAVIGATE_HOME   : The HOME button was pressed to exit the browser

int navigate_menu_browse(char** headers, const char* root, const char* filter,
	char* filename, int cch)
{
	DIR*				dir;			// Current directory
	char**				dirs;			// Directory menu items
	char**				files;			// File menu items
	struct dirent*		dirent;			// Current directory entries
	char 				temp[256];		// Temporary string buffer
	int 				index;			// Loop index variable
	int 				selection;		// Selected menu item
	int 				nav;			// Return navigation code
	
	// Initialize the output buffer.  This will get done multiple times
	// when recursing directories, and I'm OK with that
	memset(filename, 0, cch * sizeof(char));
	
	// Open the specified directory
	dir = opendir((root) ? root : "/");
	if(dir == NULL) {
		
		LOGE("navigate_menu_browse: Unable to open directory %s\n", (root) ? root : "/");
		return NAVIGATE_ERROR;
	}

	// Allocate an empty menu list that we will append all the directories to
	dirs = alloc_menu_list(NULL);
	if(dirs == NULL) {
		
		LOGE("navigate_menu_browse: Cannot allocate menu items");
		closedir(dir);
		return NAVIGATE_ERROR;
	}
	
	// Allocate an empty menu list that we will append all the files to
	files = alloc_menu_list(NULL);
	if(files == NULL) {
		
		LOGE("navigate_menu_browse: Cannot allocate file menu items");
		free_menu_list(dirs);
		closedir(dir);
		return NAVIGATE_ERROR;
	}

	// Iterate over and process all the items in this directory ...
	while((dirent = readdir(dir)) != NULL) {
		
		int name_len = strlen(dirent->d_name);
		
		// DIRECTORY
		if(dirent->d_type == DT_DIR) {
			
			// Skip "." and ".." entries
			if((name_len == 1) && (dirent->d_name[0] == '.')) continue;
			if((name_len == 2) && (dirent->d_name[0] == '.') && (dirent->d_name[1] == '.')) continue;
			
			// Verify that this directory should be included in the browse list
			sprintf(temp, "%s/%s", (root) ? root : "/", dirent->d_name);
			if(!browse_include_subdirectory(temp, filter)) continue;
		
			sprintf(temp, "%s/", dirent->d_name);
			dirs = append_menu_list(dirs, temp);
		}
		
		// REGULAR FILE
		else if(dirent->d_type == DT_REG) {
			
			// Check the file against the filter if one was specified
			if((!filter) || (fnmatch(filter, dirent->d_name, FNM_FILE_NAME | FNM_IGNORECASE) == 0))
				files = append_menu_list(files, dirent->d_name);
		}		
	}
	
	closedir(dir);							// Close the directory
	
	// Sort the directory and file lists
	qsort(dirs, len_menu_list(dirs), sizeof(char*), compare_string_insensitive);
	qsort(files, len_menu_list(files), sizeof(char*), compare_string_insensitive);	
	
	// Append the file list to the directory list
	for(index = 0; index < len_menu_list(files); index++)
		dirs = append_menu_list(dirs, files[index]);
		
	do {
		
		// Navigate the generated directory menu.  If the user chooses anything
		// other than SELECT, break the loop and return to caller
		nav = navigate_menu(headers, dirs, &selection);
		if(nav != NAVIGATE_SELECT) break;
		
		// We need the selected string to determine if this is a directory or file
		char* selected = dirs[selection];
		int sel_len = strlen(selected);
		
		// A directory was selected, recursively call this function with a new root
		if(selected[sel_len - 1] == '/') {
			
			sprintf(temp, "%s/%s", (root) ? root : "/", selected);
			nav = navigate_menu_browse(headers, temp, filter, filename, cch);
		}
		
		// A file was selected, set the output string buffer and we're done
		else snprintf(filename, cch, "%s/%s", (root) ? root : "/", selected);
		
	} while(nav == NAVIGATE_BACK);
	
	free_menu_list(files);					// Release allocated menu items
	free_menu_list(dirs);					// Release allocated menu items
	
	return nav;								// Return navigation code
}

int len_menu_list(char** list)
{
	char** 	iterator;
	int 	len = 0;
	
	if(list == NULL) return 0;
	
	iterator = list;
	while(*iterator != NULL) { len++; iterator++; }
	
	return len;
}

// TODO: this isn't exactly all that foolproof, but it will work for now
void change_menu_list_item(char** list, int index, const char* item)
{
	char** 	iterator;
	
	if(list == NULL) return;
	
	iterator = list;
	while((*iterator != NULL) && (index > 0)) { 
		
		iterator++;
		index--;
	}
	
	if(*iterator) {
		free(*iterator);
		*iterator = (char*)calloc(strlen(item) + 1, sizeof(char));
		strcpy(*iterator, item);
	}
}

//-----------------------------------------------------------------------------