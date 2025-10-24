#include "pipe.h"
#include "shell.h"
#include "mips.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <time.h>

/*==============================================================================
 * Global Pipeline State
 *============================================================================*/

Pipe_State pipe;
/*==============================================================================
 * Debugging Utilities
 *============================================================================*/

/* debug */
void print_op(Pipe_Op *op)
{
    if (op)
        printf("OP (PC=%08x inst=%08x) src1=R%d (%08x) src2=R%d (%08x) dst=R%d valid %d (%08x) br=%d taken=%d dest=%08x mem=%d addr=%08x\n",
                op->pc, op->instruction, op->reg_src1, op->reg_src1_value, op->reg_src2, op->reg_src2_value, op->reg_dst, op->reg_dst_value_ready,
                op->reg_dst_value, op->is_branch, op->branch_taken, op->branch_dest, op->is_mem, op->mem_addr);
    else
        printf("(null)\n");
}


/*==============================================================================
 * Cache Implementation
 *============================================================================*/

/**
 * @brief Creates and initializes a cache.
 */
Cache* cache_create(int size, int block_size, int associativity, int replacement_policy,int insertion_policy) {
    Cache *cache = malloc(sizeof(Cache));
    if (!cache) {
        fprintf(stderr, "Error: Failed to allocate cache\n");
        exit(1);
    }
    cache->size = size;
    cache->block_size = block_size;
    cache->associativity = associativity;
    cache->num_sets = size / (block_size * associativity);
    cache->replacement_policy = replacement_policy;
    cache->insertion_policy = insertion_policy;

    // Validate cache parameters
    if (cache->num_sets <= 0 || (cache->num_sets & (cache->num_sets - 1)) != 0) {
        fprintf(stderr, "Error: Invalid cache configuration\n");
        exit(1);
    }
    /* Calculate bit fields */
    cache->offset_bits = (int)log2(block_size);
    cache->index_bits = (int)log2(cache->num_sets);
    cache->tag_bits = 32 - cache->index_bits - cache->offset_bits;
    
    /* Allocate cache blocks */
    cache->blocks = malloc(cache->num_sets * sizeof(Cache_Block*));
    if (!cache->blocks) {
        fprintf(stderr, "Error: Failed to allocate cache blocks\n");
        exit(1);
    }
    for (int i = 0; i < cache->num_sets; i++) {
        cache->blocks[i] = malloc(associativity * sizeof(Cache_Block));
         if (!cache->blocks[i]) {
            fprintf(stderr, "Error: Failed to allocate cache set %d\n", i);
            exit(1);
        }
        for (int j = 0; j < associativity; j++) {
            cache->blocks[i][j].valid = 0;
            cache->blocks[i][j].dirty = 0;
            cache->blocks[i][j].tag = 0;
            cache->blocks[i][j].lru_counter = 0;
            memset(cache->blocks[i][j].data, 0, sizeof(cache->blocks[i][j].data));
        }
    }
    
    cache->global_lru_counter = 0;
    cache->accesses = 0;
    cache->misses = 0;
    cache->hits = 0;
    cache->writebacks = 0;
    
    return cache;
}

/**
 * @brief Destroys a cache and frees its memory.
 */
void cache_destroy(Cache *cache) {
    if (!cache) return;

    for (int i = 0; i < cache->num_sets; i++) {
        free(cache->blocks[i]);
    }
    free(cache->blocks);
    free(cache);
}
/**
 * @brief Helper function to load a block from memory into the cache.
 */
void cache_load_block(Cache *cache, int way, uint32_t index, uint32_t block_addr) {
    Cache_Block *block = &cache->blocks[index][way];
    
    /* Simulate loading entire block from memory */
    for (int i = 0; i < cache->block_size / 4; i++) {
        block->data[i] = mem_read_32(block_addr + i * 4);
    }
}

/**
 * @brief Accesses the cache for a read or write operation.
 * @return 1 on hit, 0 on miss.
 */
