// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//  ct.clmsn
//

#include <error.h>
#include <utility>
#include <algorithm>
#include <limits>

#include <stout/foreach.hpp>

#include "HwlocTopology.hpp"


#ifdef USE_CUDA

Try<bool> FALSE_ON_FAIL(CUresult& res)
{
  if (CUDA_SUCCESS != res) {
    error(-1, 1, "cuInit returned error");
    return false;
  }

  return true;
}

#endif


process::Future<float> HwlocTopologyProcess::getCoreDistance(
  const int i, 
  const int j) {
  const int ncores = nCores().get();
  return coreDistmat[i*ncores+j].latency[i*ncores+j];
}

process::Future<int> HwlocTopologyProcess::getNumaForCore(
  const int core_os_id ) {

  int retval = -1;

  if(coresPerNuma.count(core_os_id)) {

    std::for_each(std::begin(coresPerNuma), std::end(coresPerNuma),
      [&retval, &core_os_id] (std::pair<unsigned, std::vector<unsigned> > numas) {
        if(static_cast<int>(numas.first) == core_os_id && numas.second.size() > 0) {
          retval = static_cast<int>(numas.second[0]);
        }
    });

  }

  return retval;
}

struct sortpred {
  bool operator()(
    const std::pair<int, float> &left,
    const std::pair<int, float> &right) {
    return left.second < right.second;
  }
} sort_pred;

inline int map_acc(
  float lhs,
  const std::pair<hwloc_obj_t, float> & rhs) {
  return lhs + rhs.second;
}

HwlocTopologyProcess::HwlocTopologyProcess() {
  if (hwloc_topology_init(&(topology))) {
    /* error in initialize hwloc library */
    error(-1, 1, "%s: hwloc_loc.topo_init() failed", __func__);
  }

  const unsigned long topo_flags = HWLOC_TOPOLOGY_FLAG_WHOLE_SYSTEM |
                                   HWLOC_TOPOLOGY_FLAG_IO_DEVICES |
                                   HWLOC_TOPOLOGY_FLAG_IO_BRIDGES;

  hwloc_topology_set_flags(topology, topo_flags);
  hwloc_topology_load(topology);

  root = hwloc_get_root_obj(topology);

  discoverCpuTopology(topology, root, NULL);
  coreDistmat = hwloc_get_whole_distance_matrix_by_type(topology,
                                                      HWLOC_OBJ_CORE);

  discoverGpuTopology(topology, root, NULL);
}

process::Future<int> HwlocTopologyProcess::nSockets() {
  return coresPerSocket.size();
}

process::Future<int> HwlocTopologyProcess::nCores() {
  return pusPerCore.size();
}

process::Future<int> HwlocTopologyProcess::nProcessingUnits() {
  process::Future<std::vector<int>> pu_vec = nProcessUnitsPerCore();
  std::vector<int> puvec = pu_vec.get();
  return std::accumulate(puvec.begin(), puvec.end(), 0);
}

process::Future<std::vector<int>> HwlocTopologyProcess::nCoresPerSocket() {
  std::vector<int> coreCounts;

  foreachvalue(std::vector<hwloc_obj_t> socket_cores, coresPerSocket) {
    coreCounts.push_back(socket_cores.size());
  }

  return coreCounts;
}

int HwlocTopologyProcess::getCoreIndex(
  hwloc_obj_t core)
{
  int count = 0;
  foreachvalue(std::vector<hwloc_obj_t> socket_cores, coresPerSocket) {
    vector<hwloc_obj_t>::iterator i =
        find(socket_cores.begin(), socket_cores.end(), core);
    if(i == socket_cores.end()) {  return -1; }
    count+=1;
  }

  return count;
}

process::Future<std::vector<int>> HwlocTopologyProcess::nProcessUnitsPerCore() {
  std::vector<int> puCounts;

  foreachvalue(std::vector<hwloc_obj_t> core_pu, pusPerCore) {
    puCounts.push_back(core_pu.size());
  }

  return puCounts;
}

static inline bool find_parent_by_type(
  hwloc_obj_t halt,
  hwloc_obj_t obj,
  const hwloc_obj_type_t T)
{
  for(hwloc_obj_t cur = obj; cur != NULL && cur != halt; cur = cur->parent) {
    if(cur->type == T) { return true; }
  }

  return false;
}

