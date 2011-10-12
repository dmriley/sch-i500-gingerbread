//-----------------------------------------------------------------------------
// restore.c
//
// Samsung Galaxy S CDMA SCH-I500 Recovery
// Restore Operations
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
// Portions based on SIMG2IMG utility, part of the Android Open Source Project
//
// Portions based on UNYAFFS utility, Created by Kai Wei <kai.wei.cn@gmail.com>
// 		This program is free software; you can redistribute it and/or modify
//		it under the terms of the GNU General Public License version 2 as
//		published by the Free Software Foundation.
//
// Modifications: Copyright (C) 2011 Michael Brehm
//-----------------------------------------------------------------------------

// Must #define these before including the various header files
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64

// Must #define these before including the YAFFS2 header files
#define CONFIG_YAFFS_UTIL 
#define CONFIG_YAFFS_DOES_ECC

#include <sys/types.h>							// Include TYPES declarations
#include <sys/stat.h>							// Include STAT declarations
#include <sys/mman.h>							// Include MMAN declarations
#include <sys/time.h>							// Include TIME declarations
#include <unistd.h>								// Include UNISTD declarations
#include <fcntl.h>								// Include FCNTL declarations
#include <stdio.h>								// Include STDIO declarations
#include <string.h>								// Include STRING declarations
#include <stdlib.h>								// Include STDLIB declarations
#include <limits.h>								// Include LIMITS declarations
#include <zlib.h>								// Include ZLIB declarations
#include "ext4_utils/ext4_utils.h"				// Include EXT4_UTILS support
#include "ext4_utils/output_file.h"				// Include EXT4_UTILS support
#include "ext4_utils/sparse_format.h"			// Include EXT4_UTILS support
#include "ext4_utils/sparse_crc32.h"			// Include EXT4_UTILS support
#include "yaffs2/yaffs2/yaffs_ecc.h"			// Include YAFFS2 support
#include "yaffs2/yaffs2/yaffs_guts.h"			// Include YAFFS2 support
#include "yaffs2/yaffs2/yaffs_tagsvalidity.h"	// Include YAFFS2 support
#include "yaffs2/yaffs2/yaffs_packedtags2.h"	// Include YAFFS2 support
#include "restore.h"							// Include RESTORE declarations

//-----------------------------------------------------------------------------
// PRIVATE CONSTANTS / MACROS
//-----------------------------------------------------------------------------

// Constants for sparse EXT4 restore operation
// (From simg2img.c)
#define SIMG_COPY_BUF_SIZE 				(1024*1024)
#define SIMG_SPARSE_HEADER_MAJOR_VER 	1
#define SIMG_SPARSE_HEADER_LEN       	(sizeof(sparse_header_t))
#define SIMG_CHUNK_HEADER_LEN 			(sizeof(chunk_header_t))

// Constants for YAFFS2 restore operation
// (From unyaffs.c)
#define YAFFS2_CHUNK_SIZE 				2048
#define YAFFS2_SPARE_SIZE 				64
#define YAFFS2_MAX_OBJECTS 				50000
#define YAFFS2_YAFFS_OBJECTID_ROOT		1

//-----------------------------------------------------------------------------
// PRIVATE TYPE DECLARATIONS
//-----------------------------------------------------------------------------

// YAFFS2_STATE
//
// Rather than using a host of global variables like unyaffs does, I opted to
// switch the state of the YAFFS2 restore operation into a structure
typedef struct {
	
	unsigned char*		data;
	unsigned char*		chunk_data;
	unsigned char*		spare_data;
	char**				obj_list;
	int 				img_fd;
	gzFile 				img_gzfile;
	unsigned long long	img_size;
	int 				progress_modulo;
	ui_callbacks*		callbacks;

} YAFFS2_STATE;

//-----------------------------------------------------------------------------
// PRIVATE GLOBAL VARIABLES
//-----------------------------------------------------------------------------

