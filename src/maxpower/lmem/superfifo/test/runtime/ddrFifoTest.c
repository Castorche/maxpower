/*
 * tcpFifoTest.c
 *
 *  Created on: 16 May 2013
 *      Author: itay
 */


#define DESIGN_NAME DDRFifoTest
#include <MaxCompilerRT.h>
#undef DESIGN_NAME

#define INIT_NAME max_maxfile_init_DDRFifoTest

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>


static max_device_handle_t * device;

typedef struct {
	uint8_t data[384];
} single_burst_t;


#define MAX_DEPTH (1024*256)

int main(int argc, char *argv[])
{
	bool is_sim = true;
	max_maxfile_t *maxfile = INIT_NAME();
	device = max_open_device( maxfile, ( is_sim ? "sim0:sim" : "/dev/maxeler2" ) );

	assert(device != NULL);

	if( is_sim )
	{
		max_redirect_sim_debug_output_to_stdout( device );
		max_disable_sim_watchnodes( device );
	}


	max_set_shutdown_on_exit( device, 1, 10000 );
	max_set_terminate_on_error( device );
	max_set_dump_trace_on_error( device );

	max_reset_device( device );

	srand(time(NULL));
	single_burst_t *inputData = malloc(sizeof(single_burst_t) * MAX_DEPTH);
	single_burst_t *outputData = calloc(MAX_DEPTH, sizeof(single_burst_t));

	printf("Building data...\n"); fflush(stdout);
	for (size_t burst=0; burst < MAX_DEPTH; burst++) {
		uint64_t *d = (uint64_t *)inputData[burst].data;
		size_t quadsPerBurst = sizeof(single_burst_t) / sizeof(uint64_t);

		for (size_t q = 0; q < quadsPerBurst; q++) {
			d[q] = burst * quadsPerBurst + q;
		}
	}

	max_stream_handle_t *write_fifo = max_get_pcie_stream(device, "write_fifo");
	max_stream_handle_t *read_fifo = max_get_pcie_stream(device, "read_fifo");


	printf("Streaming ...\n"); fflush(stdout);
	max_queue_pcie_stream(device, write_fifo, inputData, sizeof(single_burst_t) * MAX_DEPTH, 0);
	max_queue_pcie_stream(device, read_fifo, outputData, sizeof(single_burst_t) * MAX_DEPTH, 0);


	printf("Syncing read ...\n"); fflush(stdout);
	max_sync_pcie_stream(device, write_fifo);
	max_sync_pcie_stream(device, read_fifo);


	printf("Comparing...\n"); fflush(stdout);
	uint8_t fail = 0;
	for (size_t burst=0; burst < MAX_DEPTH; burst++) {
		uint64_t *input = (uint64_t *)inputData[burst].data;
		uint64_t *output = (uint64_t *)outputData[burst].data;
		size_t quadsPerBurst = sizeof(single_burst_t) / sizeof(uint64_t);

		for (size_t q = 0; q < quadsPerBurst; q++) {
			if (input[q] != output[q]) {
				fail = 1;
				printf("[Burst: %zd, Quad: %zd] Mismatch: input 0x%lx, output 0x%lx\n", burst, q, input[q], output[q]);
			}
		}
	}

	printf("%s\n", fail ? "FAILED!" : "Success");
	return fail;
}
