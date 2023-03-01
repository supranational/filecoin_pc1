// Copyright Supranational LLC

#ifndef __TOPOLOGY_T_HPP__
#define __TOPOLOGY_T_HPP__

#include "../util/thread_pool_t.hpp"

// This is used to size buffers. There will be some waste in cases where there are
// fewer hashers per coordinator but it is minimal. This should be sized according to
// the number of hashers that can run in a CCX.
const size_t MAX_HASHERS_PER_COORD = 14;

class topology_t {
public:
  // TODO: Better way to get physical cores?
  static int get_physical_cores() {
    return std::thread::hardware_concurrency() / 2;
  }

  struct coordinator_t {
    int core;
    size_t num_hashers;

    size_t num_sectors() {
      return num_hashers * NODES_PER_HASHER;
    }

    int get_hasher_core(size_t i) {
      if (HASHERS_PER_CORE == 1) {
        return core + 1 + i;
      }

      if (i & 0x1) {
        // Odd hasher, it's on the hyperthread
        return core + 1 + i / HASHERS_PER_CORE + get_physical_cores();
      }
      return core + 1 + i / HASHERS_PER_CORE;
    }
  };
  
  coordinator_t* coordinators;

  topology_t(coordinator_t* _coordinators) {
    // Only support 1 or 2 hashers per core. The logic below assumes 
    assert(HASHERS_PER_CORE == 1 || HASHERS_PER_CORE == 2);

    coordinators = _coordinators;
    assert (coordinators != nullptr);

    for (size_t count = 0; count < num_coordinators(); count++) {
      assert (coordinators[count].num_hashers <= MAX_HASHERS_PER_COORD);
    }
    assert (num_sectors() <= PARALLEL_SECTORS);
  }

  size_t num_coordinators() {
    size_t count;
    for (count = 0; coordinators[count].num_hashers != 0; count++) {}
    return count;
  }

  int get_coordinator_core(size_t i) {
    return coordinators[i].core;
  }
  
  size_t num_hashers() {
    size_t count = 0;
    for (size_t i = 0; i < num_coordinators(); i++) {
      count += coordinators[i].num_hashers;
    }
    return count;
  }

  size_t num_sectors() {
    return num_hashers() * NODES_PER_HASHER;
  }

  size_t num_hashing_cores() {
    return num_coordinators() + (num_hashers() + HASHERS_PER_CORE - 1) / HASHERS_PER_CORE;
  }
  
  void print() {
    printf("Num coordinators:  %ld\n", num_coordinators());
    printf("Num hashers:       %ld\n", num_hashers());
    printf("Num sectors:       %ld\n", num_sectors());
    printf("Num hashing cores: %ld\n", num_hashing_cores());
    printf("core   process0      HT   process1\n");
    size_t sector = 0;
    for (size_t i = 0; i < num_coordinators(); i++) {
      printf("%2d     coord%-2ld\n", coordinators[i].core, i);
      for (size_t j = 0; j < coordinators[i].num_hashers; j++) {
        if (HASHERS_PER_CORE == 1 || j == coordinators[i].num_hashers - 1) {
          printf("%2d      %2ld,%2ld        %2d\n",
                 coordinators[i].get_hasher_core(j),
                 sector, sector + 1,
                 coordinators[i].get_hasher_core(j) + get_physical_cores());
          sector += 2;
        } else {
          printf("%2d      %2ld,%2ld        %2d     %2ld,%2ld\n",
                 coordinators[i].get_hasher_core(j),
                 sector, sector + 1,
                 coordinators[i].get_hasher_core(j + 1),
                 sector + 2, sector + 3);
          j++;
          sector += 4;
        }
      }
    }    
  }
};

// 64 sectors
const size_t NUM_HASHING_COORDS = 3;
topology_t::coordinator_t coordinators[NUM_HASHING_COORDS + 1] =
  { { 3, 8 },
    { 8, 14 },
    { 16, 10 },
    { 0, 0 }  };

// const size_t NUM_HASHING_COORDS = 5;
// topology_t::coordinator_t coordinators[NUM_HASHING_COORDS + 1] =
//   { { 3, 8 },
//     { 8, 6 },
//     { 12, 6 },
//     { 16, 6 },
//     { 20, 6 },
//     { 0, 0 }  };

// 32 sectors
// const size_t NUM_HASHING_COORDS = 2;
// topology_t::coordinator_t coordinators[NUM_HASHING_COORDS + 1] =
//   { { 3, 8 },
//     { 8, 8 },
//     { 0, 0 } };

// 2 sectors
// const size_t NUM_HASHING_COORDS = 1;
// topology_t::coordinator_t coordinators[NUM_HASHING_COORDS + 1] =
//   { { 3, 1 },
//     { 0, 0 } };

// 4 sectors
// const size_t NUM_HASHING_COORDS = 1;
// topology_t::coordinator_t coordinators[NUM_HASHING_COORDS + 1] =
//   { { 3, 2 },
//     { 0, 0 } };

#endif
