#include "hsa_util.h"

void initialize_packet(hsa_dispatch_packet_t* dispatch_packet, hsa_signal_t* signal) {
	// Contents are zeroed:
	// -Reserved fields must be 0
	// -Type is set to HSA_PACKET_TYPE_ALWAYS_RESERVED, so the packet cannot be consumed by the packet processor
	memset(dispatch_packet, 0, sizeof(hsa_dispatch_packet_t));
	dispatch_packet->header.acquire_fence_scope = HSA_FENCE_SCOPE_COMPONENT;
	dispatch_packet->header.release_fence_scope = HSA_FENCE_SCOPE_COMPONENT;
	dispatch_packet->dimensions = 1;
	dispatch_packet->workgroup_size_x = WORKGROUP_X;
	dispatch_packet->workgroup_size_y = 1;
	dispatch_packet->workgroup_size_z = 1;
	dispatch_packet->grid_size_x = GRID_X;
	dispatch_packet->grid_size_y = 1;
	dispatch_packet->grid_size_z = 1;
	dispatch_packet->completion_signal = *signal;
}

hsa_status_t get_component(hsa_agent_t agent, void* data) {
	uint32_t features = 0;
	hsa_agent_get_info(agent, HSA_AGENT_INFO_FEATURE, &features);
	if (features & HSA_AGENT_FEATURE_DISPATCH) {
		hsa_agent_t* ret = (hsa_agent_t*) data;
		*ret = agent;
		return HSA_STATUS_INFO_BREAK;
	}
	return HSA_STATUS_SUCCESS;
}

void packet_type_store_release(hsa_packet_header_t* header, hsa_packet_type_t type) {
	__atomic_store_n((uint8_t*) header, (uint8_t) type, __ATOMIC_RELEASE);
}

void create_kernel(hsa_agent_t* agent_ptr, hsa_ext_program_handle_t* hsaProgram_ptr, 
						hsa_ext_brig_module_handle_t *module_ptr, 
						hsa_ext_finalization_request_t *finalization_request_list_ptr) {

	/* module */
	hsa_ext_brig_module_t* brigModule;
	char file_name[128] = "brig/hsa_conflict.brig";
	if(create_brig_module_from_brig_file(file_name, &brigModule) != HSA_STATUS_SUCCESS) exit(1);

	/* program */
	//hsa_ext_program_handle_t hsaProgram;
	if(hsa_ext_program_create(agent_ptr, 1, HSA_EXT_BRIG_MACHINE_LARGE, HSA_EXT_BRIG_PROFILE_FULL, hsaProgram_ptr) != HSA_STATUS_SUCCESS) exit(1);

	/* add module to program */
	//hsa_ext_brig_module_handle_t module;
	if(hsa_ext_add_module(*hsaProgram_ptr, brigModule, module_ptr) != HSA_STATUS_SUCCESS) exit(1);

	/* finalization request list */
	//hsa_ext_finalization_request_t finalization_request_list;
	finalization_request_list_ptr->module = *module_ptr;
	finalization_request_list_ptr->program_call_convention = 0;
	char kernel_name[128] = "&__OpenCL_hsa_conflict_kernel"; // from xxx.hsail
	if(find_symbol_offset(brigModule, kernel_name, &(finalization_request_list_ptr->symbol)) != HSA_STATUS_SUCCESS) exit(1);

	/* finalize the hsaprogram */
	if(hsa_ext_finalize_program(*hsaProgram_ptr, *agent_ptr, 1, finalization_request_list_ptr, NULL, NULL, 0, NULL, 0) != HSA_STATUS_SUCCESS) exit(1);

	/* destory the brig module */
	destroy_brig_module(brigModule);
}


hsa_status_t find_symbol_offset(hsa_ext_brig_module_t* brig_module, char* symbol_name, hsa_ext_brig_code_section_offset32_t* offset) {
	
	hsa_ext_brig_section_header_t* data_section_header = 
		brig_module->section[HSA_EXT_BRIG_SECTION_DATA];
	/* 
	 * Get the code section
	 */
	hsa_ext_brig_section_header_t* code_section_header =
		brig_module->section[HSA_EXT_BRIG_SECTION_CODE];

	/* 
	 * First entry into the BRIG code section
	 */
	BrigCodeOffset32_t code_offset = code_section_header->header_byte_count;
	BrigBase* code_entry = (BrigBase*) ((char*)code_section_header + code_offset);
	while (code_offset != code_section_header->byte_count) {
		if (code_entry->kind == BRIG_KIND_DIRECTIVE_KERNEL) {
			/* 
			 * Now find the data in the data section
			 */
			BrigDirectiveExecutable* directive_kernel = (BrigDirectiveExecutable*) (code_entry);
			BrigDataOffsetString32_t data_name_offset = directive_kernel->name;
			BrigData* data_entry = (BrigData*)((char*) data_section_header + data_name_offset);
			if (!strncmp(symbol_name, (char*) data_entry->bytes, strlen(symbol_name))) {
				*offset = code_offset;
				return HSA_STATUS_SUCCESS;
			}
		}
		code_offset += code_entry->byteCount;
		code_entry = (BrigBase*) ((char*)code_section_header + code_offset);
	}
	return HSA_STATUS_ERROR;
}
