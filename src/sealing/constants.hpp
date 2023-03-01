// Copyright Supranational LLC

#ifndef __CONSTANTS_HPP__
#define __CONSTANTS_HPP__

#include <cstddef>       // size_t
#include <cstdint>       // uint*
#include <cmath>         // log2
#include <atomic>

///////////////////////////////////////////////////////////
// Graph constants
///////////////////////////////////////////////////////////

//const size_t SECTOR_SIZE_LG    = 11; // 2K
//const size_t SECTOR_SIZE_LG    = 24; // 16MB
//const size_t SECTOR_SIZE_LG    = 29; // 512MB
const size_t SECTOR_SIZE_LG    = 35; // 32GB

// NOTE: currently only works for 2KB sector
const size_t SECTOR_SIZE       = (1UL << SECTOR_SIZE_LG);
//const size_t SECTOR_SIZE       = (1UL << 20); // 1M
//const size_t SECTOR_SIZE       = (1UL << 20) * 512; // 512M
//const size_t SECTOR_SIZE       = (1UL << 30); // 1GB - 5s (16s linear)
//const size_t SECTOR_SIZE       = (1UL << 30) * 4; // 4GB - 38s (65s linear)
//const size_t SECTOR_SIZE       = (1UL << 30) * 8; // 8GB - 100s (128s linear)
//const size_t SECTOR_SIZE       = (32UL << 30); // 32GB - 8:34
const uint32_t LAYER_COUNT     = 11;  // Layers in graph (usually 2 or 11)

const size_t NODE_SIZE_LG      = 5; // In Bytes. SHA-256 digest size
const size_t NODE_SIZE         = (1 << NODE_SIZE_LG); // In Bytes. SHA-256 digest size
const size_t NODE_WORDS        = NODE_SIZE / sizeof(uint32_t);
const size_t NODE_COUNT        = SECTOR_SIZE / NODE_SIZE;

const size_t PARENT_COUNT_BASE = 6;  // Number of parents from same layer 
const size_t PARENT_COUNT_EXP  = 8;  // Number of parents from previous layer 
const size_t PARENT_COUNT      = PARENT_COUNT_BASE + PARENT_COUNT_EXP;
const size_t PARENT_SIZE       = sizeof(uint32_t);

const size_t NODE_0_REPEAT      = 1;
const size_t NODE_0_BLOCKS      = 2;
const size_t LAYER_1_REPEAT     = 3;
const size_t LAYERS_GT_1_REPEAT = 7;
const size_t NODE_GT_0_BLOCKS   = 20;

const size_t COMM_D_DEPTH      = log2(NODE_COUNT);

// Full padding block for the hash buffer of node 0 in each layer
const uint8_t NODE_0_PADDING[]    = {
  0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00
};

// Half padding block for the hash buffer of non node 0 in each layer
const uint8_t NODE_PADDING_X2[]    = {
  0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x27, 0x00, 0x00
};

///////////////////////////////////////////////////////////
// Application constants
///////////////////////////////////////////////////////////

// comm_d for a CC sector, always the same since CC data is all zeroes
// const uint8_t CC_COMM_D[]    = {
//   0xfc, 0x7e, 0x92, 0x82, 0x96, 0xe5, 0x16, 0xfa,
//   0xad, 0xe9, 0x86, 0xb2, 0x8f, 0x92, 0xd4, 0x4a,
//   0x4f, 0x24, 0xb9, 0x35, 0x48, 0x52, 0x23, 0x37,
//   0x6a, 0x79, 0x90, 0x27, 0xbc, 0x18, 0xf8, 0x33
// };
// CommD for 16M
const uint8_t CC_COMM_D[]    = {
  0xa2, 0x24, 0x75, 0x8, 0x28, 0x58, 0x50, 0x96,
  0x5b, 0x7e, 0x33, 0x4b, 0x31, 0x27, 0xb0, 0xc0,
  0x42, 0xb1, 0xd0, 0x46, 0xdc, 0x54, 0x40, 0x21,
  0x37, 0x62, 0x7c, 0xd8, 0x79, 0x9c, 0xe1, 0x3a
};

// Number of sectors being processed within the buffers.
// Used to determine stride between parents in node_buffer
// A good value here would be a multiple that fits in a 4KB page
// 128: 32B * 128    = 4096  Page consumed by single node index
// 64:  32B * 64 * 2 = 4096  Page consumed by two node indices
// 32:  32B * 32 * 4 = 4096  Page consumed by four node indices
// 16:  32B * 16 * 8 = 4096  Page consumed by eight node indices
// What we are trying to accomplish with this is to improve the efficiency of
//  random reads. Typcially when reading distant base parents and all 
//  expander parents an entire page needs to be read to get only a single 32B
//  node.  If this is done across many sealing threads, then the read
//  efficiency is not good. With the interleaved approach the goal is for all
//  4KB page data to be useful.  This can reduce the number of system level
//  read operations by the interleaved node factor.
// Must evenly fit in the page!

// **** NOTE: When changing this must also change NEXT_NODE in sha_ext_mbx2.asm!!! ****
const size_t PARALLEL_SECTORS  = 64;
//const size_t PARALLEL_SECTORS  = 128;

// Smallest unit of memory we are working with. A page is typically 4KB
const size_t T_PAGE_SIZE       = 4096;

