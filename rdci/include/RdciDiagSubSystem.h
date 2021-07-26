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
#ifndef RDCI_INCLUDE_RDCIDIAGSUBSYSTEM_H_
#define RDCI_INCLUDE_RDCIDIAGSUBSYSTEM_H_
#include <signal.h>
#include <map>
#include <vector>
#include <string>
#include "RdciSubSystem.h"

namespace amd {
namespace rdc {

class RdciDiagSubSystem: public RdciSubSystem {
 public:
     RdciDiagSubSystem();
     ~RdciDiagSubSystem();
     void parse_cmd_opts(int argc, char ** argv) override;
     void process() override;

 private:
     void show_help() const;

     std::string get_test_name(
               rdc_diag_test_cases_t test_case) const;

     enum OPERATIONS {
        DIAG_UNKNOWN = 0,
        DIAG_HELP,
        DIAG_RUN,
     } diag_ops_;

     rdc_gpu_group_t group_id_;
     rdc_diag_level_t run_level_;
};


}  // namespace rdc
}  // namespace amd


#endif  // RDCI_INCLUDE_RDCIDIAGSUBSYSTEM_H_
