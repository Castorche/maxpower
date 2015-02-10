#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "DGEMM.h"

#define TILE_SIZE DGEMM_tileSize
#define TILE_SIZE_2D (TILE_SIZE * TILE_SIZE)
#define FREQUENCY DGEMM_frequency

#define USE_BLAS

#ifdef USE_BLAS
#include <cblas.h>
#endif

void dgemm_model(
		const char* trans_a, const char* trans_b, size_t m, size_t n, size_t k, double alpha,
		const double* A, size_t lda, const double* B, size_t ldb, double beta, double* C, size_t ldc)
{
	// TODO support transform

#ifdef USE_BLAS
	cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
#else
	for (size_t mm = 0; mm < m; ++mm) {
		for (size_t nn = 0; nn < n; ++nn) {
			C[mm*ldc+nn] *= beta;
			for (size_t kk = 0; kk < k; ++kk) {
				C[mm*ldc+nn] += alpha * A[mm*lda+kk] * B[kk*ldb+nn];
			}
		}
	}
#endif
}

static size_t min(size_t a, size_t b) {
	return (a < b) ? a : b;
}

static size_t sizeInTiles(size_t n) {
	return (n + TILE_SIZE - 1) / TILE_SIZE;
}

max_file_t* maxfile;
max_engine_t* engine;
max_actions_t* actionsIn;
max_actions_t* actionsOut0;
max_actions_t* actionsOut1;
const max_handle_t* aHandle;
const max_handle_t* bHandle;
const max_handle_t* cHandle;
double* aIn;
double* bIn;
double* cOut0;
double* cOut1;

void dgemm_init(size_t mMax, size_t nMax, size_t kMax) {
	printf("Initializing maxfile...\n");
	maxfile = DGEMM_init();
	aHandle = max_get_handle_stream(maxfile, "A");
	bHandle = max_get_handle_stream(maxfile, "B");
	cHandle = max_get_handle_stream(maxfile, "C");

	printf("Loading engine...\n");
	engine = max_load(maxfile, "*");

	actionsIn   = max_actions_init(maxfile, NULL);
	actionsOut0 = max_actions_init_explicit(maxfile);
	actionsOut1 = max_actions_init_explicit(maxfile);

	size_t mTiles = sizeInTiles(mMax);
	size_t nTiles = sizeInTiles(nMax);
	size_t kTiles = sizeInTiles(kMax);
	aIn   = malloc(mTiles * kTiles * TILE_SIZE_2D * sizeof(double));
	bIn   = malloc(kTiles * nTiles * TILE_SIZE_2D * sizeof(double));
	cOut0 = malloc(kTiles * TILE_SIZE_2D * sizeof(double));
	cOut1 = malloc(kTiles * TILE_SIZE_2D * sizeof(double));
}

void dgemm(
		const char* trans_a, const char* trans_b, size_t m, size_t n, size_t k, double alpha,
		const double* A, size_t lda, const double* B, size_t ldb, double beta, double* C, size_t ldc)
{
	// TODO support transform

	size_t mTiles = sizeInTiles(m);
	size_t nTiles = sizeInTiles(n);
	size_t kTiles = sizeInTiles(k);

	// re-order A and B matrices into tiles and pad them up to size
	size_t pos = 0;
	for (size_t mm = 0; mm < mTiles; ++mm) {
		for (size_t kk = 0; kk < kTiles; ++kk) {
			for (size_t x = 0; x < TILE_SIZE; ++x) {
				size_t row = mm*TILE_SIZE + x;

				if (row < m) {
					for (size_t y = 0; y < TILE_SIZE; ++y) {
						size_t col = kk*TILE_SIZE + y;
						aIn[pos++] = (col < k) ? A[row*lda+col] : 0;
					}
				} else {
					for (size_t y = 0; y < TILE_SIZE; ++y) {
						aIn[pos++] = 0;
					}
				}
			}
		}
	}

	pos = 0;
	for (size_t nn = 0; nn < nTiles; ++nn) {
		for (size_t kk = 0; kk < kTiles; ++kk) {
			for (size_t x = 0; x < TILE_SIZE; ++x) {
				size_t row = kk*TILE_SIZE + x;

				if (row < k) {
					for (size_t y = 0; y < TILE_SIZE; ++y) {
						size_t col = nn*TILE_SIZE + y;
						bIn[pos++] = (col < n) ? B[row*ldb+col] : 0;
					}
				} else {
					for (size_t y = 0; y < TILE_SIZE; ++y) {
						bIn[pos++] = 0;
					}
				}
			}
		}
	}

	max_clear_queues(actionsIn);
	max_set_ticks(actionsIn, "TM", mTiles * nTiles * kTiles * TILE_SIZE_2D);

	for (size_t mm = 0; mm < mTiles; ++mm) {
		for (size_t nn = 0; nn < nTiles; ++nn) {
			max_queue_input_handle(actionsIn, aHandle, &aIn[mm*kTiles*TILE_SIZE_2D], kTiles * TILE_SIZE_2D * sizeof(double));
		}
	}

	for (size_t mm = 0; mm < mTiles; ++mm) {
		max_queue_input_handle(actionsIn, bHandle, bIn, kTiles * nTiles * TILE_SIZE_2D * sizeof(double));
	}

	max_disable_stream_sync(actionsIn, "A");
	max_disable_stream_sync(actionsIn, "B");

	max_run(engine, actionsIn);

	max_clear_queues(actionsOut0);
	max_queue_output_handle(actionsOut0, cHandle, cOut0, kTiles * TILE_SIZE_2D * sizeof(double));
	max_sync_stream(actionsOut0, "C");

	max_clear_queues(actionsOut1);
	max_queue_output_handle(actionsOut1, cHandle, cOut1, kTiles * TILE_SIZE_2D * sizeof(double));
	max_sync_stream(actionsOut1, "C");

	max_run_t* run0 = max_run_nonblock(engine, actionsOut0);
	max_run_t* run1 = NULL;
	if (mTiles * nTiles > 1) run1 = max_run_nonblock(engine, actionsOut1);

	size_t tile = 2;
	for (size_t mm = 0; mm < mTiles; ++mm) {
		size_t xMax = min(TILE_SIZE, m - mm*TILE_SIZE);

		for (size_t nn = 0; nn < nTiles; ++nn, ++tile) {
			size_t yMax = min(TILE_SIZE, n - nn*TILE_SIZE);

			for (size_t x = 0; x < xMax; ++x) {
				size_t row = mm*TILE_SIZE + x;

				for (size_t y = 0; y < yMax; ++y) {
					size_t col = nn*TILE_SIZE + y;
					C[row*ldc+col] *= beta;
				}
			}

			max_wait((tile & 1) ? run1 : run0);
			const double* cOut = (tile & 1) ? cOut1 : cOut0;

			for (size_t kk = 0; kk < kTiles; ++kk) {
				for (size_t x = 0; x < xMax; ++x) {
					size_t row = mm*TILE_SIZE + x;

					for (size_t y = 0; y < yMax; ++y) {
						size_t col = nn*TILE_SIZE + y;
						C[row*ldc+col] += alpha * cOut[TILE_SIZE_2D*kk + TILE_SIZE*x + y];
					}
				}
			}

			if (tile < mTiles*nTiles) {
				if (tile & 1) {
					run1 = max_run_nonblock(engine, actionsOut1);
				} else {
					run0 = max_run_nonblock(engine, actionsOut0);
				}
			}
		}
	}
}

