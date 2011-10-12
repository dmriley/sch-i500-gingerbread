/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/limits.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#include "bootloader.h"
#include "common.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "volume.h"
#include "deviceui.h"
#include "ui.h"						// Include UI declarations
#include "menus.h"
#include "commands.h"

void show_format_submenu(void);

extern struct fs_info info;				// <--- For make_ext4fs

static const struct option OPTIONS[] = {
  { "send_intent", required_argument, NULL, 's' },
  { "update_package", required_argument, NULL, 'u' },
  { "wipe_data", no_argument, NULL, 'w' },
  { "wipe_cache", no_argument, NULL, 'c' },
  { "show_text", no_argument, NULL, 't' },
  { NULL, 0, NULL, 0 },
};

static const char *COMMAND_FILE = "/cache/recovery/command";
static const char *INTENT_FILE = "/cache/recovery/intent";
static const char *LOG_FILE = "/cache/recovery/log";
static const char *LAST_LOG_FILE = "/cache/recovery/last_log";
static const char *SDCARD_ROOT = "/sdcard";
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";
static const char *SIDELOAD_TEMP_DIR = "/tmp/sideload";

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 *
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

//
// MOVED FROM ROOTS.C
//

//-----------------------------------------------------------------------------
// ensure_path_mounted
//
// Attempts to mount the volume for the specified path if its not already mounted
//
// Arguments:
//
//	path	- Path to locate in the global volume table

int ensure_path_mounted(const char* path) 
{
    const Volume* volume = get_volume_for_path(path);
	if(volume == NULL) {
		
		ui_print("RECOVERY::ensure_path_mounted: Unable to locate a volume suitable for path %s\n", path);
		return -1;
	}
	
	return mount_volume(volume, NULL);		// Mount the volume if necessary
}

//-----------------------------------------------------------------------------
// ensure_path_unmounted
//
// Attempts to unmount the volume for the specified path if its already mounted
//
// Arguments:
//
//	path	- Path to locate in the global volume table

int ensure_path_unmounted(const char* path) 
{
    const Volume* volume = get_volume_for_path(path);
	if(volume == NULL) {
		
		ui_print("RECOVERY::ensure_path_unmounted: Unable to locate a volume suitable for path %s\n", path);
		return -1;
	}
	
	return unmount_volume(volume, NULL);		// Mount the volume if necessary
}

// open a given path, mounting partitions as necessary
static FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1);

    FILE *fp = fopen(path, mode);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", sizeof(boot.status), boot.status);
    }

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                (*argv)[*argc] = strdup(strtok(buf, "\r\n"));  // Strip newline.
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    set_bootloader_message(&boot);
}

