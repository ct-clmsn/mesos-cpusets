#ifndef __CPUSETRESOURCESESTIMATOR_H__
#define __CPUSETRESOURCESESTIMATOR_H__ 1

#include <cmath>
#include <valarray>
#include <functional>
#include <algorithm>
#include <numeric>
#include <limits>
#include <string>
#include <map>
#include <set>

#include <stout/duration.hpp>

#include <process/clock.hpp>
#include <process/time.hpp>
#include <process/timeseries.hpp>
#include <process/dispatch.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

#include <mesos/module/resource_estimator.hpp>

#include "TopologyResourceInformation.hpp"
#include "SubmodularScheduler.hpp"

#include <leveldb/db.h>

using boost::get;

class CpusetResourceEstimatorProcess : public process::Process<CpusetResourceEstimatorProcess>
{
public:
  CpusetResourceEstimatorProcess(
    mesos::Resources const& totalRevocable,
    const std::string dbpathstr)
    : ProcessBase(process::ID::generate("cpuset-resource-estimator")),
      totalRevocable{totalRevocable},
      series(Duration::max()) {

    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, path::join(dbpathstr, "cpusetiso.db"), &db);
    if(!status.ok()) {
      perror("error opening leveldb");
      exit(-1);
    }

  }

  ~CpusetResourceEstimatorProcess() {
    delete db;
  }

private:
  static Result<double> average(const std::valarray<int>& counts) {
    // test for zero
    //
    if(counts.size() < 1) {
      return Result<double>::error("total number of entries in db is ~0");
    }

    // total core request counts
    const double sum = static_cast<double>(counts.sum());
    return Result<double>::some(sum / static_cast<double>(counts.size()));
  }

  struct PoissonDist {
    static inline double factorial(const double x) {
      return (x == 1.0 ? x : x * factorial(x - 1.0));
    }

    // http://stattrek.com/probability-distributions/poisson.aspx
    //
    double operator()(const double x, const double mu) {
      return (std::exp(-mu) * std::pow(mu, x)) / factorial(x);
    }

    static double call(const double x, const double mu) {
      return (std::exp(-mu) * std::pow(mu, x)) / factorial(x);
    }
  };