int cache_access(Cache *cache, uint32_t addr, uint32_t *data, int is_write, uint32_t write_data) {
    if (!cache) {
        fprintf(stderr, "Error: Cache is NULL\n");
        return 0;
    }
    cache->accesses++;
    
    /* Extract address components */
    uint32_t offset = addr & ((1 << cache->offset_bits) - 1);
    uint32_t index = (addr >> cache->offset_bits) & ((1 << cache->index_bits) - 1);
    uint32_t tag = addr >> (cache->offset_bits + cache->index_bits);
    
    /* Validate index bounds */
    if (index >= cache->num_sets) {
        fprintf(stderr, "Error: Cache index out of bounds\n");
        return 0;
    }
    /* Search for the block in the set */
    Cache_Block *set = cache->blocks[index];
    int hit_way = -1;
    
    for (int way = 0; way < cache->associativity; way++) {
        if (set[way].valid && set[way].tag == tag) {
            hit_way = way;
            break;
        }
    }
    
    if (hit_way != -1) {
        /* Cache hit */
        cache->hits++;
        
        /* Update LRU 
        this because fifo and  random doesnot change at hits*/ 
       if (cache->replacement_policy == REPLACEMENT_LRU) {
            set[hit_way].lru_counter = ++cache->global_lru_counter;
        }
        /* Calculate word offset within the block */
        uint32_t word_offset = offset / 4;
        if (word_offset >= cache->block_size / 4) {
            fprintf(stderr, "Error: Word offset out of bounds\n");
            return 0;
        }
        if (is_write) {
            /* Write hit */
            set[hit_way].dirty = 1;
            set[hit_way].data[word_offset] = write_data;
        } else {
            /* Read hit */
          
            *data = set[hit_way].data[word_offset];
        }
        
        return 1; /* Hit */
    } else {
        /* Cache miss */
        cache->misses++;
        
        /* Find replacement way */
    int replace_way = cache_find_replacement_way(cache, index);


        
        
       
        /* Handle dirty eviction  */
        if (set[replace_way].valid && set[replace_way].dirty) {
            /* Write back dirty block - instantaneous */
             cache->writebacks++;
            set[replace_way].dirty = 0;
        }
        
        /* Load new block from memory */
        uint32_t block_addr = addr & ~((1 << cache->offset_bits) - 1);
        cache_load_block(cache, replace_way, index, block_addr);
     printf("[DEBUG]  Replace_way=%d\n", replace_way);
       /* Update block metadata */
       set[replace_way].valid = 1;
       set[replace_way].tag = tag;
       set[replace_way].dirty = 0;

       /* Apply insertion policy */
       cache_update_insertion(cache, index, replace_way);


        /* Calculate word offset within the block */
        uint32_t word_offset = offset / 4;
        if (is_write) {
            /* Write miss */
            set[replace_way].dirty = 1;
            set[replace_way].data[word_offset] = write_data;
        } else {
            /* Read miss */
            *data = set[replace_way].data[word_offset];
        }
        
        return 0; /* Miss */
    }
}

/**
 * @brief Finds the way to replace using the LRU policy.
 */
int cache_find_lru_way(Cache *cache, uint32_t index) {
    Cache_Block *set = cache->blocks[index];
    int replace_way = 0;
    uint32_t min_lru = set[0].lru_counter;
    
    /* First, look for invalid way */
    for (int way = 0; way < cache->associativity; way++) {
        printf("[DEBUG] LRU: way=%d, valid=%d\n", way, set[way].valid);
        if (!set[way].valid) {
            return way;
        }
    }
    printf("[DEBUG] LRU:evection\n"); 
    
    /* If no invalid way, find LRU */
    for (int way = 1; way < cache->associativity; way++) {
        if (set[way].lru_counter < min_lru) {
            min_lru = set[way].lru_counter;
            replace_way = way;
        }
    }
    
    return replace_way;
}

/**
 * @brief Finds the way to replace using the FIFO policy.
 */
int cache_find_fifo_way(Cache *cache, uint32_t index) {
    Cache_Block *set = cache->blocks[index];
    int replace_way = 0;
    uint32_t min_timestamp = set[0].lru_counter; // For FIFO, this represents insertion time
    
    // First, look for invalid way
    for (int way = 0; way < cache->associativity; way++) {
        if (!set[way].valid) {
            return way;
        }
    }
     printf("[DEBUG] FIFO:evection\n"); 
    // If no invalid way, find the way with the smallest insertion timestamp
    // (oldest insertion = first to be replaced in FIFO)
    for (int way = 1; way < cache->associativity; way++) {
        if (set[way].lru_counter < min_timestamp) {
            min_timestamp = set[way].lru_counter;
            replace_way = way;
        }
    }
    
    return replace_way;
}

/**
 * @brief Finds the way to replace using the Random policy.
 */
int cache_find_random_way(Cache *cache, uint32_t index) {
    Cache_Block *set = cache->blocks[index];
   
    
    // First, look for invalid way
    for (int way = 0; way < cache->associativity; way++) {
        if (!set[way].valid) {
            return way;
        }
    }
     printf("[DEBUG] Random:evection\n"); 
    // If no invalid way, choose random way
    return rand() % cache->associativity;
}
/**
 * @brief Generic dispatcher to find a replacement way based on cache policy.
 */