static void
set_sdcard_update_bootloader_message() {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
static long tmplog_offset = 0;

static void
copy_log_file(const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if (log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(TEMPORARY_LOG_FILE, "r");
        if (tmplog == NULL) {
            LOGE("Can't open %s\n", TEMPORARY_LOG_FILE);
        } else {
            if (append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if (append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, TEMPORARY_LOG_FILE);
        }
        check_and_fclose(log, destination);
    }
}

//-----------------------------------------------------------------------------
// erase_volume
//
// Erases (formats) the specified volume
//
// Arguments:
//
//	volume		- Volume to be erased
//	fs			- Optional file system type to format volume with; NULL for default

static int erase_volume(const Volume* volume, const char* fs) 
{
	// Invariant: volume must point to a valid Volume structure
	if(volume == NULL) {
		
		ui_print("RECOVERY::erase_volume: volume is NULL\n");
		return -1;
	}
	
	// If no specific file system was provided, use the default
	if(fs == NULL) fs = volume->fs_type;
	
	// TODO: I don't think these are working right still
    ui_set_background(BACKGROUND_ICON_INSTALLING);
    ui_show_indeterminate_progress();
	
    ui_print("Formatting %s (%s) ...\n", volume->name, fs);

	// If the CACHE volume is formatted, the log written to it thus
	// far will be deleted.  Reset the pointer to copy from beginning
    if (strcmp(volume->name, "CACHE") == 0) tmplog_offset = 0;

    return format_volume(volume, fs);		// Format the volume
}


// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
static void
finish_recovery(const char *send_intent) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    // Copy logs to cache so the system can find out what happened.
    copy_log_file(LOG_FILE, true);
    copy_log_file(LAST_LOG_FILE, false);
    chmod(LAST_LOG_FILE, 0640);

    // Reset to mormal system boot so recovery won't cycle indefinitely.
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    set_bootloader_message(&boot);

    // Remove the command file, so recovery won't repeat indefinitely.
    if (ensure_path_mounted(COMMAND_FILE) != 0 ||
        (unlink(COMMAND_FILE) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    sync();  // For good measure.
}


static char*
copy_sideloaded_package(const char* original_path) {
  if (ensure_path_mounted(original_path) != 0) {
    LOGE("Can't mount %s\n", original_path);
    return NULL;
  }

  if (ensure_path_mounted(SIDELOAD_TEMP_DIR) != 0) {
    LOGE("Can't mount %s\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }

  if (mkdir(SIDELOAD_TEMP_DIR, 0700) != 0) {
    if (errno != EEXIST) {
      LOGE("Can't mkdir %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
      return NULL;
    }
  }

  // verify that SIDELOAD_TEMP_DIR is exactly what we expect: a
  // directory, owned by root, readable and writable only by root.
  struct stat st;
  if (stat(SIDELOAD_TEMP_DIR, &st) != 0) {
    LOGE("failed to stat %s (%s)\n", SIDELOAD_TEMP_DIR, strerror(errno));
    return NULL;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOGE("%s isn't a directory\n", SIDELOAD_TEMP_DIR);
    return NULL;
  }
  if ((st.st_mode & 0777) != 0700) {
    LOGE("%s has perms %o\n", SIDELOAD_TEMP_DIR, st.st_mode);
    return NULL;
  }
  if (st.st_uid != 0) {
    LOGE("%s owned by %lu; not root\n", SIDELOAD_TEMP_DIR, st.st_uid);
    return NULL;
  }

  char copy_path[PATH_MAX];
  strcpy(copy_path, SIDELOAD_TEMP_DIR);
  strcat(copy_path, "/package.zip");

  char* buffer = malloc(BUFSIZ);
  if (buffer == NULL) {
    LOGE("Failed to allocate buffer\n");
    return NULL;
  }

  size_t read;
  FILE* fin = fopen(original_path, "rb");
  if (fin == NULL) {
    LOGE("Failed to open %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }
  FILE* fout = fopen(copy_path, "wb");
  if (fout == NULL) {
    LOGE("Failed to open %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  while ((read = fread(buffer, 1, BUFSIZ, fin)) > 0) {
    if (fwrite(buffer, 1, read, fout) != read) {
      LOGE("Short write of %s (%s)\n", copy_path, strerror(errno));
      return NULL;
    }
  }

  free(buffer);

  if (fclose(fout) != 0) {
    LOGE("Failed to close %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  if (fclose(fin) != 0) {
    LOGE("Failed to close %s (%s)\n", original_path, strerror(errno));
    return NULL;
  }

  // "adb push" is happy to overwrite read-only files when it's
  // running as root, but we'll try anyway.
  if (chmod(copy_path, 0400) != 0) {
    LOGE("Failed to chmod %s (%s)\n", copy_path, strerror(errno));
    return NULL;
  }

  return strdup(copy_path);
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

//-----------------------------------------------------------------------------
// wipe_data
//
// Wipes all user data from the device
//
// Arguments:
//
//	confirm		- Flag to confirm the operation or not

static void wipe_data(int confirm) 
{
	const Volume*		iterator;		// Volume iterator
	
    if (confirm) {
        static char** title_headers = NULL;

        if (title_headers == NULL) {
            char* headers[] = { "Confirm wipe of all user data?",
                                "  THIS CAN NOT BE UNDONE.",
                                "",
                                NULL };
            title_headers = prepend_title((const char**)headers);
        }

        char* items[] = { " No",
                          " No",
                          " No",
                          " No",
                          " No",
                          " No",
                          " No",
                          " Yes -- delete all user data",   // [7]
                          " No",
                          " No",
                          " No",
                          NULL };

        int chosen_item = get_menu_selection(title_headers, items, 1, 0);
        if (chosen_item != 7) {
            return;
        }
    }

    ui_print("\n-- Wiping data...\n");
    device_wipe_data();
	
	// Iterate over all of the FSTAB volumes and format each one marked as WIPE
	// with the default primary file system ...
	iterator = foreach_volume(NULL);
	while(iterator != NULL) {
		
		if(*iterator->wipe == '1') erase_volume(iterator, iterator->fs_type);
		iterator = foreach_volume(iterator);		
	}
	
    //erase_volume(get_volume("DATA"), NULL);
    //erase_volume(get_volume("DBDATA"), NULL);
    //erase_volume(get_volume("FOTA"), NULL);
    //erase_volume(get_volume("CACHE"), NULL);
    
    ui_print("Data wipe complete.\n");
}

// static void
// prompt_and_wait() {
//     
// 	char** headers = prepend_title((const char**)MENU_HEADERS);
// 
// 	int result;
// 	
//     for (;;) {
//         finish_recovery(NULL);
//         ui_reset_progress();
// 
//         int chosen_item = get_menu_selection(headers, MENU_ITEMS, 0, 0);
// 
//         // device-specific code may take some action here.  It may
//         // return one of the core actions handled in the switch
//         // statement below.
//         chosen_item = device_perform_action(chosen_item);
// 
//         switch (chosen_item) {
//             case ITEM_REBOOT:
//                 return;
// 
//             case ITEM_WIPE_DATA:
//                 wipe_data(ui_text_visible());
//                 if (!ui_text_visible()) return;
//                 break;
// 
//             case ITEM_WIPE_CACHE:
//                 ui_print("\n-- Wiping cache...\n");
//                 erase_volume(get_volume("CACHE"), NULL);
//                 ui_print("Cache wipe complete.\n");
//                 if (!ui_text_visible()) return;
//                 break;
// 
//             case ITEM_APPLY_SDCARD:
//                 ;
//                 int status = sdcard_directory(SDCARD_ROOT);
//                 if (status >= 0) {
//                     if (status != INSTALL_SUCCESS) {
//                         ui_set_background(BACKGROUND_ICON_ERROR);
//                         ui_print("Installation aborted.\n");
//                     } else if (!ui_text_visible()) {
//                         return;  // reboot if logs aren't visible
//                     } else {
//                         ui_print("\nInstall from sdcard complete.\n");
//                     }
//                 }
//                 break;
// 				
// 			case ITEM_TEST_BACKUP:
// 				
// 				result = ensure_path_mounted(SDCARD_ROOT);
// 				if(result != 0) {
// 					
// 					ui_print("MOUNT SDCARD: %d\n", result);
// 					break;
// 				}
// 				
// 				result = backup_volume(get_volume("SYSTEM"), "/sdcard/system.img");
// 				if(result != 0) {
// 					
// 					ui_print("BACKUP FAILED. RC = %d\n", result);
// 				}
// 				else ui_print("BACKUP SUCCESS!\n");
// 				
// 				ensure_path_unmounted(SDCARD_ROOT);
// 				break;
// 				
// 			case ITEM_TEST_RESTORE:
// 				
// 				result = ensure_path_mounted(SDCARD_ROOT);
// 				if(result != 0) {
// 					
// 					ui_print("MOUNT SDCARD: %d\n", result);
// 					break;
// 				}
// 				
// 				result = restore_volume(get_volume("SYSTEM"), "/sdcard/system.img");
// 				if(result != 0) {
// 					
// 					ui_print("RESTORE FAILED. RC = %d\n", result);
// 				}
// 				else ui_print("RESTORE SUCCESS!\n");
// 				
// 				ensure_path_unmounted(SDCARD_ROOT);
// 				break;
// 				
// 			case ITEM_ADVANCED:
// 				
// 				//result = erase_volume(get_volume("SYSTEM"), NULL);
// 				//if(result != 0) {
// 				//	
// 				//	ui_print("FAILED TO WIPE SYSTEM: %d\n", result);
// 				//}
// 				//else ui_print("SYSTEM WIPED! BETTER FLASH SOMETHING NOW!!\n");
// 				//break;
// 				
// 				menu_advanced();
// 				break;
//         }
//     }
// }

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

//-----------------------------------------------------------------------------
// main
//
// Main application entry point
//
// Arguments:
//
//		argc 	- Number of command line arguments
//		argv	- Array of command line argument pointers

int main(int argc, char **argv)
{
    time_t start = time(NULL);

    // If these fail, there's not really anywhere to complain...
    freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);
    printf("Starting recovery on %s", ctime(&start));

    ui_init();
    ui_set_background(BACKGROUND_ICON_INSTALLING);

    // djp952 - at least for now I want to see everything
    ui_show_text(1);

    volumes_init("/sbin/recovery.fstab");
    ////get_args(&argc, &argv);
	
    int previous_runs = 0;
    const char *send_intent = NULL;
    const char *update_package = NULL;
    int wipe_data = 0, wipe_cache = 0;
    int toggle_secure_fs = 0;
	
	// Display the basic recovery usage information to the user
	cmd_show_usage();
	
	// TEMP: Dump arguments to see what they look like
	//int index = 0;
	//for(index = 0; index < argc; index++) ui_print("ARG%d = [%s]\n", index, argv[index]);

    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 'p': previous_runs = atoi(optarg); break;
        case 's': send_intent = optarg; break;
        case 'u': update_package = optarg; break;
        case 'w': wipe_data = wipe_cache = 1; break;
        case 'c': wipe_cache = 1; break;
        case 't': ui_show_text(1); break;
        case '?':
            LOGE("Invalid command argument\n");
            continue;
        }
    }

    device_recovery_start();

    printf("Command:");
    for (arg = 0; arg < argc; arg++) {
        printf(" \"%s\"", argv[arg]);
    }
    printf("\n");

    if (update_package) {
        // For backwards compatibility on the cache partition only, if
        // we're given an old 'root' path "CACHE:foo", change it to
        // "/cache/foo".
        if (strncmp(update_package, "CACHE:", 6) == 0) {
            int len = strlen(update_package) + 10;
            char* modified_path = malloc(len);
            strlcpy(modified_path, "/cache/", len);
            strlcat(modified_path, update_package+6, len);
            printf("(replacing path \"%s\" with \"%s\")\n",
                   update_package, modified_path);
            update_package = modified_path;
        }
    }
    printf("\n");
	
    property_list(print_property, NULL);
    printf("\n");

//    int status = INSTALL_ERROR;
//
//     if (update_package != NULL) {
//         status = install_package(update_package);
//         if (status != INSTALL_SUCCESS) ui_print("Installation aborted.\n");
//     } else if (wipe_data) {
//         //if (device_wipe_data()) status = INSTALL_ERROR;
//         if (erase_volume(get_volume("DATA"), NULL)) status = INSTALL_ERROR;
//         if (erase_volume(get_volume("DBDATA"), NULL)) status = INSTALL_ERROR;
//         if (erase_volume(get_volume("FOTA"), NULL)) status = INSTALL_ERROR;
//         if (wipe_cache && erase_volume(get_volume("CACHE"), NULL)) status = INSTALL_ERROR;
//         if (status != INSTALL_SUCCESS) ui_print("Data wipe failed.\n");
//     } else if (wipe_cache) {
//         if (wipe_cache && erase_volume(get_volume("CACHE"), NULL)) status = INSTALL_ERROR;
//         if (status != INSTALL_SUCCESS) ui_print("Cache wipe failed.\n");
//     } else {
//         status = INSTALL_ERROR;  // No command specified
//     }

//    if (status != INSTALL_SUCCESS) ui_set_background(BACKGROUND_ICON_ERROR);
//    if (status != INSTALL_SUCCESS || ui_text_visible()) {
//       // prompt_and_wait();
//       menu_mainmenu();
//    }
	
	ui_set_background(BACKGROUND_ICON_ERROR);
	menu_mainmenu();

    // Otherwise, get ready to boot the main system...
    finish_recovery(send_intent);
    ui_print("Rebooting...\n");
    sync();
    reboot(RB_AUTOBOOT);
    return EXIT_SUCCESS;
}
