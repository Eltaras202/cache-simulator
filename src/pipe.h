#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include <stdbool.h>
#include <stdint.h>
/* Cache block structure */

typedef struct Cache_Block {
    uint32_t tag;           /* tag bits */
    int valid;              /* valid bit */
    int dirty;              /* dirty bit (for data cache) */
    uint32_t lru_counter;   /* for LRU replacement */
    uint32_t data[8];       /* 32-byte block = 8 words */
} Cache_Block;
// Cache replacement policies
typedef enum {
    REPLACEMENT_LRU,
    REPLACEMENT_FIFO,
    REPLACEMENT_RANDOM
} ReplacementPolicy;

// Cache insertion policies
typedef enum {
    INSERTION_MRU,  // Most Recently Used (normal)
    INSERTION_LRU // Least Recently Used
} InsertionPolicy;

/* Cache structure */
typedef struct Cache {
    int size;               /* cache size in bytes */
    int block_size;         /* block size in bytes */
    int associativity;      /* number of ways */
    int num_sets;           /* number of sets */
    int index_bits;         /* number of index bits */
    int offset_bits;        /* number of offset bits */
    int tag_bits;           /* number of tag bits */
    Cache_Block **blocks;   /* 2D array: [set][way] */
    uint32_t global_lru_counter; /* global counter for LRU */
    ReplacementPolicy replacement_policy;
    InsertionPolicy insertion_policy;
    /* Statistics */
    uint64_t accesses;
    uint64_t misses;
    uint64_t hits;
    uint64_t writebacks;
} Cache;


// Performance metrics structure
typedef struct {
    uint64_t accesses;
    uint64_t hits;
    uint64_t misses;
    uint64_t writebacks;
    uint64_t cycles;
    double ipc;
    double hit_rate;
    double miss_rate;
} PerformanceMetrics;

/* Pipeline ops (instances of this structure) are high-level representations of
 * the instructions that actually flow through the pipeline. This struct does
 * not correspond 1-to-1 with the control signals that would actually pass
 * through the pipeline. Rather, it carries the original instruction, operand
 * information and values as they are collected, and destination information. */
typedef struct Pipe_Op {
    /* PC of this instruction */
    uint32_t pc;
    /* raw instruction */
    uint32_t instruction;
    /* decoded opcode and subopcode fields */
    int opcode, subop;

    /* immediate value, if any, for ALU immediates */
    uint32_t imm16, se_imm16;
    /* shift amount */
    int shamt;

    /* register source values */
    int reg_src1, reg_src2; /* 0 -- 31 if this inst has register source(s), or
                               -1 otherwise */
    uint32_t reg_src1_value, reg_src2_value; /* values of operands from source
                                                regs */

    /* memory access information */
    int is_mem;       /* is this a load/store? */
    uint32_t mem_addr; /* address if applicable */
    int mem_write; /* is this a write to memory? */
    uint32_t mem_value; /* value loaded from memory or to be written to memory */

    /* register destination information */
    int reg_dst; /* 0 -- 31 if this inst has a destination register, -1
                    otherwise */
    uint32_t reg_dst_value; /* value to write into dest reg. */
    int reg_dst_value_ready; /* destination value produced yet? */

    /* branch information */
    int is_branch;        /* is this a branch? */
    uint32_t branch_dest; /* branch destination (if taken) */
    int branch_cond;      /* is this a conditional branch? */
    int branch_taken;     /* branch taken? (set as soon as resolved: in decode
                             for unconditional, execute for conditional) */
    int is_link;          /* jump-and-link or branch-and-link inst? */
    int link_reg;         /* register to place link into? */

} Pipe_Op;

/* The pipe state represents the current state of the pipeline. It holds a
 * pointer to the op that is currently at the input of each stage. As stages
 * execute, they remove the op from their input (set the pointer to NULL) and
 * place an op at their output. If the pointer that represents a stage's output
 * is not null when that stage executes, then this represents a pipeline stall,
 * and the stage must not overwrite its output (otherwise an instruction would
 * be lost).
 */

typedef struct Pipe_State {
    /* pipe op currently at the input of the given stage (NULL for none) */
    Pipe_Op *decode_op, *execute_op, *mem_op, *wb_op;

    /* register file state */
    uint32_t REGS[32];
    uint32_t HI, LO;

    /* program counter in fetch stage */
    uint32_t PC;

    /* information for PC update (branch recovery). Branches should use this
     * mechanism to redirect the fetch stage, and flush the ops that came after
     * the branch as necessary. */
    int branch_recover; /* set to '1' to load a new PC */
    uint32_t branch_dest; /* next fetch will be from this PC */
    int branch_flush; /* how many stages to flush during recover? (1 = fetch, 2 = fetch/decode, ...) */

    /* multiplier stall info */
    int multiplier_stall; /* number of remaining cycles until HI/LO are ready */

    /* Cache structures */
    Cache *icache;      /* instruction cache */
    Cache *dcache;      /* data cache */
    
    /* Cache miss handling */
    int icache_stall;   /* cycles remaining for I-cache miss */
    int dcache_stall;   /* cycles remaining for D-cache miss */
    uint32_t icache_miss_addr;
    /* place other information here as necessary */
    bool is_stalled;

} Pipe_State;

/* global variable -- pipeline state */
extern Pipe_State pipe;

/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* pipe stages can call this to schedule a branch recovery */
/* flushes 'flush' stages (1 = execute only, 2 = fetch/decode, ...) and then
 * sets the fetch PC to the given destination. */
void pipe_recover(int flush, uint32_t dest);

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();



/* Cache functions */
Cache* cache_create(int size, int block_size, int associativity ,int replacement_policy, int insertion_policy);
void cache_destroy(Cache *cache);
int cache_access(Cache *cache, uint32_t addr, uint32_t *data, int is_write, uint32_t write_data);
void cache_print_stats(Cache *cache, const char* cache_name);
int cache_find_lru_way(Cache *cache, uint32_t index);
int cache_find_fifo_way(Cache *cache, uint32_t index);
int cache_find_random_way(Cache *cache, uint32_t index);
int cache_find_replacement_way(Cache *cache, uint32_t index) ;
void cache_update_insertion(Cache *cache, uint32_t index, int way) ;

void cache_load_block(Cache *cache, int way, uint32_t index, uint32_t block_addr);

#endif