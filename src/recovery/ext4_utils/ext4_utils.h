/*
 * Copyright (C) 2010 The Android Open Source Project
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

#ifndef _EXT4_UTILS_H_
#define _EXT4_UTILS_H_

#include <sys/types.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int force;

// djp952: Replaced these macros to use the pointers in the info structure.  Macros
// error() and error_errno() in the original code are now warn() and warn_errno() as
// they claim to be non-fatal conditions.  Macros critical_error() and critical_error_errno()
// in the original code are now error() and error_errno(), and no longer invoke exit().
//
#define info(fmt, ...)  if(info.ui_stdout != NULL) { info.ui_stdout("EXT4: " fmt, ##__VA_ARGS__); }
#define warn(fmt, ...)  if(info.ui_stderr != NULL) { info.ui_stderr("EXT4 (%s) W: " fmt, __func__, ##__VA_ARGS__); }
#define warn_errno(s)   if(info.ui_stdout != NULL) { info.ui_stderr("EXT4 (%s) W: " s ": %s", __func__, strerror(errno)); }
#define error(fmt, ...) if(info.ui_stderr != NULL) { info.ui_stderr("EXT4 (%s) E: " fmt, __func__, ##__VA_ARGS__); }
#define error_errno(s)  if(info.ui_stdout != NULL) { info.ui_stderr("EXT4 (%s) E: " s ": %s", __func__, strerror(errno)); }
#define setprogress(percent) if(info.ui_progress != NULL) { info.ui_progress(percent / 100); }

#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_JNL_BACKUP_BLOCKS 1

#define min(a, b) ((a) < (b) ? (a) : (b))

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1)/(y))
#define ALIGN(x, y) ((y) * DIV_ROUND_UP((x), (y)))

#define __le64 u64
#define __le32 u32
#define __le16 u16

#define __be64 u64
#define __be32 u32
#define __be16 u16

#define __u64 u64
#define __u32 u32
#define __u16 u16
#define __u8 u8

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short int u16;
typedef unsigned char u8;

// djp952: Added function pointer types to provide output and progress

// EXT4UTILS_PRINTF - Defines a callback used to output a formatted string
typedef void(*EXT4UTILS_PRINTF)(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

// EXT4UTILS_PROGRESS - Defines a callback used to indicate progress (based on 100%)
typedef void(*EXT4UTILS_PROGRESS)(float percent);

struct block_group_info;

struct ext2_group_desc {
	__le32 bg_block_bitmap;
	__le32 bg_inode_bitmap;
	__le32 bg_inode_table;
	__le16 bg_free_blocks_count;
	__le16 bg_free_inodes_count;
	__le16 bg_used_dirs_count;
	__le16 bg_pad;
	__le32 bg_reserved[3];
};

struct fs_info {
	u64 len;
	u32 block_size;
	u32 blocks_per_group;
	u32 inodes_per_group;
	u32 inode_size;
	u32 inodes;
	u32 journal_blocks;
	u16 feat_ro_compat;
	u16 feat_compat;
	u16 feat_incompat;
	const char *label;
	u8 no_journal;
	
	// djp952: additional members specific to this version of ext4_utils
	// used to replace stdout/stderr and provide a progress callback
	
	EXT4UTILS_PRINTF 	ui_stdout;
	EXT4UTILS_PRINTF 	ui_stderr;
	EXT4UTILS_PROGRESS 	ui_progress;
};

struct fs_aux_info {
	struct ext4_super_block *sb;
	struct ext2_group_desc *bg_desc;
	struct block_group_info *bgs;
	u32 first_data_block;
	u64 len_blocks;
	u32 inode_table_blocks;
	u32 groups;
	u32 bg_desc_blocks;
	u32 bg_desc_reserve_blocks;
	u32 default_i_flags;
	u32 blocks_per_ind;
	u32 blocks_per_dind;
	u32 blocks_per_tind;
};

extern struct fs_info info;
extern struct fs_aux_info aux_info;

static inline int log_2(int j)
{
	int i;

	for (i = 0; j > 0; i++)
		j >>= 1;

	return i - 1;
}

int ext4_bg_has_super_block(int bg);
void write_ext4_image(const char *filename, int gz, int sparse);
int ext4_create_fs_aux_info(void);
void ext4_free_fs_aux_info(void);
int ext4_fill_in_sb(void);
void ext4_create_resize_inode(void);
void ext4_create_journal_inode(void);
void ext4_update_free(void);
u64 get_file_size(const char *filename);
u64 parse_num(const char *arg);

#endif
