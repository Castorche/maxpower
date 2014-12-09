/*
 * maxhash.h
 *
 *  Created on: 15 May 2013
 *      Author: tperry
 */

#ifndef MAXHASH_H_
#define MAXHASH_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <MaxSLiCInterface.h>

#define NAME_BUF_LEN 64

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {MAXHASH_ERR_OK = 0, MAXHASH_ERR_ERR} maxhash_err_t;

typedef struct maxhash_table           maxhash_table_t;
typedef struct maxhash_engine_state    maxhash_engine_state_t;
typedef struct maxhash_table_params    maxhash_table_params_t;
typedef struct maxhash_entry_iterator  maxhash_entry_iterator_t;

struct maxhash_engine_state {
	max_file_t   *maxfile;
	max_engine_t *engine;
	size_t lmem_burst_size_bytes;
};

maxhash_err_t maxhash_table_params_init(
		maxhash_table_params_t **params);

maxhash_err_t maxhash_table_params_free(
		maxhash_table_params_t *params);

maxhash_err_t maxhash_table_params_set_size(
		maxhash_table_params_t *params, size_t size);

maxhash_err_t maxhash_table_params_set_key_width_bits(
		maxhash_table_params_t *params, size_t key_width_bits);

maxhash_err_t maxhash_table_params_set_value_width_bits(
		maxhash_table_params_t *params, size_t value_width_bits);

/**
 * Initialise a software-only hash table.
 *
 * Hash tables initialised using this function must be freed using the
 * "maxhash_free()" function in order to avoid memory leaks.
 *
 * Usage:
 * maxhash_table_t *table;
 * maxhash_table_params params;
 * params.foo = foo;
 * params.bar = bar;
 * ...
 * maxhash_sw_table_init(&table, &params);
 * ...
 * maxhash_free(table);
 */
maxhash_err_t maxhash_sw_table_init(maxhash_table_t **table,
		const maxhash_table_params_t *params);

/**
 * Initialise a hardware-backed hash table.
 *
 * Hash tables initialised using this function must be freed using the
 * "maxhash_free()" function in order to avoid memory leaks.
 *
 * Usage:
 * maxhash_table_t *table;
 * maxhash_init_hw(&table, ...);
 * ...
 * maxhash_free(table);
 */
maxhash_err_t maxhash_hw_table_init(maxhash_table_t **table,
		const char *kernel_name, const char *hash_table_name,
		maxhash_engine_state_t *engine_state);

/**
 * Free a hash table.
 */
maxhash_err_t maxhash_free(maxhash_table_t *table);

/*
 * Enable or disable additional debug prints.
 */
maxhash_err_t maxhash_set_debug_mode(maxhash_table_t *table, bool debug);

/**
 * Set callback function for memory accesses.
 */
maxhash_err_t maxhash_set_memory_access_fn(maxhash_table_t *table,
		void (*mem_access_fn)(void *arg, bool is_read, size_t
			base_address_bursts, void *data, size_t data_size_bursts),
		void *mem_access_fn_arg);

/**
 * Put a key-value pair in a hash table.
 * Changes are not committed to hardware.
 */
maxhash_err_t maxhash_put(maxhash_table_t *table, const void *key,
		size_t key_len, const void *value, size_t value_len);

/**
 * Get the value corresponding to the specified key from a hash table in
 * software (not the version that has been committed to hardware).
 *
 * If the key is present in the hash table, the function returns
 * MAXHASH_ERR_OK, otherwise it returns MAXHASH_ERR_ERR.
 */
maxhash_err_t maxhash_get(const maxhash_table_t *table, const void *key,
		size_t key_len, void *value);

/**
 * Set present to true if the hash table contains the specified key, otherwise
 * set it to false.
 */
maxhash_err_t maxhash_contains(const maxhash_table_t *table, bool *present,
		const void *key, size_t key_len);

/**
 * Write all of the keys contained in the hash table to the memory region
 * pointed to by the "keys" parameter.  This region should be large enough to
 * store all of the keys, which can be determined by calling maxhash_size().
 */
