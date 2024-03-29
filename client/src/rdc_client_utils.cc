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

#include "rdc/rdc_client_utils.h"

#include "rdc.grpc.pb.h"  // NOLINT
#include "rdc/rdc_client.h"

namespace amd {
namespace rdc {

rdc_status_t GrpcErrorToRdcError(grpc::StatusCode grpc_err) {
  uint32_t grpc_err_int = static_cast<uint32_t>(grpc_err);
  uint32_t rdc_grpc_base_int = static_cast<uint32_t>(RDC_STATUS_GRPC_ERR_FIRST);
  uint32_t rdc_err_int = grpc_err_int + rdc_grpc_base_int;

  return static_cast<rdc_status_t>(rdc_err_int);
}

}  // namespace rdc
}  // namespace amd
