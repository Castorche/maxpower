/*
 * lmem_cpu_access.h
 *
 *  Created on: 17 Apr 2015
 *      Author: itay
 */

#ifndef LMEM_CPU_ACCESS_H_
#define LMEM_CPU_ACCESS_H_

typedef struct lmem_cpu_access_s lmem_cpu_access_t;

extern lmem_cpu_access_t *lmem_init_cpu_access(max_file_t *maxfile, max_engine_t *engine);
extern size_t lmem_get_burst_size_bytes(lmem_cpu_access_t *handle);
extern void lmem_write(lmem_cpu_access_t *handle, uint32_t address_bursts, const void *data, size_t data_size_bursts);
extern void lmem_read(lmem_cpu_access_t *handle, uint32_t address_bursts, void *data, size_t data_size_bursts);

#endif /* LMEM_CPU_ACCESS_H_ */