void HwlocTopologyProcess::discoverCpuTopology(
  hwloc_topology_t topology,
  hwloc_obj_t parent,
  hwloc_obj_t child)
{
  hwloc_obj_t component;
  component = hwloc_get_next_child(topology, parent, child);

  if (NULL == component) {
    return;
  }

  // add vector of cores to socket map
  if (component->type == HWLOC_OBJ_SOCKET) {
    std::vector<hwloc_obj_t> cores_vec;
    coresPerSocket.insert(std::make_pair(component, cores_vec));
  }

  else if(component->type == HWLOC_OBJ_NODE) {
    std::vector<unsigned> cores_vec;
    unsigned numa_os_index = component->os_index;
    coresPerNuma.insert( std::make_pair(numa_os_index, cores_vec) );
  }

  // add core to socket
  else if (component->type == HWLOC_OBJ_CORE) {
    const unsigned core_os_index = component->os_index;

    foreachpair(
      hwloc_obj_t socket,
      vector<hwloc_obj_t> cores,
      coresPerSocket)
    {

      if(find_parent_by_type(socket, component, HWLOC_OBJ_SOCKET)) {
        cores.push_back(component);
        coresPerSocket.insert(std::make_pair(socket, cores));

        if (find_parent_by_type(root, component, HWLOC_OBJ_NODE)) {
          const unsigned numa_os_index = component->os_index;

          if(coresPerNuma.count(numa_os_index)) {
            coresPerNuma[numa_os_index].push_back(core_os_index);
          }
          else {
            std::vector<unsigned> cores_vec;
            unsigned numa_os_index = component->os_index;
            coresPerNuma.insert( std::make_pair(numa_os_index, cores_vec) );
          }
        }
      }

    }

  }

  // add pu to core
  else if (component->type == HWLOC_OBJ_PU) {
    if(find_parent_by_type(parent, component, HWLOC_OBJ_CORE)) {
      std::map< hwloc_obj_t, std::vector<hwloc_obj_t> >::iterator pu_core =
        pusPerCore.find(parent);

      if(pu_core == pusPerCore.end()) {
        std::vector<hwloc_obj_t> pusvec;
        pusvec.push_back(component);
        pusPerCore.insert(std::make_pair(parent, pusvec));
      }
      else {
        pu_core->second.push_back(component);
      }
    }
  }

  if(0 != component->arity) {
    /* This device has children so need to look recursively at them */
    discoverCpuTopology(topology, component, NULL);
    discoverCpuTopology(topology, parent, component);
  }
  else {
    discoverCpuTopology(topology, parent, component);
  }
}

// modified from http://icl.cs.utk.edu/open-mpi/faq/?category=runcuda
//
void HwlocTopologyProcess::find_gpus(
  hwloc_obj_t parent,
  hwloc_obj_t child)
{
  hwloc_obj_t pcidev;
  pcidev = hwloc_get_next_child(topology, parent, child);

  if (NULL == pcidev) {
    return;
  }
  else if (0 != pcidev->arity) {
    find_gpus(pcidev, NULL);
    find_gpus(parent, pcidev);
  }
  else {
    if (pcidev->attr->pcidev.vendor_id == 0x10de) {
      gpus.push_back(pcidev);
    }

    find_gpus(parent, pcidev);
  }
}

void HwlocTopologyProcess::discoverGpuTopology(
  hwloc_topology_t topology,
  hwloc_obj_t parent,
  hwloc_obj_t child)
{
  hwloc_obj_t bridge;
  bridge = hwloc_get_obj_by_type(topology, HWLOC_OBJ_BRIDGE, 0);
  find_gpus(bridge, NULL);
}

process::Future<std::vector<int> > HwlocTopologyProcess::getCudaCpus() {
  std::vector<int> cpus;

  #ifdef USE_CUDA

  for(int i = 0; i < gpus.size(); i++) {

    hwloc_obj_t gpu = gpus[i];
    char pciBusId[16];
    char devName[256];
    CUdevice dev;

    sprintf(pciBusId,
      "%.2x:%.2x:%.2x.%x",
      gpu->attr->pcidev.domain,
      gpu->attr->pcidev.bus,
      gpu->attr->pcidev.dev,
      gpu->attr->pcidev.func);

    if(FALSE_ON_FAIL(cuDeviceGetByPCIBusId(&dev, pciBusId))) {
      perror(-1, 1, "task requires gpu cuda did not find the pcibusid");
    }

    if(FALSE_ON_FAIL(cuDeviceGetName(devName, 256, dev))) {
      perror(-1, 1, "task requires gpu cuda did not find the device id");
    }

    hwloc_cpuset_t gpu_associated_cpuset = hwloc_bitmap_alloc();

    // https://www.open-mpi.org/projects/hwloc/doc/v1.9.1/a00107.php
    //
    // get the CPU set of logical processors that are physically
    // close to device cudevice.
    hwloc_cuda_get_device_cpuset(topology,
      gpu,
      dev,
      gpu_associated_cpuset);

    for(int j = 0; j < coresPerSocket.size(); j++) {
      for(int k = 0; j < coresPerSocket[j].size(); k++) {
        if(hwloc_cpuset_intersects(gpu_associated_cpuset, coresPerSocket[j][k]->cpuset)) {
          cpus.push_back(coresPerSocket[j][k]->os_index);
        }
      }
    }

    hwloc_bitmap_free(gpu_associated_cpuset);
  }

  #endif

  return cpus;
}



