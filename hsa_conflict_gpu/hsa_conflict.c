/* core bind */
#define _GNU_SOURCE
#include <sched.h>

/* mmap */
#include <sys/mman.h>

#include <inttypes.h>
#include <unistd.h>

#include "hsa_util.h"
#include "util.h"

//#define CHECKOUT 1
//#define PRINT 1
#define TIME 1

/* callback: choose a region of the agent to use */
static hsa_status_t get_kernarg(hsa_region_t region, void* data) {                                                                                                                                           
	hsa_region_flag_t flags;
	hsa_region_get_info(region, HSA_REGION_INFO_FLAGS, &flags);
	if (flags & HSA_REGION_FLAG_KERNARG) {
		hsa_region_t* ret = (hsa_region_t*) data;
		*ret = region;
		return HSA_STATUS_INFO_BREAK;   //add according to <runtime specification>
	}
	return HSA_STATUS_SUCCESS;
}


int main(){
	
	/* bind to a core */
	cpu_set_t cmask;
	int core = 0;
	int core_num = sysconf(_SC_NPROCESSORS_CONF);
	CPU_ZERO(&cmask);
	CPU_SET(core % core_num, &cmask);
	if (sched_setaffinity(0, core_num, &cmask) < 0){
		printf("bind core %d error\n", core);
		exit(1);
	}

	/* 1. init */
	hsa_init();

	hsa_agent_t agent;
	hsa_iterate_agents(get_component, &agent);

	hsa_queue_t *queue;
	hsa_queue_create(agent, 4, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, &queue);

	/* 2. create kernel and program */
	hsa_ext_program_handle_t hsaProgram;
	hsa_ext_brig_module_handle_t module;
	hsa_ext_finalization_request_t finalization_request_list;

	create_kernel(&agent, &hsaProgram, &module, &finalization_request_list);

	/* get hsa code descriptor */   
	hsa_ext_code_descriptor_t *hsaCodeDescriptor;
	if(hsa_ext_query_kernel_descriptor_address(hsaProgram, module, finalization_request_list.symbol, &hsaCodeDescriptor) != HSA_STATUS_SUCCESS) exit(1);
#ifdef PRINT
	printf("queue size:%d\n", queue->size);
#endif
	/* 3. Request a packet ID from the queue */
	uint64_t packet_id = hsa_queue_add_write_index_relaxed(queue, 1); 
	hsa_dispatch_packet_t* dispatch_packet = (hsa_dispatch_packet_t*) queue->base_address + (packet_id % queue->size);

	/* create signal */
	hsa_signal_t signal;
	hsa_signal_create(1, 0, NULL, &signal);
	//printf("initial signal: %lu\n", hsa_signal_load_acquire(signal));

	/* fill in a pkt except for header type */
	memset(dispatch_packet, 0, sizeof(hsa_dispatch_packet_t));
	initialize_packet(dispatch_packet, &signal);

	hsa_region_t region = 0;
	hsa_agent_iterate_regions(agent, get_kernarg, &region);
	if(region == 0) exit(1);

	/* 4. set kernel arguments */
	uint32_t shift = 8;		//gpu access bit8
	void *memchunk = NULL;
	uint64_t *start_index = (uint64_t *)malloc(sizeof(uint64_t) * ENTRY_NUM * GRID_X);
	uint64_t times = 1000000000;
	int *out = (int *)malloc(sizeof(int) * GRID_X);
	memset(out, 0, sizeof(int) * GRID_X);
	memset(start_index, 0, sizeof(uint64_t) * ENTRY_NUM * GRID_X);
	
	/* get access region */	
	uint64_t total_buffer_size = get_region(&memchunk, start_index, shift, GRID_X);
	hsa_memory_register(memchunk, total_buffer_size);
	
#ifdef PRINT
	printf("total buffer size : %lu, addr : %lx\n", total_buffer_size, (uint64_t)memchunk);
	printf("-----------------before GPU------------------\n");
	int i, j; 
	for(i = 0; i < ENTRY_NUM; i ++){
		j = start_index[i];
		printf("memchunk[%d] = %d\n", j, ((int *)memchunk)[j]);
	}
#endif

	struct __attribute__((aligned(HSA_ARGUMENT_ALIGN_BYTES))) args_t{
		void *arg0;
		void *arg1;
		void *arg2;
		void *arg3;
	} args;
	args.arg0 = memchunk;
	args.arg1 = (void *)start_index;
	args.arg3 = (void *)&times;
	args.arg2 = (void *)out;

	/* allocate memory pointers space not storage space using runtime */
	void *kern_arg_buffer = NULL;
	size_t size = hsaCodeDescriptor->kernarg_segment_byte_size;

	if(hsa_memory_allocate(region, size, &kern_arg_buffer) != HSA_STATUS_SUCCESS) exit(1);
	memset(kern_arg_buffer, 0, size);

#ifdef PRINT
	printf("args size: %lu\n", sizeof(struct args_t));
	printf("kern buf size: %lu\n", size);
#endif

	uint64_t kern_arg_start_offset = 0;
	kern_arg_start_offset += sizeof(uint64_t) * 6;	/* CLOC compiler features */
	void *kern_arg_buffer_start = kern_arg_buffer + kern_arg_start_offset;

	memcpy(kern_arg_buffer_start, &args, sizeof(struct args_t));


	/* set kernel address and argument start_address(including 6 extra args) to pkt */
	dispatch_packet->kernel_object_address = hsaCodeDescriptor->code.handle;
	dispatch_packet->kernarg_address = (uint64_t)kern_arg_buffer;

#ifdef TIME
	struct timespec start, end;
	clock_gettime(CLOCK_REALTIME, &start);
#endif

	/* 5. Notify: launch the kernel */
	packet_type_store_release(&dispatch_packet->header, HSA_PACKET_TYPE_DISPATCH);
	hsa_signal_store_relaxed(queue->doorbell_signal, packet_id);


	/* Wait for the task to finish: signal initial = 1, */
	hsa_signal_wait_acquire(signal, HSA_EQ, 0, (uint64_t) -1, HSA_WAIT_EXPECTANCY_UNKNOWN);

#ifdef TIME
	clock_gettime(CLOCK_REALTIME, &end);
	uint64_t nsdiff = get_elapsed(&start, &end);	//ns
	double bandwidth = (double)(GRID_X * sizeof(int) * times) * 1000 / nsdiff;
	printf("GPU running time is %.4fs\n", (double)nsdiff/1000000000);
	printf("*********************bandwidth is %.2fMB/s*********************\n", bandwidth);
#endif

	/* check the outcome */
#ifdef CHECKOUT
	printf("-----------------after GPU------------------\n");
	for(i = 0; i < ENTRY_NUM; i ++){
		j = start_index[i];
		printf("memchunk[%d] = %d\n", j, ((int *)memchunk)[j]);
	}

	for(i = 0; i < GRID_X; i ++){
		printf("thrd%d runs %d times\n", i, out[i]);
	}
#endif
	hsa_signal_destroy(signal);
	hsa_ext_program_destroy(hsaProgram);
	hsa_queue_destroy(queue);
	hsa_shut_down();

	free(start_index);
	munmap(memchunk, total_buffer_size);
	return 0;
}

