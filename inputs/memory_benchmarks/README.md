# Memory-Intensive Benchmarks

This directory contains a set of benchmarks designed to test cache performance with different memory access patterns. These benchmarks use significant amounts of memory to evaluate how well the cache handles various access patterns.

## Benchmark Descriptions

### 1. large_streaming.s
- **Pattern**: Sequential streaming access
- **Memory Size**: 64KB (16,384 words)
- **Characteristics**: 
  - Accesses memory sequentially from start to end
  - Should achieve high cache hit rates due to spatial locality
  - Tests cache line utilization and prefetching effectiveness

### 2. random_access.s
- **Pattern**: Pseudo-random access
- **Memory Size**: 32KB (8,192 words)
- **Characteristics**:
  - Uses linear congruential generator for pseudo-random indices
  - Poor spatial and temporal locality
  - Tests cache replacement policy effectiveness
  - Expected to have high miss rates

### 3. strided_access.s
- **Pattern**: Strided access with 16-word stride
- **Memory Size**: 64KB (16,384 words)
- **Characteristics**:
  - Accesses every 16th word (64-byte stride)
  - Tests cache performance with non-unit strides
  - May cause cache conflicts depending on cache size and associativity

### 4. matrix_transpose.s
- **Pattern**: Matrix transpose operation
- **Memory Size**: 16KB (4,096 words for 64x64 matrix)
- **Characteristics**:
  - Demonstrates poor cache locality for matrix operations
  - Accesses memory in row-major order but stores in column-major order
  - Classic example of cache-unfriendly access pattern

### 5. cache_thrashing.s
- **Pattern**: Cache thrashing with 256-word stride
- **Memory Size**: 128KB (32,768 words)
- **Characteristics**:
  - Uses stride designed to cause cache conflicts
  - Tests cache associativity and replacement policy
  - May cause high miss rates due to capacity and conflict misses

### 6. mixed_patterns.s
- **Pattern**: Combination of streaming, random, and strided access
- **Memory Size**: 64KB (16,384 words)
- **Characteristics**:
  - Phase 1: Streaming access (first 4K elements)
  - Phase 2: Random access (next 4K elements)
  - Phase 3: Strided access (remaining elements)
  - Tests cache performance under mixed workloads

## Expected Cache Performance

### High Hit Rate Benchmarks:
- **large_streaming**: Should achieve >90% hit rate due to excellent spatial locality

### Medium Hit Rate Benchmarks:
- **strided_access**: Hit rate depends on cache size and stride
- **mixed_patterns**: Variable hit rate depending on phase

### Low Hit Rate Benchmarks:
- **random_access**: Expected <50% hit rate due to poor locality
- **matrix_transpose**: Poor hit rate due to cache-unfriendly access pattern
- **cache_thrashing**: Low hit rate due to intentional cache conflicts

## Usage

To run these benchmarks with the cache simulator:

```bash
# Run a specific benchmark
./sim inputs/memory_benchmarks/large_streaming.x

# Run all memory benchmarks
./sim inputs/memory_benchmarks/*.x
```

## Cache Configuration

These benchmarks are designed to work with the following cache configuration:
- Instruction Cache: 8KB, 4-way associative, 32-byte blocks
- Data Cache: 64KB, 8-way associative, 32-byte blocks

The benchmarks will demonstrate different performance characteristics based on:
- Cache size vs. working set size
- Associativity vs. access patterns
- Block size vs. spatial locality
- Replacement policy effectiveness 