// Data buffers for sparse EXT4 restore operation
// (From simg2img.c)
// TODO: put these in an EXT4_SPARSE_STATE structure or something like I did
// with the YAFFS2 stuff and pass them around to the various helpers?
//
static u8* g_simg_copybuf = NULL;		// Sparse EXT4 chunk buffer
static u8* g_simg_zerobuf = NULL;		// Sparse EXT4 empty chunk buffer

//-----------------------------------------------------------------------------
// PRIVATE FUNCTION PROTOTYPES
//-----------------------------------------------------------------------------

// SPARSE EXT4 helper function
static int simg_process_raw_chunk(gzFile *in, FILE *out, u32 blocks, u32 blk_sz, u32 *crc32);

// SPARSE EXT4 helper function
static int simg_process_skip_chunk(FILE *out, u32 blocks, u32 blk_sz, u32 *crc32);

// SPARSE EXT4 helper function
static int simg_validate_and_skip_image_header(int in_fd, sparse_header_t* sparse_header, 
	ui_callbacks* callbacks);

// YAFFS2 helper function
static int yaffs2_init_state(YAFFS2_STATE* state, const char* rootdir, ui_callbacks* callbacks);

// YAFFS2 helper function
static void yaffs2_process_chunk(YAFFS2_STATE* state);

// YAFFS2 helper function
static int yaffs2_read_chunk(YAFFS2_STATE* state);

// YAFFS2 helper function
static void yaffs2_term_state(YAFFS2_STATE* state);

//----------------------------------------------------------------------------
// restore_dump
//
// Writes a DUMP image to the specified volume
//
// Arguments:
//
//	imgfile			- Input image file name
//	volume			- Destination volume for the restore
//	callbacks		- Optional UI callbacks for messages and progress

int restore_dump(const char* imgfile, const Volume* volume)
{
	// Invoke the UI version with a NULL callback
	return restore_dump_ui(imgfile, volume, NULL);
}

//----------------------------------------------------------------------------
// restore_dump_ui
//
// Writes a DUMP image to the specified volume, with a UI callback for
// progress and STDOUT/STDERR messages
//
// Arguments:
//
//	imgfile			- Input image file name
//	volume			- Destination volume for the restore
//	callbacks		- Optional UI callbacks for messages and progress

int restore_dump_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks)
{
	unsigned char*		buffer;				// Data buffer
	gzFile 				source;				// Source gzFile 
	unsigned long long	size;				// Size of output volume (for progress)
	int 				dest;				// Output file descriptor
	int 				read;				// Number of bytes read from source file
	unsigned long long	totalread = 0;		// Total number of bytes read from source
	int 				badthings = 0;		// Flag if bad things happened
	int 				result;				// Result from function call
	
	const int BUFFER_SIZE = 4096;			// Size of the read buffer
	
	USES_UI_CALLBACKS(callbacks);
	
	if(imgfile == NULL) return EINVAL;			// Invalid [in] argument
	if(volume == NULL) return EINVAL;			// Invalid [in] argument

	// Get the size of the output volume, but it's not the end of the world if we can't
	result = volume_size(volume, &size);
	if(result != 0) { UI_WARNING("Unable to determine output device size, progress indicator will not work\n"); }

	// Allocate the buffer memory
	buffer = malloc(BUFFER_SIZE);
	if(!buffer) { 
		
		UI_ERROR("Unable to allocate data buffer. EC = %d\n", errno); 
		return errno; 
	}

	// Attempt to open the input image file first before we do the output device
	source = gzopen(imgfile, "rb");
	if(source == Z_NULL) {
		
		UI_ERROR("Cannot open input file %s for read. EC = %d\n", imgfile, errno);
		free(buffer);
		return errno;
	}
	
	// Now attempt to open the output device
	dest = open(volume->device, O_WRONLY | O_CREAT);
	if(dest < 0) { 
		
		UI_ERROR("Cannot open output device %s for write. EC = %d\n", volume->device, errno);
		gzclose(source);
		free(buffer);
		return errno;
	}
	
	// Move the data ....
	read = gzread(source, buffer, BUFFER_SIZE);
	while(read > 0) {
	
		totalread += read;				// Tack on the number of bytes read
		
		// Write the buffer to the output device, and break the loop on an error
		result = write(dest, buffer, read);
		if(result < read) { 
			
			UI_ERROR("Unable to write data to output device %s. EC = %d\n", volume->device, errno);
			badthings = -1;
			break;
		}
		
		// If we know the size of the output device, we can provide a progress status
		if(size > 0) UI_SETPROGRESS((float)(totalread * 100) / (float)size);
		
		read = gzread(source, buffer, BUFFER_SIZE);		// Read next block of data
	}
	
	if(read < 0) { 
		
		UI_ERROR("Unable to read data from input file %s. EC = %d.\n", imgfile, errno); 
		badthings = -1;
	}
	
	close(dest);								// Close the output device
	gzclose(source);							// Close the input file
	free(buffer);								// Release buffer memory
	
	return (badthings) ? -1 : 0;
}

