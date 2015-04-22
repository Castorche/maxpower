#define _GNU_SOURCE

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#include <MaxSLiCInterface.h>

#include <lmem_cpu_access.h>

#define _CONCAT(x, y) x ## y
#define CONCAT(x, y) _CONCAT(x, y)
#define MAXFILE_INIT CONCAT(DESIGN_NAME, _init)

#define MAX_BURSTS 2048


static uint8_t *tmp_buffer, *model;
static size_t burst_size_bytes;


static void mem_write(lmem_cpu_access_t *handle, uint32_t address_bursts, void *data, size_t data_size_bursts)
{
	lmem_write(handle, address_bursts, data, data_size_bursts);
	memcpy(model + address_bursts * burst_size_bytes, data, data_size_bursts * burst_size_bytes);
}

static void mem_read(lmem_cpu_access_t *handle, uint32_t address_bursts, void *data, size_t data_size_bursts)
{
	lmem_read(handle, address_bursts, data, data_size_bursts);
}

void mem_copy(lmem_cpu_access_t *handle, uint32_t src, uint32_t dst, size_t num_bursts) {
//	printf("Copy: %u -> %u [ %zd ]\n", src, dst, num_bursts);
	mem_read(handle, src, tmp_buffer, num_bursts);
	mem_write(handle, dst, tmp_buffer, num_bursts);
}

int main(int argc, char *argv[]) {

	max_file_t *maxfile = MAXFILE_INIT();
	max_engine_t * engine = max_load(maxfile, "*");

	max_config_set_bool(MAX_CONFIG_PRINTF_TO_STDOUT, true);

	max_actions_t *actions = max_actions_init(maxfile, NULL);

	max_run(engine, actions);
	max_actions_free(actions);

	lmem_cpu_access_t * handle = lmem_init_cpu_access(maxfile, engine);
	burst_size_bytes = lmem_get_burst_size_bytes(handle);

	size_t max_size_bytes = burst_size_bytes * MAX_BURSTS;

	printf("Allocating %zd bytes...\n", max_size_bytes);
	model = malloc(max_size_bytes);
	tmp_buffer = malloc(max_size_bytes);
	uint8_t *data = malloc(max_size_bytes);

	uint32_t seed = time(NULL);
	printf("Using random seed: 0x%x\n", seed);
	srand(seed);

	printf("Initializing buffer...\n");
	for (size_t i=0; i < max_size_bytes; i++) {
		data[i] = rand() & 0xFF;
	}

	printf("Writing to LMem...\n");
	mem_write(handle, 0, data, MAX_BURSTS);

	size_t num_iterations = 200;
	for (size_t i=0; i < num_iterations; i++) {
		/*
		 * Random copies back and forth
		 */

		uint32_t half_way = MAX_BURSTS / 2;
		uint32_t size = rand() % half_way;
		uint32_t rem = half_way - size;

		bool top = rand() % 2 == 0;

		uint32_t src = (rand() % rem) + (top ? half_way : 0);
		uint32_t dst = (rand() % rem) + (top ? 0 : half_way);

		mem_copy(handle, src, dst, size);
		printf(".");
		if (i % 100 == 99) printf("%zd / %zd\n", i+1, num_iterations);
		fflush(stdout);

	}

	printf("\n");



	printf("Reading from LMem...\n");
	mem_read(handle, 0, data, MAX_BURSTS);

	printf("Verifying data...\n");
	for (size_t i=0; i < max_size_bytes; i++) {
		if (model[i] != data[i]) {
			printf("Data at index %zd mismatch - expecting 0x%x, got 0x%x\n", i, model[i], data[i]);
			printf("FAILED\n");
			exit(1);
		}
	}


	printf("PASSED.\n"); fflush(stdout);
	return 0;
}
