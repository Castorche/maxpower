/*
 * LMem.h
 *
 *  Created on: 20 Apr 2015
 *      Author: itay
 */

#ifndef LMEM_H_
#define LMEM_H_

#include <stdint.h>
#include <stdbool.h>

enum lmem_cmd_code_e {
	NOP = 0,
	ClearFlag = 1,
	SetFlag = 2,
	BlockUntilFlagSet = 4,
	BlockUntilFlagCleared = 5
};

struct normal_mode {
   uint32_t address : 28;
   uint32_t : 3;
   uint32_t echo_command : 1;
   uint8_t size;
   uint8_t inc : 7;
   uint8_t inc_mode : 1;
};

struct control_mode {
   uint32_t flag_id;
   uint8_t mode; // = 0
   uint8_t code : 3; // 0 Nop, 2 SetFlag, 1 ClearFlag, 3 spare, 4 BlockUntilFlagSet, 5 BlockUntilFlagCleared, 6 spare, 7 spare
   uint8_t : 5;
};

typedef struct lmem_cmd_s  {
    union  {
       struct normal_mode normal;
       struct control_mode cmd;
    } mode; // 48 bits

    uint32_t stream_select : 15;
    uint32_t tag : 1;
} lmem_cmd_t;

lmem_cmd_t lmem_cmd_padding();
lmem_cmd_t lmem_cmd_data(uint16_t streamIx, size_t address, size_t sizeBursts, bool genEcho, bool genInterrupt);
lmem_cmd_t lmem_cmd_control(enum lmem_cmd_code_e commandCode, uint16_t streamIx, uint32_t flagIx);


#endif /* LMEM_H_ */
