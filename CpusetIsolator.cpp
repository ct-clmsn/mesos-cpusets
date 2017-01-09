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

#include <process/process.hpp>
#include <process/subprocess.hpp>

#include "CpusetIsolator.hpp"

#include <leveldb/write_batch.h>
#include <string>

using namespace process;

static Result<process::Time> getCurrentTime(const double timewindow) {
  const Time now = Clock::now();
  const Duration dnow = now.duration();
  const double mins = ((dnow.mins() / timewindow) * timewindow);

  // compute user-defined window
  //
  Try<process::Time> tnowsec = process::Time::create(dnow.secs());
  if(tnowsec.isError()) {
    return Result<process::Time>::error("invalid time");
  }

  // see if timeseries has a sample of nowsec sample
  //
  const process::Time nowsec = tnowsec.get();
  return nowsec;
}

CpusetIsolatorProcess::CpusetIsolatorProcess(
  const mesos::Parameters& parameters) 
  : params(parameters) 
{
  Option<std::string> odbpath;
  Option<std::string> otw;

  for(const mesos::Parameter& p : parameters.parameter()) {
    if(p.has_key() && (p.key() == "cpusetdbpath") && p.has_value()) {
      odbpath = p.value();
    }
    else if(p.has_key() && (p.key() == "samplewindow") && p.has_value()) {
      otw = p.value();
    }
  }

  const std::string dbpath = (odbpath.isSome()) ? odbpath.get() : os::getcwd();
  if(otw.isNone()) {
    perror("sample window not provided");
    exit(-1);
  }

  timewindow = std::stod(otw.get());
 
  leveldb::Options opts;
  opts.create_if_missing = true;
  leveldb::Status stat = leveldb::DB::Open(opts, path::join(dbpath, "cpusetiso.db"), &db);

  if(!stat.ok()) {
    perror("cpusetisolator failed to open and/or create db");
    exit(-1);
  }

  std::string value;
  stat = db->Get(leveldb::ReadOptions(), "startDtg", &value);

  if(!stat.ok()) {
    const process::Time cur_dtg = getCurrentTime(timewindow).get();
    stat = db->Put(leveldb::WriteOptions(), "startDtg", stringify(cur_dtg));
  }

}

process::Future<Nothing> CpusetIsolatorProcess::recover(
  const list<mesos::slave::ContainerState>& states,
  const hashset<mesos::ContainerID>& orphans) {

/*
  foreach (const mesos::slave::ContainerState& run, states) {
    if (pids.contains(run.container_id())) {
      return process::Failure("Container already recovered");
    }

    pids.put(run.container_id(), run.pid());

    process::Owned<process::Promise<mesos::slave::ContainerLimitation>> promise(
        new process::Promise<mesos::slave::ContainerLimitation>());
    promises.put(run.container_id(), promise);
  } */

  return Nothing();
}

process::Future<Option<mesos::slave::ContainerPrepareInfo>> CpusetIsolatorProcess::prepare(
  const mesos::ContainerID& containerId,
  const mesos::ExecutorInfo& executorInfo,
  const std::string& directory,
  const Option<std::string>& user) {

/*  if (promises.contains(containerId)) {
    return process::Failure("Container " + stringify(containerId) +
                            " has already been prepared");
  }

  process::Owned<process::Promise<mesos::slave::ContainerLimitation>> promise(
      new process::Promise<mesos::slave::ContainerLimitation>());

  promises.put(containerId, promise);
*/
  return None();

}

Result<Nothing> CpusetIsolatorProcess::updateDb(const int cpusreq) {
  // see if timeseries has a sample of nowsec sample
  //
  const process::Time nowsec = getCurrentTime(timewindow).get();
  series.set(cpusreq, nowsec);

  JSON::Array arr;
  arr.values.reserve(series.get().size());
  std::copy(std::begin(arr.values), std::end(arr.values), std::back_inserter(arr.values));

  leveldb::WriteBatch batch;
  batch.Put(stringify(nowsec), stringify(arr));
  batch.Delete("latest");
  batch.Put("latest", stringify(nowsec));

  leveldb::Status s = db->Write(leveldb::WriteOptions(), &batch);
  if(!s.ok()) {
    return Result<Nothing>::error("failed to write to leveldb");
  }

  return Result<Nothing>::some(Nothing());
}