//----------------------------------------------------------------------------
// restore_ext4
//
// Writes a regular EXT4 image to the specified volume
//
// Arguments:
//
//	imgfile			- Input image file name
//	volume			- Destination volume for the restore

int restore_ext4(const char* imgfile, const Volume* volume)
{
	// Invoke the UI version of the function with a NULL callback
	return restore_ext4_ui(imgfile, volume, NULL);
}

//----------------------------------------------------------------------------
// restore_ext4_ui
//
// Writes a regular EXT4 image to the specified volume, with a UI callback
// for progress and STDOUT/STDERR messages
//
// Arguments:
//
//	imgfile			- Input image file name
//	volume			- Destination volume for the restore
//	callbacks		- Optional UI callbacks for messages and progress

int restore_ext4_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks)
{
	// EXT4 Images are essentially just dynamically created dump files ...
	return restore_dump_ui(imgfile, volume, callbacks);
}

//----------------------------------------------------------------------------
// restore_ext4_sparse
//
// Writes a sparse EXT4 image to a volume, with a UI callback for what would be
// typical STDOUT/STDERR messages
//
// Arguments:
//
//	imgfile			- Source image file
//	volume			- Destination volume for the restore

int restore_ext4_sparse(const char* imgfile, const Volume* volume)
{
	// Invoke the UI version of the function with a NULL callback
	return restore_ext4_sparse_ui(imgfile, volume, NULL);
}

//-----------------------------------------------------------------------------
// restore_ext4_sparse_ui
//
// Writes a sparse EXT4 image to a volume, with a UI callback for what would be
// typical STDOUT/STDERR messages
//
// Arguments:
//
//	imgfile			- Source image file
//	volume			- Destination volume for the restore
//	callbacks		- Optional UI callbacks for messages and progress

