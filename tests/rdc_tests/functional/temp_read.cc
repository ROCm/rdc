/*
Copyright (c) 2020 - Advanced Micro Devices, Inc. All rights reserved.

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
#include <stdint.h>
#include <stddef.h>

#include <iostream>
#include <string>
#include <map>

#include "gtest/gtest.h"
#include "rdc/rdc_client.h"
#include "rdc_tests/functional/temp_read.h"
#include "rdc_tests/test_common.h"

static const std::map<uint32_t, std::string> kTempSensorNameMap = {
    {RSMI_TEMP_TYPE_MEMORY, "Memory"},
    {RSMI_TEMP_TYPE_JUNCTION, "Junction"},
    {RSMI_TEMP_TYPE_EDGE, "Edge"},
};
TestTempRead::TestTempRead() : TestBase() {
  set_title("RSMI Temp Read Test");
  set_description("The Temperature Read tests verifies that the temperature "
                   "monitors can be read properly.");
}

TestTempRead::~TestTempRead(void) {
}

void TestTempRead::SetUp(void) {
  TestBase::SetUp();

  return;
}

void TestTempRead::DisplayTestInfo(void) {
  TestBase::DisplayTestInfo();
}

void TestTempRead::DisplayResults(void) const {
  TestBase::DisplayResults();
  return;
}

void TestTempRead::Close() {
  // This will close handles opened within rsmitst utility calls and call
  // rsmi_shut_down(), so it should be done after other hsa cleanup
  TestBase::Close();
}


void TestTempRead::Run(void) {
  rdc_status_t err;
  int64_t val_i64;

  TestBase::Run();

  err = AllocateRDCChannel();
  ASSERT_EQ(err, RDC_STATUS_SUCCESS);

  uint32_t type;

  for (uint32_t i = 0; i < num_monitor_devs(); ++i) {
    PrintDeviceHeader(i);

    auto print_temp_metric = [&](rsmi_temperature_metric_t met,
                                                        std::string label) {
      err = rdc_dev_temp_metric_get(rdc_channel(), i, type, met, &val_i64);

      if (err != RDC_STATUS_SUCCESS) {
        if (err == RDC_RSMI_STATUS_NOT_SUPPORTED) {
          IF_VERB(STANDARD) {
            std::cout << "\t**" << label << ": " <<
                               "Not supported on this machine" << std::endl;

            // The following functionality only works for RSMI, so don't do
            // for RDC:
            // Verify api support checking functionality is working
            // err = rdc_dev_temp_metric_get(rdc_channel(), i, type, met,
            //                                                         nullptr);
            // ASSERT_EQ(err, RDC_RSMI_STATUS_NOT_SUPPORTED);
            return;
          }
        } else {
          CHK_ERR_ASRT(err)
        }
      }
      // Verify api support checking functionality is working
      err = rdc_dev_temp_metric_get(rdc_channel(), i, type, met, nullptr);
      ASSERT_EQ(err, RDC_RSMI_STATUS_INVALID_ARGS);

      IF_VERB(STANDARD) {
        std::cout << "\t**" << label << ": " << val_i64/1000 <<
                                                           "C" << std::endl;
      }
    };
    for (type = RSMI_TEMP_TYPE_FIRST; type <= RSMI_TEMP_TYPE_LAST; ++type) {
      IF_VERB(STANDARD) {
        std::cout << "\t** **********" << kTempSensorNameMap.at(type) <<
                                      " Temperatures **********" << std::endl;
      }
      print_temp_metric(RSMI_TEMP_CURRENT, "Current Temp.");
      print_temp_metric(RSMI_TEMP_MAX, "Temperature max value");
      print_temp_metric(RSMI_TEMP_MIN, "Temperature min value");
      print_temp_metric(RSMI_TEMP_MAX_HYST,
                                "Temperature hysteresis value for max limit");
      print_temp_metric(RSMI_TEMP_MIN_HYST,
                                "Temperature hysteresis value for min limit");
      print_temp_metric(RSMI_TEMP_CRITICAL, "Temperature critical max value");
      print_temp_metric(RSMI_TEMP_CRITICAL_HYST,
                           "Temperature hysteresis value for critical limit");
      print_temp_metric(RSMI_TEMP_EMERGENCY,
                                           "Temperature emergency max value");
      print_temp_metric(RSMI_TEMP_EMERGENCY_HYST,
                          "Temperature hysteresis value for emergency limit");
      print_temp_metric(RSMI_TEMP_CRIT_MIN, "Temperature critical min value");
      print_temp_metric(RSMI_TEMP_CRIT_MIN_HYST,
                       "Temperature hysteresis value for critical min value");
      print_temp_metric(RSMI_TEMP_OFFSET, "Temperature offset");
      print_temp_metric(RSMI_TEMP_LOWEST, "Historical minimum temperature");
      print_temp_metric(RSMI_TEMP_HIGHEST, "Historical maximum temperature");
    }
  }
}
