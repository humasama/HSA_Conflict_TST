__kernel void hsa_conflict(__global int *memchunk, __global unsigned long *start_index,  __global int *out, __global unsigned long *times ){

	int id = get_global_id(0);
	int index = 0;
	int value;

	unsigned long i = 0, start = 0;
	while(i < *times){
		start = id * 32 + i % 32;
		index = id + start_index[start];
		value = memchunk[index];
		out[id] ++;
		i ++;
	}
}
	

