// special thanks to blue yonder 
// for their threshold project 
// https://github.com/blue-yonder/mesos-threshold-oversubscription
// 

#include <process/future.hpp>
#include <process/owned.hpp>
#include <process/process.hpp>
#include <process/id.hpp>
#include <process/dispatch.hpp>


#include <stout/nothing.hpp>
#include <stout/try.hpp>
#include <stout/option.hpp>
#include <stout/os.hpp>

#include "slave/flags.hpp"

#include <mesos/mesos.pb.h>
#include <mesos/slave/isolator.hpp>
#include <mesos/slave/isolator.pb.h>
#include <mesos/module/resource_estimator.hpp>

#include <mesos/mesos.hpp>
#include <mesos/module.hpp>
#include <mesos/resources.hpp>

#include <string>

#include "CpusetResourceEstimator.hpp"

struct ParsingError
{
  std::string message;

  ParsingError(std::string const& description, std::string const& error)
    : message("Failed to parse " + description + ": " + error)
  {}
};

template <typename Interface, typename ThresholdActor>
static Interface* create(mesos::Parameters const& parameters) {
  mesos::Resources resources;
  Option<std::string> dbpath;

  try {
    for (auto const& parameter : parameters.parameter()) {
      // Parse the resource to offer for oversubscription
      if (parameter.key() == "resources") {
        Try<mesos::Resources> parsed = mesos::Resources::parse(parameter.value());
        if (parsed.isError()) {
          throw ParsingError("resources", parsed.error());
        }

        resources = parsed.get();
      }

      // Parse any thresholds
      if (parameter.key() == "cpusetdbpath") {
        dbpath = parameter.value();
      } 
    }
  } catch (ParsingError e) {
    LOG(ERROR) << e.message;
    return nullptr;
  }

  const std::string dbpathval = (dbpath.isSome()) ? dbpath.get() : os::getcwd();
  return new ThresholdActor(resources, dbpathval);
}

static mesos::slave::ResourceEstimator* createEstimator(mesos::Parameters const& parameters) {
  return create<mesos::slave::ResourceEstimator, CpusetResourceEstimator>(parameters);
}

static bool compatible() {
  return true; 
}

mesos::modules::Module<mesos::slave::ResourceEstimator> CpusetThresholdResourceEstimator(
  MESOS_MODULE_API_VERSION,
  MESOS_VERSION,
  "Apache Mesos",
  "ct-clmsn@gmail.com",
  "Cpuset Resource Estimator Module.",
  compatible,
  createEstimator);