public:
  process::Future<mesos::Resources> oversubscribable() {
    // get the latest timeseries
    //
    std::string latest;
    leveldb::ReadOptions options;
    options.snapshot = db->GetSnapshot();

    leveldb::Status s = db->Get(options, "latest", &latest);

    leveldb::Iterator* it = db->NewIterator(options);
    it->Seek(latest);
    it->Valid();

    Try<JSON::Array> tarr = JSON::parse<JSON::Array>(it->value().ToString());

    if(tarr.isError()) {
      delete it;
      db->ReleaseSnapshot(options.snapshot);
      return process::Failure("json parse failed!");
    }

    JSON::Array arr = tarr.get();

    for(const JSON::Value& value : arr.values) {
      JSON::Array varr = get<JSON::Array>(value);

      if(varr.values.size() != 2) {
        delete it;
        db->ReleaseSnapshot(options.snapshot);
        return process::Failure("leveldb scan failed!");
      }
      
      Try<process::Time> tsettime = process::Time::create( get<JSON::Number>(varr.values[0]).as<double>()).get();
      if(tsettime.isError()) {
        delete it;
        db->ReleaseSnapshot(options.snapshot);
        return process::Failure("error setting time!");
      }

      series.set(
        get<JSON::Number>(varr.values[1]).as<int>(), tsettime.get() );
    }

    if(!it->status().ok()) {  // Check for any errors found during the scan
      delete it;
      db->ReleaseSnapshot(options.snapshot);
      return process::Failure("leveldb scan failed!");
    }

    if(it->key().ToString() == latest) {
    }
    else {
      delete it;
      db->ReleaseSnapshot(options.snapshot);
      return process::Failure("leveldb scan didn't find latest!");
    }

    // timeseries built, close out leveldb
    delete it;
    db->ReleaseSnapshot(options.snapshot);

    // poisson algorithm to estimate 
    // most likely cpu request given
    // the largest cpu request used
    // to inform the qos controller
    //
    // get freqdist of number of cpu 
    // cores requested
    // 
    std::valarray<int> counts(series.get().size()); 

    std::transform(
      std::begin(series.get()), 
      std::end(series.get()), 
      std::begin(counts), 
      [](const process::TimeSeries<int>::Value& v) {
        return v.data;
      });

    Result<double> rmeanval = average(counts);

    if(rmeanval.isError()) {
      return mesos::Resource();
    }

    const double meanval = rmeanval.get();
    std::map<int, double> core_ests; 

    const int max_cores_req = counts.max();
    for(int core = 1; core <= max_cores_req; core++) {
      std::valarray<double> ar(core);
      std::transform(
        std::begin(ar), 
        std::end(ar), 
        std::begin(ar), 
        [&core, &meanval](const double& val) {
          return PoissonDist::call(static_cast<double>(core), meanval); 
        });

      core_ests[core] = ar.sum();
    }

    // pick the cpu count that is most likely
    // to be requested (implied "next") under 
    // a poisson model
    //
    auto argmax =
      std::max_element(
        std::begin(core_ests),
        std::end(core_ests),
        [](const std::pair<int, double>& p1,
           const std::pair<int, double>& p2) {
             return p1.second < p2.second;
        });

    const int req_core_est = argmax->first;

    std::set<int> cpuset_to_assign;
    SubmodularScheduler<CpuTopologyResourceInformationPolicy> scheduler;
    scheduler(cpuset_to_assign, req_core_est);

    const int est_cpuset_avail = cpuset_to_assign.size();

    // successfully assigned the est cpuset required
    //
    if(est_cpuset_avail == req_core_est) {
      Try<mesos::Resource> t_toret = mesos::Resources::parse("core", stringify(est_cpuset_avail), "*");
      mesos::Resources toret;
      toret += (t_toret.isError()) ? mesos::Resource() : t_toret.get();
      return toret;
    }

    // failed, return empty resources
    return mesos::Resources();
  }

private:
 
  leveldb::DB* db;
  mesos::Resources const totalRevocable;
  process::TimeSeries<int> series;

};

static mesos::Resources makeRevocable(mesos::Resources const& any) {
  mesos::Resources revocable;
  for (auto resource : any) {
    resource.mutable_revocable();
    revocable += resource;
  }
  return revocable;
}

class CpusetResourceEstimator : public mesos::slave::ResourceEstimator
{
public:
  CpusetResourceEstimator(
    mesos::Resources const& totalRevocable,
    const std::string dbpathstr)
    : tRevocable(totalRevocable),
      str_dbpath(dbpathstr) { 
  }

  // Initializes this resource estimator. This method needs to be
  // called before any other member method is called. It registers
  // a callback in the resource estimator. The callback allows the
  // resource estimator to fetch the current resource usage for each
  // executor on agent.
  //
  virtual Try<Nothing> initialize(
    const lambda::function<process::Future<mesos::ResourceUsage>()>& usage) {

    if (process.get() != nullptr) {
      return Error("CpusetResourceEstimator has already been initialized");
    }

    process.reset(
      new CpusetResourceEstimatorProcess(
        makeRevocable(tRevocable), 
        str_dbpath));

    spawn(process.get());

    return Try<Nothing>::some(Nothing());
  }

  // Returns the current estimation about the *maximum* amount of
  // resources that can be oversubscribed on the agent. A new
  // estimation will invalidate all the previously returned
  // estimations. The agent will be calling this method periodically
  // to forward it to the master. As a result, the estimator should
  // respond with an estimate every time this method is called.
  //
  virtual process::Future<mesos::Resources> oversubscribable() {
    // get time series
    return dispatch(
             process.get(), 
             &CpusetResourceEstimatorProcess::oversubscribable);
  }

private:
  process::Owned<CpusetResourceEstimatorProcess> process;
  mesos::Resources tRevocable;
  std::string str_dbpath;

};

#endif