int cache_find_replacement_way(Cache *cache, uint32_t index) {
    int way = -1;

    switch (cache->replacement_policy) {
        case REPLACEMENT_LRU:
            way= cache_find_lru_way(cache, index);
            break;
        case REPLACEMENT_FIFO:
            way= cache_find_fifo_way(cache, index); // Note: shares LRU logic
            break;
        case REPLACEMENT_RANDOM:
            way= cache_find_random_way(cache, index);
            break;
        default:
            way= cache_find_lru_way(cache, index); // Default to LRU
            break;
    }
    printf("[DEBUG] Replacement: set=%u, selected_way=%d (policy=%d)\n", index, way, cache->replacement_policy);
    return way;
}

/**
 * @brief Updates counters based on the insertion policy.
 */
void cache_update_insertion(Cache *cache, uint32_t index, int way) {
    Cache_Block *set = cache->blocks[index];
    
   switch (cache->replacement_policy) {
        case REPLACEMENT_LRU:
            switch (cache->insertion_policy) {
                case INSERTION_MRU:
                    // Normal LRU behavior - new block becomes MRU
                    set[way].lru_counter = ++cache->global_lru_counter;
                    printf("[DEBUG] Insertion: set=%u, way=%d, lru_counter=%u\n", index, way, set[way].lru_counter);
                    break;
                    
                case INSERTION_LRU:
                    // New block becomes LRU
                    set[way].lru_counter = 0;
                    // Increment all other valid counters
                    for (int i = 0; i < cache->associativity; i++) {
                        if (i != way && set[i].valid) {
                            set[i].lru_counter++;
                        }
                    }
                    break;
                    
                default:
                    set[way].lru_counter = ++cache->global_lru_counter;
                    break;
            }
            break;
            
        case REPLACEMENT_FIFO:
            // For FIFO, we only set insertion timestamp, never update on access
            set[way].lru_counter = ++cache->global_lru_counter;
            break;
            
        case REPLACEMENT_RANDOM:
            // Random replacement doesn't need counter updates
            // Just set a dummy value for consistency
            set[way].lru_counter = cache->global_lru_counter;
            break;
            
        default:
            set[way].lru_counter = ++cache->global_lru_counter;
            break;
    }printf("[DEBUG] Global LRU: %u\n", cache->global_lru_counter);
}
/**
 * @brief Prints cache statistics.
 */
void cache_print_stats(Cache *cache, const char* cache_name) {
    printf("%s Statistics:\n", cache_name);
    printf("  Accesses: %llu\n", cache->accesses);
    printf("  Hits: %llu\n", cache->hits);
    printf("  Misses: %llu\n", cache->misses);
    if (cache->accesses > 0) {
        printf("  Hit Rate: %.2f%%\n", (double)cache->hits / cache->accesses * 100.0);
        printf("  Miss Rate: %.2f%%\n", (double)cache->misses / cache->accesses * 100.0);
    }
    printf("\n");
}


/*==============================================================================
 * Pipeline Control
 *============================================================================*/

/**
 * @brief Initializes the pipeline state and caches.
 */
void pipe_init()
{
    srand(time(NULL)); // Seed random number generator for cache replacement
    memset(&pipe, 0, sizeof(Pipe_State));
    pipe.PC = 0x00400000;
    

    printf("Initializing caches...\n");
    
    /* Initialize caches */
    /* Instruction cache: 4-way, 8KB, 32-byte blocks */
    pipe.icache = cache_create(8*1024, 32, 4,REPLACEMENT_RANDOM, INSERTION_MRU);
	if (!pipe.icache) {
        fprintf(stderr, "Error: Failed to create instruction cache\n");
        exit(1);
    }
   
     

    
    /* Data cache: 8-way, 64KB, 32-byte blocks */
    pipe.dcache = cache_create(64*1024, 32, 4,REPLACEMENT_RANDOM, INSERTION_MRU);
     if (!pipe.dcache) {
        fprintf(stderr, "Error: Failed to create data cache\n");
        exit(1);
    }
   
    
    
    pipe.icache_stall = 0;
    pipe.dcache_stall = 0;

    printf("Cache initialization complete\n\n");
}

/**
 * @brief Simulates one clock cycle of the pipeline.
 */