int restore_ext4_sparse_ui(const char* imgfile, const Volume* volume, ui_callbacks* callbacks)
{
	int 				in_fd;					// Input file handle
	gzFile*				in_gz;					// Input GZIP file handle
	FILE*				out;					// Output file handle
	unsigned int 		index;					// Loop index variable
	sparse_header_t 	sparse_header;			// EXT4 sparse image header
	chunk_header_t 		chunk_header;			// EXT4 sparse image chunk header
	u32					crc32 = 0;				// EXT4 sparse image CRC
	u32 				total_blocks = 0;		// EXT4 sparse image block count
	int 				blocks = 0;				// Return from block operation
	int 				result = 0;				// Result from function call
	
	USES_UI_CALLBACKS(callbacks);				// For UI_ERROR et al
	
	if(imgfile == NULL) return EINVAL;			// Invalid [in] argument
	if(volume == NULL) return EINVAL;			// Invalid [in] argument

	// The sparse image may have been compressed with GZIP, but the sparse
	// header is always written uncompressed first.  Open the file normally,
	// read and validate the header, then we can switch to a GZIP stream to
	// read the remainder of the data whether it's been compressed or not
	
	in_fd = open(imgfile, O_RDONLY);
	if(in_fd < 0) {
		
		UI_ERROR("Cannot open input file %s. EC = %d\n", imgfile, errno);
		return errno;
	}
	
	// Validate the input file header (does it's own STDERR callbacks)
	result = simg_validate_and_skip_image_header(in_fd, &sparse_header, callbacks);
	if(result != 0) { close(in_fd); return result; }
	
	// Reassociate the file with a GZIP stream from here onwards.  Don't duplicate
	// the file handle, we're done with it and GZIP can close it when it's finished
	in_gz = gzdopen(in_fd, "rb");
	if(in_gz == Z_NULL) {
		
		UI_ERROR("Cannot associate GZIP file with input file handle. EC = %d\n", errno); 
		close(in_fd);
		return errno; 
	}
	
	// Attempt to open the output device
	out = fopen(volume->device, "wb");
	if(out == NULL) {
	
		result = errno;
		UI_ERROR("Cannot open output device %s. EC = %d\n", volume->device, result);
		gzclose(in_gz);
		return result;
	}
	
	// Attempt to allocate the requisite data buffers
	g_simg_copybuf = calloc(SIMG_COPY_BUF_SIZE, sizeof(u8));
	g_simg_zerobuf = calloc(1, sparse_header.blk_sz);
	if(!g_simg_copybuf || !g_simg_zerobuf) {
		
		UI_ERROR("Insufficient memory available to allocate buffers\n");
		if(g_simg_copybuf) { free(g_simg_copybuf); g_simg_copybuf = NULL; }
		if(g_simg_zerobuf) { free(g_simg_zerobuf); g_simg_zerobuf = NULL; }
		fclose(out);
		gzclose(in_gz);		
		return ENOMEM;	
	}
	
	// All the invariants check out, start processing the image ...
	for(index = 0; index < sparse_header.total_chunks; index++) {
		
		// Update the progress of the restore operation
		UI_SETPROGRESS((float)index / ((float)sparse_header.total_chunks / 100));
		
		if(result != 0) break;				// Something bad happened
		
		// Read the next chunk header from the file
		if(gzread(in_gz, &chunk_header, sizeof(chunk_header)) == sizeof(chunk_header)) {

			// Skip the remaining bytes in a header that is longer than expected
			if(sparse_header.chunk_hdr_sz > SIMG_CHUNK_HEADER_LEN)
				gzseek(in_gz, sparse_header.chunk_hdr_sz - SIMG_CHUNK_HEADER_LEN, SEEK_CUR); 
			
			// Process the chunk
			switch(chunk_header.chunk_type) {
			
				case CHUNK_TYPE_RAW:
					if (chunk_header.total_sz != (sparse_header.chunk_hdr_sz + (chunk_header.chunk_sz * sparse_header.blk_sz)) ) {
						
						UI_ERROR("Bogus chunk size for chunk %d, type Raw\n", index);
						result = -1;
					}
					else {
						
						blocks = simg_process_raw_chunk(in_gz, out, chunk_header.chunk_sz, sparse_header.blk_sz, &crc32);
						if(blocks >= 0) total_blocks += blocks;
						else { UI_ERROR("A read/write error occurred copying a raw chunk\n"); result = -1; }
					}
					break;
					
				case CHUNK_TYPE_DONT_CARE:
					if (chunk_header.total_sz != sparse_header.chunk_hdr_sz) {
						
						UI_ERROR("Bogus chunk size for chunk %d, type=\"Dont Care\"\n", index);
						result = -1;
					}
					else total_blocks += simg_process_skip_chunk(out, chunk_header.chunk_sz, sparse_header.blk_sz, &crc32);
					break;
					
				default:
					UI_ERROR("Unknown chunk type 0x%4.4x\n", chunk_header.chunk_type);
					result = -1;
					break;
			}			
		}
		
		// Could not read the chunk from the image file
		else { 
			
			UI_ERROR("Error reading chunk header for chunk %d\n", index); 
			result = -1; 
			break;
		}	
	}

	// Clean up the memory buffers and file handles
	if(g_simg_zerobuf) { free(g_simg_zerobuf); g_simg_zerobuf = NULL; }	// Release buffer
	if(g_simg_copybuf) { free(g_simg_copybuf); g_simg_copybuf = NULL; }	// Release buffer
	fclose(out);														// Close output device
	gzclose(in_gz);														// Close input file

	// Execute some final invariant checks if everything went OK.  Nothing can be done
	// about it, so these are just warnings
	if(result == 0) {
		
		// Ensure that the total number of blocks written is correct
		if(sparse_header.total_blks != total_blocks)
			UI_WARNING("Wrote %d blocks, expected to write %d blocks\n", total_blocks, sparse_header.total_blks);
		
		// Ensure the the resultant checksum for the image file was correct
		if(sparse_header.image_checksum != crc32)
			UI_WARNING("Computed CRC32 of 0x%8.8x, expected 0x%8.8x\n", crc32, sparse_header.image_checksum);
	}
	
	return result;							// Return ultimate result code
}

