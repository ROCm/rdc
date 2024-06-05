/*
Copyright (c) 2022 - present Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "rdc_modules/rdc_rocp/RdcRocpBase.h"

#include <rocprofiler/rocprofiler.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <utility>
#include <vector>

// #include "hsa.h"
#include "rdc/rdc.h"
#include "rdc_lib/RdcLogger.h"
#include "rdc_lib/rdc_common.h"

namespace amd {
namespace rdc {

static hsa_status_t get_agent_handle_cb(hsa_agent_t agent, void* agent_arr) {
  hsa_device_type_t type;

  assert(agent_arr != nullptr);

  hsa_agent_arr_t* agent_arr_ = (hsa_agent_arr_t*)agent_arr;

  hsa_status_t status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &type);
  if (status != HSA_STATUS_SUCCESS) {
    return status;
  }

  if (type == HSA_DEVICE_TYPE_GPU) {
    if (agent_arr_->count >= agent_arr_->capacity) {
      agent_arr_->capacity *= 2;
      agent_arr_->agents =
          (hsa_agent_t*)realloc(agent_arr_->agents, agent_arr_->capacity * sizeof(hsa_agent_t));
      // realloc might set agents to nullptr upon failure
      assert(agent_arr_->agents != nullptr);
    }
    agent_arr_->agents[agent_arr_->count] = agent;
    ++agent_arr_->count;
  }

  return HSA_STATUS_SUCCESS;
}

void RdcRocpBase::read_feature(rocprofiler_t* context, const unsigned feature_count,
                               uint32_t gpu_index) {
  hsa_status_t status = rocprofiler_read(context, 0);
  assert(status == HSA_STATUS_SUCCESS);
  status = rocprofiler_get_data(context, 0);
  assert(status == HSA_STATUS_SUCCESS);
  status = rocprofiler_get_metrics(context);
  assert(status == HSA_STATUS_SUCCESS);
  switch (feature[gpu_index].data.kind) {
    case ROCPROFILER_DATA_KIND_DOUBLE:
      metric_to_value[feature[gpu_index].name] = feature[gpu_index].data.result_double;
      break;
    case ROCPROFILER_DATA_KIND_INT32:
      metric_to_value[feature[gpu_index].name] = feature[gpu_index].data.result_int32;
      break;
    case ROCPROFILER_DATA_KIND_INT64:
      metric_to_value[feature[gpu_index].name] = feature[gpu_index].data.result_int64;
      break;
    default:
      RDC_LOG(RDC_ERROR, "ERROR: Unexpected feature kind: " << feature[gpu_index].data.kind);
  }
}

static int get_agents(hsa_agent_arr_t* agent_arr) {
  int errcode = 0;
  hsa_status_t status = HSA_STATUS_SUCCESS;

  agent_arr->capacity = 1;
  agent_arr->count = 0;
  agent_arr->agents = (hsa_agent_t*)calloc(agent_arr->capacity, sizeof(hsa_agent_t));
  assert(agent_arr->agents);

  status = hsa_iterate_agents(get_agent_handle_cb, agent_arr);
  if (status != HSA_STATUS_SUCCESS) {
    errcode = -1;

    agent_arr->capacity = 0;
    agent_arr->count = 0;
    free(agent_arr->agents);
  }

  return errcode;
}

bool createHsaQueue(hsa_queue_t** queue, hsa_agent_t gpu_agent) {
  // create a single-producer queue
  hsa_status_t status = hsa_queue_create(gpu_agent, 64, HSA_QUEUE_TYPE_SINGLE, NULL, NULL,
                                         UINT32_MAX, UINT32_MAX, queue);
  if (status != HSA_STATUS_SUCCESS) {
    RDC_LOG(RDC_ERROR, "Queue creation failed");
  }

  status = hsa_amd_queue_set_priority(*queue, HSA_AMD_QUEUE_PRIORITY_HIGH);
  if (status != HSA_STATUS_SUCCESS) {
    RDC_LOG(RDC_ERROR, "HSA Queue Priority Set Failed");
  }

  return (status == HSA_STATUS_SUCCESS);
}

int RdcRocpBase::run_profiler(uint32_t gpu_index, rdc_field_t field) {
  // initialize hsa. hsa_init() will also load the profiler libs under the hood
  hsa_status_t status = HSA_STATUS_SUCCESS;

  feature[gpu_index].kind = (rocprofiler_feature_kind_t)ROCPROFILER_FEATURE_KIND_METRIC;
  feature[gpu_index].name = field_to_metric[field];

  // rocprofiler_t* contexts[agent_arr.count] = {0};
  std::vector<rocprofiler_t*> contexts;
  contexts.reserve(agent_arr.count);
  rocprofiler_properties_t properties = {
      queues[gpu_index],
      64,
      NULL,
      NULL,
  };
  int mode = (ROCPROFILER_MODE_STANDALONE | ROCPROFILER_MODE_SINGLEGROUP);
  status = rocprofiler_open(agent_arr.agents[gpu_index], &feature[gpu_index], 1,
                            &contexts[gpu_index], mode, &properties);
  const char* error_string = nullptr;
  rocprofiler_error_string(&error_string);
  if (error_string != nullptr) {
    if (error_string[0] != '\0') {
      RDC_LOG(RDC_ERROR, error_string);
    }
  }
  assert(status == HSA_STATUS_SUCCESS);

  status = rocprofiler_start(contexts[gpu_index], 0);
  assert(status == HSA_STATUS_SUCCESS);

  // this is the duration for which the counter increments from zero.
  // TODO: Return error if sampling interval is lower than this value
  usleep(10000);

  status = rocprofiler_stop(contexts[gpu_index], 0);
  assert(status == HSA_STATUS_SUCCESS);

  read_feature(contexts[gpu_index], 1, gpu_index);

  usleep(100);

  status = rocprofiler_close(contexts[gpu_index]);
  assert(status == HSA_STATUS_SUCCESS);

  return 0;
}

const char* RdcRocpBase::get_field_id_from_name(rdc_field_t field) {
  return field_to_metric.at(field);
}

// TODO - map RDC gpu_index to node_id
// use rocprofiler to check which metrics are supported
void check_metrics_supported(uint32_t node_id, std::vector<std::string>& metrics_all,
                             std::vector<std::string>& metrics_good) {
  typedef struct {
    std::vector<std::string>* metrics_all_;
    std::vector<std::string>* metrics_good_;
    uint32_t driver_node_id;
  } payload_t;
  // callback for rocprofiler to check which metrics are supported
  auto info_callback = [](const rocprofiler_info_data_t info, void* data) {
    payload_t* payload = reinterpret_cast<payload_t*>(data);
    if (info.agent_index == payload->driver_node_id) {
      auto it =
          std::find(payload->metrics_all_->begin(), payload->metrics_all_->end(), info.metric.name);
      if (it != payload->metrics_all_->end()) {
        payload->metrics_good_->push_back(info.metric.name);
        RDC_LOG(RDC_DEBUG, "  gpu-agent" << info.agent_index << " : " << info.metric.name << " : "
                                         << info.metric.description);
        if (info.metric.expr != NULL)  // if it's a derived metric, print it's formula
          RDC_LOG(RDC_DEBUG, "        " << info.metric.name << " = " << info.metric.expr);
      }
    }
    return HSA_STATUS_SUCCESS;
  };

  payload_t payload = {&metrics_all, &metrics_good, node_id};
  hsa_status_t status =
      rocprofiler_iterate_info(NULL, ROCPROFILER_INFO_KIND_METRIC, info_callback, &payload);
  if (status != HSA_STATUS_SUCCESS) {
    const char* errstr = nullptr;
    hsa_status_string(status, &errstr);
    RDC_LOG(RDC_ERROR, "hsa error: " << std::to_string(status) << " " << errstr);
  } else {
    for (auto& iter : *(payload.metrics_good_)) {
      RDC_LOG(RDC_DEBUG, iter << " : exists");
    }
  }
}

const std::vector<rdc_field_t> RdcRocpBase::get_field_ids() {
  std::vector<rdc_field_t> field_ids;
  for (auto& [k, v] : field_to_metric) {
    field_ids.push_back(k);
  }
  return field_ids;
}

RdcRocpBase::RdcRocpBase() {
  start_time = std::chrono::high_resolution_clock::now();
  hsa_status_t status = hsa_init();
  if (status != HSA_STATUS_SUCCESS) {
    const char* errstr = nullptr;
    hsa_status_string(status, &errstr);
    throw std::runtime_error("hsa error code: " + std::to_string(status) + " " + errstr);
  }

  // all fields
  static const std::map<rdc_field_t, const char*> temp_field_map_k = {
      {RDC_FI_PROF_CU_OCCUPANCY, "CU_OCCUPANCY"},
      {RDC_FI_PROF_MEAN_OCCUPANCY_PER_CU, "MEAN_OCCUPANCY_PER_CU"},
      {RDC_FI_PROF_MEAN_OCCUPANCY_PER_ACTIVE_CU, "MEAN_OCCUPANCY_PER_ACTIVE_CU"},
      {RDC_FI_PROF_ACTIVE_CYCLES, "ACTIVE_CYCLES"},
      {RDC_FI_PROF_ACTIVE_WAVES, "ACTIVE_WAVES"},
      {RDC_FI_PROF_ELAPSED_CYCLES, "ELAPSED_CYCLES"},
      // metrics below are divided by time passed
      {RDC_FI_PROF_EVAL_MEM_R_BW, "FETCH_SIZE"},
      {RDC_FI_PROF_EVAL_MEM_W_BW, "WRITE_SIZE"},
      {RDC_FI_PROF_EVAL_FLOPS_16, "TOTAL_16_OPS"},
      {RDC_FI_PROF_EVAL_FLOPS_32, "TOTAL_32_OPS"},
      {RDC_FI_PROF_EVAL_FLOPS_64, "TOTAL_64_OPS"},
  };

  std::vector<std::string> unchecked_fields;
  std::vector<std::string> checked_fields;

  for (auto& [k, v] : temp_field_map_k) {
    unchecked_fields.push_back(v);
  }

  // populate list of agents
  int errcode = get_agents(&agent_arr);
  if (errcode != 0) {
    return;
  }
  RDC_LOG(RDC_DEBUG, "Agent count: " << agent_arr.count);

  uint32_t driver_node_id = 0;
  for (uint32_t gpu_index = 0; gpu_index < agent_arr.count; gpu_index++) {
    status = hsa_agent_get_info(agent_arr.agents[gpu_index],
                                static_cast<hsa_agent_info_t>(HSA_AMD_AGENT_INFO_DRIVER_NODE_ID),
                                &driver_node_id);
    if (status != HSA_STATUS_SUCCESS) {
      const char* errstr = nullptr;
      hsa_status_string(status, &errstr);
      RDC_LOG(RDC_ERROR, "hsa error: " << std::to_string(status) << " " << errstr);
    } else {
      RDC_LOG(RDC_DEBUG, "gpu_index[" << gpu_index << "] = node_id[" << driver_node_id << "]");
    }
  }
  // only check metrics for the last GPU
  // TODO: add support for different metrics per GPU
  // currently it's assumed that all GPUs are the same
  check_metrics_supported(driver_node_id, unchecked_fields, checked_fields);

  for (auto& [k, v] : temp_field_map_k) {
    auto found = std::find(checked_fields.begin(), checked_fields.end(), v);
    if (found != checked_fields.end()) {
      field_to_metric.insert({k, v});
    }
  }

  RDC_LOG(RDC_DEBUG, "Rocprofiler supports " << field_to_metric.size() << " fields");

  for (auto& [k, v] : field_to_metric) {
    const char* str = v;
    metric_to_value.insert({str, 0.0});
  }
  assert(metric_to_value.size() == field_to_metric.size());

  for (uint32_t gpu_index = 0; gpu_index < agent_arr.count; gpu_index++) {
    for (auto& metric : metric_to_value) {
      rocprofiler_feature_t temp_feature;
      temp_feature.kind = (rocprofiler_feature_kind_t)ROCPROFILER_FEATURE_KIND_METRIC;
      temp_feature.name = metric.first;
      feature.insert({gpu_index, temp_feature});
    }
  }

  for (uint32_t gpu_index = 0; gpu_index < agent_arr.count; gpu_index++) {
    queues.push_back(nullptr);
    if (!createHsaQueue(&queues[gpu_index], agent_arr.agents[gpu_index])) {
      RDC_LOG(RDC_ERROR, "can't create queues[" << gpu_index << "]\n");
    }
  }
}

RdcRocpBase::~RdcRocpBase() {
  hsa_status_t status = HSA_STATUS_SUCCESS;
  status = hsa_shut_down();
  assert(status == HSA_STATUS_SUCCESS);
  status = hsa_shut_down();
  assert(status == HSA_STATUS_ERROR_NOT_INITIALIZED);
}

rdc_status_t RdcRocpBase::rocp_lookup(uint32_t gpu_index, rdc_field_t field, double* value) {
  if (value == nullptr) {
    return RDC_ST_BAD_PARAMETER;
  }

  hsa_status_t status = HSA_STATUS_SUCCESS;
  if (status != HSA_STATUS_SUCCESS) {
    return Rocp2RdcError(status);
  }
  switch (field) {
    default:
      const auto stop_time = std::chrono::high_resolution_clock::now();
      run_profiler(gpu_index, field);
      *value = metric_to_value[field_to_metric[field]];
      // extra processing required
      if (eval_fields.find(field) != eval_fields.end()) {
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(stop_time - start_time).count();
        RDC_LOG(RDC_DEBUG, "INDEX: " << gpu_index << " before[" << *value << "] after["
                                     << (*value / elapsed) << "]");
        *value = *value / elapsed;
      }
      break;
  }
  return Rocp2RdcError(status);
}

rdc_status_t RdcRocpBase::Rocp2RdcError(hsa_status_t status) {
  switch (status) {
    case HSA_STATUS_SUCCESS:
      return RDC_ST_OK;
    default:
      return RDC_ST_UNKNOWN_ERROR;
  }
}

}  // namespace rdc
}  // namespace amd
