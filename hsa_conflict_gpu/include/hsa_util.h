#ifndef _HSA_UTIL_H_
#define _HSA_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* host runtime */
#include "hsa.h"
/* extention */
#include "hsa_ext_finalize.h"
#include "elf_utils.h"

#include "hsa_types.h"


void initialize_packet(hsa_dispatch_packet_t* dispatch_packet, hsa_signal_t* signal);

hsa_status_t get_component(hsa_agent_t agent, void* data);

void packet_type_store_release(hsa_packet_header_t* header, hsa_packet_type_t type);

void create_kernel(hsa_agent_t* agent_ptr, hsa_ext_program_handle_t* hsaProgram_ptr, 
						hsa_ext_brig_module_handle_t *module_ptr, 
						hsa_ext_finalization_request_t *finalization_request_list_ptr);

hsa_status_t find_symbol_offset(hsa_ext_brig_module_t* brig_module, char* symbol_name, hsa_ext_brig_code_section_offset32_t* offset);

//static hsa_status_t get_kernarg(hsa_region_t region, void* data);

#endif
