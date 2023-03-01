// Copyright Supranational LLC

#include <vector>
#include <deque>
#include <fstream>       // file read
#include <iostream>      // printing
#include <cstring>
#include <arpa/inet.h> // htonl

// Enable profiling
//#define PROFILE

// Enable data collection in the orchestrator using the timestamp counter
//#define TSC

// Enable data collection in the hasher using the timestamp counter
//#define HASHER_TSC

// Enable more general statistics collection
//#define STATS

// Disable reading parents from disk (will not produce the correct result)
//#define NO_DISK_READS

// Print a message if the orchestrator is stalled for too long
//#define PRINT_STALLS

// Verify that hashed result matches a known good sealing
//#define VERIFY_HASH_RESULT   

#include "sealing.hpp"
#include "../util/util.hpp"

#ifdef PROFILE
#include <gperftools/profiler.h>
#endif

// 2KB
//const char *parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-652bae61e906c0732e9eb95b1217cfa6afcce221ff92a8aedf62fa778fa765bc.cache";
// 16MB
//const char *parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-7fa3ff8ffb57106211c4be413eb15ea072ebb363fa5a1316fe341ac8d7a03d51.cache";
// 512MB
//const char *parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-7ba215a1d2345774ab90b8cb1158d296e409d6068819d7b8c7baf0b25d63dc34.cache"; // old
//const char *parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-016f31daba5a32c5933a4de666db8672051902808b79d51e9b97da39ac9981d3.cache";
// 32GB
const char *parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-55c7d1e6bb501cc8be94438f89b577fddda4fafa71ee9ca72eabe2f0265aefa6.cache";

// Mutex to keep printouts contiguous
mutex print_mtx;

int g_spdk_error = 0;

