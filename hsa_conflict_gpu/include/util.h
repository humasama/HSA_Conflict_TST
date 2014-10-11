#ifndef _UTIL_H_
#define _UTIL_H_

#include <inttypes.h>

//#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#define L3_NUM_WAYS 16		// cat /sys/devices/system/cpu/cpu0/cache/index3/ways..
#define ENTRY_NUM ((L3_NUM_WAYS) * 2)

#define DEFAULT_SHIFT 23	//bank bits : 6~16
#define ENTRY_DIST (1<<(DEFAULT_SHIFT))

uint64_t get_region(void **memchunk, uint64_t *start_index, uint32_t shift, uint64_t grid_x);

uint64_t get_elapsed(struct timespec *start, struct timespec *end);
#endif