//----------------------------------------------------------------------------
// restore_yaffs2
//
// Writes a YAFFS2 image to the specified directory
//
// Arguments:
//
//	imgfile			- Input image file name
//	directory		- Destination directory for the restore

int restore_yaffs2(const char* imgfile, const char* directory)
{
	// Invoke the UI version of the function with a NULL callback
	return restore_yaffs2_ui(imgfile, directory, NULL);
}

//----------------------------------------------------------------------------
// restore_yaffs2_ui
//
// Writes a YAFFS2 image to the specified directory, with a UI callback
// for progress and STDOUT/STDERR messages
//
// Arguments:
//
//	imgfile			- Input image file name
//	directory		- Destination directory for the restore operation
//	callbacks		- Optional UI callbacks for messages and progress

int restore_yaffs2_ui(const char* imgfile, const char* directory, ui_callbacks* callbacks)
{
	YAFFS2_STATE 		state;				// Shared restore operation state
	struct stat 		filestats;			// Image file statistics
	int 				result;				// Result from function call
	
	USES_UI_CALLBACKS(callbacks);
	
	if(!imgfile) return EINVAL;				// Invalid [in] pointer
	if(!directory) return EINVAL;			// Invalid [in] pointer

	// Allocate and initialize the YAFFS2_STATE structure that replaced all those globals ...
	result = yaffs2_init_state(&state, directory, callbacks);
	if(result != 0) return result;
	
	// In order to provide progress feedback, the current position within the image file is
	// used.  Since the image file may be compressed, we use both a standard file descriptor
	// as well as a gzFile.  The FD is closed automatically when the gzFile is closed
	
	state.img_fd = open(imgfile, O_RDONLY);
	if(state.img_fd != -1) {
	
		// Try to get the length of the image file for providing progress feedback
		result = fstat(state.img_fd, &filestats);
		if(result == 0) state.img_size = filestats.st_size;	

		// Now associate the file descriptor with a gzFile
		state.img_gzfile = gzdopen(state.img_fd, "rb");
		if(state.img_gzfile != Z_NULL) {
		
			// Restore the data to the destination directory and close the image file
			while(yaffs2_read_chunk(&state) != -1) yaffs2_process_chunk(&state);
			gzclose(state.img_gzfile);
			result = 0;
		}
		
		else { 		// gzdopen() failed, be sure to close the original FD ourselves

			UI_ERROR("restore_yaffs2_ui: Unable to associate source file descriptor with gzdopen(). EC = %d\n", errno);
			close(state.img_fd);
			result = errno;
		}
	}
	
	// Could not open the image file
	else { UI_ERROR("restore_yaffs2_ui: Unable to open source image file %s for read-only access.  EC = %d\n", imgfile, errno); }
	
	yaffs2_term_state(&state);				// Clean up the state information
	return result;							// Done!
}