static struct timespec diff(struct timespec start, struct timespec finish) {
	struct timespec diff;
	if ((finish.tv_nsec < start.tv_nsec)) {
		diff.tv_sec  = finish.tv_sec  - start.tv_sec  - 1;
		diff.tv_nsec = finish.tv_nsec - start.tv_nsec + 1000000000;
	} else {
		diff.tv_sec  = finish.tv_sec  - start.tv_sec;
		diff.tv_nsec = finish.tv_nsec - start.tv_nsec;
	}

	return diff;
}

static int check_result(int m, int n, const double* expected, const double* actual) {
	for (int i = 0; i < m; ++i) {
		for (int j = 0; j < n; ++j) {
			int idx = i*n + j;
			if (expected[idx] != actual[idx]) {
				return 1;
			}
		}
	}
	return 0;
}

int main(int argc, char** argv) {
	unsigned seed = time(NULL);
	printf("Random seed: %u\n", seed);
	srand(seed);

	int m;
	int n;
	int k;

	if (argc == 1) {
		m = (random() % (5 * TILE_SIZE)) + TILE_SIZE;
		n = (random() % (5 * TILE_SIZE)) + TILE_SIZE;
		k = (random() % (5 * TILE_SIZE)) + TILE_SIZE;
	} else if (argc == 2) {
		m = n = k = atoi(argv[1]);
	} else {
		printf("Usage: %s [size]\n", argv[0]);
		exit(1);
	}

	double* A   = calloc(m * k, sizeof(double));
	double* B   = calloc(k * n, sizeof(double));
	double* Csw = calloc(m * n, sizeof(double));
	double* Chw = calloc(m * n, sizeof(double));

	printf("Matrix dimensions: m = %d, n = %d, k = %d\n", m, n, k);

	dgemm_init(m, n, k);

	size_t mTiles = sizeInTiles(m);
	size_t nTiles = sizeInTiles(n);
	size_t kTiles = sizeInTiles(k);

	printf("DFE tile size: %d\n", TILE_SIZE);
	printf("DFE compute dimensions: m = %zu, n = %zu, k = %zu\n",
			mTiles * TILE_SIZE, nTiles * TILE_SIZE, kTiles * TILE_SIZE);

	double cpuPoints = ((double) m) * n * k;
	double dfePoints = ((double) mTiles) * nTiles * kTiles * TILE_SIZE_2D;
	printf("DFE compute efficiency: %f\n", cpuPoints / (dfePoints * TILE_SIZE));
	printf("DFE frequency: %d MHz\n", FREQUENCY);
	// add one tile to account for stream offset TODO measure pipeline depth properly?
	printf("DFE predicted compute time: %f s\n", (dfePoints + TILE_SIZE_2D) / (((double) FREQUENCY) * 1e6));

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

	struct timespec start;
	struct timespec finish;

	for (int xx = 0; xx < 5; ++xx) {
		printf("Repeat %d\n", xx);
		printf("Running HW... "); fflush(stdout);
		clock_gettime(CLOCK_REALTIME, &start);
		dgemm("n", "n", m, n, k, alpha, A, k, B, n, beta, Chw, n);
		clock_gettime(CLOCK_REALTIME, &finish);

		struct timespec hw_time  = diff(start, finish);

		printf("took: %ld.%09ld s\n", hw_time.tv_sec, hw_time.tv_nsec);
	}

	printf("Running SW... "); fflush(stdout);
	clock_gettime(CLOCK_REALTIME, &start);
	dgemm_model("n", "n", m, n, k, alpha, A, k, B, n, beta, Csw, n);
	clock_gettime(CLOCK_REALTIME, &finish);

	struct timespec sw_time = diff(start, finish);

	printf("took: %ld.%09ld s\n", sw_time.tv_sec, sw_time.tv_nsec);

	printf("Comparing results... ");
	int failed = check_result(m, n, Csw, Chw);
	printf("%s\n", failed ? "WRONG" : "CORRECT");
	printf("Done.\n");
	return failed;
}
