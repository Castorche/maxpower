/*
 * maxhash_slic.c
 *
 *  Created on: 10 Jul 2013
 *      Author: tperry
 */

#include "maxhash_internal.h"

#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <unistd.h>



bool has_constant_uint64t(maxhash_engine_state_t *es,
		const char *hash_table_name,
		const char *constant_name)
{
	char name_buf[NAME_BUF_LEN] = {0};
	strncpy(name_buf, hash_table_name, NAME_BUF_LEN);

	assert(max_ok(es->maxfile->errors));
	bool has_constant = true;

	max_errors_mode(es->maxfile->errors, 0);
	max_get_constant_uint64t(es->maxfile, strcat(name_buf,
			constant_name));

	if (!max_ok(es->maxfile->errors))
	{
		has_constant = false;
		max_errors_clear(es->maxfile->errors);
	}

	max_errors_mode(es->maxfile->errors, 1);

	return has_constant;
}



bool has_constant_string(maxhash_engine_state_t *es,
		const char *hash_table_name,
		const char *constant_name)
{
	char name_buf[NAME_BUF_LEN] = {0};
	strncpy(name_buf, hash_table_name, NAME_BUF_LEN);

	assert(max_ok(es->maxfile->errors));
	bool has_constant = true;

	max_errors_mode(es->maxfile->errors, 0);
	max_get_constant_string(es->maxfile, strcat(name_buf,
			constant_name));

	if (!max_ok(es->maxfile->errors))
	{
		has_constant = false;
		max_errors_clear(es->maxfile->errors);
	}

	max_errors_mode(es->maxfile->errors, 1);

	return has_constant;
}



int get_maxfile_global_constant(maxhash_engine_state_t *es,
		const char *constant_name)
{
	return (int)max_get_constant_uint64t(es->maxfile, constant_name);
}



int get_maxfile_constant(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name)
{
	char name_buf[NAME_BUF_LEN] = {0};
	strncpy(name_buf, hash_table_name, NAME_BUF_LEN);

	return (int)max_get_constant_uint64t(es->maxfile, strcat(name_buf,
				constant_name));
}



const char *get_maxfile_string_constant(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name)
{
	char name_buf[NAME_BUF_LEN] = {0};
	strncpy(name_buf, hash_table_name, NAME_BUF_LEN);

	return max_get_constant_string(es->maxfile, strcat(name_buf,
				constant_name));
}



void maxhash_init_deep_fmem_fanout(maxhash_engine_state_t *es,
		uint8_t deep_fmem_id)
{
	max_actions_t *actions = max_actions_init(es->maxfile, NULL);
	char name_buf[NAME_BUF_LEN] = {0};
	snprintf(name_buf, sizeof(name_buf), "%u", deep_fmem_id);
	max_route(actions, "DeepFMemFanout", name_buf);
	max_run(es->engine, actions);
	max_actions_free(actions);
}



void maxhash_write_fmem(maxhash_engine_state_t *es, const char *kernel_name,
		const char *mem_name, size_t base_entry, void *data,
		size_t data_size_bytes)
{
	max_actions_t *actions = max_actions_init(es->maxfile, NULL);

	for (size_t offset = 0; offset < data_size_bytes; offset +=
			sizeof(uint64_t))
	{
		size_t entry = offset / sizeof(uint64_t);
		max_set_mem_uint64t(actions, kernel_name, mem_name, base_entry + entry,
				((uint64_t *)data)[entry]);
	}

	max_disable_validation(actions); /* Because we only set one mapped memory at
	                                    a time. */
	max_run(es->engine, actions);
	max_actions_free(actions);
}



void maxhash_write_deep_fmem(maxhash_engine_state_t *es, const char
		*kernel_name, const char *mem_name, void *data, size_t data_size_bytes)
{
	char mem_name_buf[NAME_BUF_LEN] = {0};
	snprintf(mem_name_buf, sizeof(mem_name_buf), "%s_Complete", mem_name);

	//printf("Loading %zu bytes.\n", data_size_bytes);

	max_actions_t *actions = max_actions_init(es->maxfile, NULL);
	max_disable_validation(actions);
	max_queue_input(actions, "DeepFMemWriteCPUData", data, data_size_bytes);
	max_run(es->engine, actions);
	max_actions_free(actions);

	uint64_t complete = 0;
	while (!complete)
	{
		max_actions_t *actions = max_actions_init(es->maxfile, NULL);
		max_disable_validation(actions);
		max_get_mem_uint64t(actions, kernel_name, mem_name_buf, 0, &complete);
		max_run(es->engine, actions);
		max_actions_free(actions);
		usleep(10);
		//printf("mem is %lu\n", complete);
	}

	actions = max_actions_init(es->maxfile, NULL);
	max_disable_validation(actions);
	max_set_mem_uint64t(actions, kernel_name, mem_name_buf, 0, 0);
	max_run(es->engine, actions);
	max_actions_free(actions);
}
