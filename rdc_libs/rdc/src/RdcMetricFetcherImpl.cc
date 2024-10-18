/*
Copyright (c) 2020 - present Advanced Micro Devices, Inc. All rights reserved.

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
#include "rdc_lib/impl/RdcMetricFetcherImpl.h"

#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include <chrono>  //NOLINT
#include <set>
#include <vector>

#include "amd_smi/amdsmi.h"
#include "common/rdc_capabilities.h"
#include "common/rdc_fields_supported.h"
#include "rdc/rdc.h"
#include "rdc_lib/RdcLogger.h"
#include "rdc_lib/impl/SmiUtils.h"
#include "rdc_lib/rdc_common.h"

namespace amd {
namespace rdc {

static const std::unordered_map<rdc_field_t, amdsmi_event_type_t> rdc_evnt_2_smi_field = {
    {RDC_EVNT_XGMI_0_NOP_TX, AMDSMI_EVNT_XGMI_0_NOP_TX},
    {RDC_EVNT_XGMI_0_REQ_TX, AMDSMI_EVNT_XGMI_0_REQUEST_TX},
    {RDC_EVNT_XGMI_0_RESP_TX, AMDSMI_EVNT_XGMI_0_RESPONSE_TX},
    {RDC_EVNT_XGMI_0_BEATS_TX, AMDSMI_EVNT_XGMI_0_BEATS_TX},
    {RDC_EVNT_XGMI_1_NOP_TX, AMDSMI_EVNT_XGMI_1_NOP_TX},
    {RDC_EVNT_XGMI_1_REQ_TX, AMDSMI_EVNT_XGMI_1_REQUEST_TX},
    {RDC_EVNT_XGMI_1_RESP_TX, AMDSMI_EVNT_XGMI_1_RESPONSE_TX},
    {RDC_EVNT_XGMI_1_BEATS_TX, AMDSMI_EVNT_XGMI_1_BEATS_TX},

    {RDC_EVNT_XGMI_0_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_0},
    {RDC_EVNT_XGMI_1_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_1},
    {RDC_EVNT_XGMI_2_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_2},
    {RDC_EVNT_XGMI_3_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_3},
    {RDC_EVNT_XGMI_4_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_4},
    {RDC_EVNT_XGMI_5_THRPUT, AMDSMI_EVNT_XGMI_DATA_OUT_5},
};

RdcMetricFetcherImpl::RdcMetricFetcherImpl() : task_started_(true) {
  // kick off another thread for async fetch
  updater_ = std::async(std::launch::async, [this]() {
    while (task_started_) {
      std::unique_lock<std::mutex> lk(task_mutex_);
      // Wait for tasks or stop signal
      cv_.wait(lk, [this] { return !updated_tasks_.empty() || !task_started_; });
      if (updated_tasks_.empty()) continue;

      // Get the tasks
      auto item = updated_tasks_.front();
      updated_tasks_.pop();
      // The task may take long time, release lock
      lk.unlock();

      // run task
      item.task(*this, item.field);
    }  // end while (task_started_)
  });
}

RdcMetricFetcherImpl::~RdcMetricFetcherImpl() {
  // Notify the async task to stop
  task_started_ = false;
  cv_.notify_all();
}

uint64_t RdcMetricFetcherImpl::now() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return static_cast<uint64_t>(tv.tv_sec) * 1000 + tv.tv_usec / 1000;
}

void RdcMetricFetcherImpl::get_ecc(uint32_t gpu_index, rdc_field_t field_id,
                                   rdc_field_value* value) {
  amdsmi_status_t err = AMDSMI_STATUS_SUCCESS;
  amdsmi_ras_err_state_t err_state;

  amdsmi_processor_handle processor_handle;
  err = get_processor_handle_from_id(gpu_index, &processor_handle);

  // because RDC already had an established order that is different from amd-smi : map blocks to
  // fields manually
  auto field_to_block_ = [](rdc_field_t field) -> amdsmi_gpu_block_t {
    switch (field) {
      case RDC_FI_ECC_SDMA_CE:
      case RDC_FI_ECC_SDMA_UE:
        return AMDSMI_GPU_BLOCK_SDMA;
      case RDC_FI_ECC_GFX_CE:
      case RDC_FI_ECC_GFX_UE:
        return AMDSMI_GPU_BLOCK_GFX;
      case RDC_FI_ECC_MMHUB_CE:
      case RDC_FI_ECC_MMHUB_UE:
        return AMDSMI_GPU_BLOCK_MMHUB;
      case RDC_FI_ECC_ATHUB_CE:
      case RDC_FI_ECC_ATHUB_UE:
        return AMDSMI_GPU_BLOCK_ATHUB;
      case RDC_FI_ECC_PCIE_BIF_CE:
      case RDC_FI_ECC_PCIE_BIF_UE:
        return AMDSMI_GPU_BLOCK_PCIE_BIF;
      case RDC_FI_ECC_HDP_CE:
      case RDC_FI_ECC_HDP_UE:
        return AMDSMI_GPU_BLOCK_HDP;
      case RDC_FI_ECC_XGMI_WAFL_CE:
      case RDC_FI_ECC_XGMI_WAFL_UE:
        return AMDSMI_GPU_BLOCK_XGMI_WAFL;
      case RDC_FI_ECC_DF_CE:
      case RDC_FI_ECC_DF_UE:
        return AMDSMI_GPU_BLOCK_DF;
      case RDC_FI_ECC_SMN_CE:
      case RDC_FI_ECC_SMN_UE:
        return AMDSMI_GPU_BLOCK_SMN;
      case RDC_FI_ECC_SEM_CE:
      case RDC_FI_ECC_SEM_UE:
        return AMDSMI_GPU_BLOCK_SEM;
      case RDC_FI_ECC_MP0_CE:
      case RDC_FI_ECC_MP0_UE:
        return AMDSMI_GPU_BLOCK_MP0;
      case RDC_FI_ECC_MP1_CE:
      case RDC_FI_ECC_MP1_UE:
        return AMDSMI_GPU_BLOCK_MP1;
      case RDC_FI_ECC_FUSE_CE:
      case RDC_FI_ECC_FUSE_UE:
        return AMDSMI_GPU_BLOCK_FUSE;
      case RDC_FI_ECC_UMC_CE:
      case RDC_FI_ECC_UMC_UE:
        return AMDSMI_GPU_BLOCK_UMC;
      case RDC_FI_ECC_MCA_CE:
      case RDC_FI_ECC_MCA_UE:
        return AMDSMI_GPU_BLOCK_MCA;
      case RDC_FI_ECC_VCN_CE:
      case RDC_FI_ECC_VCN_UE:
        return AMDSMI_GPU_BLOCK_VCN;
      case RDC_FI_ECC_JPEG_CE:
      case RDC_FI_ECC_JPEG_UE:
        return AMDSMI_GPU_BLOCK_JPEG;
      case RDC_FI_ECC_IH_CE:
      case RDC_FI_ECC_IH_UE:
        return AMDSMI_GPU_BLOCK_IH;
      case RDC_FI_ECC_MPIO_CE:
      case RDC_FI_ECC_MPIO_UE:
        return AMDSMI_GPU_BLOCK_MPIO;
      default:
        return AMDSMI_GPU_BLOCK_INVALID;
    }
  };

  const bool is_correctable = (field_id % 2 == 0);

  if (!value) {
    return;
  }

  auto gpu_block = field_to_block_(field_id);
  if (gpu_block == AMDSMI_GPU_BLOCK_INVALID) {
    value->status = AMDSMI_STATUS_INPUT_OUT_OF_BOUNDS;
  }

  err = amdsmi_get_gpu_ecc_status(processor_handle, gpu_block, &err_state);
  if (err != AMDSMI_STATUS_SUCCESS) {
    RDC_LOG(RDC_INFO, "Error in ecc status [" << gpu_block << "]:" << err);
    value->status = err;
    return;
  }

  amdsmi_error_count_t ec;
  err = amdsmi_get_gpu_ecc_count(processor_handle, gpu_block, &ec);
  if (err != AMDSMI_STATUS_SUCCESS) {
    RDC_LOG(RDC_ERROR, "Error in ecc count [" << gpu_block << "]:" << err);
    value->status = err;
    return;
  }

  value->status = AMDSMI_STATUS_SUCCESS;
  value->type = INTEGER;
  if (is_correctable) {
    value->value.l_int = ec.correctable_count;
  } else {
    value->value.l_int = ec.uncorrectable_count;
  }
}

void RdcMetricFetcherImpl::get_ecc_total(uint32_t gpu_index, rdc_field_t field_id,
                                         rdc_field_value* value) {
  amdsmi_status_t err = AMDSMI_STATUS_SUCCESS;
  uint64_t correctable_count = 0;
  uint64_t uncorrectable_count = 0;
  amdsmi_ras_err_state_t err_state;

  amdsmi_processor_handle processor_handle;
  err = get_processor_handle_from_id(gpu_index, &processor_handle);

  if (!value) {
    return;
  }
  for (uint32_t b = AMDSMI_GPU_BLOCK_FIRST; b <= AMDSMI_GPU_BLOCK_LAST; b = b * 2) {
    err =
        amdsmi_get_gpu_ecc_status(processor_handle, static_cast<amdsmi_gpu_block_t>(b), &err_state);
    if (err != AMDSMI_STATUS_SUCCESS) {
      RDC_LOG(RDC_INFO, "Get the ecc Status error " << b << ":" << err);
      continue;
    }

    amdsmi_error_count_t ec;
    err = amdsmi_get_gpu_ecc_count(processor_handle, static_cast<amdsmi_gpu_block_t>(b), &ec);

    if (err == AMDSMI_STATUS_SUCCESS) {
      correctable_count += ec.correctable_count;
      uncorrectable_count += ec.uncorrectable_count;
    }
  }

  value->status = AMDSMI_STATUS_SUCCESS;
  value->type = INTEGER;
  if (field_id == RDC_FI_ECC_CORRECT_TOTAL) {
    value->value.l_int = correctable_count;
  }
  if (field_id == RDC_FI_ECC_UNCORRECT_TOTAL) {
    value->value.l_int = uncorrectable_count;
  }
}

bool RdcMetricFetcherImpl::async_get_pcie_throughput(uint32_t gpu_index, rdc_field_t field_id,
                                                     rdc_field_value* value) {
  if (!value) {
    return false;
  }

  do {
    std::lock_guard<std::mutex> guard(task_mutex_);
    auto metric = async_metrics_.find({gpu_index, field_id});
    if (metric != async_metrics_.end()) {
      if (now() < metric->second.last_time + metric->second.cache_ttl) {
        RDC_LOG(RDC_DEBUG,
                "Fetch " << gpu_index << ":" << field_id_string(field_id) << " from cache");
        value->status = metric->second.value.status;
        value->type = metric->second.value.type;
        value->value = metric->second.value.value;
        return false;
      }
    }

    // add to the async task queue
    MetricTask t;
    t.field = {gpu_index, field_id};
    t.task = &RdcMetricFetcherImpl::get_pcie_throughput;
    updated_tasks_.push(t);

    RDC_LOG(RDC_DEBUG,
            "Start async fetch " << gpu_index << ":" << field_id_string(field_id) << " to cache.");
  } while (0);
  cv_.notify_all();

  return true;
}

void RdcMetricFetcherImpl::get_pcie_throughput(const RdcFieldKey& key) {
  uint32_t gpu_index = key.first;
  uint64_t sent, received, max_pkt_sz;
  amdsmi_status_t ret;

  amdsmi_processor_handle processor_handle;
  ret = get_processor_handle_from_id(gpu_index, &processor_handle);

  // Return if the cache does not expire yet
  do {
    std::lock_guard<std::mutex> guard(task_mutex_);
    auto metric = async_metrics_.find(key);
    if (metric != async_metrics_.end() &&
        now() < metric->second.last_time + metric->second.cache_ttl) {
      return;
    }
  } while (0);

  ret = amdsmi_get_gpu_pci_throughput(processor_handle, &sent, &received, &max_pkt_sz);

  uint64_t curTime = now();
  MetricValue value;
  value.cache_ttl = 30 * 1000;  // cache 30 seconds
  value.value.type = INTEGER;
  do {
    std::lock_guard<std::mutex> guard(task_mutex_);
    // Create new cache entry it does not exist
    auto tx_metric = async_metrics_.find({gpu_index, RDC_FI_PCIE_TX});
    if (tx_metric == async_metrics_.end()) {
      tx_metric = async_metrics_.insert({{gpu_index, RDC_FI_PCIE_TX}, value}).first;
      tx_metric->second.value.field_id = RDC_FI_PCIE_TX;
    }
    auto rx_metric = async_metrics_.find({gpu_index, RDC_FI_PCIE_RX});
    if (rx_metric == async_metrics_.end()) {
      rx_metric = async_metrics_.insert({{gpu_index, RDC_FI_PCIE_RX}, value}).first;
      rx_metric->second.value.field_id = RDC_FI_PCIE_RX;
    }

    // Always update the status and last_time
    tx_metric->second.last_time = curTime;
    tx_metric->second.value.status = ret;
    tx_metric->second.value.ts = curTime;

    rx_metric->second.last_time = curTime;
    rx_metric->second.value.status = ret;
    rx_metric->second.value.ts = curTime;

    if (ret == AMDSMI_STATUS_NOT_SUPPORTED) {
      RDC_LOG(RDC_ERROR, "PCIe throughput not supported on GPU " << gpu_index);
      return;
    }

    if (ret == AMDSMI_STATUS_SUCCESS) {
      rx_metric->second.value.value.l_int = received;
      tx_metric->second.value.value.l_int = sent;
      RDC_LOG(RDC_DEBUG, "Async updated " << gpu_index << ":"
                                          << "RDC_FI_PCIE_RX and RDC_FI_PCIE_TX to cache.");
    }
  } while (0);
}

rdc_status_t RdcMetricFetcherImpl::bulk_fetch_smi_fields(
    rdc_gpu_field_t* fields, uint32_t fields_count,
    std::vector<rdc_gpu_field_value_t>& results) {  // NOLINT
  const std::set<rdc_field_t> rdc_bulk_fields = {
      RDC_FI_GPU_CLOCK,    // current_gfxclk * 1000000
      RDC_FI_MEMORY_TEMP,  // temperature_mem
      RDC_FI_GPU_TEMP,     // temperature_edge
      RDC_FI_POWER_USAGE,  // average_socket_power
      RDC_FI_GPU_UTIL      // average_gfx_activity
  };

  // To prevent always call the bulk API even if it is not supported,
  // the static is used to cache last try.
  static amdsmi_status_t rs = AMDSMI_STATUS_SUCCESS;
  if (rs != AMDSMI_STATUS_SUCCESS) {
    results.clear();
    return RDC_ST_NOT_SUPPORTED;
  }

  // Organize the fields per GPU
  std::map<uint32_t, std::vector<rdc_field_t>> bulk_fields;
  for (uint32_t i = 0; i < fields_count; i++) {
    if (rdc_bulk_fields.find(fields[i].field_id) != rdc_bulk_fields.end()) {
      bulk_fields[fields[i].gpu_index].push_back(fields[i].field_id);
    }
  }

  // Call the amd_smi_lib API to bulk fetch the data
  auto cur_time = now();
  auto ite = bulk_fields.begin();
  for (; ite != bulk_fields.end(); ite++) {
    amdsmi_gpu_metrics_t gpu_metrics;
    amdsmi_processor_handle processor_handle;
    rs = get_processor_handle_from_id(ite->first, &processor_handle);

    rs = amdsmi_get_gpu_metrics_info(processor_handle, &gpu_metrics);
    if (rs != AMDSMI_STATUS_SUCCESS) {
      results.clear();
      return RDC_ST_NOT_SUPPORTED;
    }
    for (uint32_t j = 0; j < ite->second.size(); j++) {
      auto field_id = ite->second[j];
      rdc_gpu_field_value_t value;
      value.gpu_index = ite->first;
      value.field_value.field_id = field_id;
      value.field_value.type = INTEGER;
      value.field_value.status = AMDSMI_STATUS_SUCCESS;
      value.field_value.ts = cur_time;

      switch (field_id) {
        case RDC_FI_GPU_CLOCK:  // current_gfxclk * 1000000
          value.field_value.value.l_int =
              static_cast<int64_t>(gpu_metrics.current_gfxclk) * 1000000;
          break;
        case RDC_FI_MEMORY_TEMP:  // temperature_mem * 1000
          value.field_value.value.l_int = static_cast<int64_t>(gpu_metrics.temperature_mem) * 1000;
          break;
        case RDC_FI_GPU_TEMP:  // temperature_edge * 1000
          value.field_value.value.l_int = static_cast<int64_t>(gpu_metrics.temperature_edge) * 1000;
          break;
        case RDC_FI_POWER_USAGE:  // average_socket_power
          value.field_value.value.l_int = static_cast<int64_t>(gpu_metrics.average_socket_power);
          // Use current_socket_power if average_socket_power is not available
          if (value.field_value.value.l_int == 65535) {
            RDC_LOG(RDC_DEBUG, "Bulk fetch "
                                   << value.gpu_index << ":"
                                   << "RDC_FI_POWER_USAGE fallback to current_socket_power.");
            value.field_value.value.l_int = static_cast<int64_t>(gpu_metrics.current_socket_power);
          }

          // Ignore if the power is 0, which will fallback to non-bulk fetch.
          if (value.field_value.value.l_int == 0) {
            RDC_LOG(RDC_DEBUG, "Bulk fetch " << value.gpu_index << ":"
                                             << "RDC_FI_POWER_USAGE fallback to regular way.");
            continue;
          }
          value.field_value.value.l_int *= 1000000;
          break;
        case RDC_FI_GPU_UTIL:  // average_gfx_activity
          value.field_value.value.l_int = static_cast<int64_t>(gpu_metrics.average_gfx_activity);
          break;
        default:
          value.field_value.status = AMDSMI_STATUS_NOT_SUPPORTED;
          break;
      }
      if (value.field_value.status == AMDSMI_STATUS_SUCCESS) {
        results.push_back(value);
      }
    }
  }

  return RDC_ST_OK;
}

constexpr double kGig = 1000000000.0;

static uint64_t sum_xgmi_read(const amdsmi_gpu_metrics_t& gpu_metrics) {
    uint64_t total = 0;
    const auto not_supported_metrics_data = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < AMDSMI_MAX_NUM_XGMI_LINKS; ++i) {
        if (gpu_metrics.xgmi_read_data_acc[i] == not_supported_metrics_data){
          continue;
        }
        total += gpu_metrics.xgmi_read_data_acc[i];
    }
    if (total == 0){
      return not_supported_metrics_data;
    }
    return total;
}

static uint64_t sum_xgmi_write(const amdsmi_gpu_metrics_t& gpu_metrics) {
    uint64_t total = 0;
    const auto not_supported_metrics_data = std::numeric_limits<uint64_t>::max();
    for (int i = 0; i < AMDSMI_MAX_NUM_XGMI_LINKS; ++i) {
        if (gpu_metrics.xgmi_write_data_acc[i] == not_supported_metrics_data){
          continue;
        }
        total += gpu_metrics.xgmi_write_data_acc[i];
    }
    if (total == 0){
      return not_supported_metrics_data;
    }
    return total;
}

rdc_status_t RdcMetricFetcherImpl::fetch_smi_field(uint32_t gpu_index, rdc_field_t field_id,
                                                   rdc_field_value* value) {
  if (!value) {
    return RDC_ST_BAD_PARAMETER;
  }
  bool async_fetching = false;
  std::shared_ptr<FieldSMIData> smi_data;

  amdsmi_processor_handle processor_handle = {};

  amdsmi_status_t ret = get_processor_handle_from_id(gpu_index, &processor_handle);
  if (ret != AMDSMI_STATUS_SUCCESS) {
    RDC_LOG(RDC_ERROR, "Failed to get processor handle for GPU " << gpu_index << " error: " << ret);
    return Smi2RdcError(ret);
  }

  if (!is_field_valid(field_id)) {
    RDC_LOG(RDC_ERROR, "Fail to fetch field " << field_id << " which is not supported");
    return RDC_ST_NOT_SUPPORTED;
  }

  value->ts = now();
  value->field_id = field_id;
  value->status = AMDSMI_STATUS_NOT_SUPPORTED;

  auto read_smi_counter = [&](void) {
    RdcFieldKey f_key(gpu_index, field_id);
    smi_data = get_smi_data(f_key);
    if (smi_data == nullptr) {
      value->status = AMDSMI_STATUS_NOT_SUPPORTED;
      return;
    }

    value->status = amdsmi_gpu_read_counter(smi_data->evt_handle, &smi_data->counter_val);
    value->value.l_int = smi_data->counter_val.value;
    value->type = INTEGER;
  };

  auto read_gpu_metrics_uint64_t = [&](void) {
    amdsmi_gpu_metrics_t gpu_metrics;
    value->status = amdsmi_get_gpu_metrics_info(processor_handle, &gpu_metrics);
    RDC_LOG(RDC_DEBUG, "Read the gpu metrics:" << value->status);
    if (value->status != AMDSMI_STATUS_SUCCESS) {
      return;
    }

    const std::unordered_map<rdc_field_t, uint64_t> rdc_field_to_gpu_metrics = {
        {RDC_FI_XGMI_0_READ_KB, gpu_metrics.xgmi_read_data_acc[0]},
        {RDC_FI_XGMI_1_READ_KB, gpu_metrics.xgmi_read_data_acc[1]},
        {RDC_FI_XGMI_2_READ_KB, gpu_metrics.xgmi_read_data_acc[2]},
        {RDC_FI_XGMI_3_READ_KB, gpu_metrics.xgmi_read_data_acc[3]},
        {RDC_FI_XGMI_4_READ_KB, gpu_metrics.xgmi_read_data_acc[4]},
        {RDC_FI_XGMI_5_READ_KB, gpu_metrics.xgmi_read_data_acc[5]},
        {RDC_FI_XGMI_6_READ_KB, gpu_metrics.xgmi_read_data_acc[6]},
        {RDC_FI_XGMI_7_READ_KB, gpu_metrics.xgmi_read_data_acc[7]},
        {RDC_FI_XGMI_TOTAL_READ_KB, sum_xgmi_read(gpu_metrics)},
        {RDC_FI_XGMI_0_WRITE_KB, gpu_metrics.xgmi_write_data_acc[0]},
        {RDC_FI_XGMI_1_WRITE_KB, gpu_metrics.xgmi_write_data_acc[1]},
        {RDC_FI_XGMI_2_WRITE_KB, gpu_metrics.xgmi_write_data_acc[2]},
        {RDC_FI_XGMI_3_WRITE_KB, gpu_metrics.xgmi_write_data_acc[3]},
        {RDC_FI_XGMI_4_WRITE_KB, gpu_metrics.xgmi_write_data_acc[4]},
        {RDC_FI_XGMI_5_WRITE_KB, gpu_metrics.xgmi_write_data_acc[5]},
        {RDC_FI_XGMI_6_WRITE_KB, gpu_metrics.xgmi_write_data_acc[6]},
        {RDC_FI_XGMI_7_WRITE_KB, gpu_metrics.xgmi_write_data_acc[7]},
        {RDC_FI_XGMI_TOTAL_WRITE_KB, sum_xgmi_write(gpu_metrics)},
        {RDC_FI_PCIE_BANDWIDTH, gpu_metrics.pcie_bandwidth_inst},
    };

    // In gpu_metrics,the max value means not supported
    const auto not_supported_metrics_data = std::numeric_limits<uint64_t>::max();
    auto gpu_metrics_value_ite = rdc_field_to_gpu_metrics.find(field_id);
    if (gpu_metrics_value_ite != rdc_field_to_gpu_metrics.end()) {
      if (gpu_metrics_value_ite->second != not_supported_metrics_data) {
        value->value.l_int = gpu_metrics_value_ite->second;
        value->type = INTEGER;
        return;
      } else {
        RDC_LOG(RDC_DEBUG, "The gpu metrics return max value which indicate not supported:"
                               << gpu_metrics_value_ite->second);
      }
    }
    value->status = AMDSMI_STATUS_NOT_SUPPORTED;
  };

  switch (field_id) {
    case RDC_FI_GPU_MEMORY_USAGE: {
      uint64_t u64 = 0;
      value->status = amdsmi_get_gpu_memory_usage(processor_handle, AMDSMI_MEM_TYPE_VRAM, &u64);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = static_cast<int64_t>(u64);
      }
      break;
    }
    case RDC_FI_GPU_MEMORY_TOTAL: {
      uint64_t u64 = 0;
      value->status = amdsmi_get_gpu_memory_total(processor_handle, AMDSMI_MEM_TYPE_VRAM, &u64);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = static_cast<int64_t>(u64);
      }
      break;
    }
    case RDC_FI_GPU_COUNT: {
      uint32_t processor_count = 0;
      // amdsmi is initialized in AMDSMI_INIT_AMD_GPUS mode -> returned sockets are GPUs
      value->status = get_processor_count(processor_count);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = static_cast<int64_t>(processor_count);
      }
    } break;
    case RDC_FI_POWER_USAGE: {
      amdsmi_power_info_t power_info = {};
      value->status = amdsmi_get_power_info(processor_handle, &power_info);
      value->type = INTEGER;
      if (value->status != AMDSMI_STATUS_SUCCESS) {
        RDC_LOG(RDC_ERROR, "amdsmi_get_power_info failed!");
        break;
      }

      // Use current_socket_power if average_socket_power is not available
      if (power_info.average_socket_power != 65535) {
        RDC_LOG(RDC_DEBUG, "AMDSMI: using average_socket_power");
        value->value.l_int = static_cast<int64_t>(power_info.average_socket_power) * 1000 * 1000;
        break;
      }

      if (power_info.current_socket_power != 65535) {
        RDC_LOG(RDC_DEBUG, "AMDSMI: using current_socket_power");
        value->value.l_int = static_cast<int64_t>(power_info.current_socket_power) * 1000 * 1000;
        break;
      }

      value->status = AMDSMI_STATUS_NOT_SUPPORTED;
      RDC_LOG(RDC_ERROR, "AMDSMI: cannot get POWER_USAGE");
      return RDC_ST_NO_DATA;
    }
    case RDC_FI_GPU_CLOCK:
    case RDC_FI_MEM_CLOCK: {
      amdsmi_clk_type_t clk_type = AMDSMI_CLK_TYPE_SYS;
      if (field_id == RDC_FI_MEM_CLOCK) {
        clk_type = AMDSMI_CLK_TYPE_MEM;
      }
      amdsmi_frequencies_t f = {};
      value->status = amdsmi_get_clk_freq(processor_handle, clk_type, &f);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = f.frequency[f.current];
      }
      break;
    }
    case RDC_FI_GPU_UTIL: {
      amdsmi_engine_usage_t engine_usage;
      value->status = amdsmi_get_gpu_activity(processor_handle, &engine_usage);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = static_cast<int64_t>(engine_usage.gfx_activity);
      }
      break;
    }
    case RDC_FI_DEV_NAME: {
      amdsmi_asic_info_t asic_info;
      value->status = amdsmi_get_gpu_asic_info(processor_handle, &asic_info);
      value->type = STRING;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        memcpy(value->value.str, asic_info.market_name, sizeof(asic_info.market_name));
      }
      break;
    }
    case RDC_FI_GPU_TEMP:
    case RDC_FI_MEMORY_TEMP: {
      int64_t i64 = 0;
      amdsmi_temperature_type_t sensor_type = AMDSMI_TEMPERATURE_TYPE_EDGE;
      if (field_id == RDC_FI_MEMORY_TEMP) {
        sensor_type = AMDSMI_TEMPERATURE_TYPE_VRAM;
      }
      value->status =
          amdsmi_get_temp_metric(processor_handle, sensor_type, AMDSMI_TEMP_CURRENT, &i64);

      // fallback to hotspot temperature as some card may not have edge temperature.
      if (sensor_type == AMDSMI_TEMPERATURE_TYPE_EDGE &&
          value->status == AMDSMI_STATUS_NOT_SUPPORTED) {
        sensor_type = AMDSMI_TEMPERATURE_TYPE_JUNCTION;
        value->status =
            amdsmi_get_temp_metric(processor_handle, sensor_type, AMDSMI_TEMP_CURRENT, &i64);
      }

      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = i64 * 1000;
      }
      break;
    }
    case RDC_FI_OAM_ID: {
      amdsmi_asic_info_t asic_info;
      value->status = amdsmi_get_gpu_asic_info(processor_handle, &asic_info);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        // 0xFFFF means not supported for OAM ID
        if (asic_info.oam_id == 0xFFFF) {
          value->status = AMDSMI_STATUS_NOT_SUPPORTED;
        } else {
          value->value.l_int = asic_info.oam_id;
        }
      }
      break;
    }
    case RDC_FI_GPU_MM_ENC_UTIL: {
      value->status = AMDSMI_STATUS_NOT_SUPPORTED;
      RDC_LOG(RDC_ERROR, "AMDSMI No Supported: cannot get MM_ENC_ACTIVITY");
      return RDC_ST_NO_DATA;
    }
    case RDC_FI_GPU_MM_DEC_UTIL: {
      constexpr uint32_t kUTILIZATION_COUNTERS(1);
      amdsmi_utilization_counter_t utilization_counters[kUTILIZATION_COUNTERS];
      utilization_counters[0].type = AMDSMI_COARSE_DECODER_ACTIVITY;
      uint64_t timestamp;

      value->status = amdsmi_get_utilization_count(processor_handle, utilization_counters,
                                                  kUTILIZATION_COUNTERS, &timestamp);
      value->type = INTEGER;
      if (value->status == AMDSMI_STATUS_SUCCESS) {
        value->value.l_int = static_cast<int64_t>(utilization_counters[0].value);
      }
      break;
    }
    case RDC_FI_ECC_CORRECT_TOTAL:
    case RDC_FI_ECC_UNCORRECT_TOTAL:
      get_ecc_total(gpu_index, field_id, value);
      break;
    case RDC_FI_ECC_SDMA_CE:
    case RDC_FI_ECC_SDMA_UE:
    case RDC_FI_ECC_GFX_CE:
    case RDC_FI_ECC_GFX_UE:
    case RDC_FI_ECC_MMHUB_CE:
    case RDC_FI_ECC_MMHUB_UE:
    case RDC_FI_ECC_ATHUB_CE:
    case RDC_FI_ECC_ATHUB_UE:
    case RDC_FI_ECC_PCIE_BIF_CE:
    case RDC_FI_ECC_PCIE_BIF_UE:
    case RDC_FI_ECC_HDP_CE:
    case RDC_FI_ECC_HDP_UE:
    case RDC_FI_ECC_XGMI_WAFL_CE:
    case RDC_FI_ECC_XGMI_WAFL_UE:
    case RDC_FI_ECC_DF_CE:
    case RDC_FI_ECC_DF_UE:
    case RDC_FI_ECC_SMN_CE:
    case RDC_FI_ECC_SMN_UE:
    case RDC_FI_ECC_SEM_CE:
    case RDC_FI_ECC_SEM_UE:
    case RDC_FI_ECC_MP0_CE:
    case RDC_FI_ECC_MP0_UE:
    case RDC_FI_ECC_MP1_CE:
    case RDC_FI_ECC_MP1_UE:
    case RDC_FI_ECC_FUSE_CE:
    case RDC_FI_ECC_FUSE_UE:
    case RDC_FI_ECC_UMC_CE:
    case RDC_FI_ECC_UMC_UE:
    case RDC_FI_ECC_MCA_CE:
    case RDC_FI_ECC_MCA_UE:
    case RDC_FI_ECC_VCN_CE:
    case RDC_FI_ECC_VCN_UE:
    case RDC_FI_ECC_JPEG_CE:
    case RDC_FI_ECC_JPEG_UE:
    case RDC_FI_ECC_IH_CE:
    case RDC_FI_ECC_IH_UE:
    case RDC_FI_ECC_MPIO_CE:
    case RDC_FI_ECC_MPIO_UE:
      get_ecc(gpu_index, field_id, value);
      break;
    case RDC_FI_PCIE_TX:
    case RDC_FI_PCIE_RX:
      async_fetching = async_get_pcie_throughput(gpu_index, field_id, value);
      break;
    case RDC_EVNT_XGMI_0_NOP_TX:
    case RDC_EVNT_XGMI_0_REQ_TX:
    case RDC_EVNT_XGMI_0_RESP_TX:
    case RDC_EVNT_XGMI_0_BEATS_TX:
    case RDC_EVNT_XGMI_1_NOP_TX:
    case RDC_EVNT_XGMI_1_REQ_TX:
    case RDC_EVNT_XGMI_1_RESP_TX:
    case RDC_EVNT_XGMI_1_BEATS_TX:
      read_smi_counter();
      break;
    case RDC_EVNT_XGMI_0_THRPUT:
    case RDC_EVNT_XGMI_1_THRPUT:
    case RDC_EVNT_XGMI_2_THRPUT:
    case RDC_EVNT_XGMI_3_THRPUT:
    case RDC_EVNT_XGMI_4_THRPUT:
    case RDC_EVNT_XGMI_5_THRPUT: {
      double coll_time_sec = 0;
      read_smi_counter();
      if (value->status == RDC_ST_OK) {
        if (smi_data->counter_val.time_running > 0) {
          coll_time_sec = static_cast<double>(smi_data->counter_val.time_running) / kGig;
          value->value.l_int = (value->value.l_int * 32) / coll_time_sec;
        } else {
          value->value.l_int = 0;
        }
      }
      break;
    }
    case RDC_FI_XGMI_0_READ_KB:
    case RDC_FI_XGMI_1_READ_KB:
    case RDC_FI_XGMI_2_READ_KB:
    case RDC_FI_XGMI_3_READ_KB:
    case RDC_FI_XGMI_4_READ_KB:
    case RDC_FI_XGMI_5_READ_KB:
    case RDC_FI_XGMI_6_READ_KB:
    case RDC_FI_XGMI_7_READ_KB:
    case RDC_FI_XGMI_TOTAL_READ_KB:
    case RDC_FI_XGMI_0_WRITE_KB:
    case RDC_FI_XGMI_1_WRITE_KB:
    case RDC_FI_XGMI_2_WRITE_KB:
    case RDC_FI_XGMI_3_WRITE_KB:
    case RDC_FI_XGMI_4_WRITE_KB:
    case RDC_FI_XGMI_5_WRITE_KB:
    case RDC_FI_XGMI_6_WRITE_KB:
    case RDC_FI_XGMI_7_WRITE_KB:
    case RDC_FI_XGMI_TOTAL_WRITE_KB:
    case RDC_FI_PCIE_BANDWIDTH:
      read_gpu_metrics_uint64_t();
      break;

    default:
      break;
  }

  int64_t latency = now() - value->ts;
  if (value->status != AMDSMI_STATUS_SUCCESS) {
    if (async_fetching) {  //!< Async fetching is not an error
      RDC_LOG(RDC_DEBUG, "Async fetch " << field_id_string(field_id));
    } else {
      RDC_LOG(RDC_ERROR, "Fail to fetch " << gpu_index << ":" << field_id_string(field_id)
                                          << " with rsmi error code " << value->status
                                          << ", latency " << latency);
    }
  } else if (value->type == INTEGER) {
    RDC_LOG(RDC_DEBUG, "Fetch " << gpu_index << ":" << field_id_string(field_id) << ":"
                                << value->value.l_int << ", latency " << latency);
  } else if (value->type == DOUBLE) {
    RDC_LOG(RDC_DEBUG, "Fetch " << gpu_index << ":" << field_id_string(field_id) << ":"
                                << value->value.dbl << ", latency " << latency);
  } else if (value->type == STRING) {
    RDC_LOG(RDC_DEBUG, "Fetch " << gpu_index << ":" << field_id_string(field_id) << ":"
                                << value->value.str << ", latency " << latency);
  }

  return value->status == AMDSMI_STATUS_SUCCESS ? RDC_ST_OK : RDC_ST_MSI_ERROR;
}

std::shared_ptr<FieldSMIData> RdcMetricFetcherImpl::get_smi_data(RdcFieldKey key) {
  std::map<RdcFieldKey, std::shared_ptr<FieldSMIData>>::iterator r_info = smi_data_.find(key);

  if (r_info != smi_data_.end()) {
    return r_info->second;
  }
  return nullptr;
}

static rdc_status_t init_smi_counter(RdcFieldKey fk, amdsmi_event_group_t grp,
                                     amdsmi_event_handle_t* handle) {
  amdsmi_status_t ret;
  uint32_t counters_available;
  uint32_t dv_ind = fk.first;
  rdc_field_t f = fk.second;

  assert(handle != nullptr);

  amdsmi_processor_handle processor_handle;
  ret = get_processor_handle_from_id(dv_ind, &processor_handle);

  ret = amdsmi_gpu_counter_group_supported(processor_handle, grp);

  if (ret != AMDSMI_STATUS_SUCCESS) {
    return Smi2RdcError(ret);
  }

  ret = amdsmi_get_gpu_available_counters(processor_handle, grp, &counters_available);
  if (ret != AMDSMI_STATUS_SUCCESS) {
    return Smi2RdcError(ret);
  }
  if (counters_available == 0) {
    return RDC_ST_INSUFF_RESOURCES;
  }

  amdsmi_event_type_t evt = rdc_evnt_2_smi_field.at(f);

  // Temporarily get DAC capability
  ScopedCapability sc(CAP_DAC_OVERRIDE, CAP_EFFECTIVE);

  if (sc.error()) {
    RDC_LOG(RDC_ERROR, "Failed to acquire required capabilities. Errno " << sc.error());
    return RDC_ST_PERM_ERROR;
  }

  ret = amdsmi_gpu_create_counter(processor_handle, evt, handle);
  if (ret != AMDSMI_STATUS_SUCCESS) {
    return Smi2RdcError(ret);
  }

  ret = amdsmi_gpu_control_counter(*handle, AMDSMI_CNTR_CMD_START, nullptr);

  // Release DAC capability
  sc.Relinquish();

  if (sc.error()) {
    RDC_LOG(RDC_ERROR, "Failed to relinquish capabilities. Errno " << sc.error());
    return RDC_ST_PERM_ERROR;
  }

  return Smi2RdcError(ret);
}

rdc_status_t RdcMetricFetcherImpl::delete_smi_handle(RdcFieldKey fk) {
  amdsmi_status_t ret;

  switch (fk.second) {
    case RDC_EVNT_XGMI_0_NOP_TX:
    case RDC_EVNT_XGMI_0_REQ_TX:
    case RDC_EVNT_XGMI_0_RESP_TX:
    case RDC_EVNT_XGMI_0_BEATS_TX:
    case RDC_EVNT_XGMI_1_NOP_TX:
    case RDC_EVNT_XGMI_1_REQ_TX:
    case RDC_EVNT_XGMI_1_RESP_TX:
    case RDC_EVNT_XGMI_1_BEATS_TX:
    case RDC_EVNT_XGMI_0_THRPUT:
    case RDC_EVNT_XGMI_1_THRPUT:
    case RDC_EVNT_XGMI_2_THRPUT:
    case RDC_EVNT_XGMI_3_THRPUT:
    case RDC_EVNT_XGMI_4_THRPUT:
    case RDC_EVNT_XGMI_5_THRPUT: {
      amdsmi_event_handle_t h;
      if (smi_data_.find(fk) == smi_data_.end()) {
        return RDC_ST_NOT_SUPPORTED;
      }

      h = smi_data_[fk]->evt_handle;

      // Stop counting.
      ret = amdsmi_gpu_control_counter(h, AMDSMI_CNTR_CMD_STOP, nullptr);
      if (ret != AMDSMI_STATUS_SUCCESS) {
        smi_data_.erase(fk);

        RDC_LOG(RDC_ERROR, "Error in stopping event counter: " << Smi2RdcError(ret));
        return Smi2RdcError(ret);
      }

      // Release all resources (e.g., counter and memory resources) associated
      // with evnt_handle.
      ret = amdsmi_gpu_destroy_counter(h);

      smi_data_.erase(fk);
      return Smi2RdcError(ret);
    }
    default:
      return RDC_ST_NOT_SUPPORTED;
  }
  return RDC_ST_OK;
}

rdc_status_t RdcMetricFetcherImpl::acquire_smi_handle(RdcFieldKey fk) {
  rdc_status_t ret = RDC_ST_OK;

  auto get_evnt_handle = [&](amdsmi_event_group_t grp) {
    amdsmi_event_handle_t handle;
    rdc_status_t result;

    if (get_smi_data(fk) != nullptr) {
      // This event has already been initialized.
      return RDC_ST_ALREADY_EXIST;
    }

    result = init_smi_counter(fk, grp, &handle);
    if (result != RDC_ST_OK) {
      RDC_LOG(RDC_ERROR, "Failed to init SMI counter. Return:" << result);
      return result;
    }
    auto fsh = std::shared_ptr<FieldSMIData>(new FieldSMIData);

    if (fsh == nullptr) {
      return RDC_ST_INSUFF_RESOURCES;
    }

    fsh->evt_handle = handle;

    smi_data_[fk] = fsh;

    return RDC_ST_OK;
  };

  switch (fk.second) {
    case RDC_EVNT_XGMI_0_NOP_TX:
    case RDC_EVNT_XGMI_0_REQ_TX:
    case RDC_EVNT_XGMI_0_RESP_TX:
    case RDC_EVNT_XGMI_0_BEATS_TX:
    case RDC_EVNT_XGMI_1_NOP_TX:
    case RDC_EVNT_XGMI_1_REQ_TX:
    case RDC_EVNT_XGMI_1_RESP_TX:
    case RDC_EVNT_XGMI_1_BEATS_TX:
      ret = get_evnt_handle(AMDSMI_EVNT_GRP_XGMI);
      break;

    case RDC_EVNT_XGMI_0_THRPUT:
    case RDC_EVNT_XGMI_1_THRPUT:
    case RDC_EVNT_XGMI_2_THRPUT:
    case RDC_EVNT_XGMI_3_THRPUT:
    case RDC_EVNT_XGMI_4_THRPUT:
    case RDC_EVNT_XGMI_5_THRPUT:
      ret = get_evnt_handle(AMDSMI_EVNT_GRP_XGMI_DATA_OUT);
      break;

    default:
      break;
  }

  if (ret == RDC_ST_INSUFF_RESOURCES) {
    amd::rdc::fld_id2name_map_t& field_id_to_descript =
        amd::rdc::get_field_id_description_from_id();

    RDC_LOG(RDC_ERROR, "No event counters are available for "
                           << field_id_to_descript.at(fk.second).enum_name << " event.");
  } else if (ret != RDC_ST_OK) {
    RDC_LOG(RDC_ERROR, "Error in getting event counter handle: " << ret);
  }
  return ret;
}

}  // namespace rdc
}  // namespace amd
