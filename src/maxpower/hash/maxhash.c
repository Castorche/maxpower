/*
 * maxhash.c
 *
 *  Created on: 23 May 2013
 *      Author: tperry
 */

#include "maxhash.h"
#include "maxhash_internal.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <limits.h>

#define DEEP_FMEM_ID_BITS 4

#define PAD_KEY(tparams, item, length) \
	uint8_t key_padded[(tparams)->key_width_bytes]; \
	if (pad(&(item), key_padded, (length), (tparams)->key_width_bits, (tparams)->key_width_bytes, false) != MAXHASH_ERR_OK) \
		return MAXHASH_ERR_ERR;

#define PAD_VAL(tparams, item, length) \
	uint8_t value_padded[(tparams)->width_bytes]; \
	if (pad(&(item), value_padded, (length), (tparams)->width_bits, (tparams)->width_bytes, true) != MAXHASH_ERR_OK) \
		return MAXHASH_ERR_ERR;



maxhash_err_t pad(const void **item, void *padded_item, size_t item_size_bytes,
		size_t max_size_bits, size_t max_size_bytes, bool is_value)
{
	/* Check that any bits beyond max_size_bits are zero. */
	size_t start = max_size_bits / 8;
	size_t mod = max_size_bits % 8;
	for (size_t i = start; i < item_size_bytes; i++)
	{
		uint8_t val = ((const uint8_t *)*item)[i];
		bool bad = false;
		if (i == start && mod != 0)
		{
			uint8_t mask = ((1 << mod) - 1) << mod;
			if (val & mask)
				bad = true;
		}
		else if (val)
			bad = true;
		if (bad)
		{
			const char *type = is_value ? "value" : "key";
			fprintf(stderr, "Error: %s has bits set beyond the maximum %s "
					"size of the hash table (%zu bits): byte %zu has the "
					"value 0x%x.\n", type, type, max_size_bits, i, val);
			return MAXHASH_ERR_ERR;
		}
	}
	size_t copy_size = item_size_bytes < max_size_bytes ? item_size_bytes :
		max_size_bytes;
	memset(padded_item, 0, max_size_bytes);
	memcpy(padded_item, *item, copy_size);
	*item = padded_item;
	return MAXHASH_ERR_OK;
}



static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}



#define TIME_START(n) \
	uint64_t time_diff_ ## n = rdtsc(); \
	static uint64_t total_time_ ## n;
#define TIME_END(n) \
	time_diff_ ## n = rdtsc() - time_diff_ ## n; \
	total_time_ ## n += time_diff_ ## n; \
	printf("Time (%s #%d): %ld cycles, total: %ld\n", __func__, n, \
			time_diff_ ## n, total_time_ ## n);

maxhash_err_t maxhash_set_debug_mode(maxhash_table_t *table, bool debug)
{
	table->tparams.debug = debug;
	return MAXHASH_ERR_OK;
}



void maxhash_debug_print(const maxhash_table_t *table, const char *fmt, ...)
{
	if (!table->tparams.debug)
		return;

	va_list ap;
	va_start(ap, fmt);

	char message[32 * 1024];
	int message_len = vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);

	if (message[message_len - 1] == '\n')
		message[message_len - 1] = '\0';

    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    struct tm utc_time;
    gmtime_r(&current_time.tv_sec, &utc_time);

    char time_string[64];
    snprintf(time_string, sizeof(time_string), "[%04d-%02d-%02d %02d:%02d:%02d.%06ld] ",
            1900 + utc_time.tm_year, 1 + utc_time.tm_mon,
            utc_time.tm_mday, utc_time.tm_hour, utc_time.tm_min,
            utc_time.tm_sec, current_time.tv_usec);

	FILE *log_file = stdout;
    fprintf(log_file, "%s%s\n", time_string, message);
    fflush(log_file);
}



uint32_t maxhash_function_jenkins(const void *data, size_t data_len, uint32_t
		hashparam, size_t chunk_width)
{
	bool debug = false;
	uint32_t hash = hashparam;

	if (debug) printf("key: %*s\n", (int)data_len, (char *)data);
	if (debug) printf("param: %x\n", hashparam);
	if (debug) printf("data_len: %lx\n", data_len);

	for (size_t chunk = 0; chunk < data_len; chunk += chunk_width)
	{
		for (size_t octet = 0; octet < chunk_width; octet++)
			hash += chunk + octet < data_len ? ((uint8_t *)data)[chunk + octet]
				<< (octet * 8) : 0;
		hash += hash << 10;
		hash ^= hash >> 6;
		if (debug) printf("hash: %x\n", hash);
	}

	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;

	if (debug) printf("final hash: %x\n", hash);

	return hash;
}



int compare_bucket_num_keys(const void *first, const void *second)
{
	maxhash_bucket_t *f = (maxhash_bucket_t *)first;
	maxhash_bucket_t *s = (maxhash_bucket_t *)second;
	if (f->num_keys > s->num_keys) return -1;
	if (f->num_keys < s->num_keys) return  1;
	return 0;
}



void hash_print_entry(const maxhash_internal_table_t *itable, const
		maxhash_entry_t *entry)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;
	printf("[key: ");
	for (size_t i = 0; i < tparams->key_width_bytes; i++)
		printf("%02x", ((uint8_t *)entry->key)[i]);
	printf(" (");
	for (size_t i = 0; i < tparams->key_width_bytes; i++)
	{
		char c = ((char *)entry->key)[i];
		if (c < 0x20 || c >= 0x7F) c = '.';
		printf("%c", c);
	}
	printf("), value: ");
	for (size_t i = 0; i < itable->iparams.width_bytes; i++)
		printf("%02x", ((uint8_t *)entry->value)[i]);
	printf(" (");
	for (size_t i = 0; i < itable->iparams.width_bytes; i++)
	{
		char c = ((char *)entry->value)[i];
		if (c < 0x20 || c >= 0x7F) c = '.';
		printf("%c", c);
	}
	printf("), flags: ");
	for (size_t flag = 0; flag < NUM_ENTRY_FLAGS; flag++)
		printf("%d", entry->flags[NUM_ENTRY_FLAGS - flag - 1]);
	if (entry->flags[FLAG_VALID] ||
			entry->flags[FLAG_PERFECT_DIRECT])
	{
		printf(" (");
		if (entry->flags[FLAG_VALID])
			printf("VALID");
		if (entry->flags[FLAG_VALID] &&
				entry->flags[FLAG_PERFECT_DIRECT])
			printf(", ");
		if (entry->flags[FLAG_PERFECT_DIRECT])
			printf("PERFECT_DIRECT");
		printf(")");
	}
	printf("]\n");
}



void hash_print(const maxhash_internal_table_t *itable, bool sparse)
{
	maxhash_debug_print(itable->table, "Printing hash table...\n");
	printf("%s:\n", itable->iparams.name);
	printf("Number of buckets: %zu\n", itable->iparams.num_buckets);
	for (size_t entry_id = 0; entry_id < itable->iparams.num_buckets;
			entry_id++)
	{
		maxhash_entry_t *entry = itable->buckets[entry_id].entry_list;
		if (!sparse || entry)
		{
			printf("entries in bucket 0x%04lx: ", entry_id);
			if (!entry)
				printf("\n");
			while (entry)
			{
				hash_print_entry(itable, entry);
				if (entry->next)
					printf("                          ");
				entry = entry->next;
			}
		}
	}
	printf("\n");
}



