#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>

#include <signal.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/time.h>
#include <time.h>

#include <inttypes.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <sys/types.h>
#include <errno.h>
#include <sys/resource.h>
#include <assert.h>

#define L3_NUM_WAYS   16                    // cat /sys/devices/system/cpu/cpu0/cache/index3/ways..
#define NUM_ENTRIES   (uint64_t)(L3_NUM_WAYS * 2)       // # of list entries to iterate
#define CACHE_LINE_SIZE 64

#define MAX(a,b) ((a>b)?(a):(b))
#define CEIL(val,unit) (((val + unit - 1)/unit)*unit)

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
   __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)


#define ENTRY_SHIFT_CNT (5)
static uint64_t entry_shift[ENTRY_SHIFT_CNT];

#define NUM_DIST ((uint64_t)1 << ENTRY_SHIFT_CNT)
static uint64_t entry_dist[NUM_DIST] = {0};
static uint64_t g_mem_size = 0;
static uint64_t min_interval = 0;
static uint64_t farest_dist = 0;

/* max access bit number */
#define MAX_SHIFT_NUM 20
static double bandwidth[MAX_SHIFT_NUM] = {0.0};

static int* list;
static uint64_t indices[NUM_ENTRIES];
static uint64_t next;

uint64_t get_elapsed(struct timespec *start, struct timespec *end)
{
	uint64_t dur;
	if (start->tv_nsec > end->tv_nsec)
		dur = (uint64_t)(end->tv_sec - 1 - start->tv_sec) * 1000000000 +
			(1000000000 + end->tv_nsec - start->tv_nsec);
	else
		dur = (uint64_t)(end->tv_sec - start->tv_sec) * 1000000000 +
			(end->tv_nsec - start->tv_nsec);

	return dur;

}

uint64_t run(uint64_t iter)
{
	uint64_t i, j = 0;
	uint64_t cnt = 0;
	int data;

	//printf("*****************access running:\n");
	for (i = 0; i < iter; i++){
		//printf("%luth: access list[0x%lx], next time: indices[j=%lu] = 0x%lx\n", i, next, j, indices[j]);
		data = list[next];
		next = indices[j];
#if 0
		/* hardware prefetch case */
		j ++;
		if(j == NUM_ENTRIES) j = 0;
#endif
#if 0
		/* sequence access && avoid hardware prefetch 
		* && twice access: because next has been updated!! 
		*/
		j = (list[next] + i + j ) % NUM_ENTRIES;
#endif
		j = (data + i + j) % NUM_ENTRIES;
		cnt ++;
	}
	return cnt;
}

void handler(int sig){
	if(sig == SIGINT){
		int i = 0;
		for(; i < MAX_SHIFT_NUM; i ++){
			printf("shift%d %.4f MB/s\n", i, bandwidth[i]);
		}
		exit(0);
	}
	else{
		printf("signal error!\n");
		exit(1);
	}
}

void access_bank(int gpu_shift, int shift, uint64_t iter){

	int oft = 0;
	if(gpu_shift >= 0){ 
		g_mem_size += (uint64_t)1 << gpu_shift;
		oft +=  ((uint64_t)1 << gpu_shift) / 4;
	}
	if(shift >= 0){
		g_mem_size += (uint64_t)1 << shift;
		oft += ((uint64_t)1 << shift) / 4;
	}

	g_mem_size +=  farest_dist;
	g_mem_size = CEIL(g_mem_size, min_interval);

	/* alloc memory. align to a page boundary */
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	void *addr = (void *) 0x1000000080000000;
	int *memchunk = NULL;
	
	if (fd < 0) {
		perror("Open failed");
		exit(1);
	}

	memchunk = mmap(0, g_mem_size,
			PROT_READ | PROT_WRITE, 
			MAP_SHARED, 
			fd, (off_t)addr);

	if (memchunk == MAP_FAILED) {
		perror("failed to alloc");
		exit(1);
	}

	list = &memchunk[oft];

	next = 0; 
	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);

	/* access banks */
	uint64_t naccess = run(iter);

	clock_gettime(CLOCK_REALTIME, &end);

	uint64_t nsdiff = get_elapsed(&start, &end);
	//printf("bandwidth %.2f MB/s\n", 64.0*1000.0*(double)naccess/(double)nsdiff);
	if(shift >= 0)
		bandwidth[shift] = 64.0 * 1000.0 * (double)naccess/(double)nsdiff;
	else
		bandwidth[MAX_SHIFT_NUM - 1] = 64.0 * 1000.0 * (double)naccess/(double)nsdiff;

		munmap(memchunk, g_mem_size);
		close(fd);
}