void pipe_cycle()
{
   
    if (pipe.icache_stall > 1) {
        pipe.icache_stall--;
        return;
       
    }
    if (pipe.dcache_stall > 1) {
        pipe.dcache_stall--;
        return;
    }

    /* If stalled, do not advance pipeline stages */
   
#ifdef DEBUG
    printf("\n\n----\n\nPIPELINE:\n");
    printf("DCODE: "); print_op(pipe.decode_op);
    printf("EXEC : "); print_op(pipe.execute_op);
    printf("MEM  : "); print_op(pipe.mem_op);
    printf("WB   : "); print_op(pipe.wb_op);
    printf("\n");
#endif


    
    if(pipe.icache_stall == 0 && pipe.dcache_stall == 0){
        pipe_stage_wb();
        pipe_stage_mem();
        pipe_stage_execute();
        pipe_stage_decode();
        pipe_stage_fetch();
    }
    if(pipe.dcache_stall == 1){
        pipe_stage_mem();
        pipe_stage_execute();
        pipe_stage_decode();
        pipe_stage_fetch();
        pipe.dcache_stall = 0; // Reset dcache stall after processing
    }
     if(pipe.icache_stall == 1){
        pipe_stage_fetch();
        pipe.icache_stall = 0; // Reset icache stall after processing
    }
    if(pipe.icache_stall > 0 || pipe.dcache_stall > 0) {
        return;
    }


    /* handle branch recoveries */
    if (pipe.branch_recover) {
        printf("Entered branch recovery\n");
#ifdef DEBUG
        printf("branch recovery: new dest %08x flush %d stages\n", pipe.branch_dest, pipe.branch_flush);
#endif

        pipe.PC = pipe.branch_dest;

        if (pipe.branch_flush >= 2) {
            if (pipe.decode_op) free(pipe.decode_op);
            pipe.decode_op = NULL;
        }

        if (pipe.branch_flush >= 3) {
            if (pipe.execute_op) free(pipe.execute_op);
            pipe.execute_op = NULL;
        }

        if (pipe.branch_flush >= 4) {
            if (pipe.mem_op) free(pipe.mem_op);
            pipe.mem_op = NULL;
        }

        if (pipe.branch_flush >= 5) {
            if (pipe.wb_op) free(pipe.wb_op);
            pipe.wb_op = NULL;
        }

        pipe.branch_recover = 0;
        pipe.branch_dest = 0;
        pipe.branch_flush = 0;

        stat_squash++;
    }
}
/**
 * @brief Schedules a branch recovery (flush).
 */
void pipe_recover(int flush, uint32_t dest)
{
    if (pipe.branch_recover) return; // A recovery is already scheduled

    pipe.branch_recover = 1;
    pipe.branch_flush = flush;
    pipe.branch_dest = dest;
}

/*==============================================================================
 * Pipeline Stages
 *============================================================================*/

/**
 * @brief The Write-Back stage.
 */
void pipe_stage_wb()
{
     /* if there is no instruction in this pipeline stage, we are done */
    if (!pipe.wb_op)
        return;

    /* grab the op out of our input slot */
    Pipe_Op *op = pipe.wb_op;
    pipe.wb_op = NULL;

    /* if this instruction writes a register, do so now */
    if (op->reg_dst != -1 && op->reg_dst != 0) {
        pipe.REGS[op->reg_dst] = op->reg_dst_value;
#ifdef DEBUG
        printf("R%d = %08x\n", op->reg_dst, op->reg_dst_value);
#endif
    }

    /* if this was a syscall, perform action */
    if (op->opcode == OP_SPECIAL && op->subop == SUBOP_SYSCALL) {
        if (op->reg_src1_value == 0xA) {
            pipe.PC = op->pc; /* fetch will do pc += 4, then we stop with correct PC */
            RUN_BIT = 0;
        }
    }

    /* free the op */
    free(op);

    stat_inst_retire++;
}

/**
 * @brief The Memory stage.
 */
