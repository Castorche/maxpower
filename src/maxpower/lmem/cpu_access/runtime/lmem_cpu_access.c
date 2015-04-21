/*
 * lmem_cpu_access.c
 *
 *  Created on: 17 Apr 2015
 *      Author: itay
 */

#include <MaxSLiCInterface.h>
#include "lmem_cpu_access.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/time.h>

#include "../../runtime/lmem.h"


#define MAX_NAME_LENGTH 64
#define MAX_STREAM_NAME_LENGTH (MAX_NAME_LENGTH + 64)

#define LMEM_CMD_STREAM_NAME "CpuAccessLMemCommands"
#define LMEM_CONTROL_GROUP_NAME "CpuAccessControlGroup"

#define LMEM_WRITE_CPU_STREAM_NAME "CpuToLMem"
#define LMEM_READ_CPU_STREAM_NAME "LMemToCpu"

#define LMEM_WRITE_STREAM_NAME "LMemCpuWrite"
#define LMEM_READ_STREAM_NAME "LMemCpuRead"

#define TIMEOUT_SECONDS 5

struct lmem_cpu_access_s {
	max_file_t *maxfile;
	max_engine_t *engine;
	size_t cmd_buffer_size;
	void *cmd_buffer;

	uint32_t burst_size_bytes;


	max_llstream_t *cmd_stream;
	uint16_t to_lmem_stream_id;
	uint16_t from_lmem_stream_id;
};

enum access_mode_e {
	LMemRead,
	LMemWrite
};

typedef struct ATTRIB_PACKED mem_cmd_stream_slot_s {
	lmem_cmd_t cmd1;
	lmem_cmd_t cmd2;
} mem_cmd_stream_slot_t;


size_t lmem_get_burst_size_bytes(lmem_cpu_access_t *handle)
{
	return handle->burst_size_bytes;
}


lmem_cpu_access_t *init_lmem_cpu_access(max_file_t *maxfile, max_engine_t *engine)
{
	assert(maxfile != NULL);
	assert(engine != NULL);

	lmem_cpu_access_t *handle = malloc(sizeof(lmem_cpu_access_t));
	if (handle == NULL) {
		printf("%s: Failed to allocate memory for handle.\n", __func__);
		abort();
	}

	handle->engine = engine;
	handle->maxfile = maxfile;
	handle->burst_size_bytes = max_get_constant_uint64t(maxfile, "MemCtrlPro_DataBurstSizeInBytes");

	handle->cmd_buffer_size = 512 * sizeof(mem_cmd_stream_slot_t);
	posix_memalign(&handle->cmd_buffer, 4096, handle->cmd_buffer_size);
	handle->cmd_stream = max_llstream_setup(handle->engine, LMEM_CMD_STREAM_NAME, 512, sizeof(mem_cmd_stream_slot_t), handle->cmd_buffer);

	handle->to_lmem_stream_id = 1 << max_lmem_get_id_within_group(maxfile, LMEM_WRITE_STREAM_NAME);
	handle->from_lmem_stream_id = 1 << max_lmem_get_id_within_group(maxfile, LMEM_READ_STREAM_NAME);

	return handle;
}

static void *acquire_memory_command_slot(lmem_cpu_access_t *handle, int timeout_seconds)
{
	struct timeval timeout = { .tv_sec = timeout_seconds, .tv_usec = 0 };
	struct timeval time_start, time_now, time_delta;

	void *cmd_slot;

	gettimeofday(&time_start, NULL);
	while (max_llstream_write_acquire(handle->cmd_stream, 1, &cmd_slot) != 1) {
		gettimeofday(&time_now, NULL);
		timersub(&time_now, &time_start, &time_delta);

		if (timercmp(&time_delta, &timeout, >=)) {
			printf("%s: Timed-out while waiting for a memory command stream\n", __func__);
			abort();
		} else {
			usleep(1);
		}
	}

	return cmd_slot;
}

static void commit_memory_command_slot(lmem_cpu_access_t *handle)
{
	max_llstream_write(handle->cmd_stream, 1);
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static void do_memory_access(lmem_cpu_access_t *handle,
		enum access_mode_e access_mode,
		uint32_t base_address_bursts,
		void *data, size_t data_size_bursts)
{
	max_actions_t *actions = max_actions_init(handle->maxfile, NULL);

	if (access_mode == LMemRead) max_queue_output(actions, LMEM_READ_CPU_STREAM_NAME,  data, data_size_bursts * handle->burst_size_bytes);
	else                         max_queue_input (actions, LMEM_WRITE_CPU_STREAM_NAME, data, data_size_bursts * handle->burst_size_bytes);

	max_run_t *runContext = max_run_nonblock(handle->engine, actions);

	uint16_t stream_id = access_mode == LMemRead ? handle->from_lmem_stream_id : handle->to_lmem_stream_id;
	size_t position = 0;
	size_t size_remaining_bursts = data_size_bursts;
	mem_cmd_stream_slot_t *cmdSlot;




	while (size_remaining_bursts > 0) {
		cmdSlot = acquire_memory_command_slot(handle, TIMEOUT_SECONDS);

		bool isPadding = size_remaining_bursts == 0;

		size_t now = MIN(size_remaining_bursts, 127);
		size_remaining_bursts -= now;
		// First command in the slot
		cmdSlot->cmd1 = lmem_cmd_data(stream_id, base_address_bursts + position, now, false, false);
		position += now;


		isPadding = size_remaining_bursts == 0;
		// Second command in the slot
		now = MIN(size_remaining_bursts, 127);
		size_remaining_bursts -= now;
		cmdSlot->cmd2 = isPadding ?
				lmem_cmd_padding() :
				lmem_cmd_data(stream_id, base_address_bursts + position, now, false, false);
		position += now;

		commit_memory_command_slot(handle);
	}


	max_wait(runContext);

	max_actions_free(actions);
}

void lmem_write(lmem_cpu_access_t *handle, uint32_t address_bursts, void *data, size_t data_size_bursts)
{
	do_memory_access(handle, LMemWrite, address_bursts, data, data_size_bursts);
}

void lmem_read(lmem_cpu_access_t *handle, uint32_t address_bursts, void *data, size_t data_size_bursts)
{
	do_memory_access(handle, LMemRead, address_bursts, data, data_size_bursts);
}