int main(int argc, char* argv[])
{
	cpu_set_t cmask;
	int num_processors;
	int cpuid = 0;
	int opt, i, j;

	uint64_t repeat = 10000;

	int page_shift = -1;
	int shift_l = -1, shift_r = -1;

	signal(SIGINT, handler);

	//init entry_shift: increase
	entry_shift[0] = 23;
	entry_shift[1] = 24;
	entry_shift[2] = 25;
	entry_shift[3] = 26;
	entry_shift[4] = 27;

	min_interval = ((uint64_t)1 << entry_shift[1]) - ((uint64_t)1 << entry_shift[0]);
	//printf("min_interval = 0x%lx\n", min_interval);
	farest_dist = 0;

	//printf("****************init entry_dist:\n");
	for(i = 0; i < NUM_DIST; i ++){
		j = 0;
		int index = i;
		while(index > 0){
			if(index & 1) entry_dist[i] += ((uint64_t)1 << entry_shift[j]);
			j ++;
			index = index >> 1;
		}
		if(farest_dist < entry_dist[i]) farest_dist = entry_dist[i];
		//printf("entry_dist[%d] = 0x%lx\n", i, entry_dist[i]);
	}
	
	//printf("farest_dist = 0x%lx\n", farest_dist);
	/*
	 * get command line options 
	 */
	while ((opt = getopt(argc, argv, "b:c:i:l:r:h")) != -1) {
		switch (opt) {
			case 'b': /* bank bit */
				page_shift = strtol(optarg, NULL, 0);
				break;
			case 'c': /* set CPU affinity */
				cpuid = strtol(optarg, NULL, 0);
				num_processors = sysconf(_SC_NPROCESSORS_CONF);
				CPU_ZERO(&cmask);
				CPU_SET(cpuid % num_processors, &cmask);
				if (sched_setaffinity(0, num_processors, &cmask) < 0)
					perror("error");
				break;
			case 'i': /* iterations */
				repeat = (uint64_t)strtol(optarg, NULL, 0);
				break;
			case 'l':
				shift_l = strtol(optarg, NULL, 0);
				break;
			case 'r':
				shift_r = strtol(optarg, NULL, 0);
				break;
		}

	}


	/* access bit aligning to GPU */
	if(page_shift >= 0)
		printf("align to GPU: access bit %d\n", page_shift);
	if((shift_l < 0) || (shift_r < 0)){
		printf("illegal l or r!\n");
		exit(1);
	}

	struct timespec seed;
	int mask[NUM_DIST] = {0};
	int ibit = 0, per_num;
	if(NUM_DIST >= NUM_ENTRIES){
		/*randomly choose one for a indices[]*/
		per_num = 1;
	}
	else{
		per_num = NUM_ENTRIES / NUM_DIST;
	}
	
	//printf("***************init indices:\n");
	for(i = 0; i < NUM_ENTRIES; i ++){
		while(1){
			clock_gettime(CLOCK_REALTIME, &seed);
			ibit = seed.tv_nsec % NUM_DIST;
			if(mask[ibit] < per_num){
				mask[ibit] ++;
				break;
			}
		}
		indices[i] = entry_dist[ibit] / 4;
		//printf("indices[%d] = 0x%lx\n", i, indices[i]);
	}
	
	while(1){
		for(i = shift_l; i <= shift_r; i ++){
			access_bank(page_shift, i, repeat);
		}
	}

	return 0;
}


