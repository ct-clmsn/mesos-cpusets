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
#include "cgroupcpusets.hpp"

#include <list>
#include <algorithm>
#include <iostream>
#include <sstream>

#include <unistd.h>

Try<Nothing> has_cgroup_cpuset_subsystem() {
  const std::string cpuset_dir_path = "/sys/fs/cgroup/cpuset";
  if(!os::exists(cpuset_dir_path)) {
    return Error("/sys/fs/cgroup/cpuset <cpuset cgroup subsystem> does not exist!");
  }

  return Nothing();
}

int get_cpuset_groups(std::vector<std::string>& cpuset_groups) {
  Try<Nothing> found_cgroup_cpuset_subsystem = has_cgroup_cpuset_subsystem();
  if(found_cgroup_cpuset_subsystem.isError()) {
    return 0;
  }

  const std::string cpuset_dir_path = "/sys/fs/cgroup/cpuset";
  Try<std::list<std::string> > cpuset_dir_entries = os::ls(cpuset_dir_path);

  std::copy_if(std::begin(cpuset_dir_entries.get()), std::end(cpuset_dir_entries.get()), 
    std::back_inserter(cpuset_groups),
    [&cpuset_dir_path](std::string entry) {
      std::string entry_path = path::join(cpuset_dir_path, entry);
      return (os::stat::isdir(entry_path) && !os::stat::islink(entry_path));
    });

  return 1;
}

Try<std::vector<std::string> > get_cpuset_groups() {
  std::vector<std::string> cpuset_groups;

  if(get_cpuset_groups(cpuset_groups) != 1) {
    return Error("/sys/fs/cgroup/cpuset not found");
  }

  return cpuset_groups;
}

static inline int parse_os_index_file(
  const std::string& os_idx_file_path, 
  std::vector<int>& indices ) 
{
  std::string fline;
  std::ifstream fileistr(os_idx_file_path);

  // parsing code for the cpuset.cpus file...
  // assumes structure found in testfile-testfile4
  //
  if(fileistr.is_open()) {
    std::getline(fileistr, fline);
    const size_t endpos = fline.find_last_not_of(" \t\n");
    if( std::string::npos != endpos ) {
      fline = fline.substr( 0, endpos+1 );
    }

    if(fline.find(",") != std::string::npos) {
      std::istringstream lin(fline);
      std::string procstr;

      while(std::getline(lin, procstr, ',')) {

        if(procstr.find("-") != std::string::npos) {

          std::istringstream procstrm(procstr);
          std::string proctok;
          while(std::getline(procstrm, proctok, '-')) {
            indices.push_back(std::stoi(proctok));
          }

          int i = *std::prev(std::end(indices));
          int j = *std::end(indices);
          for(int ii = i; ii < j; ii++) {
            indices.push_back(ii);
          }
        }
        else {
          indices.push_back(std::stoi(procstr));
        }
      }
    }
    else if(fline.find("-") != std::string::npos) {
      std::istringstream procstrm(fline);
      std::string proctok;
      while(std::getline(procstrm, proctok, '-')) {
        indices.push_back(std::stoi(proctok));
      }

      int i = *std::prev(std::end(indices));
      int j = *std::end(indices);
      for(int ii = i; ii < j; ii++) {
        indices.push_back(ii);
      }
    }
    else {
      indices.push_back(std::stoi(fline));
    }

    fileistr.close();
  }

  return 1;
}

int get_cpuset_cpus(std::vector<int>& cpus) {
  Try<Nothing> found_cgroup_cpuset_subsystem = has_cgroup_cpuset_subsystem();

  if(found_cgroup_cpuset_subsystem.isError()) {
    return 0;
  }

  //const std::string cpuset_dir_path = "testfile"; //path::join("/sys/fs/cgroup/cpuset/", "cpuset.cpus");
  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", "cpuset.cpus");
  return parse_os_index_file(cpuset_dir_path, cpus);
}

Try<std::vector<int> > get_cpuset_cpus() {
  std::vector<int> cpuset_cpus;
  if(get_cpuset_cpus(cpuset_cpus) != 1) {
    return Error("cpuset cpus not found");
  }

  std::sort(std::begin(cpuset_cpus), std::end(cpuset_cpus));
  return cpuset_cpus;
}

int get_cpuset_mems(std::vector<int>& mems) {
  Try<Nothing> found_cgroup_cpuset_subsystem = has_cgroup_cpuset_subsystem();

  if(found_cgroup_cpuset_subsystem.isError()) {
    return 0;
  }

  const std::string mems_dir_path = path::join("/sys/fs/cgroup/cpuset/", "cpuset.mems");
  return parse_os_index_file(mems_dir_path, mems);
}

Try<std::vector<int> > get_cpuset_mems() {
  std::vector<int> cpuset_mems;
  if(get_cpuset_cpus(cpuset_mems) != 1) {
    return Error("cpuset cpus not found");
  }

  std::sort(std::begin(cpuset_mems), std::end(cpuset_mems));
  return cpuset_mems;
}

Try<Nothing> create_cpuset_group(const std::string& group) {
  Try<Nothing> found_cgroup_cpuset_subsystem = has_cgroup_cpuset_subsystem();
  if(found_cgroup_cpuset_subsystem.isError()) {
    return found_cgroup_cpuset_subsystem;
  }

  if(os::mkdir(path::join("/sys/fs/cgroup/cpuset/", group)).isError()) {
    return Error("mkdir failed!");
  }

  return Nothing();
}

