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

#include <stdlib.h>
#include <stdio.h>

#include "ext4_utils.h"
#include "ext4.h"
#include "ext4_extents.h"
#include "backed_block.h"
#include "indirect.h"
#include "allocate.h"

/* Creates data buffers for the first backing_len bytes of a block allocation
   and queues them to be written */
static u8 *create_backing(struct block_allocation *alloc,
		unsigned long backing_len)
{
	int result;						// Result from function call
	
	if (DIV_ROUND_UP(backing_len, info.block_size) > EXT4_NDIR_BLOCKS) {
		error("indirect backing larger than %d blocks\n", EXT4_NDIR_BLOCKS);
		return NULL;
	}

	u8 *data = calloc(backing_len, 1);
	if (!data) {
		error_errno("calloc\n");
		return NULL;
	}

	u8 *ptr = data;
	for (; alloc != NULL && backing_len > 0; get_next_region(alloc)) {
		u32 region_block;
		u32 region_len;
		u32 len;
		get_region(alloc, &region_block, &region_len);

		len = min(region_len * info.block_size, backing_len);

		result = queue_data_block(ptr, len, region_block);
		if(result != 0) return NULL;
		
		ptr += len;
		backing_len -= len;
	}

	return data;
}

static void reserve_indirect_block(struct block_allocation *alloc, int len)
{
	if (reserve_oob_blocks(alloc, 1)) {
		warn("failed to reserve oob block\n");
		return;
	}

	if (advance_blocks(alloc, len)) {
		warn("failed to advance %d blocks\n", len);
		return;
	}
}

static void reserve_dindirect_block(struct block_allocation *alloc, int len)
{
	if (reserve_oob_blocks(alloc, 1)) {
		warn("failed to reserve oob block\n");
		return;
	}

	while (len > 0) {
		int ind_block_len = min((int)aux_info.blocks_per_ind, len);

		reserve_indirect_block(alloc, ind_block_len);

		len -= ind_block_len;
	}

}

static void reserve_tindirect_block(struct block_allocation *alloc, int len)
{
	if (reserve_oob_blocks(alloc, 1)) {
		warn("failed to reserve oob block\n");
		return;
	}

	while (len > 0) {
		int dind_block_len = min((int)aux_info.blocks_per_dind, len);

		reserve_dindirect_block(alloc, dind_block_len);

		len -= dind_block_len;
	}
}

static void fill_indirect_block(u32 *ind_block, int len, struct block_allocation *alloc)
{
	int i;
	for (i = 0; i < len; i++) {
		ind_block[i] = get_block(alloc, i);
	}
}

static void fill_dindirect_block(u32 *dind_block, int len, struct block_allocation *alloc)
{
	int i;
	u32 ind_block;
	int result;

	for (i = 0; len >  0; i++) {
		ind_block = get_oob_block(alloc, 0);
		if (advance_oob_blocks(alloc, 1)) {
			warn("failed to reserve oob block\n");
			return;
		}

		dind_block[i] = ind_block;

		u32 *ind_block_data = calloc(info.block_size, 1);
		result = queue_data_block((u8*)ind_block_data, info.block_size, ind_block);
		if(result != 0) return;
		int ind_block_len = min((int)aux_info.blocks_per_ind, len);

		fill_indirect_block(ind_block_data, ind_block_len, alloc);

		if (advance_blocks(alloc, ind_block_len)) {
			warn("failed to advance %d blocks\n", ind_block_len);
			return;
		}

		len -= ind_block_len;
	}
}

static void fill_tindirect_block(u32 *tind_block, int len, struct block_allocation *alloc)
{
	int i;
	u32 dind_block;
	int result;

	for (i = 0; len > 0; i++) {
		dind_block = get_oob_block(alloc, 0);
		if (advance_oob_blocks(alloc, 1)) {
			warn("failed to reserve oob block\n");
			return;
		}

		tind_block[i] = dind_block;

		u32 *dind_block_data = calloc(info.block_size, 1);
		result = queue_data_block((u8*)dind_block_data, info.block_size, dind_block);
		if(result != 0) return;
		int dind_block_len = min((int)aux_info.blocks_per_dind, len);

		fill_dindirect_block(dind_block_data, dind_block_len, alloc);

		len -= dind_block_len;
	}
}

