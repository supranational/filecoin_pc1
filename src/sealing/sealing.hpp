// Copyright Supranational LLC

#ifndef __SEALING_T_HPP__
#define __SEALING_T_HPP__

#include <mutex>
#include "../util/stats.hpp"
#include "../sealing/constants.hpp"
#include "../nvme/nvme.hpp"
#include "../sealing/data_structures.hpp"

// Forward declarations
class coordinator_t;
template<class B>
class node_rw_t;
class orchestrator_t;

const size_t STATS_PERIOD = 1<<22;
const size_t STATS_MASK   = STATS_PERIOD - 1;

extern mutex print_mtx;

#include "../util/debug_helpers.hpp"
#include "topology_t.hpp"
#include "system_buffers_t.hpp"
#include "parent_iter_t.hpp"
#include "orchestrator_t.hpp"
#include "node_rw_t.hpp"
#include "coordinator_t.hpp"

#endif