Try<Nothing> destroy_cpuset_group(const std::string& group) {
  Try<Nothing> found_cgroup_cpuset_subsystem = has_cgroup_cpuset_subsystem();
  if(found_cgroup_cpuset_subsystem.isError()) {
    return found_cgroup_cpuset_subsystem;
  }

  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", group);
  if(!os::exists(cpuset_dir_path)) {
    std::stringstream errorstrm;
    errorstrm << "/sys/fs/cgroup/cpuset/" << group <<" does not exist!";
    return Error(errorstrm.str());
  }

  if(os::rmdir(cpuset_dir_path).isError()) {
    return Error("mkdir failed!");
  }

  return Nothing();
}

Try<Nothing> attach_cpuset_group_pid(const std::string& group, const pid_t pid) {
  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", group);
  if(!os::exists(cpuset_dir_path)) {
    std::stringstream errorstrm;
    errorstrm << "/sys/fs/cgroup/cpuset/" << group <<" does not exist!";
    return Error(errorstrm.str());
  }

  std::stringstream cpus_str_strm;
  cpus_str_strm << pid;

  std::ofstream cpufile(path::join(cpuset_dir_path, "tasks"));

  if(cpufile.is_open()) {
    cpufile << cpus_str_strm.str();
    cpufile.flush();
    cpufile.close();
  }
  else {
    return Error("error opening tasks");
  }

  return Nothing();
}

Try<Nothing> assign_cpuset_group_cpus(const std::string& group, const std::vector<int> cpus) {
  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", group);
  if(!os::exists(cpuset_dir_path)) {
    std::stringstream errorstrm; 
    errorstrm << "/sys/fs/cgroup/cpuset/" << group << " does not exist!";
    return Error(errorstrm.str());
  }

  std::stringstream cpus_str_strm;

  if(cpus.size() > 1) {
    std::for_each(std::begin(cpus), std::prev(std::end(cpus), 1), 
      [&cpus_str_strm] (int cpu) {
        cpus_str_strm << cpu << ",";
    });

    cpus_str_strm << (*std::end(cpus));
  }
  else {
    cpus_str_strm << (*std::begin(cpus));
  }

  std::ofstream cpufile(path::join(cpuset_dir_path, "cpuset.cpus"));
  if(cpufile.is_open()) {
    cpufile << cpus_str_strm.str();
    cpufile.close();
  }
  else {
    return Error("error opening cpuset.cpus");
  }

  return Nothing();
}

Try<Nothing> assign_cpuset_group_mems(
  const std::string& group, 
  const std::vector<int> mems)
{
  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", group);

  if(!os::exists(cpuset_dir_path)) {
    std::stringstream errorstrm;
    errorstrm << "/sys/fs/cgroup/cpuset/" << group << " does not exist!";
    return Error(errorstrm.str());
  }

  std::stringstream cpus_str_strm;

  if(mems.size() > 1) {
    std::for_each(std::begin(mems), std::prev(std::end(mems), 1),
      [&cpus_str_strm] (int cpu) {
        cpus_str_strm << cpu << ",";
    });

    cpus_str_strm << (*std::end(mems));
  }
  else {
    cpus_str_strm << (*std::begin(mems));
  }

  std::ofstream cpufile(path::join(cpuset_dir_path, "cpuset.mems"));
  if(cpufile.is_open()) {
    cpufile << cpus_str_strm.str();
    cpufile.close();
  }
  else {
    return Error("error opening cpuset.mems");
  }

  return Nothing();
}

int get_cpuset_cpu_utilization(
  const std::string& cpuset_group, 
  std::map<int, int>& cpuset_utilization )
{
  const std::string cpuset_dir_path = path::join("/sys/fs/cgroup/cpuset/", cpuset_group);

  if(!os::exists(cpuset_dir_path)) {
    return -1;
  }

  std::vector<int> cpus;
  const std::string cpuset_cpus_path = path::join(cpuset_dir_path, "cpuset.cpus");

  if(!os::exists(cpuset_cpus_path)) {
    return -1;
  }

  if(parse_os_index_file( cpuset_cpus_path , cpus ) != -1 ) {
    std::for_each(
      std::begin(cpus),
      std::end(cpus),
      [&cpuset_utilization] (int cpu) {
        cpuset_utilization[cpu] = cpuset_utilization.count(cpu) ? cpuset_utilization[cpu] + 1 : 1;
      });
  }
  else {
    return -1;
  }
  
  return 1;
}

Try<std::map<int, int> > get_cpuset_cpu_utilization(const std::vector<std::string>& cpuset_groups) {
  std::map<int,int> cpuset_util;
  std::string error_msg;

  if(!std::any_of(
    std::begin(cpuset_groups),
    std::end(cpuset_groups),
    [&cpuset_util, &error_msg] (const std::string& cpuset_group) {
      if(get_cpuset_cpu_utilization(cpuset_group, cpuset_util) == -1) {
        error_msg = cpuset_group;
        return false;
      }
      return true;
    })) {
      return Error("error processing utilization for cpuset: " + error_msg);
    }

  return cpuset_util;
}