/* Given an allocation, attach as many blocks as possible to direct inode
   blocks, and return the rest */
static int inode_attach_direct_blocks(struct ext4_inode *inode,
		struct block_allocation *alloc, u32 *block_len)
{
	int len = min(*block_len, EXT4_NDIR_BLOCKS);
	int i;

	for (i = 0; i < len; i++) {
		inode->i_block[i] = get_block(alloc, i);
	}

	if (advance_blocks(alloc, len)) {
		warn("failed to advance %d blocks\n", len);
		return -1;
	}

	*block_len -= len;
	return 0;
}

/* Given an allocation, attach as many blocks as possible to indirect blocks,
   and return the rest
   Assumes that the blocks necessary to hold the indirect blocks were included
   as part of the allocation */
static int inode_attach_indirect_blocks(struct ext4_inode *inode,
		struct block_allocation *alloc, u32 *block_len)
{
	int len = min(*block_len, aux_info.blocks_per_ind);
	int result;
	int ind_block = get_oob_block(alloc, 0);
	inode->i_block[EXT4_IND_BLOCK] = ind_block;

	if (advance_oob_blocks(alloc, 1)) {
		warn("failed to advance oob block\n");
		return -1;
	}

	u32 *ind_block_data = calloc(info.block_size, 1);
	result = queue_data_block((u8*)ind_block_data, info.block_size, ind_block);
	if(result != 0) return result;

	fill_indirect_block(ind_block_data, len, alloc);

	if (advance_blocks(alloc, len)) {
		error("failed to advance %d blocks\n", len);
		return -1;
	}

	*block_len -= len;
	return 0;
}

/* Given an allocation, attach as many blocks as possible to doubly indirect
   blocks, and return the rest.
   Assumes that the blocks necessary to hold the indirect and doubly indirect
   blocks were included as part of the allocation */
static int inode_attach_dindirect_blocks(struct ext4_inode *inode,
		struct block_allocation *alloc, u32 *block_len)
{
	int len = min(*block_len, aux_info.blocks_per_dind);
	int result;

	int dind_block = get_oob_block(alloc, 0);
	inode->i_block[EXT4_DIND_BLOCK] = dind_block;

	if (advance_oob_blocks(alloc, 1)) {
		warn("failed to advance oob block\n");
		return -1;
	}

	u32 *dind_block_data = calloc(info.block_size, 1);
	result = queue_data_block((u8*)dind_block_data, info.block_size, dind_block);
	if(result != 0) return result;

	fill_dindirect_block(dind_block_data, len, alloc);

	if (advance_blocks(alloc, len)) {
		warn("failed to advance %d blocks\n", len);
		return -1;
	}

	*block_len -= len;
	return 0;
}

/* Given an allocation, attach as many blocks as possible to triply indirect
   blocks, and return the rest.
   Assumes that the blocks necessary to hold the indirect, doubly indirect and
   triply indirect blocks were included as part of the allocation */
static int inode_attach_tindirect_blocks(struct ext4_inode *inode,
		struct block_allocation *alloc, u32 *block_len)
{
	int len = min(*block_len, aux_info.blocks_per_tind);
	int result;

	int tind_block = get_oob_block(alloc, 0);
	inode->i_block[EXT4_TIND_BLOCK] = tind_block;

	if (advance_oob_blocks(alloc, 1)) {
		warn("failed to advance oob block\n");
		return -1;
	}

	u32 *tind_block_data = calloc(info.block_size, 1);
	result = queue_data_block((u8*)tind_block_data, info.block_size, tind_block);
	if(result != 0) return result;

