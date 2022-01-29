#include "header.h"

struct blockmap_str {
	int block_map[BLOCKS_PER_NAND];
	int current_block;
	int copyback_free_block;
};

struct blockmap_stats {
	int copyback_count;
};

struct blockmap_str blockmap_str;
struct blockmap_stats blockmap_stats;

static void blockmap_print_stats(void)
{
	printf("blockmap_stats: page copyback count:         %d\n",
			blockmap_stats.copyback_count);
}

static int blockmap_get_pbn(int lbn)
{
	return blockmap_str.block_map[lbn];
}

static void blockmap_set_pbn(int lbn, int pbn)
{
	int old_pbn;

	assert(lbn < BLOCKS_PER_NAND);
	assert(pbn < BLOCKS_PER_NAND);

	old_pbn = blockmap_str.block_map[lbn];
	if (old_pbn < 0)
			goto set_pbn;
	
	if (VERBOSE_MODE) {
		printf("%s:%d old_block %d update_block %d\n",
				__func__, __LINE__, old_pbn, pbn);
	}

set_pbn:
	blockmap_str.block_map[lbn] = pbn;
}

static int blockmap_read_op(int lpn)
{
	int lbn, page_offset;
	int pbn, data;

	lbn = lpn / PAGES_PER_BLOCK;
	page_offset = lpn % PAGES_PER_BLOCK;
	
	pbn = blockmap_get_pbn(lbn);
	
	data = nand_page_read(pbn, page_offset);

	return data;
}

static int blockmap_copy_back(int lbn, int victim_block, int victim_page_offset, int data)
{
	int block_idx, page_idx;
	int data_lpn;
	int copyback_free_block, copyback_current_page_offset;

	if (VERBOSE_MODE)
			printf("%s:%d victim_block %d page_offset %d data_lpn %d\n",
				   __func__, __LINE__, victim_block, victim_page_offset, data);
	
	assert(victim_block < BLOCKS_PER_NAND);
	assert(victim_block != blockmap_str.copyback_free_block);
	
	copyback_free_block = blockmap_str.copyback_free_block;
	copyback_current_page_offset = 0;

	for (page_idx = 0; page_idx < PAGES_PER_BLOCK; page_idx++) {
		data_lpn = nand_page_read(victim_block, page_idx);
		
		if (data_lpn == -1)
				break;
		
		if (data_lpn == data)
				blockmap_stats.copyback_count--;
		
		nand_page_program(copyback_free_block, copyback_current_page_offset, data_lpn);
		copyback_current_page_offset++;
		
		blockmap_stats.copyback_count++;
	}
	assert(copyback_current_page_offset < PAGES_PER_BLOCK + 1);
	blockmap_set_pbn(lbn, copyback_free_block);
	
	nand_block_erase(victim_block);
	blockmap_str.copyback_free_block = victim_block;
	blockmap_str.current_block = copyback_free_block;

	return copyback_free_block;
}

static void blockmap_get_free_pbn_and_program(int lbn, int *block_idx, int page_offset, int data)
{
	int pbn = blockmap_get_pbn(lbn);
	if (pbn == -1) {
			*block_idx = blockmap_str.current_block;
			blockmap_str.current_block++;
			nand_page_program(*block_idx, page_offset, data);
			blockmap_set_pbn(lbn, *block_idx);
	}
	else {
			int data_lpn = nand_page_read(pbn, page_offset);
			if (data_lpn == -1) {
				*block_idx = pbn;
				nand_page_program(*block_idx, page_offset, data);
			}
			else {
				/* copy back */
				*block_idx = blockmap_copy_back(lbn, pbn, page_offset, data);
			}
	}
}

static void blockmap_write_op(int lpn, int data)
{
	int block_idx, page_offset;
	int lbn, pbn;

	lbn = lpn / PAGES_PER_BLOCK;
	page_offset = lpn % PAGES_PER_BLOCK;

	blockmap_get_free_pbn_and_program(lbn, &block_idx, page_offset, data);
	pbn = block_idx;

	if (VERBOSE_MODE)
			printf("%s:%d lbn %d pbn %d\n", __func__, __LINE__, lbn, pbn);
}

void blockmap_init(struct ftl_operation *op)
{
	int block_idx;

	blockmap_str.current_block = 0;
	blockmap_str.copyback_free_block = BLOCKS_PER_NAND - 2;

	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++)
			blockmap_str.block_map[block_idx] = -1;

	nand_init();
	op->write_op = blockmap_write_op;
	op->read_op = blockmap_read_op;
	op->print_stats = blockmap_print_stats;
}
