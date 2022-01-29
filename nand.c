#include "header.h"

struct page {
	int pbn, page_offset;	/* physical block number, page offset in block */ 
	int lpn; 		/* logical page number for debug */
};

struct block {
	/* 한 block 당 page는 총 4개임 */
	struct page page[PAGES_PER_BLOCK];
	int last_page_offset; /* current page offset, for debug */
};

struct nand {
	struct block block[BLOCKS_PER_NAND]; /* + Free Block for GC */
};

struct nand_stats {
	int total_nand_page_program;
	int total_nand_page_read;
	int total_block_erase_count;
};

struct nand nand;
struct nand_stats stats;

void nand_init(void)
{
	int block_idx, page_idx;

	for (block_idx = 0; block_idx < (BLOCKS_PER_NAND); block_idx++) {
		nand.block[block_idx].last_page_offset = 0;

		for (page_idx = 0; page_idx < PAGES_PER_BLOCK; page_idx++) {
			nand.block[block_idx].page[page_idx].pbn = block_idx;
			nand.block[block_idx].page[page_idx].page_offset = page_idx;
			nand.block[block_idx].page[page_idx].lpn = -1; /* -1 means invalid page */
		}
	}
	
	/* 모든 block / page에 대한 초기화를 수행함 */
	/* block의 last_page_offset은 0으로,
	 * page의 pbn은 해당 block_idx,
	 * page의 page_offset은 page_idx,
	 * page의 lpn은 -1로 세팅
	 */

	stats.total_nand_page_program = 0;
	stats.total_nand_page_read = 0;
	stats.total_block_erase_count = 0;
}

void nand_page_program(int block_idx, int page_idx, int data_lpn)
{
	/* block_idx가 NANA의 총 block을 넘을 수 없음 */
	assert(block_idx < BLOCKS_PER_NAND);
	/* page_idx 또한 page_index를 넘을 수 없음 */
	assert(page_idx < PAGES_PER_BLOCK);

	/* 순차쓰기에 대한 예외처리 */
	assert(nand.block[block_idx].last_page_offset == page_idx);

	nand.block[block_idx].page[page_idx].lpn = data_lpn;
	nand.block[block_idx].last_page_offset++;

	stats.total_nand_page_program++;
}

int nand_page_read(int block_idx, int page_idx)
{
	assert(block_idx < BLOCKS_PER_NAND);
	assert(page_idx < PAGES_PER_BLOCK);

	stats.total_nand_page_read++;
	/* 해당 block_idx, page_idx의 lpn 값을 반환함 */
	return nand.block[block_idx].page[page_idx].lpn;
}

void nand_block_erase(int block_idx)
{
	int page_idx;

	assert(block_idx < BLOCKS_PER_NAND);

	nand.block[block_idx].last_page_offset = 0;

	for (page_idx = 0; page_idx < PAGES_PER_BLOCK; page_idx++)
		nand.block[block_idx].page[page_idx].lpn = -1; /* -1 means invalid page */

	stats.total_block_erase_count++;
}

int nand_print_stats(void)
{
	printf("nand_stats: total_nand_page_program:: %d\n",
	       stats.total_nand_page_program);
	printf("nand_stats: total_nand_page_read::    %d\n",
	       stats.total_nand_page_read);
	printf("nand_stats: total_block_erase_count:: %d\n",
	       stats.total_block_erase_count);

	return stats.total_nand_page_program;
}