// Number of nodes stored per page (packed)
const size_t NODES_PER_PAGE    = T_PAGE_SIZE / (PARALLEL_SECTORS * NODE_SIZE);
// Number of pages per layer
const size_t PAGES_PER_LAYER   = NODE_COUNT / NODES_PER_PAGE;

const size_t NODES_PER_HASHER  = 2;   // Number of nodes to calculate per hashing thread
const size_t HASHERS_PER_CORE  = 2;   // Number of hashing threads per core

// Number of NVME controllers for layer storage
const size_t NUM_CONTROLLERS   = 12;
// Number of pages per layer per controller when striped (rounded up)
const size_t PAGES_PER_LAYER_PER_CONTROLLER = ((PAGES_PER_LAYER + NUM_CONTROLLERS - 1) /
                                               NUM_CONTROLLERS);

/////////////////////////////////////////////////////////
// Constants solely for testing, these will be inputs
/////////////////////////////////////////////////////////
// prover_id will be the miner's id
const uint8_t PROVER_ID[] = { 9, 9, 9, 9, 9, 9, 9, 9,
                              9, 9, 9, 9, 9, 9, 9, 9,
                              9, 9, 9, 9, 9, 9, 9, 9,
                              9, 9, 9, 9, 9, 9, 9, 9 };

// sector_id will be from the miner and must be a new value for them
//const uint8_t SECTOR_ID[] = { 0, 0, 0, 0, 0, 0, 250, 206 };
const uint8_t SECTOR_ID[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

// ticket comes from on-chain randomness
const uint8_t TICKET[]    = { 1, 1, 1, 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1, 1, 1, 1,
                              1, 1, 1, 1, 1, 1, 1, 1 };

// porep_seed is the config porep_id
// Mostly 0's with a single byte indicating the sector size and api version
// See porep_id() in rust-filecoin-proofs-api/src/registry.rs
const uint8_t POREP_SEED[] = { 99, 99, 99, 99, 99, 99, 99, 99,
                               99, 99, 99, 99, 99, 99, 99, 99,
                               99, 99, 99, 99, 99, 99, 99, 99,
                               99, 99, 99, 99, 99, 99, 99, 99 };

// Default ordering for atomics
static const std::memory_order DEFAULT_MEMORY_ORDER = std::memory_order_seq_cst;
//static const std::memory_order DEFAULT_MEMORY_ORDER = std::memory_order_relaxed;


/////////////////////////////////////////////////////////
// Constants for buffer sizing
/////////////////////////////////////////////////////////

// Operations between threads are batched for efficient coordination.
// BATCH_SIZE must be a multiple of PARENT_COUNT or PARENT_COUNT - 1
// TODO: BATCH_MULTIPLIER must be 1 at the moment
const size_t BATCH_MULTIPLIER = 1;
// Parent pointers contain pointers to all 14 parents
const size_t PARENT_PTR_BATCH_SIZE = PARENT_COUNT * BATCH_MULTIPLIER;
// We only need 13 nodes from disk/cache since 1 was always just hashed
const size_t PAGE_BATCH_SIZE = (PARENT_COUNT - 1) * BATCH_MULTIPLIER;

// Parents buffer stores base and expander parent values that are not located
//  in the node buffer. The intention is the parents buffer is filled with
//  values read from disk. The first parent is not required since it is the
//  previous node and we know for sure it is in the node buffer.
const size_t PARENT_BUFFER_BATCHES = 1<<19;
const size_t PARENT_BUFFER_NODES = PARENT_BUFFER_BATCHES * PAGE_BATCH_SIZE;

// Number of hashing buffers allocated in memory.
// Once hit then the buffer wraps around to the beginning
// This needs to be tuned based on available RAM and timing required to keep
//  hashing threads and reading threads in sync.
// Should be sized parent buffer + desired cache nodes
const size_t NODE_BUFFER_BATCHES = PARENT_BUFFER_BATCHES * 2;
const size_t NODE_BUFFER_NODES = NODE_BUFFER_BATCHES * BATCH_MULTIPLIER;
const size_t NODE_BUFFER_SYNC_LG_BATCH_SIZE = 2;
const size_t NODE_BUFFER_SYNC_BATCH_SIZE = 1<<NODE_BUFFER_SYNC_LG_BATCH_SIZE;
const size_t NODE_BUFFER_SYNC_BATCH_MASK = NODE_BUFFER_SYNC_BATCH_SIZE - 1;
const size_t NODE_BUFFER_SYNC_BATCHES = NODE_BUFFER_BATCHES / NODE_BUFFER_SYNC_BATCH_SIZE;
#define NODE_IDX_TO_SYNC_IDX(node) ((node) >> NODE_BUFFER_SYNC_LG_BATCH_SIZE)

// The coordinator will create a contiguous block of memory where it
// copies the data from the parent pointers so the hashers can simply
// walk through it. There will be COORD_BATCH_COUNT batches, each of
// size COORD_BATCH_SIZE. Further we guarantee all of these nodes will
// be present in the node buffer so we won't bother with
// reference/consumed counts.
static const size_t COORD_BATCH_SIZE = 256;
static const size_t COORD_BATCH_COUNT = 4;
static const size_t COORD_BATCH_NODE_COUNT = COORD_BATCH_SIZE * COORD_BATCH_COUNT;

#endif // __CONSTANTS_HPP__
