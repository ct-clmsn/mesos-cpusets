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

#ifndef __CPUISOLATOR_HPP__
#define __CPUISOLATOR_HPP__ 1

#include <sys/types.h>

#include <string>
#include <vector>

#include <mesos/mesos.hpp>
#include <mesos/resources.hpp>
#include <mesos/module/isolator.hpp>
#include <mesos/slave/isolator.hpp>

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>
#include <process/timeseries.hpp>

#include <stout/nothing.hpp>
#include <stout/try.hpp>
#include <stout/option.hpp>

#include "slave/flags.hpp"

#include <mesos/mesos.pb.h>
#include <mesos/slave/isolator.hpp>
#include <mesos/slave/isolator.pb.h>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>

#include <leveldb/db.h>

#include "CpusetAssigner.hpp"

using namespace std;
using namespace mesos::internal::slave;

// Use the Linux cpu cgroup controller for cpu isolation which uses the
// Completely Fair Scheduler (CFS).
// - cpushare implements proportionally weighted scheduling.
// - cfs implements hard quota based scheduling.
class CpusetIsolatorProcess: 
  public process::Process<CpusetIsolatorProcess>
{
public:
  CpusetIsolatorProcess(const mesos::Parameters& parameters);

  process::Future<Nothing> recover(
      const std::list<mesos::slave::ContainerState>& states,
      const hashset<mesos::ContainerID>& orphans);

  process::Future<Option<mesos::slave::ContainerPrepareInfo>> prepare(
      const mesos::ContainerID& containerId,
      const mesos::ExecutorInfo& executorInfo,
      const std::string& directory,
      const Option<std::string>& user);

  process::Future<Nothing> isolate(
      const mesos::ContainerID& containerId,
      pid_t pid);

  process::Future<mesos::slave::ContainerLimitation> watch(
      const mesos::ContainerID& containerId);

  process::Future<Nothing> update(
      const mesos::ContainerID& containerId,
      const mesos::Resources& resources);

  process::Future<mesos::ResourceStatistics> usage(
      const mesos::ContainerID& containerId);

  process::Future<Nothing> cleanup(
      const mesos::ContainerID& containerId);

private:
  Result<Nothing> updateDb(const int cpusreq);

  process::Future<Nothing> _cleanup(
      const mesos::ContainerID& containerId);

  mesos::Parameters params;

  hashmap<mesos::ContainerID, mesos::Resources> containerResources;
  hashmap<mesos::ContainerID, pid_t> pids;

  double timewindow;
  process::TimeSeries<int> series;
  leveldb::DB* db;

};

// A basic Isolator that keeps track of the pid but doesn't do any resource
// isolation. Subclasses must implement usage() for their appropriate
// resource(s).

class CpusetIsolator : public mesos::slave::Isolator
{
public:
  static Try<mesos::slave::Isolator*> create(
      const mesos::Parameters& parameters);

  CpusetIsolator(process::Owned<CpusetIsolatorProcess> process_, bool activated_)
    : process(process_), 
      activated(activated_) {
    if(activated) {
      spawn(CHECK_NOTNULL(process.get()));
    }
  }

  virtual ~CpusetIsolator() {
    if(activated) {
      terminate(process.get());
      wait(process.get());
    }
  }

  virtual process::Future<Nothing> recover(
    const std::list<mesos::slave::ContainerState>& states,
    const hashset<mesos::ContainerID>& orphans) {
    if(!activated) {
      return Nothing();
    }

    return dispatch(process.get(),
                    &CpusetIsolatorProcess::recover,
                    states,
                    orphans);
  }

  virtual process::Future<Option<mesos::slave::ContainerPrepareInfo>> prepare(
    const mesos::ContainerID& containerId,
    const mesos::ExecutorInfo& executorInfo,
    const std::string& directory,
    const Option<std::string>& user) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::prepare,
                    containerId,
                    executorInfo,
                    directory,
                    user);
  }

  virtual process::Future<Nothing> isolate(
    const mesos::ContainerID& containerId,
    pid_t pid) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::isolate,
                    containerId,
                    pid);
  }

  virtual process::Future<mesos::slave::ContainerLimitation> watch(
    const mesos::ContainerID& containerId) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::watch,
                    containerId);
  }

  virtual process::Future<Nothing> update(
    const mesos::ContainerID& containerId,
    const mesos::Resources& resources) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::update,
                    containerId,
                    resources);
  }

  virtual process::Future<mesos::ResourceStatistics> usage(
    const mesos::ContainerID& containerId) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::usage,
                    containerId);
  }

  virtual process::Future<Nothing> cleanup(
    const mesos::ContainerID& containerId) {
    return dispatch(process.get(),
                    &CpusetIsolatorProcess::cleanup,
                    containerId);
  }

private:
  process::Owned<CpusetIsolatorProcess> process;
  bool activated;
};

#endif // __CPUISOLATOR_HPP__
