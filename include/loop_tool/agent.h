/*
Copyright (c) Facebook, Inc. and its affiliates.

This source code is licensed under the MIT license found in the
LICENSE file in the root directory of this source tree.
*/
#pragma once

#include <map>

#include "ir.h"
#include "mutate.h"
#include "serialization.h"

namespace loop_tool {

class LoopTreeAgent {
public:
  LoopTree lt;
  LoopTree::TreeRef cursor;
  typedef LoopTreeAgent& (LoopTreeAgent::*ActionFn)(
      void);
  const std::map<std::string, ActionFn> actions_fn = {
      {"up", &LoopTreeAgent::up},
      {"down", &LoopTreeAgent::down},
      {"swap_down", &LoopTreeAgent::swap_down},
      {"swap_up", &LoopTreeAgent::swap_up},
      {"split_2", &LoopTreeAgent::split_2},
      {"split_4", &LoopTreeAgent::split_4},
      {"split_8", &LoopTreeAgent::split_8},
      {"split_16", &LoopTreeAgent::split_16},
      {"split_32", &LoopTreeAgent::split_32},
      {"split_64", &LoopTreeAgent::split_64},
      {"split_128", &LoopTreeAgent::split_128},
      {"split_256", &LoopTreeAgent::split_256},
      {"merge", &LoopTreeAgent::merge},
      {"vectorize", &LoopTreeAgent::vectorize},
      {"unroll", &LoopTreeAgent::unroll},
      {"copy_input_0", &LoopTreeAgent::copy_input_0},
      {"copy_input_1", &LoopTreeAgent::copy_input_1},
      {"increase_reuse", &LoopTreeAgent::increase_reuse},
      {"decrease_reuse", &LoopTreeAgent::decrease_reuse},
  };

  typedef double (LoopTreeAgent::*EvalFn)(void);
  const std::map<std::string, EvalFn> metrics_fn = {
      {"FLOPs", &LoopTreeAgent::FLOPs},
      {"FLOPS", &LoopTreeAgent::FLOPS},
      {"seconds", &LoopTreeAgent::seconds},
    };

  LoopTreeAgent() {}
  LoopTreeAgent(const LoopTree& lt, LoopTree::TreeRef cursor = 0)
      : lt(lt), cursor(cursor) {}

  LoopTreeAgent(const LoopTreeAgent& agent) {
    lt = LoopTree(agent.lt.ir);
    cursor = agent.cursor;
  }
  ~LoopTreeAgent() {}

  /**********************************************
   * Public API
   **********************************************/
  LoopTreeAgent& apply_action(std::string action) {
    ASSERT(actions_fn.count(action) > 0) << help_actions();
    std::invoke(actions_fn.at(action), this);
    return *this;
  }

  double eval(std::string metric){
    ASSERT(metrics_fn.count(metric) > 0) << help_metrics();
    return std::invoke(metrics_fn.at(metric), this);
  }


  std::vector<std::string> get_available_actions() {
    LoopTree lt_copy(lt);
    LoopTree::TreeRef cursor_copy(cursor);

    std::vector<std::string> available_actions;

    for (auto& action : actions_fn) {
      try {
        apply_action(action.first);
        available_actions.push_back(action.first);
      } catch (std::runtime_error e) {}
      lt = lt_copy;
      cursor = cursor_copy;
    }

    return available_actions;
  }

  std::string serialize(){
    return std::to_string(cursor) + "\n" + loop_tool::serialize(lt.ir);
  }

  LoopTreeAgent deserialize(std::string ser){
    std::cout << "ser = " << ser << std::endl;
    int delimiter_pos = ser.find('\n');
    LoopTree::TreeRef cursor = std::stoi( ser.substr(0, delimiter_pos) );
    LoopTree lt(loop_tool::deserialize(ser.substr(delimiter_pos+1, ser.length())));
    return LoopTreeAgent(lt, cursor);
  }

  std::string dump() {
    return lt.dump([=](LoopTree::TreeRef ref) -> std::string {
      if (ref == cursor) {
        return "<<<<<< cursor (line " + std::to_string(cursor) + " )";
      }
      return "";
    });
  }


  /**********************************************
   * Actions
   **********************************************/
  LoopTreeAgent& up() {
    cursor = loop_tool::previous_ref(lt, cursor);
    return *this;
  }

  LoopTreeAgent& down() {
    cursor = loop_tool::next_ref(lt, cursor);
    return *this;
  }

  LoopTreeAgent& swap_up() {
    lt = loop_tool::try_swap(lt, cursor, previous_ref(lt, cursor));
    return *this;
  }

  LoopTreeAgent& swap_down() {
    lt = loop_tool::try_swap(lt, cursor, next_ref(lt, cursor));
    return *this;
  }

  LoopTreeAgent& split_2() { return split(2); }

  LoopTreeAgent& split_4() { return split(4); }

  LoopTreeAgent& split_8() { return split(8); }

  LoopTreeAgent& split_16() { return split(16); }

  LoopTreeAgent& split_32() { return split(32); }

  LoopTreeAgent& split_64() { return split(64); }

  LoopTreeAgent& split_128() { return split(128); }

  LoopTreeAgent& split_256() { return split(256); }

  LoopTreeAgent& merge() {
    lt = loop_tool::merge(lt, cursor);
    return *this;
  }

  LoopTreeAgent& vectorize() { return annotate("vectorize"); }

  LoopTreeAgent& unroll() { return annotate("unroll"); }

  LoopTreeAgent& copy_input_0() {
    auto input_id = loop_tool::get_inputs(lt, cursor)[0];
    lt = loop_tool::copy_input(lt, cursor, input_id);
    return *this;
  }

  LoopTreeAgent& copy_input_1() {
    auto input_id = loop_tool::get_inputs(lt, cursor)[1];
    lt = loop_tool::copy_input(lt, cursor, input_id);
    return *this;
  }

  LoopTreeAgent& increase_reuse() {
    lt = loop_tool::increase_reuse(lt, cursor);
    return *this;
  }

  LoopTreeAgent& decrease_reuse() {
    lt = loop_tool::decrease_reuse(lt, cursor);
    return *this;
  }

  /**********************************************
   * Evaluate metric
   **********************************************/

  double FLOPS() {
    return (double) loop_tool::FLOPS(lt);
  }

  double FLOPs() {
    return (double) loop_tool::FLOPs(lt);
  }

  double seconds() {
    return (double) loop_tool::eval_runtime(lt);
  }

  /**********************************************
   * Auxilary Functions
   **********************************************/
  std::string help_actions(){
    std::stringstream ss_actions;

    ss_actions << "Available actions are:" << std::endl;
    for (auto& action : actions_fn) {
      ss_actions << action.first << std::endl;
    }
    return ss_actions.str();
  }

  std::string help_metrics(){
    std::stringstream ss_metrics;

    ss_metrics << "Available metrics are:" << std::endl;
    for (auto& metric : metrics_fn) {
      ss_metrics << metric.first << std::endl;
    }
    return ss_metrics.str();
  }

  LoopTreeAgent& annotate(std::string annotation) {
    if (lt.annotation(cursor) == annotation) {
      lt = loop_tool::annotate(lt, cursor, "");
    } else {
      lt = loop_tool::annotate(lt, cursor, annotation);
    }
    return *this;
  }

  LoopTreeAgent& split(int split_size) {
    lt = loop_tool::split(lt, cursor, split_size);
    return *this;
  }
};

}  // namespace loop_tool