//-----------------------------------------------------------------------------
// simg_process_raw_chunk (private)
//
// Processes a sparse EXT4 file chunk that contains data
// (From simg2img.c)
//
// Arguments:
//
//	in			- INPUT file pointer
//	out			- OUTPUT file pointer
//	blocks		- Number of blocks to process
//	blk_sz		- Size of each block
//	crc32		- Running CRC32 checksum

static int simg_process_raw_chunk(gzFile *in, FILE *out, u32 blocks, u32 blk_sz, u32 *crc32)
{
	u64 	len = (u64)blocks * blk_sz;			// Calculated data length
	int 	chunk;								// Calculated chunk size

	while (len) {
		
		// Determine the length of the chunk to read from input file and read it
		chunk = (len > SIMG_COPY_BUF_SIZE) ? SIMG_COPY_BUF_SIZE : len;
		if(gzread(in, g_simg_copybuf, chunk) < chunk) return -1;

		// Update the CRC32 value with the next chunk
		*crc32 = sparse_crc32(*crc32, g_simg_copybuf, chunk);
		
		// Write the chunk to the output file
		if (fwrite(g_simg_copybuf, chunk, 1, out) != 1) return -1;

		len -= chunk; 
	}

	return blocks;
}

//-----------------------------------------------------------------------------
// simg_process_skip_chunk (private)
//
// Processes a sparse EXT4 file chunk that contains no data
// (From simg2img.c)
//
// Arguments:
//
//	out			- OUTPUT file pointer
//	blocks		- Number of blocks to process
//	blk_sz		- Size of each block
//	crc32		- Running CRC32 checksum

static int simg_process_skip_chunk(FILE *out, u32 blocks, u32 blk_sz, u32 *crc32)
{
	/* len needs to be 64 bits, as the sparse file specifies the skip amount
	 * as a 32 bit value of blocks.
	 */
	u64 len = (u64)blocks * blk_sz;
	u64 len_save;
	u32 skip_chunk = 0;

	/* Fseek takes the offset as a long, which may be 32 bits on some systems.
	 * So, lets do a sequence of fseeks() with SEEK_CUR to get the file pointer
	 * where we want it.
	 */
	len_save = len;
	while (len) {
		skip_chunk = (len > 0x80000000) ? 0x80000000 : len;
		fseek(out, skip_chunk, SEEK_CUR);
		len -= skip_chunk;
	}
	/* And compute the CRC of the skipped region a chunk at a time */
	len = len_save;
	while (len) {
		skip_chunk = (skip_chunk > blk_sz) ? blk_sz : skip_chunk;
		*crc32 = sparse_crc32(*crc32, g_simg_zerobuf, skip_chunk);
		len -= skip_chunk;
	}

	return blocks;
}

//-----------------------------------------------------------------------------
// simg_validate_image_header (private)
//
// Helper function that validates the header of the input file
// (From simg2img.c)
//
// Arguments:
//
//	in				- Open input file handle
//	sparse_header	- Pointer to a sparse_header_t to receive information
//	callbacks		- Optional UI callbacks

static int simg_validate_and_skip_image_header(int in_fd, sparse_header_t* sparse_header,
	ui_callbacks* callbacks)
{
	if(in_fd < 0) return EINVAL;					// Invalid [in] handle
	if(!sparse_header) return EINVAL;			// Invalid [in] pointer

	USES_UI_CALLBACKS(callbacks);				// For UI_ERROR et al

	// Initialize the output structure
	memset(sparse_header, 0, sizeof(sparse_header_t));
	
	// Read the sparse EXT4 file header into the structure
	if(read(in_fd, sparse_header, SIMG_SPARSE_HEADER_LEN) < (int)SIMG_SPARSE_HEADER_LEN) { 
		
		UI_ERROR("Error reading sparse file header\n");
		return -2;
	}

	// Check the magic number
	if(sparse_header->magic != SPARSE_HEADER_MAGIC) {
		
		UI_ERROR("Sparse file header is missing the magic number\n");
		return -1;
	}

	// Check the major version number
	if(sparse_header->major_version != SIMG_SPARSE_HEADER_MAJOR_VER) {

		UI_ERROR("Sparse file header has unknown major version number\n");
		return -1;
	}

	// Skip the remaining bytes in a header that is longer than expected
	if(sparse_header->file_hdr_sz > SIMG_SPARSE_HEADER_LEN)
		lseek(in_fd, sparse_header->file_hdr_sz - SIMG_SPARSE_HEADER_LEN, SEEK_CUR);

	return 0;						// <-- Sparse file header is OK
}