maxhash_err_t maxhash_get_keys(const maxhash_table_t *table, void *keys);



/**
 * Initialise an iterator structure which can be used to iterate over the
 * entries in a MaxHash table.
 *
 * Iterators initialised using this function must be freed using the
 * "maxhash_entry_iterator_free()" function in order to avoid memory leaks.
 */
maxhash_err_t maxhash_entry_iterator_init(maxhash_entry_iterator_t **iterator,
		const maxhash_table_t *table);

/**
 * Free an iterator structure.
 */
maxhash_err_t maxhash_entry_iterator_free(maxhash_entry_iterator_t *iterator);

/*
 * If there is a next entry in the hash table, this function advances the
 * iterator to that entry.
 *
 * No guarantees can be made regarding the order in which the entries are
 * visited.
 */
maxhash_err_t maxhash_entry_iterator_next(maxhash_entry_iterator_t *iterator);

/*
 * If there is a next entry in the hash table, this function sets "has_next" to
 * true.  Otherwise, it sets it to false.
 *
 * No guarantees can be made regarding the order in which the entries are
 * visited.
 */
maxhash_err_t maxhash_entry_iterator_has_next(maxhash_entry_iterator_t *iterator,
		bool *has_next);

/*
 * For the entry pointed to by the iterator, copy the address of the key in
 * that entry to the "key" parameter.
 */
maxhash_err_t maxhash_entry_iterator_get_key(maxhash_entry_iterator_t *iterator,
		const void **key);

/*
 * For the entry pointed to by the iterator, copy the address of the value in
 * that entry to the "value" parameter.
 */
maxhash_err_t maxhash_entry_iterator_get_value(maxhash_entry_iterator_t *iterator,
		const void **value);

/**
 * Remove a value from a hash table.
 * Changes are not committed to hardware.
 */
maxhash_err_t maxhash_remove(maxhash_table_t *table, const void *key,
		size_t key_len);

/**
 * Clear all values from a hash table.
 * Changes are not committed to hardware.
 */
maxhash_err_t maxhash_clear(maxhash_table_t *table);

/**
 * Add all entries in source hash to destination hash.
 * Changes are not committed to hardware.
 */
maxhash_err_t maxhash_putall(maxhash_table_t *destination, const
		maxhash_table_t *source);

/**
 * Print the contents of a hash table.
 */
maxhash_err_t maxhash_print(const maxhash_table_t *table);

/**
 * Get the number of entries contained within a hash table.
 */
maxhash_err_t maxhash_size(const maxhash_table_t *table, size_t *size);

/**
 * Get the key width of the hash table, in bytes.
 */
maxhash_err_t maxhash_get_key_width(const maxhash_table_t *table, size_t
		*key_width);

/**
 * Get the value width of the hash table, in bytes.
 */
maxhash_err_t maxhash_get_value_width(const maxhash_table_t *table, size_t
		*value_width);

/**
 * Commit changes to hardware.  (Calls maxhash_perfect_create internally.)
 */
maxhash_err_t maxhash_commit(maxhash_table_t *table);

/**
 * Create a perfect hash table from a non-perfect hash table, without
 * committing to hardware.
 */
maxhash_err_t maxhash_perfect_create(maxhash_table_t *table);

/**
 * Get the capacity of a perfect hash table.  Note that this metric isn't
 * useful in non-perfect hash tables, which become full only when the maximum
 * number of entries in any of the buckets is reached.
 */
maxhash_err_t maxhash_perfect_get_capacity(const maxhash_table_t *table,
		size_t *capacity);

/**
 * Get a value from a perfect hash table.
 */
maxhash_err_t maxhash_perfect_get(maxhash_table_t *table, const void *key,
		size_t key_len, void *value, bool *valid);

/**
 * Get the index that has been assigned to a key in a perfect hash table.
 */
maxhash_err_t maxhash_perfect_get_index(maxhash_table_t *table,
		const void *key, size_t key_len, size_t *index);

#ifdef __cplusplus
}
#endif

#endif /* MAXHASH_H_ */
