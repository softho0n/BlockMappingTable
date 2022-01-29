#include "header.h"

struct pagemap_str {
	int page_map[NUM_LPNS];
	int num_invalid_pages[BLOCKS_PER_NAND];

	int current_block;	 /* current write block */
	int current_page_offset; /* current write page offset in block */

	int free_block_count; /* total free block count */

	int gc_free_block; /* free block index for GC */
};

struct pagemap_stats {
	/* GC 수행 횟수, copyback 횟수 */
	int gc_count;
	int copyback_count;
};

struct pagemap_str pagemap_str;
struct pagemap_stats pagemap_stats;

static void pagemap_print_stats(void)
{
	printf("pagemap_stats: garbage collection count::   %d\n",
	       pagemap_stats.gc_count);
	printf("pagemap_stats: page copyback count:         %d\n",
	       pagemap_stats.copyback_count);
}

static int pagemap_get_ppn(int lpn)
{
	return pagemap_str.page_map[lpn];
}

static void pagemap_set_ppn(int lpn, int ppn)
{
	int old_block_idx, old_ppn;
	
	/* lpn과 ppn에 대한 유효성 검증 */
	assert(lpn < NUM_LPNS);
	assert(ppn < (BLOCKS_PER_NAND * PAGES_PER_BLOCK));

	/* increase invalidation count of previous block */
	old_ppn = pagemap_str.page_map[lpn];
	if (old_ppn < 0)
		goto set_ppn;

	/* 나중 gc 수행을 위해 page invalidation count를 증가시킴 */
	old_block_idx = old_ppn / PAGES_PER_BLOCK;
	assert(old_block_idx < BLOCKS_PER_NAND);

	pagemap_str.num_invalid_pages[old_block_idx]++;
	if (VERBOSE_MODE) {
		printf("%s:%d old_block %d invalid_count %d\n",
		       __func__, __LINE__, old_block_idx,
		       pagemap_str.num_invalid_pages[old_block_idx]);
	}

	/* set ppn */
set_ppn:
	pagemap_str.page_map[lpn] = ppn;
}

static int pagemap_garbage_collection(void)
{
	int block_idx, page_idx, ppn_in_map, ppn_in_victim;
	int data_lpn;
	int gc_free_block, gc_current_page_offset;
	int victim_block;
	int max_invalid_pages;

	victim_block = -1;
	max_invalid_pages = -1;

	/* the first block with invalidation pages selected as the victim */
	for (block_idx = (BLOCKS_PER_NAND)-1; block_idx >= 0; block_idx--) {
		if (pagemap_str.num_invalid_pages[block_idx] > 0) {
			victim_block = block_idx;
			max_invalid_pages = pagemap_str.num_invalid_pages[block_idx];
			break;
		}
	}

	if (VERBOSE_MODE)
		printf("%s:%d victim_block %d max_invalid_pages %d\n",
		       __func__, __LINE__, victim_block, max_invalid_pages);

	assert(victim_block != -1);
	assert(victim_block < BLOCKS_PER_NAND);
	assert(victim_block != pagemap_str.gc_free_block);
	assert(max_invalid_pages != 0);

	gc_free_block = pagemap_str.gc_free_block;

	assert(pagemap_str.num_invalid_pages[gc_free_block] == 0);

	gc_current_page_offset = 0;

	/* copy valid page from victim to gc_free_block */
	for (page_idx = 0; page_idx < PAGES_PER_BLOCK ; page_idx++) {
		data_lpn = nand_page_read(victim_block, page_idx);

		ppn_in_map = pagemap_get_ppn(data_lpn);

		ppn_in_victim = victim_block * PAGES_PER_BLOCK + page_idx;

		if (ppn_in_map != ppn_in_victim)
			continue;

		nand_page_program(gc_free_block, gc_current_page_offset, data_lpn);
		pagemap_set_ppn(data_lpn, gc_free_block * PAGES_PER_BLOCK + gc_current_page_offset);
		gc_current_page_offset++;

		pagemap_stats.copyback_count++;
	}
	assert(gc_current_page_offset == (PAGES_PER_BLOCK - max_invalid_pages));

	/* erase victim block and set next gc free block */
	nand_block_erase(victim_block);
	pagemap_str.num_invalid_pages[victim_block] = 0;
	pagemap_str.gc_free_block = victim_block;

	/* set current gc free block to current block */
	pagemap_str.current_block = gc_free_block;
	pagemap_str.current_page_offset = gc_current_page_offset;
	pagemap_str.num_invalid_pages[gc_free_block] = 0;

	pagemap_stats.gc_count++;
}

static void pagemap_get_free_ppn(int *block_idx, int *page_offset, int lpn)
{
	printf("lpn: %d ppb: %d\n", lpn, pagemap_get_ppn(lpn));
	if (pagemap_str.current_page_offset >= PAGES_PER_BLOCK) {
		if (pagemap_str.free_block_count) {
			pagemap_str.free_block_count--;
			pagemap_str.current_block++;
			pagemap_str.current_page_offset = 0;
		} else {
			pagemap_garbage_collection();
		}
	}

	*block_idx = pagemap_str.current_block;
	*page_offset= pagemap_str.current_page_offset;

	pagemap_str.current_page_offset++;
}

static void pagemap_write_op(int lpn, int data)
{
	int block_idx, page_offset;
	int ppn;

	/* get free physical page index or do garbage-collect */
	/* overwrite 되는 구조가 아니라면 단순히 현재 block, page_offset + 1 */
	pagemap_get_free_ppn(&block_idx, &page_offset, lpn);

	nand_page_program(block_idx, page_offset, data);

	/* program 후 old ppn에 대한 갱신이 필요함 */
	ppn = block_idx * PAGES_PER_BLOCK + page_offset;
	pagemap_set_ppn(lpn, ppn);

	if (VERBOSE_MODE)
		printf("%s:%d lpn %d ppn %d\n", __func__, __LINE__, lpn, ppn);
}

static int pagemap_read_op(int lpn)
{
	int ppn, data;
	int pbn, page_offset;

	/* get physical page number from page-map table */
	ppn = pagemap_get_ppn(lpn);

	pbn = ppn / PAGES_PER_BLOCK;	      /* get physical block number */
	page_offset = ppn % PAGES_PER_BLOCK;  /* get page offset in block */

	data = nand_page_read(pbn, page_offset);

	return data;
}

void pagemap_init(struct ftl_operation *op)
{
	int block_idx, page_idx;

	/* 현재 사용중인 Physical Block, Offset을 초기화 */
	pagemap_str.current_block = 0;
	pagemap_str.current_page_offset = 0;

	/* 마지막 Physical Block을 Garbage Collection을 위한 Free Block으로 설정 */
	pagemap_str.gc_free_block = BLOCKS_PER_NAND - 1;

	/* 전체 Block 중 gc free block에서 1개 사용, 1개는 바로 사용한다고 가정 */
	pagemap_str.free_block_count = BLOCKS_PER_NAND - 2;

	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++)
		pagemap_str.num_invalid_pages[block_idx] = 0;

	for (page_idx = 0; page_idx < NUM_LPNS; page_idx++)
		pagemap_str.page_map[page_idx] = -1;

	op->write_op = pagemap_write_op;
	op->read_op = pagemap_read_op;
	op->print_stats = pagemap_print_stats;
}
