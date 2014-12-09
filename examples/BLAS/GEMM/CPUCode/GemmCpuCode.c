#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DGEMM.h"

#define NUM_TMS   DGEMM_numTileMultipliers
#define TILE_SIZE DGEMM_tileSize

void dgemm_model(
		const char* trans_a, const char* trans_b, size_t m, size_t n, size_t k, double alpha,
		const double* A, size_t lda, const double* B, size_t ldb, double beta, double* C, size_t ldc)
{
	// TODO support transform

	for (size_t mm = 0; mm < m; ++mm) {
		for (size_t nn = 0; nn < n; ++nn) {
			C[mm*ldc+nn] *= beta;
			for (size_t kk = 0; kk < k; ++kk) {
				C[mm*ldc+nn] += alpha * A[mm*lda+kk] * B[kk*ldb+nn];
			}
		}
	}
}

max_file_t* maxfile;
max_engine_t* engine;
const max_handle_t* aHandle;
const max_handle_t* bHandle;
const max_handle_t* cHandle;

void dgemm_init() {
	printf("Initializing maxfile...\n");
	maxfile  = DGEMM_init();
	aHandle  = max_get_handle_stream(maxfile, "A");
	bHandle  = max_get_handle_stream(maxfile, "B");
	cHandle  = max_get_handle_stream(maxfile, "C");
	printf("Loading engine...\n");
	engine   = max_load(maxfile, "*");
}

void dgemm(
		const char* trans_a, const char* trans_b, size_t m, size_t n, size_t k, double alpha,
		const double* A, size_t lda, const double* B, size_t ldb, double beta, double* C, size_t ldc)
{
	// TODO support transform

	// TODO add support for arbitrary tile size and add padding
	if (((m % TILE_SIZE) != 0) || ((k % TILE_SIZE) != 0) || ((n % TILE_SIZE) != 0)) {
		printf("Matrix dimensions must be an exact multiple of the tile size (%d)\n", TILE_SIZE);
		exit(1);
	}

	size_t mTiles = (m + TILE_SIZE - 1) / TILE_SIZE;
	size_t nTiles = (n + TILE_SIZE - 1) / TILE_SIZE;
	size_t kTiles = (k + TILE_SIZE - 1) / TILE_SIZE;

	size_t numTiles   = mTiles * nTiles * kTiles;
	size_t tileSize2D = TILE_SIZE * TILE_SIZE;

	double* aIn  = malloc(mTiles * kTiles * tileSize2D * sizeof(double));
	double* bIn  = malloc(kTiles * nTiles * tileSize2D * sizeof(double));
	double* cOut = malloc(mTiles * nTiles * tileSize2D * sizeof(double));

	// TODO OMP

	size_t pos = 0;
	for (size_t mm = 0; mm < mTiles; ++mm) {
		for (size_t kk = 0; kk < kTiles; ++kk) {
			// TODO special case for m == mTiles-1 or n == nTiles-1 (add padding)
			for (size_t x = 0; x < TILE_SIZE; ++x) {
				for (size_t y = 0; y < TILE_SIZE; ++y) {
					aIn[pos++] = A[(mm*TILE_SIZE+x)*lda+kk*TILE_SIZE+y];
				}
			}
		}
	}

	pos = 0;
	for (size_t nn = 0; nn < nTiles; ++nn) {
		for (size_t kk = 0; kk < kTiles; ++kk) {
			// TODO special case for m == mTiles-1 or n == nTiles-1 (add padding)
			for (size_t x = 0; x < TILE_SIZE; ++x) {
				for (size_t y = 0; y < TILE_SIZE; ++y) {
					bIn[pos++] = B[(kk*TILE_SIZE+x)*ldb+nn*TILE_SIZE+y];
				}
			}
		}
	}

	max_actions_t* actions = max_actions_init(maxfile, NULL);
	for (size_t i = 0; i < NUM_TMS; ++i) {
		char name[128];
		snprintf(name, sizeof(name), "TileMultiplier%zu", i);

		max_set_ticks(actions, name, (numTiles+1) * tileSize2D);
		max_set_uint64t(actions, name, "numTiles", numTiles);
	}

	if (NUM_TMS > 1) {
		max_set_ticks(actions, "RoundRobinA", numTiles * tileSize2D);
		max_set_ticks(actions, "RoundRobinB", numTiles * tileSize2D);
	}

	max_set_ticks(actions, "TileAccumulator", numTiles * tileSize2D);
	max_set_uint64t(actions, "TileAccumulator", "sumTiles", kTiles);

	for (size_t mm = 0; mm < mTiles; ++mm) {
		for (size_t nn = 0; nn < nTiles; ++nn) {
			max_queue_input_handle(actions, aHandle, &aIn[mm*kTiles*tileSize2D], kTiles * tileSize2D * sizeof(double));
		}
	}

	for (size_t mm = 0; mm < mTiles; ++mm) {
		max_queue_input_handle(actions, bHandle, bIn, kTiles * nTiles * tileSize2D * sizeof(double));
	}

	max_queue_output_handle(actions, cHandle, cOut, mTiles * nTiles * tileSize2D * sizeof(double));

	printf("Running action...\n");
	max_run(engine, actions);

	pos = 0;
	for (size_t mm = 0; mm < mTiles; ++mm) {
		for (size_t nn = 0; nn < nTiles; ++nn) {
			for (size_t x = 0; x < TILE_SIZE; ++x) {
				for (size_t y = 0; y < TILE_SIZE; ++y) {
					C[(mm*TILE_SIZE+x)*ldc+nn*TILE_SIZE+y] *= beta;
					C[(mm*TILE_SIZE+x)*ldc+nn*TILE_SIZE+y] += alpha * cOut[pos++];
				}
			}
		}
	}
}

int main() {
	dgemm_init();

	const int m = 2 * TILE_SIZE;
	const int n = 4 * TILE_SIZE;
	const int k = 3 * TILE_SIZE;
	double* A   = calloc(m * k, sizeof(double));
	double* B   = calloc(k * n, sizeof(double));
	double* Csw = calloc(m * n, sizeof(double));
	double* Chw = calloc(m * n, sizeof(double));

	for (int i = 0; i < m*k; ++i) {
		A[i] = random() % 100;
	}

	for (int i = 0; i < k*n; ++i) {
		B[i] = random() % 100;
	}

	for (int i = 0; i < m*n; ++i) {
		Csw[i] = random() % 100;
		Chw[i] = Csw[i];
	}

	// TODO random
	double alpha = 1;
	double beta  = 0;

	printf("Running SW...\n");
	dgemm_model("n", "n", m, n, k, alpha, A, k, B, n, beta, Csw, n);
	printf("Running HW...\n");
	dgemm("n", "n", m, n, k, alpha, A, k, B, n, beta, Chw, n);

	printf("Comparing results...\n");
	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; ++j) {
			int idx = i*n + j;

			if (Csw[idx] != Chw[idx])
				printf("Csw_ij = %f, Chw_ji = %f\n", Csw[idx], Chw[idx]);
		}
	}

	printf("Done.\n");
	return 0;
}