int main(int argc, char** argv) {
  int rc;

  uint64_t node_to_read = 0;
  uint64_t block_offset = 0;
  
  enum { HASH_MODE, READ_MODE, PARENTS_MODE } mode = HASH_MODE;
  int opt;
  while ((opt = getopt(argc, argv, "hr:p:o:")) != -1) {
    switch (opt) {
    case 'h':
      mode = HASH_MODE;
      break;
    case 'r':
      mode = READ_MODE;
      node_to_read = strtol(optarg, NULL, 16);
      break;
    case 'p':
      mode = PARENTS_MODE;
      node_to_read = strtol(optarg, NULL, 16);
      break;
    case 'o':
      block_offset = strtol(optarg, NULL, 16);
      break;
    }
  }
  printf("mode %d, node_to_read %lx, block_offset %lx\n",
         mode, node_to_read, block_offset);
  node_to_read += block_offset;

  // Read and print the parents for the given node
  if (mode == PARENTS_MODE) {
    printf("Opening parents file file %s\n", parents_cache_filename);
    int parents_fd = open(parents_cache_filename, O_RDONLY);
    assert (parents_fd != -1);
    uint32_t* parents_buf = (uint32_t*)mmap(NULL, parent_iter_t::bytes(), PROT_READ, MAP_PRIVATE, parents_fd, 0);
    if (parents_buf == MAP_FAILED) {
      perror("mmap failed for parents file");
      exit(1);
    }
    printf("parents for node %lx\n", node_to_read);
    for (size_t i = 0; i < PARENT_COUNT; i++) {
      printf("%2ld: %08x\n", i, parents_buf[node_to_read * PARENT_COUNT + i]);
    }
    
    exit(0);
  }

  // Initialize SPDK
  struct spdk_env_opts opts;
  spdk_env_opts_init(&opts);
  opts.name = "nvme";
  rc = spdk_env_init(&opts);
  if (rc < 0) {
    fprintf(stderr, "Unable to initialize SPDK env\n");
    return 1;
  }

  // Construct a set of NVMEs to use for sealing
  set<string> allowed_nvme;
  // WD black
  allowed_nvme.insert("0000:2a:00.0");
  allowed_nvme.insert("0000:2b:00.0");
  allowed_nvme.insert("0000:2c:00.0");
  allowed_nvme.insert("0000:2d:00.0");
  allowed_nvme.insert("0000:61:00.0");
  allowed_nvme.insert("0000:62:00.0");
  allowed_nvme.insert("0000:63:00.0");
  allowed_nvme.insert("0000:64:00.0");
  // Samsung
  allowed_nvme.insert("0000:01:00.0");
  allowed_nvme.insert("0000:02:00.0");
  allowed_nvme.insert("0000:03:00.0");
  allowed_nvme.insert("0000:04:00.0");
  
  assert (allowed_nvme.size() == NUM_CONTROLLERS);

  topology_t topology(coordinators);
  printf("\nTopology\n");
  topology.print();
  printf("\n");
  
  nvme_controllers_t controllers(allowed_nvme);
  controllers.init(2); // qpairs
  controllers.sort();
  
  printf("Layer disks\n");
  for (size_t i = 0; i < controllers.size(); i++) {
    printf("  %s\n", controllers[i].get_name().c_str());
  }
  if (controllers.size() != NUM_CONTROLLERS) {
    printf("ERROR: only %ld controllers found, expected %ld\n",
           controllers.size(), NUM_CONTROLLERS);
    exit(1);
  }

  print_parameters();
  
  thread_pool_t pool(3 + NUM_HASHING_COORDS);
  atomic<bool> terminator(false);

  node_id_t node_start = NODE_COUNT * 0;
  //node_id_t node_stop(NODE_COUNT * 1 + NODE_COUNT / 32);
  node_id_t node_stop(NODE_COUNT * 11);

  printf("Hashing node %lx to node %lx\n", node_start.id(), node_stop.id());

  // Perform sealing
  if (mode == HASH_MODE) {
    system_buffers_t system;
    SPDK_ERROR(system.init(controllers.size()));

    // Parent reader
    node_rw_t parent_reader(terminator, controllers, system.parent_read_fifo, 0, block_offset);
    SPDK_ERROR(parent_reader.init());
    system.parent_reader = &parent_reader;

    // Node writer
    node_rw_t node_writer(terminator, controllers, system.node_write_fifo, 1, block_offset);
    SPDK_ERROR(node_writer.init());

    // Orchestrator
    orchestrator_t orchestrator(terminator, system, node_start, node_stop, parents_cache_filename);
    SPDK_ERROR(orchestrator.init());
    system.orchestrator = &orchestrator;
    
#ifdef PROFILE
    ProfilerStart("nvme.prof");
#endif
    
    // Replica ID hashing buffer for all sectors
    replica_id_buffer_t replica_id_buffer __attribute__ ((aligned (4096)));
    std::memset(&replica_id_buffer, 0, sizeof(replica_id_buffer_t));

    // Canned IDs for testing
    // 16MB
    // [0, 68, b, b7, c6, 97, b4, b3, bb, 2e, e9, dd, f4, 2c, cc, dd, 6c, e, 11, 31, fe, e5, e5, fc, c8, 66, 10, f0, 54, af, dc, 34])
    // Replica ID
    // 00680bb7 c697b4b3 bb2ee9dd f42cccdd 6c0e1131 fee5e5fc c86610f0 54afdc34 
    
    // 512MB
    // uint8_t replica_id_buf[] = {
    //   37, 249, 121, 174, 70, 206, 91, 232,
    //   165, 246, 66, 184, 198, 10, 232, 126,
    //   215, 171, 221, 76, 26, 2, 117, 118,
    //   201, 142, 116, 143, 25, 131, 167, 37
    // };
    // 32GB
    uint8_t replica_id_buf[] = {
      229, 91, 17, 249, 156, 151, 42, 202,
      166, 244, 38, 151, 243, 192, 151, 186,
      160, 136, 174, 126, 102, 91, 130, 181,
      24, 181, 140, 93, 251, 38, 207, 37
    };
    // Byte reverse
    uint32_t *replica_id_32 = (uint32_t*)replica_id_buf;
    for (size_t i = 0; i < 8; i++) {
      replica_id_32[i] = htonl(replica_id_32[i]);
    }
    
    for (size_t i = 0; i < NODES_PER_HASHER; ++i) {
      // TODO - PROVER_ID, SECTOR_ID, and TICKET should be inputs to the app
      // create_replica_id(&(replica_id_buffer.ids[i][0]), PROVER_ID, SECTOR_ID,
      //                   TICKET, CC_COMM_D, POREP_SEED);
      memcpy(&(replica_id_buffer.ids[i][0]), replica_id_buf, sizeof(replica_id_buf));
      replica_id_buffer.pad_0[i][0]  = 0x80000000; // byte 67
      replica_id_buffer.pad_1[i][7]  = 0x00000200; // byte 125
      replica_id_buffer.padding[i][0]  = 0x80000000; // byte 67
      replica_id_buffer.padding[i][7]  = 0x00002700; // byte 125
    }
    std::cout << "Replica ID" << std::endl;
    print_digest(&(replica_id_buffer.ids[0][0]));
    print_digest(&(replica_id_buffer.cur_loc[0][0]));
    print_digest(&(replica_id_buffer.pad_0[0][0]));
    print_digest(&(replica_id_buffer.pad_1[0][0]));
    print_digest(&(replica_id_buffer.padding[0][0]));
    std::cout << std::endl;
    
    channel_t<size_t> ch;
    pool.spawn([&]() {
      size_t core_num = 1;
      printf("Setting affinity for rw handler to core %ld\n", core_num);
      set_core_affinity(core_num);
      assert(parent_reader.process() == 0);
      ch.send(0);
    });
    pool.spawn([&]() {
      size_t core_num = 0;
      printf("Setting affinity for node_writer to core %ld\n", core_num);
      set_core_affinity(core_num);
      assert(node_writer.process() == 0);
      ch.send(0);
    });

    size_t sector = 0;
    for (size_t coord_id = 0; coord_id < topology.num_coordinators(); coord_id++) {
      size_t core_num = topology.get_coordinator_core(coord_id);
      printf("Setting affinity for hasher %ld to core %ld\n", coord_id, core_num);
      pool.spawn([&, coord_id, core_num, sector]() {
        set_core_affinity(core_num);
        
        vector<replica_id_buffer_t> replica_ids;
        replica_ids.resize(topology.coordinators[coord_id].num_hashers);
        for (size_t j = 0; j < topology.coordinators[coord_id].num_hashers; j++) {
          replica_ids[j] = replica_id_buffer;
        }
        
        coordinator_t coordinator(terminator, system,
                                  coord_id, sector, topology.coordinators[coord_id],
                                  node_start, node_stop,
                                  replica_ids);
        system.coordinators[coord_id] = &coordinator;
        assert(coordinator.run() == 0);
        ch.send(0);
      });
      sector += topology.coordinators[coord_id].num_sectors();
    }
    
    size_t core_num = 2;
    printf("Setting affinity for orchestrator_t to core %ld\n", core_num);
    set_core_affinity(core_num);
    
    orchestrator.process(true);
#ifdef PROFILE
    ProfilerStop();
#endif
    
    // Wait for completions
    for (size_t i = 0; i < NUM_HASHING_COORDS; i++) {
      ch.recv(); // each coordinator
    }
    terminator = true;
    ch.recv(); // rw handler
    ch.recv(); // node_writer handler

  } else if (mode == READ_MODE) {
    // Read and print a hashed node
    size_t pages_to_read = 1;
  
    page_t *pages = (page_t *)spdk_dma_zmalloc(sizeof(page_t) * pages_to_read,
                                               PAGE_SIZE, NULL);
    assert (pages != nullptr);

    size_t ctrl_id;
    size_t block_on_controller;
    nvme_node_indexes(node_to_read, ctrl_id, block_on_controller);

    printf("Reading block %ld on controller %ld\n", block_on_controller, ctrl_id);
    
    sequential_io_t sio(controllers[ctrl_id]);
    SPDK_ERROR(sio.rw(true, pages_to_read, (uint8_t *)&pages[0], block_on_controller));

    size_t node_in_page = node_to_read % NODES_PER_PAGE;
    printf("Node %8lx, ctrl %ld, block %ld, node_in_page %ld\n",
           node_to_read, ctrl_id, block_on_controller, node_in_page);
    
    char prefix[32];
    snprintf(prefix, 32, "Node %8lx: ", node_to_read);
    print_node(&pages[0].nodes[node_in_page], 0, prefix, true);
  }
    
  exit(0);
}