void pipe_stage_mem()
{
    /* if there is no instruction in this pipeline stage, we are done */
    if (!pipe.mem_op)
        return;

    /* grab the op out of our input slot */
    Pipe_Op *op = pipe.mem_op;

    uint32_t val = 0;
    if (op->is_mem) {
        /* Access data cache */
        int cache_hit;
        if (op->mem_write) {
            /* Store operation */
            uint32_t store_val = 0;
            switch (op->opcode) {
                case OP_SW:
                    store_val = op->mem_value;
                    break;
                case OP_SH:
                    /* Read-modify-write for partial word stores */
                    cache_hit = cache_access(pipe.dcache, op->mem_addr & ~3, &val, 0, 0);
                    if (!cache_hit) {
                        pipe.dcache_stall = 50;
                        return; /* Stall for cache miss */
                    }
                    if (op->mem_addr & 2)
                        store_val = (val & 0x0000FFFF) | (op->mem_value << 16);
                    else
                        store_val = (val & 0xFFFF0000) | (op->mem_value & 0xFFFF);
                    break;
                case OP_SB:
                    /* Read-modify-write for partial word stores */
                    cache_hit = cache_access(pipe.dcache, op->mem_addr & ~3, &val, 0, 0);
                    if (!cache_hit) {
                        pipe.dcache_stall = 50;
                        return; /* Stall for cache miss */
                    }
                    switch (op->mem_addr & 3) {
                        case 0: store_val = (val & 0xFFFFFF00) | ((op->mem_value & 0xFF) << 0); break;
                        case 1: store_val = (val & 0xFFFF00FF) | ((op->mem_value & 0xFF) << 8); break;
                        case 2: store_val = (val & 0xFF00FFFF) | ((op->mem_value & 0xFF) << 16); break;
                        case 3: store_val = (val & 0x00FFFFFF) | ((op->mem_value & 0xFF) << 24); break;
                    }
                    break;
            }
            
            cache_hit = cache_access(pipe.dcache, op->mem_addr & ~3, NULL, 1, store_val);
            if (!cache_hit) {
                pipe.dcache_stall = 50;
                return; /* Stall for cache miss */
            }
        } else {
            /* Load operation */
            cache_hit = cache_access(pipe.dcache, op->mem_addr & ~3, &val, 0, 0);
            if (!cache_hit) {
                pipe.dcache_stall = 50;
                return; /* Stall for cache miss */
            }
        }
    }

    switch (op->opcode) {
        case OP_LW:
        case OP_LH:
        case OP_LHU:
        case OP_LB:
        case OP_LBU:
            {
                /* extract needed value */
                op->reg_dst_value_ready = 1;
                if (op->opcode == OP_LW) {
                    op->reg_dst_value = val;
                }
                else if (op->opcode == OP_LH || op->opcode == OP_LHU) {
                    if (op->mem_addr & 2)
                        val = (val >> 16) & 0xFFFF;
                    else
                        val = val & 0xFFFF;

                    if (op->opcode == OP_LH)
                        val |= (val & 0x8000) ? 0xFFFF8000 : 0;

                    op->reg_dst_value = val;
                }
                else if (op->opcode == OP_LB || op->opcode == OP_LBU) {
                    switch (op->mem_addr & 3) {
                        case 0:
                            val = val & 0xFF;
                            break;
                        case 1:
                            val = (val >> 8) & 0xFF;
                            break;
                        case 2:
                            val = (val >> 16) & 0xFF;
                            break;
                        case 3:
                            val = (val >> 24) & 0xFF;
                            break;
                    }

                    if (op->opcode == OP_LB)
                        val |= (val & 0x80) ? 0xFFFFFF80 : 0;

                    op->reg_dst_value = val;
                }
            }
            break;

        case OP_SB:
        case OP_SH:
        case OP_SW:
            /* Store operations already handled above */
            break;
    }

    /* clear stage input and transfer to next stage */
    pipe.mem_op = NULL;
    pipe.wb_op = op;
}
/**
 * @brief The Execute stage.
 */
