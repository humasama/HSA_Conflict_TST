#include "util.h"

#include <sys/mman.h>

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hsa_types.h"

#define CEIL(val,unit) (((val + unit - 1)/unit)*unit)

uint64_t get_region(void **memchunk, uint64_t *start_index, uint32_t shift, uint64_t grid_x){
	
	void *addr = (void *)0x1000000080000000;
	
	uint64_t region_unit = grid_x * sizeof(int);
	uint64_t dist = region_unit > ENTRY_DIST ? region_unit : ENTRY_DIST;

	printf("dist = %lu\n", dist);

	uint64_t g_mem_size = (uint64_t)(dist * ENTRY_NUM);
	g_mem_size += (1 << shift);
	g_mem_size = CEIL(g_mem_size, dist);

	printf("g_mem_size = %lu\n", g_mem_size);

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if(fd < 0){
		printf("open file failed!\n");
		exit(1);
	}
	
	//auto page aligned 
	*memchunk = mmap(0, g_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)addr);
	if(*memchunk == MAP_FAILED){
		printf("mmap /dev/mem failed!\n");
		exit(1);
	}

	int i = 0;
	for(; i < ENTRY_NUM - 1; i++){
		start_index[i] = (i + 1) * dist / 4;
	}
	start_index[i] = 0;

	for(i = 1; i < GRID_X; i ++){
		memcpy(start_index + i * ENTRY_NUM, start_index, ENTRY_NUM * sizeof(uint64_t));
	}	
	

	return g_mem_size;
}

uint64_t get_elapsed(struct timespec *start, struct timespec *end){
	
	uint64_t dur;
	if (start->tv_nsec > end->tv_nsec)
		dur = (uint64_t)(end->tv_sec - 1 - start->tv_sec) * 1000000000 +
			(1000000000 + end->tv_nsec - start->tv_nsec);
	else
		dur = (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000 +
			(end->tv_nsec - start->tv_nsec);

	return dur;
}