process::Future<Nothing> CpusetIsolatorProcess::isolate(
  const mesos::ContainerID& containerId,
  pid_t pid)
{
  if(!pids.contains(containerId)) {
    return process::Failure("Unknown container");
  }

  if(!containerResources.contains(containerId)) {
    return process::Failure("Unknown container resources");
  }

  const mesos::Resources r = containerResources[containerId];
  const double cpus = r.cpus().get();
  //const double gpus = r.gpus().get();
  const double gpus = 0.0;

  updateDb(cpus);

  create_cpuset_group(containerId.value());

  CpusetAssigner cpuSetAssigner;

  process::Future<bool> assigned = 
    cpuSetAssigner.assign(
      containerId,
      pid,
      cpus,
      gpus);

  if(assigned.isFailed()) {
    return process::Failure("unable to allocate requested # of cores");
  }

  return Nothing();
}


process::Future<mesos::slave::ContainerLimitation> CpusetIsolatorProcess::watch(
  const mesos::ContainerID& containerId)
{
/*
  if (!promises.contains(containerId)) {
    return process::Failure("Unknown container: " + stringify(containerId));
  }

  return promises[containerId]->future();
*/

  return process::Future<mesos::slave::ContainerLimitation>();
}


process::Future<Nothing> CpusetIsolatorProcess::update(
  const mesos::ContainerID& containerId,
  const mesos::Resources& resources)
{
  if(containerResources.find(containerId) == containerResources.end()) {
    containerResources.insert(std::make_pair(containerId, resources));
  }

  return Nothing();
}


process::Future<mesos::ResourceStatistics> CpusetIsolatorProcess::usage(
  const mesos::ContainerID& containerId)
{
  if (!pids.contains(containerId)) {
    LOG(WARNING) << "No resource usage for unknown container '"
                 << containerId << "'";
  }

  // compute resource statistics
  // make this contingent on cpus
  // requested or available
  //
  return mesos::ResourceStatistics();
}


process::Future<Nothing> CpusetIsolatorProcess::cleanup(
  const mesos::ContainerID& containerId) {
  return _cleanup(containerId);
}


process::Future<Nothing> CpusetIsolatorProcess::_cleanup(
  const mesos::ContainerID& containerId)
{
  if (!containerResources.contains(containerId)) {
    return Failure("Unknown container");
  }

  containerResources.erase(containerId);
  destroy_cpuset_group(containerId.value()).get();

  return Nothing();
}

Try<mesos::slave::Isolator*> CpusetIsolator::create(
    const mesos::Parameters& parameters)
{
  return new CpusetIsolator(
     process::Owned<CpusetIsolatorProcess>(new CpusetIsolatorProcess(parameters)), 
     true);
}

static bool compatible() {
  return true;
}

static mesos::slave::Isolator* createCpusetIsolator(const mesos::Parameters& parameters)
{
  Try< mesos::slave::Isolator* > cpusetIsolator = CpusetIsolator::create(parameters);

  if (cpusetIsolator.isError()) {
    return NULL;
  }

  return cpusetIsolator.get();
}

// Declares a module named 'org_apache_mesos_TestModule' of
// 'TestModule' kind.
// Mesos core binds the module instance pointer as needed.
// The compatible() hook is provided by the module for compatibility checks.
// The create() hook returns an object of type 'TestModule'.
mesos::modules::Module<mesos::slave::Isolator> org_apache_mesos_CpusetIsolatorProcess(
  MESOS_MODULE_API_VERSION,
  MESOS_VERSION,
  "Apache Mesos",
  "ct.clmsn@gmail.com",
  "Binds processes to cgroup cpusets using a greedy submodular set selection algorithm.",
  NULL,
  createCpusetIsolator);

