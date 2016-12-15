#include "cgroupcpusets.hpp"

int main(int argc, char** argv) {
  std::cout << "\ncpuset groups" << std::endl;

  Try<std::vector<std::string> > cpuset_groups = get_cpuset_groups();
  for(auto i = std::begin(cpuset_groups.get()); i != std::end(cpuset_groups.get()); i++) {
    std::cout << *i << std::endl;
  }

  std::cout << "\ncpuset cpus" << std::endl;
  Try<std::vector<int> > cpuset_cpus = get_cpuset_cpus();
  for(auto i = std::begin(cpuset_cpus.get()); i != std::end(cpuset_cpus.get()); i++) {
    std::cout << *i << std::endl;
  }

  std::cout << get_cpu_max_shares().get() << std::endl;

  int pipe_a[2], pipe_b[2], pipe_c[2];

  pipe(pipe_a);
  pipe(pipe_b);
  pipe(pipe_c);

  pid_t pid = fork();

  if(pid == -1) {
    perror("error");
    exit(1);
  }

  if(pid == 0) {
    int flag = 100;
    pid_t curpid = getpid();

    close(pipe_a[0]);
    write(pipe_a[1], &curpid, sizeof(pid_t));

    close(pipe_b[1]);
    while(read(pipe_b[0], &flag, sizeof(int))) {
    }

    sleep(10);

    write(pipe_a[1], &flag, sizeof(int));
    exit(0);
  }
  else {
    int status;
    pid_t chld_pid, w;
    close(pipe_a[1]);
    while(read(pipe_a[0], &chld_pid, sizeof(pid_t))) {}
    printf("child pid_t\t%d\n", chld_pid);

    Try<std::vector<int> > cpuset_mems = get_cpuset_mems();

    create_cpuset_group("demo");
    assign_cpuset_group_cpus("demo", cpuset_cpus.get());
    assign_cpuset_group_mems("demo", cpuset_mems.get());
    attach_cpuset_group_pid("demo", chld_pid); 
    sleep(10);

    Try<std::vector<std::string> > groups = get_cpuset_groups();
    if(groups.isError()) {
      perror("ERROR WITH CPUSET GROUPS");
      return -1;
    }

    Try<std::map<int,int> > cpuset_cpu_util = get_cpuset_cpu_utilization(groups.get());
    if(cpuset_cpu_util.isError()) {
      perror("ERROR WITH CPUSET UTIL");
      return -1;
    }

    std::cout << "CPU SETUTILS!" << std::endl;
    std::for_each(std::begin(cpuset_cpu_util.get()), std::end(cpuset_cpu_util.get()),
      [](const std::pair<int,int> p) {
      std::cout << p.first << "\t" << p.second << std::endl;
    });
 
    destroy_cpuset_group("demo");

    do {
      w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    int val = 1;
    while(read(pipe_a[0], &val, sizeof(int))) { }
  }

  return 1;
}

