/*
 * maxhash_internal.h
 *
 *  Created on: 29 May 2013
 *      Author: tperry
 */

#ifndef MAXHASH_INTERNAL_H_
#define MAXHASH_INTERNAL_H_

#include "maxhash.h"

#define NUM_ENTRY_FLAGS     2
#define FLAG_VALID          0
#define FLAG_PERFECT_DIRECT 1

#define UNRELEASED_VERSION_STRING "0"

//#define PRINT_VAR(type, var) if (global_debug) printf("%-25s %-15s %" #type "\n", __func__, #var ":", var)
#define PRINT_VAR(type, var)

#ifdef __cplusplus
extern "C" {
#endif

struct maxhash_engine_state;

enum maxhash_mem_type {
	MAXHASH_MEM_TYPE_UNDEFINED,
	MAXHASH_MEM_TYPE_FMEM,
	MAXHASH_MEM_TYPE_DEEP_FMEM,
	MAXHASH_MEM_TYPE_LMEM
};

struct maxhash_internal_table_params {
	char name[NAME_BUF_LEN];
	size_t width_bits;
	size_t width_bytes;
	size_t num_buckets;
	enum maxhash_mem_type mem_type;
	uint8_t deep_fmem_id;
	size_t base_address_bursts;
	bool validate_results;
};

struct maxhash_internal_table {
	struct maxhash_internal_table_params iparams;
	const struct maxhash_table *table;
	struct maxhash_bucket *buckets;
	size_t num_entries;
};

struct maxhash_table_params {
	char kernel_name[NAME_BUF_LEN];
	char hash_table_name[NAME_BUF_LEN];
	size_t max_bucket_entries;
	size_t jenkins_chunk_width_bytes;
	bool perfect;
	bool is_double_buffered;
	struct maxhash_engine_state *engine_state;
	size_t key_width_bits;
	size_t key_width_bytes;
	void (*mem_access_fn)(void *arg, bool is_read, size_t base_address_bursts,
			void *data, size_t data_size_bursts);
	void *mem_access_fn_arg;
	struct maxhash_internal_table_params values;
	struct maxhash_internal_table_params intermediate;
	bool debug;
};

struct maxhash_entry {
	void *key;
	void *value;
	bool flags[NUM_ENTRY_FLAGS];
	struct maxhash_entry *next;
};

struct maxhash_bucket {
	size_t num_keys;
	struct maxhash_entry *entry_list;
};

struct maxhash_table {
	struct maxhash_table_params tparams;
	struct maxhash_internal_table sw;
	struct maxhash_internal_table recent;
	struct maxhash_internal_table intermediate;
	struct maxhash_internal_table values;
	bool load_buffer_select;
};

struct maxhash_entry_iterator {
	const struct maxhash_internal_table *itable;
	size_t bucket_id;
	struct maxhash_entry *entry;
	struct maxhash_entry *next;
};

typedef struct maxhash_internal_table        maxhash_internal_table_t;
typedef struct maxhash_internal_table_params maxhash_internal_table_params_t;
typedef struct maxhash_entry                 maxhash_entry_t;
typedef struct maxhash_bucket                maxhash_bucket_t;
typedef enum   maxhash_mem_type              maxhash_mem_type_t;



/**
 * Create a minimal perfect hash table.
 */
maxhash_err_t maxhash_create_mph(maxhash_table_t *source);

/**
 * Apply Jenkins' 32-bit "one-at-a-time" hash function.
 */
uint32_t maxhash_function_jenkins(const void *data, size_t data_len, uint32_t
		hash, size_t chunk_width);

bool has_constant_uint64t(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name);
bool has_constant_string(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name);

int get_maxfile_constant(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name);

const char *get_maxfile_string_constant(maxhash_engine_state_t *es,
		const char *hash_table_name, const char *constant_name);

int get_maxfile_global_constant(maxhash_engine_state_t *es,
		const char *constant_name);

const char *get_maxfile_global_string_constant(maxhash_engine_state_t *es,
		const char *constant_name);

void maxhash_write_fmem(maxhash_engine_state_t *es, const char *kernel_name,
		const char *mem_name, size_t base_entry, void *data_buf, size_t
		data_size_bytes);

void maxhash_write_deep_fmem(maxhash_engine_state_t *es, const char
		*kernel_name, const char *mem_name, void *data, size_t
		data_size_bytes);

void maxhash_init_deep_fmem_fanout(maxhash_engine_state_t *es, uint8_t
		deep_fmem_id);

#ifdef __cplusplus
}
#endif

#endif /* MAXHASH_INTERNAL_H_ */
