MESOS_BASE_PATH=$(HOME)/git/mesos
LIBMESOS_PATH=-L$(MESOS_BASE_PATH)/src/.libs/ -L$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/glog-0.3.3/src/glog-0.3.3-lib/lib/lib
MESOS_INC_PATH=-I$(MESOS_BASE_PATH)/3rdparty/libprocess/include/ -I$(MESOS_BASE_PATH)/3rdparty/libprocess/include/ -I$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/glog-0.3.3/src -I$(MESOS_BASE_PATH)/3rdparty/libprocess/3rdparty/stout/include -I$(MESOS_BASE_PATH)/src -I$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/picojson-1.3.0 -I$(MESOS_BASE_PATH)/include -I$(MESOS_BASE_PATH)/build/include -I$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/protobuf-2.6.1/src -I$(MESOS_BASE_PATH)/build/src/ -I$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/glog-0.3.3/src/glog-0.3.3-lib/lib/include -I$(MESOS_BASE_PATH)/build/3rdparty/libprocess/3rdparty/picojson-1.3.0/src/picojson-1.3.0

CC=g++
CFLAGS=$(LIBMESOS_PATH) $(MESOS_INC_PATH) -std=c++0x 

all:
	$(CC) $(CFLAGS) -fPIC -c cgroupcpusets.cpp
	$(CC) $(CFLAGS) cgroupcpusets.o cgroupcpusets_main.cpp -o cgroupcpusets_main
	$(CC) $(CFLAGS) -fPIC -c HwlocTopology.cpp
	$(CC) $(CFLAGS) -fPIC -c TopologyResourceInformation.cpp
	$(CC) $(CFLAGS) -fPIC -c CpusetAssigner.cpp 
	$(CC) $(CFLAGS) -fPIC -c CpusetIsolator.cpp
	$(CC) $(CFLAGS) -fPIC cgroupcpusets.o HwlocTopology.o TopologyResourceInformation.o CpusetAssigner.o CpusetIsolator.o -shared -o libCpusetIsolatorModule.so -lleveldb -lhwloc -lmesos
	$(CC) $(CFLAGS) -fPIC -c CpusetResourceEstimator.cpp
	$(CC) $(CFLAGS) -fPIC -c CpusetResourceEstimatorModule.cpp
	$(CC) $(CFLAGS) -fPIC cgroupcpusets.o HwlocTopology.o TopologyResourceInformation.o CpusetResourceEstimator.o CpusetResourceEstimatorModule.o -shared -o libCpusetResourceEstimatorModule.so -lleveldb -lhwloc -lmesos


subtest:
	$(CC) $(CFLAGS) -g submodularscheduler-test.cpp -o submodularscheduler_test

clean:
	rm cgroupcpusets.o HwlocTopology.o TopologyResourceInformation.o CpusetAssigner.o CpusetIsolator.o libCpusetIsolatorModule.so
	rm CpusetResourceEstimator.o CpusetResourceEstimatorModule.o libCpusetResourceEstimatorModule.so
	rm cgroupcpusets_main submodularscheduler_test

