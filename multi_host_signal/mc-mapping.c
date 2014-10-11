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
#define ENTRY_SHIFT   (17)                  // [27:23] bits are used for iterations? interval:32MB
#define ENTRY_DIST    (uint64_t)(1<<ENTRY_SHIFT)      // distance between the two entries
#define CACHE_LINE_SIZE 64

#define MAX(a,b) ((a>b)?(a):(b))
#define CEIL(val,unit) (((val + unit - 1)/unit)*unit)

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
   __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)


static uint64_t g_mem_size = (uint64_t)((NUM_ENTRIES) * (ENTRY_DIST));
/* bank data */
static int* list;
/* index array for accessing banks */
static uint64_t indices[NUM_ENTRIES];
static uint64_t next;

#define MAX_BANK_BIT_NUM 20
static double bandwidth[MAX_BANK_BIT_NUM] = {0.0};




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

//using array accesses phy address
uint64_t run(uint64_t iter)
{
	uint64_t i, j = 0;
	uint64_t cnt = 0;
	int data;

	for (i = 0; i < iter; i++) {
		data = list[next];
		next = indices[j];

		j ++;
		if(j == NUM_ENTRIES) j = 0;
		cnt ++;
	}
	return cnt;
}


void access_bank(int page_shift, int xor_page_shift, int align_gpu_bank, int core, uint64_t repeat)
{
	struct sched_param param;
	cpu_set_t cmask;
	int num_processors;
	int cpuid = core;;
	int use_dev_mem = 0;

	int *memchunk = NULL;
	int i,j;

	num_processors = sysconf(_SC_NPROCESSORS_CONF);
	CPU_ZERO(&cmask);
	CPU_SET(cpuid % num_processors, &cmask);
	if (sched_setaffinity(0, num_processors, &cmask) < 0)
		perror("error");

	g_mem_size += (1 << page_shift) + (1 << xor_page_shift); 	//fix bug of XOR

	/* align base accessing bank of GPU */
	if(align_gpu_bank >= 0) g_mem_size += (1 << align_gpu_bank);

	g_mem_size = CEIL(g_mem_size, ENTRY_DIST);

	/* alloc memory. align to a page boundary */
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	void *addr = (void *) 0x1000000080000000;

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

	int off_idx = (1<<page_shift) / 4;

	if (xor_page_shift > 0) {
		off_idx = ((1<<page_shift) + (1<<xor_page_shift)) / 4;
	}

#if defined(TEST)
	printf("/////////////////ENTRY_DIST : %lu\n////////////////", ENTRY_DIST);
	printf("/////////////////NUM_ENTRIES : %lu\n////////////////", NUM_ENTRIES);
	printf("/////////////////NUM_ENTRIES * ENTRY_DIST : %lu\n////////////////", NUM_ENTRIES * ENTRY_DIST);
	printf("/////////////////g_mem_size = NUM_ENTRIES * ENTRY_DIST : %lu\n////////////////", g_mem_size);
#endif

	list = &memchunk[off_idx];
	for (i = 0; i < NUM_ENTRIES; i++) {
		if (i == (NUM_ENTRIES - 1))
			indices[i] = 0;
		else
			indices[i] = (i + 1) * ENTRY_DIST/4;
	}
	
	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);

	/* access banks */
	uint64_t naccess = run(repeat);

	clock_gettime(CLOCK_REALTIME, &end);

	int64_t nsdiff = get_elapsed(&start, &end);
	double  avglat = (double)nsdiff/naccess;

	//printf("size: %ld (%ld KB)\n", g_mem_size, g_mem_size/1024);
	//printf("duration %ld ns, #access %ld\n", nsdiff, naccess);
	//printf("average latency: %ld ns\n", nsdiff/naccess);
	
	//printf("bit%d %.2f MB/s\n", page_shift, 64.0*1000.0*(double)naccess/(double)nsdiff);
	bandwidth[page_shift] = 64.0 * 1000.0 * (double)naccess/(double)nsdiff;

	munmap(memchunk, g_mem_size);
	close(fd);
}

void handler(int sig){
	if(sig == SIGINT){
		int i = 0;
		for(; i < MAX_BANK_BIT_NUM; i++){
			printf("bit%d %.4f MB/s\n", i, bandwidth[i]);
		}
		exit(0);
	}
	else{
		printf("sig error!\n");
	}
}


int main(int argc, char* argv[]){

	uint64_t repeat = 1000;

	int page_shift_l = 0, page_shift_r = 0;
	int xor_page_shift = 0;
	int cpuid;
	int align_gpu_bank = -8;
	int opt, reverse = 0;

	signal(SIGINT, handler);

	/* 'h' is necessary for the third argument of getopt*/
	while ((opt = getopt(argc, argv, "l:r:s:m:c:i:a:v")) != -1) {
		switch (opt) {
			case 'l': /* bank bit */
				page_shift_l = strtol(optarg, NULL, 0);
				break;
			case 'r' :
				page_shift_r = strtol(optarg, NULL, 0);
				break;
			case 's': /* xor-bank bit */
				xor_page_shift = strtol(optarg, NULL, 0);
				break;
			case 'm': /* set memory size */
				g_mem_size = 1024 * strtol(optarg, NULL, 0);
				break;
			case 'c': /* set CPU affinity */
				cpuid = strtol(optarg, NULL, 0);
				break;
			case 'i': /* iterations */
				repeat = (uint64_t)strtol(optarg, NULL, 0);
				break;
			case 'a':
				align_gpu_bank = (int)strtol(optarg, NULL, 0);
				break;
			case 'v':
				reverse = 1;
				break;
		}
	}

	int shift;
	if(!reverse){
		while(1){
			for(shift = page_shift_l; shift <= page_shift_r; shift ++){
				access_bank(shift, xor_page_shift, align_gpu_bank, cpuid, repeat);
			}
		}
	}
	else{
		while(1){
			for(shift = page_shift_r; shift >= page_shift_l; shift --){
				access_bank(shift, xor_page_shift, align_gpu_bank, cpuid, repeat);
			}
		}
	}

	return 0;
}
