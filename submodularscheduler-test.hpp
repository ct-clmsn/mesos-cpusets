#include <vector>
#include <valarray>

struct TestPolicy {

  int getNumItems() {
    return cpu_cost.size();
  }

  std::vector<int> getItems() {
    std::vector<int> cpus;
    for(int i = 0; i < getNumItems(); i++) {
      cpus.push_back(i);
    }

    return cpus;
  }

  int getSimilarity(const int r, const int c) {
    std::valarray<float> toret = distance[std::slice(r, cpu_cost.size(), cpu_cost.size())];
    return toret[c];
  }

  std::valarray<float> getCostVector() {
    return cpu_cost;
  }

  std::valarray<float> getWeightVector() {
    cpu_weights.resize(cpu_cost.size());
    cpu_weights = 0.0;

    for(int i = 0; i < cpu_cost.size(); i++) {
      cpu_weights[i] = cpu_cost[i]/PU;
    }

    return cpu_weights;
  }

  const float PU = 2.0;

  std::valarray<float> cpu_cost = { 1.0, 2.0, 1.0, 1.0 };

  std::valarray<float> cpu_weights;

  std::valarray<float> distance = { 0.0, 1.0, 2.0, 3.0, 
                                    1.0, 0.0, 1.0, 2.0,
                                    2.0, 1.0, 0.0, 1.0, 
                                    3.0, 2.0, 1.0, 0.0 };

};

