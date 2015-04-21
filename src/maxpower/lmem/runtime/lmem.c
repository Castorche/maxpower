/*
 * LMem.c
 *
 *  Created on: 20 Apr 2015
 *      Author: itay
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "lmem.h"

#define MAX_ADDRESS ((1<<27)-1)
#define MAX_STREAM_ID (1<<15)

static bool is_power_of_2(uint32_t v) {
	return ((v & (v-1)) == v);
}

static bool is_one_hot_encoded(uint32_t v) {
	return is_power_of_2(v);
}


lmem_cmd_t lmem_cmd_padding()
{
	static lmem_cmd_t null_cmd = { .mode = { .cmd = {0} }, .stream_select = 0, .tag = 0 };
	return null_cmd;
}

lmem_cmd_t lmem_cmd_data(uint16_t streamId, size_t address, size_t sizeBursts, bool genEcho, bool genInterrupt)
{
	lmem_cmd_t cmd = lmem_cmd_padding();

	if (address > MAX_ADDRESS) {
		printf("lmem_cmd_data: Memory address is out of range: %zu is greater than max %u.\n", address, MAX_ADDRESS);
		abort();
	}

	if (sizeBursts == 0) {
		printf("lmem_cmd_data: Size in Burst must be > 0\n");
		abort();
	}

	if (!is_one_hot_encoded(streamId)) {
		printf("lmem_cmd_data: streamId must be one-hot encoded. Supplied argument: 0x%x is not.\n", streamId);
		abort();
	}

	if (streamId > MAX_STREAM_ID) {
		printf("lmem_cmd_data: streamId out of range. Supplied argument: 0x%x, max: 0x%x.\n", streamId, MAX_STREAM_ID);
		abort();
	}

	cmd.tag = genInterrupt;
	cmd.stream_select = streamId;

	cmd.mode.normal.size = sizeBursts;
	cmd.mode.normal.address = address;
	cmd.mode.normal.inc = 1;
	cmd.mode.normal.inc_mode = 0;
	cmd.mode.normal.echo_command = genEcho;


	return cmd;
}

lmem_cmd_t lmem_cmd_control(enum lmem_cmd_code_e command_code, uint16_t streamId, uint32_t flagIx)
{
	lmem_cmd_t cmd = lmem_cmd_padding();

	if (flagIx > 31) {
		printf("lmem_cmd_control: flagIx out of range. Value needs to be between 0 and 31 inclusive. Supplied argument is %d\n", flagIx);
		abort();
	}

	cmd.stream_select = streamId;
	cmd.mode.cmd.mode = 0;
	cmd.mode.cmd.flag_id = 1 << flagIx;
	cmd.mode.cmd.code = command_code;

	return cmd;
}

