/*
Copyright (c) 2021 - present Advanced Micro Devices, Inc. All rights reserved.

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

/// \file
/// RocR related helper functions for sequeneces that come up frequently

#ifndef RDC_MODULES_RDC_ROCR_COMMON_H_
#define RDC_MODULES_RDC_ROCR_COMMON_H_

#include <stdio.h>
#include <string.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "hsa/hsa.h"
#include "hsa/hsa_ext_amd.h"

namespace amd {
namespace rdc {

#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__((aligned(x)))
#endif  // __GNUC__
#endif  // _MSC_VER

#define MULTILINE(...) #__VA_ARGS__

#define ASSERT_EQ(a, b) (a == b)

void SetEnv(const char* env_var_name, const char* env_var_value);
intptr_t AlignDown(intptr_t value, size_t alignment);
void* AlignDown(void* value, size_t alignment);
void* AlignUp(void* value, size_t alignment);

// define below should be deleted. Leaving in commented out until code that
// refers to it has been corrected
// #define HSA_ARGUMENT_ALIGN_BYTES 16

// This structure holds memory pool information acquired through hsa info
// related calls, and is later used for reference when displaying the
// information.
typedef struct pool_info_t_ {
  uint32_t segment;
  size_t size;
  bool alloc_allowed;
  size_t alloc_granule;
  size_t alloc_alignment;
  bool accessible_by_all;
  uint32_t global_flag;
  uint64_t aggregate_alloc_max;
  inline bool operator==(const pool_info_t_& a) {
    if (a.segment == segment && a.size == size && a.alloc_allowed == alloc_allowed &&
        a.alloc_granule == alloc_granule && a.alloc_alignment == alloc_alignment &&
        a.accessible_by_all == accessible_by_all && a.aggregate_alloc_max == aggregate_alloc_max &&
        a.global_flag == global_flag)
      return true;
    else
      return false;
  }
} pool_info_t;

struct agent_pools_t {
  hsa_agent_t agent;
  std::vector<hsa_amd_memory_pool_t> pools;
};

/// Fill in the pool_info_t structure for the provided pool.
/// \param[in] pool Pool for which information will be retrieved
/// \param[out] pool_i Pointer to structure where pool info will be stored
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t AcquirePoolInfo(hsa_amd_memory_pool_t pool, pool_info_t* pool_i);

/// If the provided agent is associated with a GPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find GPU agents.
/// \param[in] agent Agent to evaluate if GPU
/// \param[out] data If agent is associated with a GPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t FindGPUDevice(hsa_agent_t agent, void* data);

/// If the provided agent is associated with a CPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find CPU agents.
/// \param[in] agent Agent to evaluate if CPU
/// \param[out] data If agent is associated with a CPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t FindCPUDevice(hsa_agent_t agent, void* data);

// TODO(cfreehil): get rid of FindGlobalPool and replace with FindStandardPool
hsa_status_t FindGlobalPool(hsa_amd_memory_pool_t pool, void* data);

/// If the provided agent is associated with a CPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find all the CPU agents.
/// \param[in] agent Agent to evaluate if CPU
/// \param[out] data If agent is associated with a CPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t IterateCPUAgents(hsa_agent_t agent, void* data);

/// If the provided agent is associated with a GPU, return that agent through
/// output parameter. This function is meant to be the call-back function used
/// with hsa_iterate_agents to find  all the GPU agents.
/// \param[in] agent Agent to evaluate if GPU
/// \param[out] data If agent is associated with a GPU, this pointer will point
///  to the agent upon return
/// \returns HSA_STATUS_SUCCESS if no errors are encountered.
hsa_status_t IterateGPUAgents(hsa_agent_t agent, void* data);

/// Find a GLOBAL memory pool. By this, we mean not a kernel args pool.
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t GetGlobalMemoryPool(hsa_amd_memory_pool_t pool, void* data);

/// Find a "kernel arg" pool.
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t GetKernArgMemoryPool(hsa_amd_memory_pool_t pool, void* data);

/// Find a "standard" pool. By this, we mean not a kernel args pool.
/// The pool found will have the following properties:
///     HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL: Don't care
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT: Off
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED: Don't care
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t FindStandardPool(hsa_amd_memory_pool_t pool, void* data);
hsa_status_t FindAPUStandardPool(hsa_amd_memory_pool_t pool, void* data);

/// Find a "kernel arg" pool.
/// The pool found will have the following properties:
///     HSA_AMD_MEMORY_POOL_INFO_ACCESSIBLE_BY_ALL: Don't care
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_KERNARG_INIT: On
///     HSA_AMD_MEMORY_POOL_GLOBAL_FLAG_FINE_GRAINED: Don't care
/// This function is meant to be the call-back function used
/// with hsa_amd_agent_iterate_memory_pools.
/// \param[in] pool Pool to evaluate for required properties
/// \param[in] data If pool meets criteria, this pointer will point
///  to the pool upon return
/// \returns hsa_status_t
///      -HSA_STATUS_INFO_BREAK - we found a pool that meets criteria
///      -HSA_STATUS_SUCCESS - we did not find a pool that meets the criteria
///      -else return an appropriate error code for any error encountered
hsa_status_t FindKernArgPool(hsa_amd_memory_pool_t pool, void* data);

/// Dump information about provided memory pool to STDOUT
/// \param[in] pool Pool to gather and dump information for
/// \param[in] indent Number of spaces to indent output.
/// \returns hsa_status_t HSA_STATUS_SUCCESS if no errors
hsa_status_t DumpMemoryPoolInfo(const pool_info_t* pool_i, uint32_t indent = 0);

/// Dump information about a provided pointer to STDOUT.
/// \param[in] ptr Pointer about which information is dumped.
/// \returns HSA_STATUS_SUCCESS if there are no errors
hsa_status_t DumpPointerInfo(void* ptr);

hsa_status_t GetAgentPools(std::vector<std::shared_ptr<agent_pools_t>>* agent_pools);

void throw_if_error(hsa_status_t err, const std::string& msg = "");

void throw_if_skip(const std::string& msg);

// The customize exception when the test has to be skipped
class SkipException : public std::exception {
 public:
  explicit SkipException(const char* msg) : _msg(msg) {}
  virtual const char* what() const noexcept { return _msg.c_str(); }

 private:
  std::string _msg;
};

}  // namespace rdc
}  // namespace amd

#endif  // RDC_MODULES_RDC_ROCR_COMMON_H_