//-----------------------------------------------------------------------------
// yaffs2_init_state (private)
//
// Helper function used to intialize a YAFFS2_STATE structure, which replaced a
// host of global variables and static buffers in unyaffs
//
// Arguments:
//
//	state		- YAFFS2_STATE structure to be initialized
//	rootdir		- Root directory of restore operation (OBJECTID_ROOT)
//	callbacks	- Recovery UI callbacks

static int yaffs2_init_state(YAFFS2_STATE* state, const char* rootdir, ui_callbacks* callbacks)
{
	if(!state) return EINVAL;			// Invalid [in] pointer

	memset(state, 0, sizeof(YAFFS2_STATE));
	
	// Allocate the general data buffer
	state->data = (unsigned char*)malloc(YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE);
	if(state->data) memset(state->data, 0, YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE);
	else return ENOMEM;
	
	// Allocate the object list
	state->obj_list = (char**)malloc(sizeof(char*) * YAFFS2_MAX_OBJECTS);
	if(state->obj_list) memset(state->obj_list, 0, sizeof(char*) * YAFFS2_MAX_OBJECTS);
	else { free(state->data); state->data = NULL; return ENOMEM; }
	
	// Set the special root object string pointer now
	state->obj_list[YAFFS2_YAFFS_OBJECTID_ROOT] = (char*)rootdir;
	
	// Set the pointers into the general data buffer
	state->chunk_data = state->data;
	state->spare_data = state->data + YAFFS2_CHUNK_SIZE;
	
	// Initialize the image file handles and information
	state->img_fd = -1;
	state->img_gzfile = Z_NULL;
	state->img_size = 0;
	
	// Set the UI callbacks
	state->callbacks = callbacks;
	
	return 0;
}

//-----------------------------------------------------------------------------
// yaffs2_process_chunk (private)
//
// Processes a chunk of data from a YAFFS2 image file
// (From unyaffs.c)
//
// Arguments:
//
//	state		- YAFFS2_STATE structure for this restore operation

static void yaffs2_process_chunk(YAFFS2_STATE* state)
{
	int 			out_file;
    unsigned 		remain, s;
	
	USES_UI_CALLBACKS(state->callbacks);
	
	//
	// TODO: This function could still use cleaning up
	//

	yaffs_PackedTags2 *pt = (yaffs_PackedTags2*)state->spare_data;
	if (pt->t.byteCount == 0xffff)  {	//a new object 

		yaffs_ObjectHeader oh;
		memcpy(&oh, state->chunk_data, sizeof(yaffs_ObjectHeader));

		// Allocate the buffer that will hold the path for this object, which gets
		// assigned to the global object list.  Do not release the memory here
		char* full_path_name = (char*)malloc(strlen(oh.name) + strlen(state->obj_list[oh.parentObjectId]) + 2);
		if (full_path_name == NULL) {
			UI_ERROR("yaffs2_process_chunk: malloc full path name\n");
		}
		strcpy(full_path_name, state->obj_list[oh.parentObjectId]);
		strcat(full_path_name, "/");
		strcat(full_path_name, oh.name);
		state->obj_list[pt->t.objectId] = full_path_name;

		switch(oh.type) {
			case YAFFS_OBJECT_TYPE_FILE:
				remain = oh.fileSize;
				out_file = creat(full_path_name, oh.yst_mode);
				while(remain > 0) {
					if (yaffs2_read_chunk(state))
						return;
					s = (remain < pt->t.byteCount) ? remain : pt->t.byteCount;	
					if (write(out_file, state->chunk_data, s) == -1)
						return;
					remain -= s;
				}
				close(out_file);
				break;
			case YAFFS_OBJECT_TYPE_SYMLINK:
				symlink(oh.alias, full_path_name);
				break;
			case YAFFS_OBJECT_TYPE_DIRECTORY:
				mkdir(full_path_name, 0777);
				break;
			case YAFFS_OBJECT_TYPE_HARDLINK:
				link(state->obj_list[oh.equivalentObjectId], full_path_name);
				break;
			case YAFFS_OBJECT_TYPE_UNKNOWN:
			case YAFFS_OBJECT_TYPE_SPECIAL:
				break;
		}
		lchown(full_path_name, oh.yst_uid, oh.yst_gid);
		if (oh.type != YAFFS_OBJECT_TYPE_SYMLINK)
			chmod(full_path_name, oh.yst_mode);
		struct timeval times[2];
		times[0].tv_sec = oh.yst_atime;
		times[1].tv_sec = oh.yst_mtime;
		utimes(full_path_name, times);
	}
}