void pipe_stage_execute()
{
    /* if a multiply/divide is in progress, decrement cycles until value is ready */
    if (pipe.multiplier_stall > 0)
        pipe.multiplier_stall--;

    /* if downstream stall, return (and leave any input we had) */
    if (pipe.mem_op != NULL)
        return;

    /* if no op to execute, return */
    if (pipe.execute_op == NULL)
        return;

    /* grab op and read sources */
    Pipe_Op *op = pipe.execute_op;

    /* read register values, and check for bypass; stall if necessary */
    int stall = 0;
    if (op->reg_src1 != -1) {
        if (op->reg_src1 == 0)
            op->reg_src1_value = 0;
        else if (pipe.mem_op && pipe.mem_op->reg_dst == op->reg_src1) {
            if (!pipe.mem_op->reg_dst_value_ready)
                stall = 1;
            else
                op->reg_src1_value = pipe.mem_op->reg_dst_value;
        }
        else if (pipe.wb_op && pipe.wb_op->reg_dst == op->reg_src1) {
            op->reg_src1_value = pipe.wb_op->reg_dst_value;
        }
        else
            op->reg_src1_value = pipe.REGS[op->reg_src1];
    }
    if (op->reg_src2 != -1) {
        if (op->reg_src2 == 0)
            op->reg_src2_value = 0;
        else if (pipe.mem_op && pipe.mem_op->reg_dst == op->reg_src2) {
            if (!pipe.mem_op->reg_dst_value_ready)
                stall = 1;
            else
                op->reg_src2_value = pipe.mem_op->reg_dst_value;
        }
        else if (pipe.wb_op && pipe.wb_op->reg_dst == op->reg_src2) {
            op->reg_src2_value = pipe.wb_op->reg_dst_value;
        }
        else
            op->reg_src2_value = pipe.REGS[op->reg_src2];
    }

    /* if bypassing requires a stall (e.g. use immediately after load),
     * return without clearing stage input */
    if (stall) 
        return;

    /* execute the op */
    switch (op->opcode) {
        case OP_SPECIAL:
            op->reg_dst_value_ready = 1;
            switch (op->subop) {
                case SUBOP_SLL:
                    op->reg_dst_value = op->reg_src2_value << op->shamt;
                    break;
                case SUBOP_SLLV:
                    op->reg_dst_value = op->reg_src2_value << op->reg_src1_value;
                    break;
                case SUBOP_SRL:
                    op->reg_dst_value = op->reg_src2_value >> op->shamt;
                    break;
                case SUBOP_SRLV:
                    op->reg_dst_value = op->reg_src2_value >> op->reg_src1_value;
                    break;
                case SUBOP_SRA:
                    op->reg_dst_value = (int32_t)op->reg_src2_value >> op->shamt;
                    break;
                case SUBOP_SRAV:
                    op->reg_dst_value = (int32_t)op->reg_src2_value >> op->reg_src1_value;
                    break;
                case SUBOP_JR:
                case SUBOP_JALR:
                    op->reg_dst_value = op->pc + 4;
                    op->branch_dest = op->reg_src1_value;
                    op->branch_taken = 1;
                    break;

                case SUBOP_MULT:
                    {
                        /* we set a result value right away; however, we will
                         * model a stall if the program tries to read the value
                         * before it's ready (or overwrite HI/LO). Also, if
                         * another multiply comes down the pipe later, it will
                         * update the values and re-set the stall cycle count
                         * for a new operation.
                         */
                        int64_t val = (int64_t)((int32_t)op->reg_src1_value) * (int64_t)((int32_t)op->reg_src2_value);
                        uint64_t uval = (uint64_t)val;
                        pipe.HI = (uval >> 32) & 0xFFFFFFFF;
                        pipe.LO = (uval >>  0) & 0xFFFFFFFF;

                        /* four-cycle multiplier latency */
                        pipe.multiplier_stall = 4;
                    }
                    break;
                case SUBOP_MULTU:
                    {
                        uint64_t val = (uint64_t)op->reg_src1_value * (uint64_t)op->reg_src2_value;
                        pipe.HI = (val >> 32) & 0xFFFFFFFF;
                        pipe.LO = (val >>  0) & 0xFFFFFFFF;

                        /* four-cycle multiplier latency */
                        pipe.multiplier_stall = 4;
                    }
                    break;

                case SUBOP_DIV:
                    if (op->reg_src2_value != 0) {

                        int32_t val1 = (int32_t)op->reg_src1_value;
                        int32_t val2 = (int32_t)op->reg_src2_value;
                        int32_t div, mod;

                        div = val1 / val2;
                        mod = val1 % val2;

                        pipe.LO = div;
                        pipe.HI = mod;
                    } else {
                        // really this would be a div-by-0 exception
                        pipe.HI = pipe.LO = 0;
                    }

                    /* 32-cycle divider latency */
                    pipe.multiplier_stall = 32;
                    break;

                case SUBOP_DIVU:
                    if (op->reg_src2_value != 0) {
                        pipe.HI = (uint32_t)op->reg_src1_value % (uint32_t)op->reg_src2_value;
                        pipe.LO = (uint32_t)op->reg_src1_value / (uint32_t)op->reg_src2_value;
                    } else {
                        /* really this would be a div-by-0 exception */
                        pipe.HI = pipe.LO = 0;
                    }

                    /* 32-cycle divider latency */
					/* 32-cycle divider latency */
                    pipe.multiplier_stall = 32;
                    break;

                case SUBOP_MFHI:
                    /* stall until value is ready */
                    if (pipe.multiplier_stall > 0)
                        return;

                    op->reg_dst_value = pipe.HI;
                    break;
                case SUBOP_MTHI:
                    /* stall to respect WAW dependence */
                    if (pipe.multiplier_stall > 0)
                        return;

                    pipe.HI = op->reg_src1_value;
                    break;

                case SUBOP_MFLO:
                    /* stall until value is ready */
                    if (pipe.multiplier_stall > 0)
                        return;

                    op->reg_dst_value = pipe.LO;
                    break;
                case SUBOP_MTLO:
                    /* stall to respect WAW dependence */
                    if (pipe.multiplier_stall > 0)
                        return;

                    pipe.LO = op->reg_src1_value;
                    break;

                case SUBOP_ADD:
                case SUBOP_ADDU:
                    op->reg_dst_value = op->reg_src1_value + op->reg_src2_value;
                    break;
                case SUBOP_SUB:
                case SUBOP_SUBU:
                    op->reg_dst_value = op->reg_src1_value - op->reg_src2_value;
                    break;
                case SUBOP_AND:
                    op->reg_dst_value = op->reg_src1_value & op->reg_src2_value;
                    break;
                case SUBOP_OR:
                    op->reg_dst_value = op->reg_src1_value | op->reg_src2_value;
                    break;
                case SUBOP_NOR:
                    op->reg_dst_value = ~(op->reg_src1_value | op->reg_src2_value);
                    break;
                case SUBOP_XOR:
                    op->reg_dst_value = op->reg_src1_value ^ op->reg_src2_value;
                    break;
                case SUBOP_SLT:
                    op->reg_dst_value = ((int32_t)op->reg_src1_value <
                            (int32_t)op->reg_src2_value) ? 1 : 0;
                    break;
                case SUBOP_SLTU:
                    op->reg_dst_value = (op->reg_src1_value < op->reg_src2_value) ? 1 : 0;
                    break;
            }
            break;

        case OP_BRSPEC:
            switch (op->subop) {
                case BROP_BLTZ:
                case BROP_BLTZAL:
                    if ((int32_t)op->reg_src1_value < 0) op->branch_taken = 1;
                    break;

                case BROP_BGEZ:
                case BROP_BGEZAL:
                    if ((int32_t)op->reg_src1_value >= 0) op->branch_taken = 1;
                    break;
            }
            break;

        case OP_BEQ:
            if (op->reg_src1_value == op->reg_src2_value) op->branch_taken = 1;
            break;

        case OP_BNE:
            if (op->reg_src1_value != op->reg_src2_value) op->branch_taken = 1;
            break;

        case OP_BLEZ:
            if ((int32_t)op->reg_src1_value <= 0) op->branch_taken = 1;
            break;

        case OP_BGTZ:
            if ((int32_t)op->reg_src1_value > 0) op->branch_taken = 1;
            break;

        case OP_ADDI:
        case OP_ADDIU:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = op->reg_src1_value + op->se_imm16;
            break;
        case OP_SLTI:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = (int32_t)op->reg_src1_value < (int32_t)op->se_imm16 ? 1 : 0;
            break;
        case OP_SLTIU:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = (uint32_t)op->reg_src1_value < (uint32_t)op->se_imm16 ? 1 : 0;
            break;
        case OP_ANDI:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = op->reg_src1_value & op->imm16;
            break;
        case OP_ORI:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = op->reg_src1_value | op->imm16;
            break;
        case OP_XORI:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = op->reg_src1_value ^ op->imm16;
            break;
        case OP_LUI:
            op->reg_dst_value_ready = 1;
            op->reg_dst_value = op->imm16 << 16;
            break;

        case OP_LW:
        case OP_LH:
        case OP_LHU:
        case OP_LB:
        case OP_LBU:
            op->mem_addr = op->reg_src1_value + op->se_imm16;
            break;

        case OP_SW:
        case OP_SH:
        case OP_SB:
            op->mem_addr = op->reg_src1_value + op->se_imm16;
            op->mem_value = op->reg_src2_value;
            break;
    }

    /* handle branch recoveries at this point */
    if (op->branch_taken)
        pipe_recover(3, op->branch_dest);

    /* remove from upstream stage and place in downstream stage */
    pipe.execute_op = NULL;
    pipe.mem_op = op;
}

