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
#ifndef __CGROUPS_CPUSETS_H__
#define __CGROUPS_CPUSETS_H__ 1

#include <string>
#include <vector>
#include <map>

#include <stout/os.hpp>
#include <stout/try.hpp>

Try<Nothing> has_cgroup_cpuset_subsystem();

Try<std::vector<std::string> > get_cpuset_groups();

Try<std::vector<int> > get_cpuset_cpus();

Try<std::vector<int> > get_cpuset_mems();

Try<Nothing> create_cpuset_group(const std::string& group);

Try<Nothing> destroy_cpuset_group(const std::string& group);

Try<Nothing> attach_cpuset_group_pid(
  const std::string& group, 
  const pid_t pid );

Try<Nothing> assign_cpuset_group_cpus(
  const std::string& group, 
  const std::vector<int> cpus );

Try<Nothing> assign_cpuset_group_mems(
  const std::string& group, 
  const std::vector<int> mems );

Try<std::map<int, int> > get_cpuset_cpu_utilization(
  const std::vector<std::string>& cpuset_groups );

#endif

