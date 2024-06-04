/*
Copyright (c) 2024 - present Advanced Micro Devices, Inc. All rights reserved.

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

#include <unistd.h>

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <string_view>
#include <vector>

#include "rdc/rdc.h"

rdc_handle_t rdc_handle;
rdc_status_t result;

constexpr std::string_view value_to_string(rdc_field_value value) {
  switch (value.type) {
    case INTEGER:
      return std::to_string(value.value.l_int);
    case DOUBLE:
      return std::to_string(value.value.dbl);
    case STRING:
      return value.value.str;
    default:
      return "UNKNOWN";
  }
}

// Cleanup consists of shutting down RDC.
rdc_status_t cleanup() {
  std::cout << "Cleaning up.\n";
  rdc_stop_embedded(rdc_handle);
  rdc_shutdown();
  return result;
}

int run() {
  char group_name[] = {"group1"};
  char field_group_name[] = {"fieldgroup1"};
  uint64_t since_timestamp = 0;
  uint64_t next_timestamp = 0;
  uint64_t start_timestamp = 0;
  uint32_t count = 0;

  // Select the embedded mode and standalone mode dynamically.
  std::cout << "Starting rdci in Embedded mode\n";

  // Init the rdc
  result = rdc_init(0);

  if (result != RDC_ST_OK) {
    std::cout << "Error initializing RDC. Return: " << rdc_status_string(result) << std::endl;
    return cleanup();
  } else {
    std::cout << "RDC Initialized.\n";
  }

  result = rdc_start_embedded(RDC_OPERATION_MODE_AUTO, &rdc_handle);
  if (result != RDC_ST_OK) {
    std::cout << "Error starting embedded RDC engine. Return: " << rdc_status_string(result)
              << std::endl;
    return cleanup();
  }

  // Get the list of devices in the system
  uint32_t gpu_index_list[RDC_MAX_NUM_DEVICES];
  result = rdc_device_get_all(rdc_handle, gpu_index_list, &count);
  if (result != RDC_ST_OK) {
    std::cout << "Error to find devices on the system. Return: " << rdc_status_string(result);
    return cleanup();
  }
  if (count == 0) {
    std::cout << "No GPUs find on the sytem ";
    return cleanup();
  } else {
    std::cout << count << " GPUs found in the system.\n";
  }

  // Create the group
  rdc_gpu_group_t group_id = 0;
  result = rdc_group_gpu_create(rdc_handle, RDC_GROUP_EMPTY, group_name, &group_id);
  if (result != RDC_ST_OK) {
    std::cout << "Error creating group. Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Created the GPU group " << group_id << std::endl;

  for (uint32_t i = 0; i < count; i++) {
    result = rdc_group_gpu_add(rdc_handle, group_id, gpu_index_list[i]);  // Add GPU 0
    if (result != RDC_ST_OK) {
      std::cout << "Error adding group. Return: " << rdc_status_string(result);
      return cleanup();
    }
    rdc_device_attributes_t attribute;
    result = rdc_device_get_attributes(rdc_handle, gpu_index_list[i], &attribute);
    if (result != RDC_ST_OK) {
      std::cout << "Error get GPU attribute. Return: " << rdc_status_string(result);
      return cleanup();
    }
    std::cout << "Add GPU " << gpu_index_list[i] << ":" << attribute.device_name << " to group "
              << group_id << std::endl;
  }

  // Create a field group to monitor multiple fields
  rdc_field_grp_t field_group_id = 0;
  std::vector<rdc_field_t> field_ids{};

  field_ids.push_back(RDC_FI_GPU_MEMORY_USAGE);
  field_ids.push_back(RDC_FI_POWER_USAGE);
  field_ids.push_back(RDC_FI_PROF_CU_UTILIZATION);
  field_ids.push_back(RDC_FI_PROF_CU_OCCUPANCY);
  field_ids.push_back(RDC_FI_PROF_FLOPS_16);
  field_ids.push_back(RDC_FI_PROF_FLOPS_32);
  field_ids.push_back(RDC_FI_PROF_FLOPS_64);
  field_ids.push_back(RDC_FI_PROF_ACTIVE_CYCLES);
  field_ids.push_back(RDC_FI_PROF_ACTIVE_WAVES);
  field_ids.push_back(RDC_FI_PROF_ELAPSED_CYCLES);
  field_ids.push_back(RDC_FI_PROF_FETCH_SIZE);
  field_ids.push_back(RDC_FI_PROF_WRITE_SIZE);
  field_ids.push_back(RDC_FI_PROF_GRBM_COUNT);
  field_ids.push_back(RDC_FI_PROF_SQ_WAVES);
  field_ids.push_back(RDC_FI_PROF_TA_BUSY_AVR);
  result = rdc_group_field_create(rdc_handle, field_ids.size(), field_ids.data(), field_group_name,
                                  &field_group_id);
  if (result != RDC_ST_OK) {
    std::cout << "Error create field group, Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Created the field group " << field_group_id << "\n";
  std::cout << "fields: ";
  for (auto field_id : field_ids) {
    std::cout << "- " << field_id << "\n";
  }

  // Let the RDC to watch the fields and groups. The fields will be updated
  // once per second, the max keep age is 1 minutes and only keep 10 samples.
  result = rdc_field_watch(rdc_handle, group_id, field_group_id,
                           static_cast<uint64_t>(1) * 10 * 1000, 60, 10);
  if (result != RDC_ST_OK) {
    std::cout << "Error watch group fields. Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Start to watch group:" << group_id << ", field_group:" << field_group_id
            << std::endl;
  std::cout << "Sleep a few seconds before retreive the data ...\n";

  // Since we are running the RDC_OPERATION_MODE_AUTO mode, the rdc_update_
  // all_fields() will be called periodically at background. If running as
  // RDC_OPERATION_MODE_MANUAL mode, we must call rdc_field_update_all()
  // periodically to take samples.
  usleep(5 * 10 * 1000);  // sleep 0.05 seconds before fetch the stats

  // Retreive the field and group information from RDC
  rdc_group_info_t group_info;
  rdc_field_group_info_t field_info;
  result = rdc_group_gpu_get_info(rdc_handle, group_id, &group_info);
  if (result != RDC_ST_OK) {
    std::cout << "Error get gpu group info. Return: " << rdc_status_string(result);
    return cleanup();
  }
  result = rdc_group_field_get_info(rdc_handle, field_group_id, &field_info);
  if (result != RDC_ST_OK) {
    std::cout << "Error get field group info. Return: " << rdc_status_string(result);
    return cleanup();
  }

  // Get the latest metrics
  std::cout << "Get the latest metrics for group:" << group_id << " field_group:" << field_group_id
            << std::endl;
  std::cout << "time_stamp\t"
            << "GPU_index\t"
            << "field_name\t\t"
            << "field_value\n";
  for (uint32_t gindex = 0; gindex < group_info.count; gindex++) {
    for (uint32_t findex = 0; findex < field_info.count; findex++) {
      rdc_field_value value;
      result = rdc_field_get_latest_value(rdc_handle, group_info.entity_ids[gindex],
                                          field_info.field_ids[findex], &value);
      if (result == RDC_ST_NOT_FOUND) {
        continue;
      }
      if (result != RDC_ST_OK) {
        std::cout << "Error get least value. Return: " << rdc_status_string(result);
        return cleanup();
      }
      // We only support the integer metrics so far
      std::cout << value.ts << "\t" << group_info.entity_ids[gindex] << "\t\t" << std::left
                << std::setw(16) << field_id_string(value.field_id) << "\t"
                << value_to_string(value) << std::endl;
    }
  }

  // Stop watching the field group
  result = rdc_field_unwatch(rdc_handle, group_id, field_group_id);
  if (result != RDC_ST_OK) {
    std::cout << "Error stop watch fields. Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Stop watch group:" << group_id << ", field_group:" << field_group_id << std::endl;

  // Get the history data last 0.1 seconds
  std::cout << "Get last 0.1 seconds metrics for group:" << group_id
            << " field_group:" << field_group_id << std::endl;
  std::cout << "time_stamp\t"
            << "GPU_index\t"
            << "field_name\t\t"
            << "field_value\n";
  start_timestamp = static_cast<uint64_t>(time(nullptr) - 10) * 1000;
  for (uint32_t gindex = 0; gindex < group_info.count; gindex++) {
    for (uint32_t findex = 0; findex < field_info.count; findex++) {
      since_timestamp = start_timestamp;
      while (true) {
        rdc_field_value value;
        result = rdc_field_get_value_since(rdc_handle, group_info.entity_ids[gindex],
                                           field_info.field_ids[findex], since_timestamp,
                                           &next_timestamp, &value);
        if (result == RDC_ST_NOT_FOUND) {
          break;
        }
        if (result != RDC_ST_OK) {
          std::cout << "Error get history data. Return: " << rdc_status_string(result);
          return cleanup();
        }
        std::cout << value.ts << "\t" << group_info.entity_ids[gindex] << "\t\t" << std::left
                  << std::setw(16) << field_id_string(value.field_id) << "\t"
                  << value_to_string(value) << std::endl;
        since_timestamp = next_timestamp;
      }  // while
    }    // for findex
  }      // for gindex

  // Delete the field group and GPU group
  result = rdc_group_field_destroy(rdc_handle, field_group_id);
  if (result != RDC_ST_OK) {
    std::cout << "Error delete field group. Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Deleted the field group " << field_group_id << std::endl;

  result = rdc_group_gpu_destroy(rdc_handle, group_id);
  if (result != RDC_ST_OK) {
    std::cout << "Error delete GPU group. Return: " << rdc_status_string(result);
    return cleanup();
  }
  std::cout << "Deleted the GPU group " << group_id << std::endl;

  return cleanup();
}

int main(int, char**) { return run(); }
