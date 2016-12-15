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
//  The algorithm used for this Scheduler is based
//  on an extractive summarization technique found
//  in the paper,
//
//  "Multi-document Summarization via Budgeted
//   Maximization of Submodular Functions" by
//  Hui Lin & Jeff Bilmes naaclhlt2010.pdf
//
//  This technique exploits the diminishing returns
//  property associated with submodular selection
//  algorithms.
//
//  This technique selects B cores, each with a cost
//  of 1 and with a "value" equivalent to the latency
//  of selecting a core multipled by the weighted
//  cost of work being currently performed on the
//  core being considered
//
//  By default the least expensive core will be
//  will be selected.
//
//  ct.clmsn
//

#ifndef __MESOSSUBMODSCHEDULER__
#define __MESOSSUBMODSCHEDULER__ 1

#include <set>
#include <map>
#include <valarray>
#include <algorithm>
#include <limits>
#include <vector>
#include <iterator>
#include <cmath>

#include "stout/foreach.hpp"

using namespace std;

template< typename IndexSetPolicy >
class SubmodularScheduler : private IndexSetPolicy {
  using IndexSetPolicy::getItems;
  using IndexSetPolicy::getSimilarity;
  using IndexSetPolicy::getCostVector;
  using IndexSetPolicy::getWeightVector;

private:

  std::set<int> U, V;

  float f(
    const std::valarray<float>& weights, 
    std::set<int> S) {

    float fscore = 0.0;
    std::vector<int> diff;

    std::set_difference(
      V.begin(), V.end(),
      S.begin(), S.end(),
      std::inserter(diff, diff.begin()));

    foreach(int core_i, diff) {
      const float i_weight = weights[core_i];

      foreach(int core_j, S) {
        const float j_weight = weights[core_j];
        float latency = getSimilarity(core_i, core_j); 
        latency = (FP_ZERO == std::fpclassify(latency)) ? 1e-10 : latency;
        fscore += ( (i_weight + j_weight) / latency );
      }
    }

    return fscore;
  }

  static bool pair_cmp(
    std::pair<int, float> a,
    std::pair<int, float> b) {

    return (a.second < b.second);
  }

  static bool fin_cmp(
    std::pair< std::set<int>, float> a,
    std::pair< std::set<int>, float> b) {

    return a.second < b.second;
  }

  static float sum_func(
    float a, 
    std::pair<int, float> b) {

    return a + b.second;
  }

public:

  SubmodularScheduler() {
  }

  void operator()(
    std::set<int>& Gf,
    const float B,
    const float r = 1.0,
    const float differenceEpsilon = 0.75) {

  const std::vector<int> nCores = getItems();
  for(int i = 0; i < nCores.size(); i++) {
    U.insert(nCores[i]);
    V.insert(nCores[i]);
  }

  // cost is the number of
  // tasks per core / total
  // tasks on cpu
  //
  std::valarray<float> cost =
    getCostVector();

  // num tasks per core weighted by num processing 
  // units (physical threads) per core
  //
  std::valarray<float> weights =
    getWeightVector();

  std::set<int> G;

  while(U.size() > 0) {
    std::vector< std::pair<int, float> > pick_k;

    foreach(int l, U) {
      std::set<int> Gltmp = G;
      Gltmp.insert(l);
      const float cl = cost[l];
      pick_k.push_back(
        std::make_pair(l, (f(weights, Gltmp) - f(weights, G)) / std::pow(cl, r))
      );
    }

    // find the cheapest core to add to the list
    //
    std::vector< std::pair<int, float> >::iterator k_itr =
      std::max_element(pick_k.begin(), pick_k.end(), pair_cmp);

    std::vector< std::pair<int, float> > cost_test;
    foreach(int obj_i, G) {
      const float ci = cost[obj_i];
      const float k_element = cost[k_itr->first];
      cost_test.push_back(
        std::make_pair(obj_i, ci + k_element)
      );
    }

    std::set<int> Gktmp = G;
    Gktmp.insert(k_itr->first);

    const float cost_test_sum = 
      std::accumulate(cost_test.begin(), cost_test.end(), 0.0, sum_func);

    if( (cost_test_sum <= B) &&
        ((f(weights, Gktmp) - f(weights, G)) >= 0.0) ) {
      G.insert(k_itr->first);
    }

    U.erase(k_itr->first);
  }

  std::vector< std::pair<int, float> > vlist;
  foreach(int v, V) {
    const float vitr = cost[v];
    std::set<int> vset;
    vset.insert(v);
    if(vitr <= B) {
      vlist.push_back( std::make_pair(v, f(weights, vset)) );
    }
  }

  std::vector< pair<int, float> >::iterator vstar =
    max_element(vlist.begin(), vlist.end(), pair_cmp);

  std::set<int> vstarset;
  vstarset.insert(vstar->first);

  std::vector< std::pair< std::set<int>, float> > fin;
  fin.push_back( std::make_pair(vstarset, f(weights, vstarset)) );
  fin.push_back( std::make_pair(G, f(weights, G)) );

  std::vector< std::pair< std::set<int>, float> >::iterator finG =
    std::max_element(fin.begin(), fin.end(), fin_cmp);

  Gf = finG->first;
}

};

#endif