/**
 * @brief The Decode stage.
 */
void pipe_stage_decode()
{
    /* if downstream stall, return (and leave any input we had) */
    if (pipe.execute_op != NULL)
        return;

    /* if no op to decode, return */
    if (pipe.decode_op == NULL)
        return;

    /* grab op and remove from stage input */
    Pipe_Op *op = pipe.decode_op;
    pipe.decode_op = NULL;

    /* set up info fields (source/dest regs, immediate, jump dest) as necessary */
    uint32_t opcode = (op->instruction >> 26) & 0x3F;
    uint32_t rs = (op->instruction >> 21) & 0x1F;
    uint32_t rt = (op->instruction >> 16) & 0x1F;
    uint32_t rd = (op->instruction >> 11) & 0x1F;
    uint32_t shamt = (op->instruction >> 6) & 0x1F;
    uint32_t funct1 = (op->instruction >> 0) & 0x1F;
    uint32_t funct2 = (op->instruction >> 0) & 0x3F;
    uint32_t imm16 = (op->instruction >> 0) & 0xFFFF;
    uint32_t se_imm16 = imm16 | ((imm16 & 0x8000) ? 0xFFFF8000 : 0);
    uint32_t targ = (op->instruction & ((1UL << 26) - 1)) << 2;

    op->opcode = opcode;
    op->imm16 = imm16;
    op->se_imm16 = se_imm16;
    op->shamt = shamt;

    switch (opcode) {
        case OP_SPECIAL:
            /* all "SPECIAL" insts are R-types that use the ALU and both source
             * regs. Set up source regs and immediate value. */
            op->reg_src1 = rs;
            op->reg_src2 = rt;
            op->reg_dst = rd;
            op->subop = funct2;
            if (funct2 == SUBOP_SYSCALL) {
                op->reg_src1 = 2; // v0
                op->reg_src2 = 3; // v1
            }
            if (funct2 == SUBOP_JR || funct2 == SUBOP_JALR) {
                op->is_branch = 1;
                op->branch_cond = 0;
            }

            break;

        case OP_BRSPEC:
            /* branches that have -and-link variants come here */
            op->is_branch = 1;
            op->reg_src1 = rs;
            op->reg_src2 = rt;
            op->is_branch = 1;
            op->branch_cond = 1; /* conditional branch */
            op->branch_dest = op->pc + 4 + (se_imm16 << 2);
            op->subop = rt;
            if (rt == BROP_BLTZAL || rt == BROP_BGEZAL) {
                /* link reg */
                op->reg_dst = 31;
                op->reg_dst_value = op->pc + 4;
                op->reg_dst_value_ready = 1;
            }
            break;

        case OP_JAL:
            op->reg_dst = 31;
            op->reg_dst_value = op->pc + 4;
            op->reg_dst_value_ready = 1;
            op->branch_taken = 1;
            /* fallthrough */
        case OP_J:
            op->is_branch = 1;
            op->branch_cond = 0;
            op->branch_taken = 1;
            op->branch_dest = (op->pc & 0xF0000000) | targ;
			 
            break;

        case OP_BEQ:
        case OP_BNE:
        case OP_BLEZ:
        case OP_BGTZ:
            /* ordinary conditional branches (resolved after execute) */
            op->is_branch = 1;
            op->branch_cond = 1;
            op->branch_dest = op->pc + 4 + (se_imm16 << 2);
            op->reg_src1 = rs;
            op->reg_src2 = rt;
            break;

        case OP_ADDI:
        case OP_ADDIU:
        case OP_SLTI:
        case OP_SLTIU:
            /* I-type ALU ops with sign-extended immediates */
            op->reg_src1 = rs;
            op->reg_dst = rt;
            break;

        case OP_ANDI:
        case OP_ORI:
        case OP_XORI:
        case OP_LUI:
            /* I-type ALU ops with non-sign-extended immediates */
            op->reg_src1 = rs;
            op->reg_dst = rt;
            break;

        case OP_LW:
        case OP_LH:
        case OP_LHU:
        case OP_LB:
        case OP_LBU:
        case OP_SW:
        case OP_SH:
        case OP_SB:
            /* memory ops */
            op->is_mem = 1;
            op->reg_src1 = rs;
            if (opcode == OP_LW || opcode == OP_LH || opcode == OP_LHU || opcode == OP_LB || opcode == OP_LBU) {
                /* load */
                op->mem_write = 0;
                op->reg_dst = rt;
            }
            else {
                /* store */
                op->mem_write = 1;
                op->reg_src2 = rt;
            }
            break;
    }

    /* we will handle reg-read together with bypass in the execute stage */

    /* place op in downstream slot */
    pipe.execute_op = op;
}