	fill_tindirect_block(tind_block_data, len, alloc);

	if (advance_blocks(alloc, len)) {
		warn("failed to advance %d blocks\n", len);
		return -1;
	}

	*block_len -= len;
	return 0;
}

static void reserve_all_indirect_blocks(struct block_allocation *alloc, u32 len)
{
	if (len <= EXT4_NDIR_BLOCKS)
		return;

	len -= EXT4_NDIR_BLOCKS;
	advance_blocks(alloc, EXT4_NDIR_BLOCKS);

	u32 ind_block_len = min(aux_info.blocks_per_ind, len);
	reserve_indirect_block(alloc, ind_block_len);

	len -= ind_block_len;
	if (len == 0)
		return;

	u32 dind_block_len = min(aux_info.blocks_per_dind, len);
	reserve_dindirect_block(alloc, dind_block_len);

	len -= dind_block_len;
	if (len == 0)
		return;

	u32 tind_block_len = min(aux_info.blocks_per_tind, len);
	reserve_tindirect_block(alloc, tind_block_len);

	len -= tind_block_len;
	if (len == 0)
		return;

	warn("%d blocks remaining\n", len);
}

static u32 indirect_blocks_needed(u32 len)
{
	u32 ind = 0;

	if (len <= EXT4_NDIR_BLOCKS)
		return ind;

	len -= EXT4_NDIR_BLOCKS;

	/* We will need an indirect block for the rest of the blocks */
	ind += DIV_ROUND_UP(len, aux_info.blocks_per_ind);

	if (len <= aux_info.blocks_per_ind)
		return ind;

	len -= aux_info.blocks_per_ind;

	ind += DIV_ROUND_UP(len, aux_info.blocks_per_dind);

	if (len <= aux_info.blocks_per_dind)
		return ind;

	len -= aux_info.blocks_per_dind;

	ind += DIV_ROUND_UP(len, aux_info.blocks_per_tind);

	if (len <= aux_info.blocks_per_tind)
		return ind;

	error("request too large\n");
	return 0;
}

static int do_inode_attach_indirect(struct ext4_inode *inode,
		struct block_allocation *alloc, u32 block_len)
{
	u32 count = block_len;

	if (inode_attach_direct_blocks(inode, alloc, &count)) {
		warn("failed to attach direct blocks to inode\n");
		return -1;
	}

	if (count > 0) {
		if (inode_attach_indirect_blocks(inode, alloc, &count)) {
			warn("failed to attach indirect blocks to inode\n");
			return -1;
		}
	}

	if (count > 0) {
		if (inode_attach_dindirect_blocks(inode, alloc, &count)) {
			warn("failed to attach dindirect blocks to inode\n");
			return -1;
		}
	}

	if (count > 0) {
		if (inode_attach_tindirect_blocks(inode, alloc, &count)) {
			warn("failed to attach tindirect blocks to inode\n");
			return -1;
		}
	}

	if (count) {
		warn("blocks left after triply-indirect allocation\n");
		return -1;
	}

	rewind_alloc(alloc);

	return 0;
}

static struct block_allocation *do_inode_allocate_indirect(
		struct ext4_inode *inode, u32 block_len)
{
	u32 indirect_len = indirect_blocks_needed(block_len);

	struct block_allocation *alloc = allocate_blocks(block_len + indirect_len);

	if (alloc == NULL) {
		warn("Failed to allocate %d blocks\n", block_len + indirect_len);
		return NULL;
	}

	return alloc;
}