//-----------------------------------------------------------------------------
// yaffs2_read_chunk (private)
//
// Helper function that reads a chunk of data from a YAFFS2 file
// (From unyaffs.c)
//
// Arguments:
//
//	state		- YAFFS2_STATE structure for this restore operation

static int yaffs2_read_chunk(YAFFS2_STATE* state)
{
	ssize_t 			cb;						// Bytes read from image
	u64 				filepos = 0;			// Offset into source image
	int 				result = -1;			// Return value
	
	USES_UI_CALLBACKS(state->callbacks);
	
	// Reset and read in the next chunk of data from the source image file
	memset(state->data, 0xFF, YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE);
	cb = gzread(state->img_gzfile, state->data, YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE);
	
	// We don't want to update the progress for every single chunk of data read, it's
	// useless and slow.  While this may not be much better, only do it every x times
	state->progress_modulo++;
	if((state->img_size > 0) && ((state->progress_modulo % 20) == 0)) {
	
		filepos = lseek(state->img_fd, 0, SEEK_CUR);
		UI_SETPROGRESS((float)(filepos * 100) / (float)state->img_size);
	}
	
	// If the right amount of data was returned, all good.  Otherwise, if more than zero byes
	// were returned, the file is corrupt (truncated).  If -1 was returned, a read error occurred
	if(cb == YAFFS2_CHUNK_SIZE + YAFFS2_SPARE_SIZE) result = 0;
	else if(cb > 0) { UI_ERROR("yaffs2_read_chunk: Source image file is corrupt.\n"); }
	else if(cb == -1) { UI_ERROR("yaffs2_read_chunk: Unable to read from the source image file.\n"); }

	return result;
}

//-----------------------------------------------------------------------------
// yaffs2_term_state (private)
//
// Helper function used to clean up a YAFFS2_STATE structure
//
// Arguments:
//
//	state		- YAFFS2_STATE structure to be cleaned up

static void yaffs2_term_state(YAFFS2_STATE* state)
{
	int				index;				// Loop index variable
	
	// The object list contains an array of dynamically allocated
	// string pointers that need to be chased, just not the OBJECTID_ROOT
	// as that's a const pointer set in init_yaffs2_state
	for(index = 0; index < YAFFS2_MAX_OBJECTS; index++) {

		if(index == YAFFS2_YAFFS_OBJECTID_ROOT) continue;
		if(state->obj_list[index] != NULL) free(state->obj_list[index]);
	}	
	
	// Release the dynamically allocated general data and object list buffers
	if(state->data) free(state->data);
	if(state->obj_list) free(state->obj_list);
	
	// Clear out the remainder of the structure
	memset(state, 0, sizeof(YAFFS2_STATE));
	
	state->img_fd = -1;					// Reset to invalid file descriptor
	state->img_gzfile = Z_NULL;			// Reset to invalid gzFile pointer
}

//-----------------------------------------------------------------------------