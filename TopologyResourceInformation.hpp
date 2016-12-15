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
// ct-clmsn
//

#ifndef __TOPOLOGY_INFO_H___
#define __TOPOLOGY_INFO_H___ 1

#include <valarray>
#include <vector>
#include <map>

#include <process/dispatch.hpp>
#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>

#include <stout/try.hpp>
#include <stout/option.hpp>

#include "cgroupcpusets.hpp"
#include "HwlocTopology.hpp"

using namespace std;
using namespace process;

class TopologyResourceInformationProcess : 
  public process::Process<TopologyResourceInformationProcess> 
{

public:
  // constructor, inits
  // several member variables
  // detects hwloc support
  // finds all relevant devices
  // system components
  TopologyResourceInformationProcess() {
    Try<std::vector<std::string> > cpuset_groups = get_cpuset_groups();

    if(!cpuset_groups.isError()) {
      std::copy(std::begin(cpuset_groups.get()), 
        std::end(cpuset_groups.get()), 
        std::back_inserter(cpusetGroups) );
    }

    Try<std::vector<int> > cpuset_cpus = get_cpuset_cpus();

    if(!cpuset_cpus.isError()) {
      std::copy(std::begin(cpuset_cpus.get()),
        std::end(cpuset_cpus.get()),
        std::back_inserter(cpusetCpus) );
    }

  }

  // get the number of sockets
  // hwloc found, filtered by what cgroups has
  //
  process::Future<int> nSockets() {
    return topology.nSockets().get();
  }

  // get the number of cores
  // hwloc found
  process::Future<int> nCores() {
    return topology.nCores().get();
  }

  // get the number of processing
  // units hwloc found
  process::Future<int> nProcessingUnits() {
    return topology.nProcessingUnits().get();
  }

  // get a list of # cores
  // per socket
  process::Future<std::vector<int>> nCoresPerSocket() {
    return topology.nCoresPerSocket().get();
  }

  // get the number of tasks
  // assigned to cores 
  //
  process::Future<std::map<int, int> > getTaskCount() {
    // add support for getting all cpus
    //
    Try<std::map<int,int> > cpuset_cpu_util = get_cpuset_cpu_utilization(cpusetGroups);
    /* if(cpuset_cpu_util.isError()) {
      return Error("get cpuset utility map failed");
    } */

    return cpuset_cpu_util.get();
  }

  // get normalized frequency
  // of "work" per core
  //
  process::Future<std::valarray<float> > getTaskFrequencyVector() {
    std::valarray<float> costVec;

    Future<std::map<int, int> > fcpu_util_mat = getTaskCount();
    std::map<int, int> cpu_util_mat = fcpu_util_mat.get();

    costVec.resize(cpu_util_mat.size());

    const int total_cpu_work = std::accumulate(std::begin(cpu_util_mat), std::end(cpu_util_mat), 0,
      [] (int cpuATaskCount, std::pair<int, int> cpuB) { return cpuATaskCount + cpuB.second; });

    std::for_each(std::begin(cpu_util_mat), std::end(cpu_util_mat),
      [&costVec, &cpu_util_mat, &total_cpu_work] (std::pair<int, int> cpu) {
        costVec[cpu.first] = static_cast<float>(cpu.second) / static_cast<float>(total_cpu_work);
    });

    return costVec;
  }

  // get task weights - #tasks-on-a-core / #core-processing-units
  //
  process::Future<std::valarray<float> > getWeightedTaskFrequencyVector() {
    std::valarray<float> weightVec;

    Future<std::map<int, int> > fcpu_util_mat = getTaskCount();
    Future<std::vector<int> > fpu_per_core = topology.nProcessUnitsPerCore();

    weightVec.resize(fcpu_util_mat.get().size());  

    std::for_each(std::begin(fcpu_util_mat.get()), std::end(fcpu_util_mat.get()),
      [&weightVec, &fpu_per_core] (std::pair<int, int> cpu) {
      weightVec = static_cast<float>(cpu.second) / static_cast<float>(fpu_per_core.get()[cpu.first]);
    });

    return weightVec;
  }

  process::Future<float> getCoreDistance(
    const int i,
    const int j) {
    return topology.getCoreDistance(i, j).get();
  }

  process::Future<int> getNumaForCore(const int core) {
    return topology.getNumaForCore(core);
  }

  process::Future<std::vector<int> > getCudaCpus() {
    return topology.getCudaCpus();
  }

private:

  HwlocTopology topology;

  std::vector<std::string> cpusetGroups;
  std::vector<int> cpusetCpus;

};

class TopologyResourceInformation {
public:

  TopologyResourceInformation() {}

  process::Future<int> nSockets() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::nSockets);
  }

  process::Future<int> nCores() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::nCores);
  }

  process::Future<std::vector<int> > nCoresPerSocket() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::nCoresPerSocket);
  }

  process::Future<float> getCoreDistance(
    const int i,
    const int j){
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::getCoreDistance,
      i, j);
  }

  process::Future<std::valarray<float> > getTaskFrequencyVector() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::getTaskFrequencyVector);
  }

  process::Future<std::valarray<float> > getWeightedTaskFrequencyVector() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::getWeightedTaskFrequencyVector);
  }
  
  process::Future<int> getNumaForCore(const int core) {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::getNumaForCore,
      core);
  }

  process::Future<std::vector<int> > getCudaCpus() {
    return dispatch(process.get(),
      &TopologyResourceInformationProcess::getCudaCpus);
  }

  virtual ~TopologyResourceInformation() {
    terminate(process.get());
    wait(process.get());
  }

private:
  process::Owned<TopologyResourceInformationProcess> process;

};

struct CpuTopologyResourceInformationPolicy {

  int getNumItems() {
    return topology.nCores().get();
  }

  std::vector<int> getItems() {
    std::vector<int> cpus;
    for(int i = 0; i < getNumItems(); i++) {
      cpus.push_back(i);
    }

    return cpus;
  }

  int getSimilarity(const int i, const int j) {
    return topology.getCoreDistance(i, j).get();
  }

  std::valarray<float> getCostVector() {
    return topology.getTaskFrequencyVector().get();
  }

  std::valarray<float> getWeightVector() {
    return topology.getWeightedTaskFrequencyVector().get();
  }

  TopologyResourceInformation topology;

};

struct CudaTopologyResourceInformationPolicy : public CpuTopologyResourceInformationPolicy {
 
  std::vector<int> getCudaCpus() {
    return topology.getCudaCpus().get();
  }

  std::valarray<float> getCudaCpusWeightVector() {
    const std::valarray<float> cpuweights = getWeightVector();
    const std::vector<int> cudacpus = getCudaCpus();

    std::valarray<float> cudacpuweights;
    cudacpuweights.resize(getNumItems());
    cudacpuweights = 0.0;

    std::for_each(std::begin(cudacpus), std::end(cudacpus),
      [&cpuweights, &cudacpuweights] (const int cudacpu) {
          cudacpuweights[cudacpu] = cpuweights[cudacpu];
    });

    return cudacpuweights;
  }

  std::valarray<float> getWeightVector() {
    return getCudaCpusWeightVector();
  }

  std::vector<int> getItems() {
    return getCudaCpus();
  }
  
};

#endif