/* Allocates enough blocks to hold len bytes and connects them to an inode */
void inode_allocate_indirect(struct ext4_inode *inode, unsigned long len)
{
	struct block_allocation *alloc;
	u32 block_len = DIV_ROUND_UP(len, info.block_size);
	u32 indirect_len = indirect_blocks_needed(block_len);

	alloc = do_inode_allocate_indirect(inode, block_len);
	if (alloc == NULL) {
		warn("failed to allocate extents for %lu bytes\n", len);
		return;
	}

	reserve_all_indirect_blocks(alloc, block_len);
	rewind_alloc(alloc);

	if (do_inode_attach_indirect(inode, alloc, block_len))
		warn("failed to attach blocks to indirect inode\n");

	inode->i_flags = 0;
	inode->i_blocks_lo = (block_len + indirect_len) * info.block_size / 512;
	inode->i_size_lo = len;

	free_alloc(alloc);
}

// djp952: Added integer return value; non-zero indicates critical failure
int inode_attach_resize(struct ext4_inode *inode,
		struct block_allocation *alloc)
{
	u32 block_len = block_allocation_len(alloc);
	u32 superblocks = block_len / aux_info.bg_desc_reserve_blocks;
	u32 i, j;
	u64 blocks;
	u64 size;
	int result;

	if (block_len % aux_info.bg_desc_reserve_blocks) {
		error("reserved blocks not a multiple of %d\n", aux_info.bg_desc_reserve_blocks);
		return -1;
	}

	append_oob_allocation(alloc, 1);
	u32 dind_block = get_oob_block(alloc, 0);

	u32 *dind_block_data = calloc(info.block_size, 1);
	if (!dind_block_data) {
		error_errno("calloc");
		return -1;
	}
	result = queue_data_block((u8 *)dind_block_data, info.block_size, dind_block);
	if(result != 0) return result;

	u32 *ind_block_data = calloc(info.block_size, aux_info.bg_desc_reserve_blocks);
	if (!ind_block_data) {
		error_errno("calloc\n");
		return errno;
	}
	result = queue_data_block((u8 *)ind_block_data,
			info.block_size * aux_info.bg_desc_reserve_blocks,
			get_block(alloc, 0));
	if(result != 0) return result;

	for (i = 0; i < aux_info.bg_desc_reserve_blocks; i++) {
		int r = (i - aux_info.bg_desc_blocks) % aux_info.bg_desc_reserve_blocks;
		if (r < 0)
			r += aux_info.bg_desc_reserve_blocks;

		dind_block_data[i] = get_block(alloc, r);

		for (j = 1; j < superblocks; j++) {
			u32 b = j * aux_info.bg_desc_reserve_blocks + r;
			ind_block_data[r * aux_info.blocks_per_ind + j - 1] = get_block(alloc, b);
		}
	}

	u32 last_block = EXT4_NDIR_BLOCKS + aux_info.blocks_per_ind +
			aux_info.blocks_per_ind * (aux_info.bg_desc_reserve_blocks - 1) +
			superblocks - 2;

	blocks = ((u64)block_len + 1) * info.block_size / 512;
	size = (u64)last_block * info.block_size;

	inode->i_block[EXT4_DIND_BLOCK] = dind_block;
	inode->i_flags = 0;
	inode->i_blocks_lo = blocks;
	inode->osd2.linux2.l_i_blocks_high = blocks >> 32;
	inode->i_size_lo = size;
	inode->i_size_high = size >> 32;
	
	return 0;
}

/* Allocates enough blocks to hold len bytes, with backing_len bytes in a data
   buffer, and connects them to an inode.  Returns a pointer to the data
   buffer. */
u8 *inode_allocate_data_indirect(struct ext4_inode *inode, unsigned long len,
		unsigned long backing_len)
{
	struct block_allocation *alloc;
	u8 *data = NULL;

	alloc = do_inode_allocate_indirect(inode, len);
	if (alloc == NULL) {
		warn("failed to allocate extents for %lu bytes\n", len);
		return NULL;
	}

	if (backing_len) {
		data = create_backing(alloc, backing_len);
		if (!data)
			warn("failed to create backing for %lu bytes\n", backing_len);
	}

	free_alloc(alloc);

	return data;
}