/**
 * @brief The Fetch stage.
 */
void pipe_stage_fetch()
{
      /* if pipeline is stalled (our output slot is not empty), return */
    if (pipe.decode_op != NULL)
        return;

    /* Validate PC alignment */
    if (pipe.PC & 0x3) {
        fprintf(stderr, "Error: Unaligned PC: 0x%08x\n", pipe.PC);
        return;
    }
    /* Access instruction cache */
    uint32_t instruction;
    int cache_hit = cache_access(pipe.icache, pipe.PC, &instruction, 0, 0);

    /* Handle cache miss */ 
    if (!cache_hit) {
        printf("Cache miss at PC: %08x\n", pipe.PC);
        /* Set stall counter for a 50-cycle penalty */
        pipe.icache_stall = 50; 
        
        /* Do not advance PC or send an op down the pipeline.
         * The fetch will be retried with the same PC after the stall. */
      return;
    }

    /* On a cache hit, proceed as normal */
    Pipe_Op *op = malloc(sizeof(Pipe_Op));
    if (!op) {
        fprintf(stderr, "Error: Failed to allocate Pipe_Op\n");
        return;
    }
    memset(op, 0, sizeof(Pipe_Op));
    op->reg_src1 = op->reg_src2 = op->reg_dst = -1;

    op->instruction = instruction; // Use the instruction fetched from the cache
    printf("Fetched instruction: %08x\n", instruction);
    op->pc = pipe.PC;
    pipe.decode_op = op;

    /* update PC for the next instruction */
    pipe.PC += 4;

    stat_inst_fetch++;
}