maxhash_err_t maxhash_print(const maxhash_table_t *table)
{
	hash_print(&table->sw, false);
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_print_full(const maxhash_internal_table_t *itable)
{
	hash_print(itable, false);
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_print_sparse(const maxhash_internal_table_t *itable)
{
	hash_print(itable, true);
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_size(const maxhash_table_t *table, size_t *size)
{
	*size = table->sw.num_entries;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_table_init(
		maxhash_internal_table_t *itable,
		const maxhash_table_t *table,
		const maxhash_internal_table_params_t *itable_params,
		const char *itable_name)
{
	memcpy(&itable->iparams, itable_params,
			sizeof(maxhash_internal_table_params_t));

	itable->iparams.width_bytes = (itable->iparams.width_bits + 7) / 8;
	itable->table = table;
	itable->buckets = calloc(itable_params->num_buckets,
			sizeof(maxhash_bucket_t));

	if (itable->buckets == NULL)
	{
		fprintf(stderr, "Error: failed to allocate hash table memory.\n");
		return MAXHASH_ERR_ERR;
	}

	if (strlen(table->tparams.hash_table_name) == 0)
	{
		strncpy(itable->iparams.name, itable_name,
				sizeof(itable->iparams.name));
	}
	else
	{
		snprintf(itable->iparams.name,
				sizeof(itable->iparams.name),
				"%s_%s", table->tparams.hash_table_name, itable_name);
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_sw_init(maxhash_table_t **table,
		const maxhash_table_params_t *tparams)
{
	printf("MaxHash software version: %s\n", SVN_REVISION);

	maxhash_err_t err = MAXHASH_ERR_OK;

	*table = calloc(1, sizeof(maxhash_table_t));
	if (*table == NULL)
		err = MAXHASH_ERR_ERR;

	maxhash_table_t *table_p = *table;

	memcpy(&table_p->tparams, tparams, sizeof(maxhash_table_params_t));

	table_p->tparams.key_width_bytes = (table_p->tparams.key_width_bits + 7) / 8;
	table_p->load_buffer_select = 1;

	const maxhash_internal_table_params_t *intermediate_params =
		&tparams->intermediate;
	const maxhash_internal_table_params_t *values_params =
		&tparams->values;

	maxhash_internal_table_params_t sw_params = {{0}};
	sw_params.width_bits = values_params->width_bits;
	sw_params.width_bytes = values_params->width_bytes;
	sw_params.num_buckets = intermediate_params->num_buckets;

	err |= maxhash_internal_table_init(&table_p->sw,
			table_p, &sw_params, "Software");
	err |= maxhash_internal_table_init(&table_p->recent,
			table_p, &sw_params, "Recent");
	err |= maxhash_internal_table_init(&table_p->intermediate,
			table_p, intermediate_params, "HashParams");
	err |= maxhash_internal_table_init(&table_p->values,
			table_p, values_params, "Values");

	if (err != MAXHASH_ERR_OK)
		fprintf(stderr, "Error: failed to initialise hash table.\n");

	return err;
}



maxhash_err_t maxhash_table_params_init(
		maxhash_table_params_t **tparams)
{
	*tparams = calloc(1, sizeof(maxhash_table_params_t));
	if (*tparams == NULL)
		return MAXHASH_ERR_ERR;
	return MAXHASH_ERR_OK;
}

maxhash_err_t maxhash_table_params_free(
		maxhash_table_params_t *tparams)
{
	free(tparams);
	return MAXHASH_ERR_OK;
}

maxhash_err_t maxhash_table_params_set_size(
		maxhash_table_params_t *tparams, size_t size)
{
	tparams->values.num_buckets = size;
	return MAXHASH_ERR_OK;
}

maxhash_err_t maxhash_table_params_set_key_width_bits(
		maxhash_table_params_t *tparams, size_t key_width_bits)
{
	tparams->key_width_bits = key_width_bits;
	return MAXHASH_ERR_OK;
}

maxhash_err_t maxhash_table_params_set_value_width_bits(
		maxhash_table_params_t *tparams, size_t value_width_bits)
{
	tparams->values.width_bits = value_width_bits;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_sw_table_init(maxhash_table_t **table,
		const maxhash_table_params_t *tparams)
{
	if (tparams->values.num_buckets == 0)
	{
		fprintf(stderr, "Error: hash table size cannot be zero.\n");
		return MAXHASH_ERR_ERR;
	}

	if (tparams->key_width_bits == 0)
	{
		fprintf(stderr, "Error: key width cannot be zero.\n");
		return MAXHASH_ERR_ERR;
	}

	if (tparams->values.width_bits == 0)
	{
		fprintf(stderr, "Error: value width cannot be zero.\n");
		return MAXHASH_ERR_ERR;
	}

	maxhash_table_params_t params_copy;
	memset(&params_copy, 0, sizeof(params_copy));
	strncpy(params_copy.hash_table_name, tparams->hash_table_name,
			sizeof(params_copy.hash_table_name));
	params_copy.max_bucket_entries = 1; // FIXME
	params_copy.jenkins_chunk_width_bytes = 4; // FIXME
	params_copy.perfect = true; // FIXME
	params_copy.key_width_bits = tparams->key_width_bits;

	params_copy.intermediate.width_bits = 32; // FIXME
	if (tparams->intermediate.num_buckets == 0)
		params_copy.intermediate.num_buckets = tparams->values.num_buckets;
	else
		params_copy.intermediate.num_buckets = tparams->intermediate.num_buckets;

	params_copy.values.width_bits = tparams->values.width_bits;
	params_copy.values.num_buckets = tparams->values.num_buckets;

	maxhash_err_t err = maxhash_internal_sw_init(table, &params_copy);

	return err;
}



maxhash_err_t maxhash_check_version(maxhash_engine_state_t *es)
{
	bool has_version = has_constant_string(es, "", "MaxHash_Version");
	if (!has_version)
		fprintf(stderr, "*** Warning: MaxHash version is not defined in "
				"the MaxFile. ***\n");

	const char *maxhash_version_maxfile = get_maxfile_string_constant(es, "",
			"MaxHash_Version");
	if (strncmp(maxhash_version_maxfile, SVN_REVISION, strlen(SVN_REVISION)))
	{
		if (!strncmp(maxhash_version_maxfile, UNRELEASED_VERSION_STRING,
					strlen(UNRELEASED_VERSION_STRING)))
		{
			fprintf(stderr, "\n*** Warning: MaxFile uses an unreleased version "
					"of MaxHash. ***\n\n");
		}
		else
		{
			fprintf(stderr, "\n*** Warning: MaxHash version in MaxFile (%s) "
					"does not match the version in software (%s). ***\n\n",
					maxhash_version_maxfile, SVN_REVISION);
		}
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_check_presence(maxhash_engine_state_t *es,
		const char *kernel_name, const char *hash_table_name)
{
	char full_name[NAME_BUF_LEN] = {0};
	snprintf(full_name, sizeof(full_name), "%s_%s", kernel_name,
			hash_table_name);

	bool hash_present = has_constant_uint64t(es, full_name, "_IsPresent");
	if (!hash_present)
	{
		fprintf(stderr, "Error: there is no MaxHash instance named '%s' in "
				"the kernel named '%s'.\n", hash_table_name, kernel_name);
		return MAXHASH_ERR_ERR;
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_get_hw_mem_backing_params(
		maxhash_internal_table_params_t *mem_backing_params,
		maxhash_engine_state_t *es, const char *full_hash_name, const char
		*mem_name)
{
	char buf[1024];
	snprintf(buf, sizeof(buf), "_%s_MemType", mem_name);
	const char *mem_type = get_maxfile_string_constant(es, full_hash_name, buf);

	if (!strcmp(mem_type, "FMem"))
		mem_backing_params->mem_type = MAXHASH_MEM_TYPE_FMEM;
	else if (!strcmp(mem_type, "DeepFMem"))
	{
		mem_backing_params->mem_type = MAXHASH_MEM_TYPE_DEEP_FMEM;
		snprintf(buf, sizeof(buf), "_%s_DeepFMemID", mem_name);
		int deep_fmem_id = get_maxfile_constant(es, full_hash_name, buf);
		if (deep_fmem_id < 0 || deep_fmem_id > 15)
		{
			fprintf(stderr, "Error: deep FMem ID (%d) is invalid.\n",
					deep_fmem_id);
			return MAXHASH_ERR_ERR;
		}
		maxhash_init_deep_fmem_fanout(es, deep_fmem_id);
		mem_backing_params->deep_fmem_id = deep_fmem_id;
	}
	else if (!strcmp(mem_type, "LMem"))
	{
		mem_backing_params->mem_type = MAXHASH_MEM_TYPE_LMEM;
		snprintf(buf, sizeof(buf), "_%s_BaseAddressBursts", mem_name);
		mem_backing_params->base_address_bursts = get_maxfile_constant(es,
				full_hash_name, buf);
	}
	else
	{
		fprintf(stderr, "Error: hash table memory type is invalid.\n");
		return MAXHASH_ERR_ERR;
	}

	snprintf(buf, sizeof(buf), "_%s_Width", mem_name);
	int width_bits = get_maxfile_constant(es, full_hash_name, buf);

	if (width_bits <= 0)
	{
		fprintf(stderr, "Error: table entry width in hardware hash table "
				"(%d) is invalid.\n", width_bits);
		return MAXHASH_ERR_ERR;
	}

	mem_backing_params->width_bits  = width_bits;
	mem_backing_params->width_bytes = (width_bits + 7) / 8;

	snprintf(buf, sizeof(buf), "_%s_NumBuckets", mem_name);
	int num_buckets = get_maxfile_constant(es, full_hash_name, buf);

	if (num_buckets <= 0)
	{
		fprintf(stderr, "Error: number of buckets in hardware hash table "
				"(%d) is invalid.\n", num_buckets);
		return MAXHASH_ERR_ERR;
	}

	mem_backing_params->num_buckets = num_buckets;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_get_hw_params(maxhash_table_params_t *tparams,
		maxhash_engine_state_t *es,
		const char *kernel_name,
		const char *hash_table_name)
{
	maxhash_err_t err = MAXHASH_ERR_OK;

	err |= maxhash_check_version(es);
	err |= maxhash_check_presence(es, kernel_name, hash_table_name);

	if (err != MAXHASH_ERR_OK)
		return err;

	char full_name[NAME_BUF_LEN] = {0};
	snprintf(full_name, sizeof(full_name), "%s_%s", kernel_name,
			hash_table_name);

	int max_bucket_entries       = get_maxfile_constant(es, full_name,
			"_MaxBucketEntries");
	int perfect                  = get_maxfile_constant(es, full_name,
			"_Perfect");
	int is_double_buffered       = get_maxfile_constant(es, full_name,
			"_IsDoubleBuffered");
	int key_width_bits           = get_maxfile_constant(es, full_name,
			"_KeyWidth");
	int jenkins_chunk_width_bits = get_maxfile_constant(es, full_name,
			"_JenkinsChunkWidth");

	if (max_bucket_entries <= 0)
	{
		fprintf(stderr, "Error: maximum bucket entries in hardware hash table "
				"(%d) is invalid.\n", max_bucket_entries);
		return MAXHASH_ERR_ERR;
	}

	if (perfect < 0 || perfect > 1)
	{
		fprintf(stderr, "Error: value of perfect flag (%d) is invalid.\n",
				perfect);
		return MAXHASH_ERR_ERR;
	}

	if (key_width_bits <= 0)
	{
		fprintf(stderr, "Error: key width in hardware hash table (%d) is "
				"invalid.\n", key_width_bits);
		return MAXHASH_ERR_ERR;
	}

	if (jenkins_chunk_width_bits <= 0 || jenkins_chunk_width_bits % 8 != 0)
	{
		fprintf(stderr, "Error: Jenkins chunk width in hardware hash table "
				"(%d) is invalid.\n", jenkins_chunk_width_bits);
		return MAXHASH_ERR_ERR;
	}

	maxhash_internal_table_params_t *intermediate_params = &tparams->intermediate;
	maxhash_internal_table_params_t *values_params = &tparams->values;

	if (perfect == 1 && max_bucket_entries == 1)
	{
		err |= maxhash_get_hw_mem_backing_params(intermediate_params, es,
				full_name, "HashParams");
		err |= maxhash_get_hw_mem_backing_params(values_params, es,
				full_name, "Values");

		if (intermediate_params->mem_type == MAXHASH_MEM_TYPE_LMEM
				|| values_params->mem_type == MAXHASH_MEM_TYPE_LMEM)
			es->lmem_burst_size_bytes = get_maxfile_global_constant(es,
					"MemCtrlPro_DataBurstSizeInBytes");

		int validate_results = get_maxfile_constant(es, full_name,
				"_ValidateResults");

		if (validate_results < 0 || validate_results > 1)
		{
			fprintf(stderr, "Error: validate keys value in hardware hash "
					"table (%d) is invalid.\n", validate_results);
			return MAXHASH_ERR_ERR;
		}

		intermediate_params->validate_results = false;
		values_params->validate_results = validate_results == 1 ? true : false;
	}
	else
	{
		intermediate_params->validate_results = false;
		values_params->validate_results = true;
	}

	tparams->engine_state              = es;
	tparams->max_bucket_entries        = max_bucket_entries;
	tparams->perfect                   = perfect;
	tparams->is_double_buffered        = is_double_buffered;
	tparams->key_width_bits            = key_width_bits;
	tparams->key_width_bytes           = (key_width_bits + 7) / 8;
	tparams->jenkins_chunk_width_bytes = (jenkins_chunk_width_bits + 7) / 8;

	return err;
}



maxhash_err_t maxhash_hw_table_init(maxhash_table_t **table,
		const char *kernel_name, const char *hash_table_name,
		maxhash_engine_state_t *es)
{
	maxhash_err_t err = MAXHASH_ERR_OK;

	maxhash_table_params_t tparams;
	memset(&tparams, 0, sizeof(tparams));
	strncpy(tparams.kernel_name, kernel_name, sizeof(tparams.kernel_name));
	strncpy(tparams.hash_table_name, hash_table_name,
			sizeof(tparams.hash_table_name));

	err = maxhash_get_hw_params(&tparams, es, kernel_name, hash_table_name);
	if (err != MAXHASH_ERR_OK)
	{
		fprintf(stderr, "Error: failed to access required MaxHash "
				"information in the MaxFile.\n");
		return err;
	}

	err = maxhash_internal_sw_init(table, &tparams);
	if (err != MAXHASH_ERR_OK)
	{
		fprintf(stderr, "Error: failed to initialize MaxHash software "
				"layer.\n");
		return err;
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_set_memory_access_fn(maxhash_table_t *table,
		void (*mem_access_fn)(void *arg, bool is_read, size_t
			base_address_bursts, void *data, size_t data_size_bursts),
		void *mem_access_fn_arg)
{
	table->tparams.mem_access_fn = mem_access_fn;
	table->tparams.mem_access_fn_arg = mem_access_fn_arg;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_clear_bucket(maxhash_internal_table_t *itable,
		size_t bucket_id)
{
	maxhash_bucket_t *bucket = &itable->buckets[bucket_id];
	maxhash_entry_t *entry = bucket->entry_list;

	while (entry)
	{
		maxhash_entry_t *tmp = entry;
		entry = entry->next;
		free(tmp->key);
		free(tmp->value);
		free(tmp);
	}

	bucket->entry_list = NULL;
	bucket->num_keys = 0;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_clear(maxhash_internal_table_t *itable)
{
	for (size_t bucket_id = 0; bucket_id < itable->iparams.num_buckets;
			bucket_id++)
		maxhash_internal_clear_bucket(itable, bucket_id);

	itable->num_entries = 0;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_clear(maxhash_table_t *table)
{
	maxhash_internal_clear(&table->sw);
	maxhash_internal_clear(&table->recent);
	maxhash_internal_clear(&table->intermediate);
	maxhash_internal_clear(&table->values);

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_free(maxhash_table_t *table)
{
	maxhash_clear(table); /* Free collision lists. */
	free(table->sw.buckets);
	free(table->intermediate.buckets);
	free(table->values.buckets);
	free(table);

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_contains_in_bucket(
		const maxhash_internal_table_t *itable,
		bool *present, const void *key, size_t bucket_id)
{
	*present = false;

	for (maxhash_entry_t *entry = itable->buckets[bucket_id].entry_list;
			entry; entry = entry->next)
		if (!memcmp(key, entry->key, itable->table->tparams.key_width_bytes))
		{
			*present = true;
			break;
		}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_get_bucket_id(
		const maxhash_internal_table_t *itable, size_t *bucket_id,
		const void *key, size_t hashparam)
{
	*bucket_id = maxhash_function_jenkins(key,
			itable->table->tparams.key_width_bytes, hashparam,
			itable->table->tparams.jenkins_chunk_width_bytes)
		% itable->iparams.num_buckets;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_contains(const maxhash_table_t *table, bool *present,
		const void *key, size_t key_len)
{
	PAD_KEY(&table->tparams, key, key_len);
	size_t bucket_id;
	maxhash_internal_get_bucket_id(&table->sw, &bucket_id, key, 0);
	return maxhash_contains_in_bucket(&table->sw, present, key, bucket_id);
}



maxhash_err_t maxhash_internal_set_entry_flag(maxhash_internal_table_t *itable,
		const void *key, uint8_t flag_id, bool flag_value)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;
	size_t bucket_id;
	maxhash_internal_get_bucket_id(itable, &bucket_id, key, 0);
	maxhash_bucket_t *bucket = &itable->buckets[bucket_id];

	bool already_present;
	maxhash_contains_in_bucket(itable, &already_present, key,
			bucket_id);

	if (!already_present)
		return MAXHASH_ERR_ERR;

	for (maxhash_entry_t *e = bucket->entry_list; e; e = e->next)
		if (!memcmp(key, e->key, tparams->key_width_bytes))
			e->flags[flag_id] = flag_value;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_put_in_bucket(maxhash_internal_table_t *itable,
		const void *key, const void *value, size_t bucket_id)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;
	maxhash_bucket_t *bucket = &itable->buckets[bucket_id];

	bool already_present;
	maxhash_contains_in_bucket(itable, &already_present, key, bucket_id);

	if (already_present)
	{
		for (maxhash_entry_t *e = bucket->entry_list; e; e = e->next)
			if (!memcmp(key, e->key, tparams->key_width_bytes)) {
				memcpy(e->value, value, itable->iparams.width_bytes);
				maxhash_debug_print(itable->table, "Adding entry to \"%s\" table...\n",
						itable->iparams.name);
				if (itable->table->tparams.debug)
					hash_print_entry(itable, e);
			}
	}
	else
	{
		if (tparams->perfect && tparams->max_bucket_entries == 1 &&
				itable->num_entries >= tparams->values.num_buckets)
		{
			fprintf(stderr, "Error: perfect hash table is full.\n");
			return MAXHASH_ERR_ERR;
		}

		bucket->num_keys++;
		itable->num_entries++;

		maxhash_entry_t *new_entry = calloc(1, sizeof(maxhash_entry_t));

		new_entry->key   = calloc(1, tparams->key_width_bytes);
		new_entry->value = calloc(1, itable->iparams.width_bytes);

		new_entry->flags[FLAG_VALID] = true;

		memcpy(new_entry->key,   key,   tparams->key_width_bytes);
		memcpy(new_entry->value, value, itable->iparams.width_bytes);

		maxhash_debug_print(itable->table, "Adding entry to \"%s\" table...\n",
				itable->iparams.name);
		if (itable->table->tparams.debug)
			hash_print_entry(itable, new_entry);

		if (!bucket->entry_list)
			bucket->entry_list = new_entry;
		else
		{
			maxhash_entry_t *last_entry = bucket->entry_list;
			while (last_entry->next)
				last_entry = last_entry->next;
			last_entry->next = new_entry;
		}
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_put(maxhash_internal_table_t *itable,
		const void *key, const void *value)
{
	size_t bucket_id;
	maxhash_internal_get_bucket_id(itable, &bucket_id, key, 0);
	return maxhash_internal_put_in_bucket(itable, key, value, bucket_id);
}



maxhash_err_t maxhash_put(maxhash_table_t *table, const void *key, size_t
		key_len, const void *value, size_t value_len)
{
	PAD_KEY(&table->tparams, key, key_len);
	PAD_VAL(&table->values.iparams, value, value_len);

	maxhash_err_t err = MAXHASH_ERR_OK;
	err |= maxhash_internal_put(&table->sw, key, value);
	err |= maxhash_internal_put(&table->recent, key, value);
	return err;
}



maxhash_err_t maxhash_internal_get_entry_in_bucket(
		const maxhash_internal_table_t *itable, bool check_key,
		const void *key, maxhash_entry_t *entry, size_t bucket_id)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;
	maxhash_bucket_t *bucket = &itable->buckets[bucket_id];

	for (maxhash_entry_t *e = bucket->entry_list; e; e = e->next)
		if (!check_key || !memcmp(key, e->key, tparams->key_width_bytes))
		{
			memcpy(entry, e, sizeof(maxhash_entry_t));
			return MAXHASH_ERR_OK;
		}

	return MAXHASH_ERR_ERR;
}



maxhash_err_t maxhash_internal_get_entry(
		const maxhash_internal_table_t *itable, bool check_key,
		const void *key, maxhash_entry_t *entry)
{
	size_t bucket_id;
	maxhash_internal_get_bucket_id(itable, &bucket_id, key, 0);
	return maxhash_internal_get_entry_in_bucket(itable, check_key, key, entry,
			bucket_id);
}



maxhash_err_t maxhash_internal_get(const maxhash_internal_table_t *itable,
		bool check_key, const void *key, void *value)
{
	maxhash_err_t err = MAXHASH_ERR_OK;
	maxhash_entry_t entry;
	err |= maxhash_internal_get_entry(itable, check_key, key, &entry);

	if (err == MAXHASH_ERR_ERR)
		return err;

	memcpy(value, entry.value, itable->iparams.width_bytes);
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_get_flag(const maxhash_entry_t *entry, int flag,
		bool *value)
{
	memcpy(value, &entry->flags[flag], sizeof(bool));
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_internal_perfect_get_index(const maxhash_table_t *table,
		const void *key, size_t *index)
{
	maxhash_err_t err = MAXHASH_ERR_OK;
	*index = 0;
	bool perfect_direct;

	maxhash_entry_t entry;
	err |= maxhash_internal_get_entry(&table->intermediate, false, key,
			&entry);

	if (err != MAXHASH_ERR_OK)
		return err;

	memcpy(index, entry.value, table->intermediate.iparams.width_bytes);

	err |= maxhash_internal_get_flag(&entry, FLAG_PERFECT_DIRECT,
			&perfect_direct);

	if (err != MAXHASH_ERR_OK)
		return err;

	if (!perfect_direct)
		maxhash_internal_get_bucket_id(&table->values, index, key, *index);

	//printf("key: %s, intermediate: %u, perfect_direct: %u, index: %zu\n",
			//(char *)key, intermediate, perfect_direct, *index);

	return err;
}



maxhash_err_t maxhash_perfect_get_index(maxhash_table_t *table,
		const void *key, size_t key_len, size_t *index)
{
	PAD_KEY(&table->tparams, key, key_len);
	return maxhash_internal_perfect_get_index(table, key, index);
}



maxhash_err_t maxhash_internal_perfect_get(const maxhash_table_t *table,
		const void *key, void *value, bool *valid)
{
	maxhash_err_t err = MAXHASH_ERR_OK;
	size_t index;
	err |= maxhash_internal_perfect_get_index(table, key, &index);

	maxhash_entry_t entry;
	err |= maxhash_internal_get_entry_in_bucket(&table->values, true, key,
			&entry, index);

	if (err != MAXHASH_ERR_OK)
		return err;

	memcpy(value, entry.value, table->values.iparams.width_bytes);
	err |= maxhash_internal_get_flag(&entry, FLAG_VALID, valid);

	//printf("index: %zu, value: %lu\n", index, *(uint64_t *)value);

	return err;
}



maxhash_err_t maxhash_perfect_get(maxhash_table_t *table, const void *key,
		size_t key_len, void *value, bool *valid)
{
	PAD_KEY(&table->tparams, key, key_len);
	return maxhash_internal_perfect_get(table, key, value, valid);
}



maxhash_err_t maxhash_get(const maxhash_table_t *table, const void *key, size_t
		key_len, void *value)
{
	PAD_KEY(&table->tparams, key, key_len);
	return maxhash_internal_get(&table->sw, true, key, value);
}



maxhash_err_t maxhash_remove_from_bucket(maxhash_internal_table_t *itable,
		const void *key, size_t bucket_id)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;
	maxhash_bucket_t *bucket = &itable->buckets[bucket_id];
	maxhash_entry_t *cur = bucket->entry_list;

	/* Empty list. */
	if (!cur)
	{
		fprintf(stderr, "Error: attempted to remove an entry that is "
				"not present in the \"%s\" hash table.\n",
				itable->iparams.name);
		return MAXHASH_ERR_ERR;
	}

	/* Delete head node? */
	if (!memcmp(key, cur->key, tparams->key_width_bytes))
	{
		bucket->entry_list = cur->next;
		free(cur->key);
		free(cur->value);
		free(cur);
		bucket->num_keys--;
		itable->num_entries--;
		return MAXHASH_ERR_OK;
	}

	maxhash_entry_t *prev = cur;
	cur = cur->next;

	/* Search for entry. */
	while (cur)
	{
		if (!memcmp(key, cur->key, tparams->key_width_bytes))
		{
			prev->next = cur->next;
			free(cur->key);
			free(cur->value);
			free(cur);
			bucket->num_keys--;
			itable->num_entries--;
			return MAXHASH_ERR_OK;
		}

		prev = cur;
		cur = cur->next;
	}

	fprintf(stderr, "Error: attempted to remove an entry that is not present "
			"in the requested hash table.\n");
	return MAXHASH_ERR_ERR;
}



maxhash_err_t maxhash_internal_remove(
		maxhash_internal_table_t *itable, const void *key)
{
	size_t bucket_id;
	maxhash_internal_get_bucket_id(itable, &bucket_id, key, 0);
	return maxhash_remove_from_bucket(itable, key, bucket_id);
}



maxhash_err_t maxhash_perfect_values_remove(maxhash_table_t *table,
		const void *key)
{
	maxhash_err_t err = MAXHASH_ERR_OK;
	size_t old_values_bucket;
	err |= maxhash_internal_perfect_get_index(table, key, &old_values_bucket);
	err |= maxhash_remove_from_bucket(&table->values, key, old_values_bucket);
	return err;
}



maxhash_err_t maxhash_remove(maxhash_table_t *table, const void *key, size_t
		key_len)
{
	PAD_KEY(&table->tparams, key, key_len);

	maxhash_err_t err = MAXHASH_ERR_OK;

	err |= maxhash_internal_remove(&table->sw, key);

	// FIXME
	err |= maxhash_internal_remove(&table->recent, key);

	err |= maxhash_perfect_values_remove(table, key);

	// FIXME
	err |= maxhash_internal_remove(&table->intermediate, key);

	return err;
}



maxhash_err_t write_mem(const maxhash_internal_table_t *itable,
		const char *buf_name, void *buf, size_t mem_size_bytes)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;

	PRINT_VAR(s, buf_name);
	PRINT_VAR(zd, mem_size_bytes);

	if (false)
		for (size_t i = 0; i < mem_size_bytes; i += 8)
		{
			size_t limit = i + 8 > mem_size_bytes ? mem_size_bytes : i + 8;
			for (size_t j = i; j < limit; j++)
				printf("%02x ", ((uint8_t *)buf)[j]);
			printf("\n");
		}

	if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_FMEM)
		maxhash_write_fmem(tparams->engine_state,
				tparams->kernel_name, buf_name,
				itable->table->load_buffer_select * mem_size_bytes /
				sizeof(uint64_t), buf, mem_size_bytes);
	else if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_DEEP_FMEM)
		maxhash_write_deep_fmem(tparams->engine_state,
				tparams->kernel_name, buf_name,
				buf,
				mem_size_bytes);
	else if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_LMEM)
	{
		size_t lmem_burst_size_bytes =
			tparams->engine_state->lmem_burst_size_bytes;
		size_t mem_size_bursts = (mem_size_bytes + lmem_burst_size_bytes - 1) /
			lmem_burst_size_bytes;
		tparams->mem_access_fn(tparams->mem_access_fn_arg, false,
				itable->iparams.base_address_bursts +
				itable->table->load_buffer_select * mem_size_bursts, buf,
				mem_size_bursts);
	}
	else
	{
		fprintf(stderr, "Error: hash table memory type is invalid.\n");
		return MAXHASH_ERR_ERR;
	}

	return MAXHASH_ERR_OK;
}



static size_t write_entry(void *dest, size_t offset_bits, const void *src,
		size_t size_bits)
{
	/* Essentially a bitwise memcpy. */

	int offset_div = offset_bits / 8;
	int offset_mod = offset_bits % 8;

	int size_div = size_bits / 8;
	int size_mod = size_bits % 8;

	const uint8_t *src_u  = (const uint8_t *)src;
	uint8_t       *dest_u = (uint8_t *)dest;

	uint8_t carry_mask = (1 <<      offset_mod ) - 1;
	uint8_t new_mask   = (1 << (8 - offset_mod)) - 1;

	uint8_t carry = dest_u[offset_div] & carry_mask;

	/* Update carry mask to read from MSBs of bytes in src. */
	carry_mask <<= (8 - offset_mod);

	int src_byte;
	for (src_byte = 0; src_byte < size_div; src_byte++)
	{
		uint8_t new = (src_u[src_byte] & new_mask) << offset_mod;
		dest_u[offset_div + src_byte] = carry | new;
		carry = (src_u[src_byte] & carry_mask) >> (8 - offset_mod);
	}

	if (size_mod != 0)
	{
		new_mask = (1 << size_mod) - 1;
		uint8_t new = (src_u[src_byte] & new_mask) << offset_mod;
		dest_u[offset_div + src_byte] = carry | new;
	}

	return size_bits;
}



maxhash_err_t write_table_data(const maxhash_internal_table_t *itable,
		bool has_direct_flag)
{
	const maxhash_table_params_t *tparams = &itable->table->tparams;

	size_t deep_fmem_id_bits = itable->iparams.mem_type ==
		MAXHASH_MEM_TYPE_DEEP_FMEM ? DEEP_FMEM_ID_BITS : 0;

	size_t num_flags = has_direct_flag ? NUM_ENTRY_FLAGS : NUM_ENTRY_FLAGS - 1;

	size_t mem_entry_size_bits = deep_fmem_id_bits + num_flags +
		itable->iparams.width_bits;
	if (itable->iparams.validate_results)
		mem_entry_size_bits += tparams->key_width_bits;

	size_t mem_entry_size_bytes = (mem_entry_size_bits + 7) / 8;

	size_t burst_size_bytes = 0;
	if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_FMEM)
		burst_size_bytes = sizeof(uint64_t);
	else if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_DEEP_FMEM)
		burst_size_bytes = mem_entry_size_bytes; // FIXME
	else if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_LMEM)
		burst_size_bytes = tparams->engine_state->lmem_burst_size_bytes;
	else if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_UNDEFINED)
		return MAXHASH_ERR_OK;
	else
	{
		fprintf(stderr, "Error: hash table memory type is invalid.\n");
		return MAXHASH_ERR_ERR;
	}

	const size_t PCIE_WIDTH = 16;
	if (burst_size_bytes < PCIE_WIDTH)
	{
		/* Round burst size up to a power of two, if not already. */
		size_t power = 1;
		while (power < burst_size_bytes)
			power *= 2;
		burst_size_bytes = power;
	}
	else
	{
		/* Round burst size up to a multiple of the PCIe interface size, if not
		 * already. */
		burst_size_bytes = ((burst_size_bytes + PCIE_WIDTH - 1) / PCIE_WIDTH) *
			PCIE_WIDTH;
	}

	/* We don't support multi-burst entries yet. */
	if (mem_entry_size_bytes > burst_size_bytes)
	{
		fprintf(stderr, "Error: memory entries are too large.\n");
		return MAXHASH_ERR_ERR;
	}

	size_t entries_per_burst = 1;
	while (entries_per_burst <= burst_size_bytes / mem_entry_size_bytes
			&& burst_size_bytes % entries_per_burst == 0)
		entries_per_burst *= 2;
	entries_per_burst /= 2;

	mem_entry_size_bytes = burst_size_bytes / entries_per_burst;

	size_t num_bursts = (tparams->max_bucket_entries *
			itable->iparams.num_buckets + entries_per_burst - 1) /
		entries_per_burst;

	size_t mem_size = num_bursts * burst_size_bytes;
	void *mem_contents = calloc(1, mem_size);

	PRINT_VAR(zd, mem_entry_size_bytes);
	PRINT_VAR(zd, itable->iparams.num_buckets);
	PRINT_VAR(zd, entries_per_burst);
	PRINT_VAR(zd, num_bursts);
	PRINT_VAR(zd, burst_size_bytes);
	PRINT_VAR(zd, mem_size);

	for (size_t bucket_id = 0; bucket_id < itable->iparams.num_buckets;
			bucket_id++)
	{
		maxhash_bucket_t *bucket = &itable->buckets[bucket_id];
		maxhash_entry_t *e = bucket->entry_list;

		for (size_t bucket_entry = 0; bucket_entry < tparams->max_bucket_entries;
				bucket_entry++)
		{
			size_t entry_id = bucket_entry * itable->iparams.num_buckets +
				bucket_id;

			size_t burst = entry_id / entries_per_burst;
			size_t entry_in_burst = entry_id % entries_per_burst;
			size_t offset_bits = (burst * burst_size_bytes + entry_in_burst *
				mem_entry_size_bytes) * 8;

			if (offset_bits >= mem_size * 8)
			{
				fprintf(stderr, "Error: attempted to write to an invalid "
						"memory location.\n");
				return MAXHASH_ERR_ERR;
			}

			if (itable->iparams.mem_type == MAXHASH_MEM_TYPE_DEEP_FMEM)
			{
				uint8_t deep_fmem_id = 0;
				deep_fmem_id |= itable->iparams.deep_fmem_id;
				offset_bits += write_entry(mem_contents, offset_bits,
						&deep_fmem_id, DEEP_FMEM_ID_BITS);
			}

			if (e && e->flags[FLAG_VALID])
			{
				uint8_t flags = 0;
				flags |= e->flags[FLAG_VALID] << FLAG_VALID;
				if (has_direct_flag)
					flags |= e->flags[FLAG_PERFECT_DIRECT] <<
						FLAG_PERFECT_DIRECT;

				offset_bits += write_entry(mem_contents, offset_bits, &flags,
						num_flags);

				if (itable->iparams.validate_results)
					offset_bits += write_entry(mem_contents, offset_bits,
							e->key, tparams->key_width_bits);

				offset_bits += write_entry(mem_contents, offset_bits, e->value,
						itable->iparams.width_bits);

				e = e->next;
			}
		}
	}

	if (tparams->max_bucket_entries > 1)
		for (size_t mem_id = 0; mem_id < tparams->max_bucket_entries; mem_id++)
		{
			char name_buf[NAME_BUF_LEN] = {0};
			snprintf(name_buf, sizeof(name_buf), "%s_Buckets%zu",
					itable->iparams.name, mem_id);
			write_mem(itable, name_buf, mem_contents, mem_size);
		}
	else
		write_mem(itable, itable->iparams.name, mem_contents, mem_size);

	free(mem_contents);

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_switch_buffer(maxhash_table_t *table)
{
	char name_buf[NAME_BUF_LEN] = {0};
	snprintf(name_buf, sizeof(name_buf), "%s_BufferSelect",
			table->tparams.hash_table_name);

	uint64_t load_buffer_select = table->load_buffer_select;

	maxhash_write_fmem(table->tparams.engine_state, table->tparams.kernel_name,
			name_buf, 0, &load_buffer_select, 1);

	table->load_buffer_select ^= 1;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_commit(maxhash_table_t *table)
{
	/* Sanity check. */
	for (size_t i = 0; i < table->intermediate.iparams.num_buckets; i++)
		assert((table->values.buckets[i].num_keys == 0 &&
					!table->values.buckets[i].entry_list)
				|| (table->values.buckets[i].num_keys > 0 &&
					table->values.buckets[i].entry_list));

	maxhash_err_t err = MAXHASH_ERR_OK;

	if (table->tparams.debug) maxhash_print_sparse(&table->sw);
	if (table->tparams.debug) maxhash_print_sparse(&table->recent);

	if (table->tparams.max_bucket_entries == 1)
	{
		maxhash_debug_print(table, "Creating perfect hash table...\n");
		err |= maxhash_perfect_create(table);
		maxhash_debug_print(table, "Finished creating perfect hash table.\n");
		if (table->tparams.debug) maxhash_print_sparse(&table->recent);
		if (table->tparams.debug) maxhash_print_sparse(&table->intermediate);
		if (table->tparams.debug) maxhash_print_sparse(&table->values);
		maxhash_debug_print(table, "Writing table of hash parameters...\n");
		err |= write_table_data(&table->intermediate, true);
		maxhash_debug_print(table, "Finished writing table of hash parameters.\n");
		maxhash_debug_print(table, "Writing table of values...\n");
		err |= write_table_data(&table->values, false);
		maxhash_debug_print(table, "Finished writing table of values.\n");
	}
	else
		err |= write_table_data(&table->values, false);

	if (table->tparams.is_double_buffered)
	{
		maxhash_debug_print(table, "Switching buffer...\n");
		maxhash_switch_buffer(table);
		maxhash_debug_print(table, "Finished switching buffer.\n");
	}

	return err;
}



maxhash_err_t maxhash_putall(maxhash_table_t *destination, const
		maxhash_table_t *source)
{
	for (size_t bucket_id = 0; bucket_id <
			source->sw.iparams.num_buckets; bucket_id++)
	{
		maxhash_bucket_t *bucket = &source->sw.buckets[bucket_id];
		for (maxhash_entry_t *e = bucket->entry_list; e; e = e->next)
		{
			maxhash_err_t err = maxhash_internal_put(&destination->sw, e->key,
					e->value);
			if (err != MAXHASH_ERR_OK)
				return err;
		}
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_perfect_get_capacity(const maxhash_table_t *table,
		size_t *capacity)
{
	if (table->tparams.max_bucket_entries == 1)
	{
		*capacity = table->values.iparams.num_buckets;
		return MAXHASH_ERR_OK;
	}
	else
	{
		fprintf(stderr, "Error: not a perfect hash table.\n");
		return MAXHASH_ERR_ERR;
	}
}



maxhash_err_t maxhash_get_key_width(const maxhash_table_t *table, size_t
		*key_width)
{
	*key_width = table->tparams.key_width_bytes;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_get_value_width(const maxhash_table_t *table, size_t
		*value_width)
{
	*value_width = table->values.iparams.width_bytes;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_get_keys(const maxhash_table_t *table, void *keys)
{
	size_t key_width_bytes = table->tparams.key_width_bytes;
	size_t entry_id = 0;
	for (size_t bucket_id = 0; bucket_id < table->sw.iparams.num_buckets;
			bucket_id++)
	{
		maxhash_bucket_t *bucket = &table->sw.buckets[bucket_id];
		for (maxhash_entry_t *e = bucket->entry_list; e; e = e->next)
		{
			memcpy(keys + entry_id * key_width_bytes, e->key, key_width_bytes);
			entry_id++;
		}
	}

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_entry_iterator_init(maxhash_entry_iterator_t **iterator,
		const maxhash_table_t *table)
{
	*iterator = calloc(1, sizeof(maxhash_entry_iterator_t));
	if (*iterator == NULL)
	{
		fprintf(stderr, "Error: failed to allocate memory for MaxHash entry iterator.\n");
		return MAXHASH_ERR_ERR;
	}

	(*iterator)->itable = &table->sw;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_entry_iterator_free(maxhash_entry_iterator_t *iterator)
{
	free(iterator);
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_entry_iterator_next(maxhash_entry_iterator_t *iterator)
{
	/* Reuse next entry that was found incidentally in
	 * maxhash_entry_iterator_has_next. */
	if (iterator->next)
	{
		iterator->entry = iterator->next;
		iterator->next = NULL;
		return MAXHASH_ERR_OK;
	}

	/* User shouldn't call next() without checking has_next(), but if they do,
	 * handle this correctly. */
	bool has_next;
	maxhash_entry_iterator_has_next(iterator, &has_next);

	if (has_next)
	{
		iterator->entry = iterator->next;
		iterator->next = NULL;
		return MAXHASH_ERR_OK;
	}

	return MAXHASH_ERR_ERR;
}



maxhash_err_t maxhash_entry_iterator_has_next(maxhash_entry_iterator_t *iterator, bool *has_next)
{
	/* Look for next entry, and if found, save it in iterator->next for use by
	 * maxhash_entry_iterator_next. */

	*has_next = true;
	if (iterator->next && iterator->next->next)
	{
		iterator->next = iterator->next->next;
		return MAXHASH_ERR_OK;
	}

	iterator->next = NULL;
	while (!iterator->next &&
			iterator->bucket_id
			< iterator->itable->iparams.num_buckets)
	{
		iterator->next = iterator->itable->buckets[iterator->bucket_id].entry_list;
		iterator->bucket_id++;
	}

	if (!iterator->next)
		*has_next = false;

	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_entry_iterator_get_key(
		maxhash_entry_iterator_t *iterator, const void **key)
{
	*key = iterator->entry->key;
	return MAXHASH_ERR_OK;
}



maxhash_err_t maxhash_entry_iterator_get_value(
		maxhash_entry_iterator_t *iterator, const void **value)
{
	*value = iterator->entry->value;
	return MAXHASH_ERR_OK;
}




maxhash_err_t maxhash_perfect_create(maxhash_table_t *table)
{
	maxhash_err_t err = MAXHASH_ERR_OK;
	struct timeval tv_start, tv_end;
	gettimeofday(&tv_start, NULL);

	maxhash_internal_table_t *source_itable;

	bool enable_incremental_puts = false;
	if (enable_incremental_puts)
	{
		/*
		 * 1. We have a hash table containing all of the keys added since the
		 * last commit, called "recent".  Each time we add a hash entry, it's
		 * placed both in the recent table and the existing "sw" table.
		 *
		 * 2. Some of the new keys will collide with others in the sw table,
		 * some won't.
		 *
		 * 3. We start with the keys that collide, specifically those that
		 * collide with the greatest number of other keys, and work towards
		 * those that don't cause any collisions.
		 *
		 * 4. For each key in the recent table, we add all of the colliding
		 * entries in the sw table into the recent table and remove them from
		 * the intermediate and values tables.
		 *
		 * 5. We then carry out the normal perfect hash calculation process and
		 * re-add the removed colliding entries.
		 *
		 * 6. Note that if a bucket in the values table is vacated, we do not
		 * reuse it until the next call to this function.  This simplifies the
		 * algorithm, at the expense only of slightly longer searches to find
		 * valid hash parameters.
		 */

		size_t moves = 0;
		source_itable = &table->recent;
		if (table->tparams.debug) maxhash_print_sparse(&table->intermediate);
		if (table->tparams.debug) maxhash_print_sparse(&table->values);

		for (size_t bucket_id = 0; bucket_id <
				table->recent.iparams.num_buckets; bucket_id++)
		{
			maxhash_bucket_t *recent_bucket = &table->recent.buckets[bucket_id];
			maxhash_bucket_t *sw_bucket = &table->sw.buckets[bucket_id];
			if (recent_bucket->num_keys > 0 && sw_bucket->num_keys >
					recent_bucket->num_keys)
			{
				for (maxhash_entry_t *entry = sw_bucket->entry_list; entry;
						entry = entry->next)
				{
					bool is_new_entry;
					maxhash_contains_in_bucket(&table->recent, &is_new_entry,
							entry->key, bucket_id);
					if (!is_new_entry)
					{
						/* Collision, needs to be moved. */
						moves++;
						err |= maxhash_perfect_values_remove(table, entry->key);
						err |= maxhash_internal_put(&table->recent, entry->key,
								entry->value);
						if (err != MAXHASH_ERR_OK)
						{
							fprintf(stderr, "Error: incremental put failed.\n");
							return err;
						}
					}
				}
				err |= maxhash_internal_clear_bucket(&table->intermediate,
						bucket_id);
			}
		}

		if (table->tparams.debug) maxhash_print_sparse(&table->recent);

		printf("Incremental puts: need to move %zu entries in the values"
				" table.\n", moves);
	}
	else
	{
		/* Calculate/recalculate the perfect hash from scratch. */
		source_itable = &table->sw;
		maxhash_internal_clear(&table->intermediate);
		maxhash_internal_clear(&table->values);
	}

	/* Copy bucket list and sort in-place in reverse order of the number of
	 * collisions. */
	size_t buckets_size = source_itable->iparams.num_buckets *
		sizeof(maxhash_bucket_t);
	maxhash_bucket_t *sorted_buckets = malloc(buckets_size);
	memcpy(sorted_buckets, source_itable->buckets, buckets_size);
	qsort(sorted_buckets, source_itable->iparams.num_buckets,
			sizeof(maxhash_bucket_t), compare_bucket_num_keys);

	size_t bucket_id = 0;
	maxhash_bucket_t *bucket = &sorted_buckets[bucket_id];

	/* Temporary array in which to store the hashes of all of the entries in a
	 * bucket. */
	size_t new_hashes_size = 1024;
	uint32_t new_hashes[new_hashes_size];
	size_t entry_id;

	size_t prev_num_keys = ULLONG_MAX;
	ssize_t prev_bucket_id = 0;
	size_t parameter_sum = 0;
	size_t parameter_max = 0;
	size_t num_hashes = 0;
	size_t total_parameter_max = 0;
	size_t total_num_hashes = 0;

	const char *columns[] = {"Collisions", "Buckets", "Max. attempts",
		"Avg. attempts", "Num. hashes"};

	if (bucket->num_keys >= 2)
	{
		for (size_t col = 0; col < sizeof(columns) / sizeof(char *); col++)
			printf("  %s", columns[col]);
		printf("\n");
	}

	/* Iterate through all of the buckets with at least two entries. */
	while (bucket_id < table->intermediate.iparams.num_buckets &&
			bucket->num_keys >= 2)
	{
		uint32_t d = 0;
		bool found = false;

		/* Search for a value of 'd' that assigns keys to unique slots. */
		while (!found)
		{
			if ((size_t)d >= (1ul << table->intermediate.iparams.width_bytes *
						8) - 1)
			{
				fprintf(stderr, "Error: failed to find a hash parameter "
						"(d: %d, bucket: %zd).\n", d, bucket_id);
				return MAXHASH_ERR_ERR;
			}

			maxhash_entry_t *entry = bucket->entry_list;
			found = true;
			entry_id = 0;

			while (entry && found)
			{
				uint32_t new_hash = maxhash_function_jenkins(entry->key,
						table->tparams.key_width_bytes, d,
						table->tparams.jenkins_chunk_width_bytes)
					% table->values.iparams.num_buckets;
				num_hashes++;

				/* Check for collisions with previously placed buckets. */
				if (table->values.buckets[new_hash].num_keys > 0)
					found = false;
				else
					/* Check for collisions within this bucket. */
					for (size_t i = 0; i < entry_id; i++)
						if (new_hash == new_hashes[i])
							found = false;

				if (found)
				{
					if (entry_id >= new_hashes_size)
					{
						fprintf(stderr, "Error: this implementation only "
								"supports up to %zu collisions per hash "
								"bucket.\n", new_hashes_size);
						return MAXHASH_ERR_ERR;
					}
					new_hashes[entry_id++] = new_hash;
					entry = entry->next;
				}
			}

			if (!found)
				d++;
		}

		/* Populate the correct locations in the hardware tables. */
		entry_id = 0;
		for (maxhash_entry_t *entry = bucket->entry_list; entry; entry =
				entry->next)
			maxhash_internal_put_in_bucket(&table->values, entry->key,
					entry->value, new_hashes[entry_id++]);

		maxhash_internal_put(&table->intermediate,
				bucket->entry_list->key, &d);

		/* Print statistics. */
		if (parameter_max < d) parameter_max = d;
		parameter_sum += d;
		bucket_id++;

		if (bucket->num_keys != prev_num_keys)
		{
			double average_searches = (double)parameter_sum / (bucket_id -
					prev_bucket_id);

			printf("  %*zu",  (int)strlen(columns[0]), bucket->num_keys);
			printf("  %*zu",  (int)strlen(columns[1]), bucket_id - prev_bucket_id);
			printf("  %*zu",  (int)strlen(columns[2]), parameter_max);
			printf("  %*.1f", (int)strlen(columns[3]), average_searches);
			printf("  %*zu",  (int)strlen(columns[4]), num_hashes);
			printf("\n");

			prev_num_keys = bucket->num_keys;
			prev_bucket_id = bucket_id;
			parameter_sum = 0;
			if (total_parameter_max < parameter_max)
				total_parameter_max = parameter_max;
			parameter_max = 0;
			total_num_hashes += num_hashes;
			num_hashes = 0;
		}

		bucket = &sorted_buckets[bucket_id];
	}

	uint32_t values_bucket_id = 0;
	maxhash_bucket_t *values_bucket =
		&table->values.buckets[values_bucket_id];

	/* Iterate through the rest of the occupied buckets, assigning entries
	 * directly to buckets in the new table rather than searching for a hash
	 * parameter. */
	while (bucket_id < table->intermediate.iparams.num_buckets
			&& values_bucket_id < table->values.iparams.num_buckets
			&& bucket->num_keys)
	{
		if (!values_bucket->num_keys)
		{
			err |= maxhash_internal_put(&table->intermediate,
					bucket->entry_list->key, &values_bucket_id);
			err |= maxhash_internal_set_entry_flag(&table->intermediate,
					bucket->entry_list->key, FLAG_PERFECT_DIRECT, true);
			err |= maxhash_internal_put_in_bucket(&table->values,
					bucket->entry_list->key, bucket->entry_list->value,
					values_bucket_id);
			bucket = &sorted_buckets[++bucket_id];
		}

		values_bucket = &table->values.buckets[++values_bucket_id];
	}

	free(sorted_buckets);

	err |= maxhash_internal_clear(&table->recent);

	/* Print statistics. */
	gettimeofday(&tv_end, NULL);

	maxhash_debug_print(table, "Mapped entries directly in %ld bucket(s) with 1 collision.\n",
			bucket_id - prev_bucket_id);

	uint64_t time_diff = (tv_end.tv_sec * 1000000 + tv_end.tv_usec) -
		(tv_start.tv_sec * 1000000 + tv_start.tv_usec);
	maxhash_debug_print(table, "Perfect hash table creation took %lu.%02lu seconds.\n",
			time_diff / 1000000, (time_diff % 1000000) / 10000);
	maxhash_debug_print(table, "Largest hash parameter:  %zu.\n", total_parameter_max);

	uint8_t num_param_bits = 1;
	while (total_parameter_max >>= 1 != 0) num_param_bits++;
	maxhash_debug_print(table, "Number of bits required: %u.\n", num_param_bits);

	maxhash_debug_print(table, "Total number of hashes:  %zu.\n", total_num_hashes);

	return err;
}
