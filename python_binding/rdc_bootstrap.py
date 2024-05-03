import os, time
import ctypes.util
from ctypes import *
from enum import Enum
librdc = "librdc_bootstrap.so"


# The python ctypes wrapper for "librdc_bootstrap.so"

# Search librdc_bootstrap.so paths
current_folder = os.path.dirname(os.path.realpath(__file__))
rdc_paths = [ "", # without path
        current_folder+"/../../../lib/",   # package installation
        current_folder+"/../../../lib64/", # package installation
        current_folder+"/../build/rdc_libs/" # build from source code
]

rdc = None
for r in rdc_paths:
    try:
        rdc = CDLL(r+librdc)
        break
    except:
        pass

if rdc == None:
    print("Unable to load the librdc_bootstrap.so. Set LD_LIBRARY_PATH to the folder containing librdc_bootstrap.so.")
    exit(1)


GPU_ID_INVALID = -1
RDC_GROUP_ALL_GPUS = -1000
RDC_JOB_STATS_FIELDS = -1000
RDC_MAX_STR_LENGTH = 256
RDC_GROUP_MAX_ENTITIES = 64
RDC_MAX_NUM_DEVICES = 16
RDC_MAX_FIELD_IDS_PER_FIELD_GROUP = 128
RDC_MAX_NUM_GROUPS = 64
RDC_MAX_NUM_FIELD_GROUPS = 64
class rdc_status_t(Enum):
     def from_param(cls, obj):
          return int(obj)
     RDC_ST_OK = 0
     RDC_ST_NOT_SUPPORTED = 1
     RDC_ST_MSI_ERROR = 2
     RDC_ST_FAIL_LOAD_MODULE = 3
     RDC_ST_INVALID_HANDLER = 4
     RDC_ST_BAD_PARAMETER = 5
     RDC_ST_NOT_FOUND = 6
     RDC_ST_CONFLICT = 7
     RDC_ST_CLIENT_ERROR = 8
     RDC_ST_ALREADY_EXIST = 9
     RDC_ST_MAX_LIMIT = 10
     RDC_ST_INSUFF_RESOURCES = 11
     RDC_ST_FILE_ERROR = 12
     RDC_ST_NO_DATA = 13
     RDC_ST_PERM_ERROR = 14
     RDC_ST_UNKNOWN_ERROR = 4294967295

class rdc_operation_mode_t(c_int):
     RDC_OPERATION_MODE_AUTO = 0
     RDC_OPERATION_MODE_MANUAL = 1

class rdc_group_type_t(c_int):
     RDC_GROUP_DEFAULT = 0
     RDC_GROUP_EMPTY = 1

class rdc_field_type_t(c_int):
     INTEGER = 0
     DOUBLE = 1
     STRING = 2
     BLOB = 3

class rdc_field_t(c_int):
     RDC_FI_INVALID = 0
     RDC_FI_GPU_COUNT = 1
     RDC_FI_DEV_NAME = 2
     RDC_FI_GPU_CLOCK = 100
     RDC_FI_MEM_CLOCK = 101
     RDC_FI_MEMORY_TEMP = 200
     RDC_FI_GPU_TEMP = 201
     RDC_FI_POWER_USAGE = 300
     RDC_FI_PCIE_TX = 400
     RDC_FI_PCIE_RX = 401
     RDC_FI_PCIE_BANDWIDTH = 402
     RDC_FI_GPU_UTIL = 500
     RDC_FI_GPU_MEMORY_USAGE = 501
     RDC_FI_GPU_MEMORY_TOTAL = 502
     RDC_FI_ECC_CORRECT_TOTAL = 600
     RDC_FI_ECC_UNCORRECT_TOTAL = 601
     RDC_FI_ECC_SDMA_SEC = 602
     RDC_FI_ECC_SDMA_DED = 603
     RDC_FI_ECC_GFX_SEC = 604
     RDC_FI_ECC_GFX_DED = 605
     RDC_FI_ECC_MMHUB_SEC = 606
     RDC_FI_ECC_MMHUB_DED = 607
     RDC_FI_ECC_ATHUB_SEC = 608
     RDC_FI_ECC_ATHUB_DED = 609
     RDC_FI_ECC_BIF_SEC = 610
     RDC_FI_ECC_BIF_DED = 611
     RDC_FI_ECC_HDP_SEC = 612
     RDC_FI_ECC_HDP_DED = 613
     RDC_FI_ECC_XGMI_WAFL_SEC = 614
     RDC_FI_ECC_XGMI_WAFL_DED = 615
     RDC_FI_ECC_DF_SEC = 616
     RDC_FI_ECC_DF_DED = 617
     RDC_FI_ECC_SMN_SEC = 618
     RDC_FI_ECC_SMN_DED = 619
     RDC_FI_ECC_SEM_SEC = 620
     RDC_FI_ECC_SEM_DED = 621
     RDC_FI_ECC_MP0_SEC = 622
     RDC_FI_ECC_MP0_DED = 623
     RDC_FI_ECC_MP1_SEC = 624
     RDC_FI_ECC_MP1_DED = 625
     RDC_FI_ECC_FUSE_SEC = 626
     RDC_FI_ECC_FUSE_DED = 627
     RDC_FI_ECC_UMC_SEC = 628
     RDC_FI_ECC_UMC_DED = 629
     RDC_FI_XGMI_0_READ_KB = 700
     RDC_FI_XGMI_1_READ_KB = 701
     RDC_FI_XGMI_2_READ_KB = 702
     RDC_FI_XGMI_3_READ_KB = 703
     RDC_FI_XGMI_4_READ_KB = 704
     RDC_FI_XGMI_5_READ_KB = 705
     RDC_FI_XGMI_6_READ_KB = 706
     RDC_FI_XGMI_7_READ_KB = 707
     RDC_FI_XGMI_0_WRITE_KB = 708
     RDC_FI_XGMI_1_WRITE_KB = 709
     RDC_FI_XGMI_2_WRITE_KB = 710
     RDC_FI_XGMI_3_WRITE_KB = 711
     RDC_FI_XGMI_4_WRITE_KB = 712
     RDC_FI_XGMI_5_WRITE_KB = 713
     RDC_FI_XGMI_6_WRITE_KB = 714
     RDC_FI_XGMI_7_WRITE_KB = 715
     RDC_EVNT_XGMI_0_NOP_TX = 1000
     RDC_EVNT_XGMI_0_REQ_TX = 1001
     RDC_EVNT_XGMI_0_RESP_TX = 1002
     RDC_EVNT_XGMI_0_BEATS_TX = 1003
     RDC_EVNT_XGMI_1_NOP_TX = 1004
     RDC_EVNT_XGMI_1_REQ_TX = 1005
     RDC_EVNT_XGMI_1_RESP_TX = 1006
     RDC_EVNT_XGMI_1_BEATS_TX = 1007
     RDC_EVNT_XGMI_0_THRPUT = 1500
     RDC_EVNT_XGMI_1_THRPUT = 1501
     RDC_EVNT_NOTIF_VMFAULT = 2000
     RDC_EVNT_NOTIF_THERMAL_THROTTLE = 2001
     RDC_EVNT_NOTIF_PRE_RESET = 2002
     RDC_EVNT_NOTIF_POST_RESET = 2003

rdc_handle_t = c_void_p
rdc_gpu_group_t = c_uint32
rdc_field_grp_t = c_uint32
class rdc_device_attributes_t(Structure):
    _fields_ = [
            ("device_name", c_char*256)
            ]

class rdc_group_info_t(Structure):
    _fields_ = [
            ("count", c_uint32)
            ,("group_name", c_char*256)
            ,("entity_ids", c_uint32*64)
            ]

class rdc_stats_summary_t(Structure):
    _fields_ = [
            ("max_value", c_uint64)
            ,("min_value", c_uint64)
            ,("average", c_uint64)
            ,("standard_deviation", c_double)
            ]

class rdc_gpu_usage_info_t(Structure):
    _fields_ = [
            ("gpu_id", c_uint32)
            ,("start_time", c_uint64)
            ,("end_time", c_uint64)
            ,("energy_consumed", c_uint64)
            ,("ecc_correct", c_uint64)
            ,("ecc_uncorrect", c_uint64)
            ,("pcie_tx", rdc_stats_summary_t)
            ,("pcie_rx", rdc_stats_summary_t)
            ,("power_usage", rdc_stats_summary_t)
            ,("gpu_clock", rdc_stats_summary_t)
            ,("memory_clock", rdc_stats_summary_t)
            ,("gpu_utilization", rdc_stats_summary_t)
            ,("gpu_temperature", rdc_stats_summary_t)
            ,("max_gpu_memory_used", c_uint64)
            ,("memory_utilization", rdc_stats_summary_t)
            ]

class rdc_job_info_t(Structure):
    _fields_ = [
            ("num_gpus", c_uint32)
            ,("summary", rdc_gpu_usage_info_t)
            ,("gpus", rdc_gpu_usage_info_t*16)
            ]

class rdc_anonymous_0(ctypes.Union):
    _fields_ = [
            ("l_int", c_int64)
            ,("dbl", c_double)
            ,("str", c_char*256)
            ]

class rdc_field_value(Structure):
    _fields_ = [
            ("field_id", rdc_field_t)
            ,("status", c_int)
            ,("ts", c_uint64)
            ,("type", rdc_field_type_t)
            ,("value", rdc_anonymous_0)
            ]

class rdc_field_group_info_t(Structure):
    _fields_ = [
            ("count", c_uint32)
            ,("group_name", c_char*256)
            ,("field_ids", rdc_field_t*128)
            ]

class rdc_job_group_info_t(Structure):
    _fields_ = [
            ("job_id", c_char*256)
            ,("group_id", rdc_gpu_group_t)
            ,("start_time", c_uint64)
            ,("stop_time", c_uint64)
            ]

rdc.rdc_init.restype = rdc_status_t
rdc.rdc_init.argtypes = [ c_uint64 ]
rdc.rdc_shutdown.restype = rdc_status_t
rdc.rdc_shutdown.argtypes = [  ]
rdc.rdc_start_embedded.restype = rdc_status_t
rdc.rdc_start_embedded.argtypes = [ rdc_operation_mode_t,POINTER(rdc_handle_t) ]
rdc.rdc_stop_embedded.restype = rdc_status_t
rdc.rdc_stop_embedded.argtypes = [ rdc_handle_t ]
rdc.rdc_connect.restype = rdc_status_t
rdc.rdc_connect.argtypes = [ c_char_p,POINTER(rdc_handle_t),c_char_p,c_char_p,c_char_p ]
rdc.rdc_disconnect.restype = rdc_status_t
rdc.rdc_disconnect.argtypes = [ rdc_handle_t ]
rdc.rdc_job_start_stats.restype = rdc_status_t
rdc.rdc_job_start_stats.argtypes = [ rdc_handle_t,rdc_gpu_group_t,POINTER(c_char),c_uint64 ]
rdc.rdc_job_get_stats.restype = rdc_status_t
rdc.rdc_job_get_stats.argtypes = [ rdc_handle_t,POINTER(c_char),POINTER(rdc_job_info_t) ]
rdc.rdc_job_stop_stats.restype = rdc_status_t
rdc.rdc_job_stop_stats.argtypes = [ rdc_handle_t,POINTER(c_char) ]
rdc.rdc_job_remove.restype = rdc_status_t
rdc.rdc_job_remove.argtypes = [ rdc_handle_t,POINTER(c_char) ]
rdc.rdc_job_remove_all.restype = rdc_status_t
rdc.rdc_job_remove_all.argtypes = [ rdc_handle_t ]
rdc.rdc_field_update_all.restype = rdc_status_t
rdc.rdc_field_update_all.argtypes = [ rdc_handle_t,c_uint32 ]
rdc.rdc_device_get_all.restype = rdc_status_t
rdc.rdc_device_get_all.argtypes = [ rdc_handle_t,POINTER(c_uint32),POINTER(c_uint32) ]
rdc.rdc_device_get_attributes.restype = rdc_status_t
rdc.rdc_device_get_attributes.argtypes = [ rdc_handle_t,c_uint32,POINTER(rdc_device_attributes_t) ]
rdc.rdc_group_gpu_create.restype = rdc_status_t
rdc.rdc_group_gpu_create.argtypes = [ rdc_handle_t,rdc_group_type_t,c_char_p,POINTER(rdc_gpu_group_t) ]
rdc.rdc_group_gpu_add.restype = rdc_status_t
rdc.rdc_group_gpu_add.argtypes = [ rdc_handle_t,rdc_gpu_group_t,c_uint32 ]
rdc.rdc_group_gpu_get_info.restype = rdc_status_t
rdc.rdc_group_gpu_get_info.argtypes = [ rdc_handle_t,rdc_gpu_group_t,POINTER(rdc_group_info_t) ]
rdc.rdc_group_get_all_ids.restype = rdc_status_t
rdc.rdc_group_get_all_ids.argtypes = [ rdc_handle_t,POINTER(rdc_gpu_group_t),POINTER(c_uint32) ]
rdc.rdc_group_gpu_destroy.restype = rdc_status_t
rdc.rdc_group_gpu_destroy.argtypes = [ rdc_handle_t,rdc_gpu_group_t ]
rdc.rdc_group_field_create.restype = rdc_status_t
rdc.rdc_group_field_create.argtypes = [ rdc_handle_t,c_uint32,POINTER(rdc_field_t),c_char_p,POINTER(rdc_field_grp_t) ]
rdc.rdc_group_field_get_info.restype = rdc_status_t
rdc.rdc_group_field_get_info.argtypes = [ rdc_handle_t,rdc_field_grp_t,POINTER(rdc_field_group_info_t) ]
rdc.rdc_group_field_get_all_ids.restype = rdc_status_t
rdc.rdc_group_field_get_all_ids.argtypes = [ rdc_handle_t,POINTER(rdc_field_grp_t),POINTER(c_uint32) ]
rdc.rdc_group_field_destroy.restype = rdc_status_t
rdc.rdc_group_field_destroy.argtypes = [ rdc_handle_t,rdc_field_grp_t ]
rdc.rdc_field_watch.restype = rdc_status_t
rdc.rdc_field_watch.argtypes = [ rdc_handle_t,rdc_gpu_group_t,rdc_field_grp_t,c_uint64,c_double,c_uint32 ]
rdc.rdc_field_get_latest_value.restype = rdc_status_t
rdc.rdc_field_get_latest_value.argtypes = [ rdc_handle_t,c_uint32,rdc_field_t,POINTER(rdc_field_value) ]
rdc.rdc_field_get_value_since.restype = rdc_status_t
rdc.rdc_field_get_value_since.argtypes = [ rdc_handle_t,c_uint32,rdc_field_t,c_uint64,POINTER(c_uint64),POINTER(rdc_field_value) ]
rdc.rdc_field_unwatch.restype = rdc_status_t
rdc.rdc_field_unwatch.argtypes = [ rdc_handle_t,rdc_gpu_group_t,rdc_field_grp_t ]
rdc.rdc_status_string.restype = c_char_p
rdc.rdc_status_string.argtypes = [ rdc_status_t ]
rdc.field_id_string.restype = c_char_p
rdc.field_id_string.argtypes = [ rdc_field_t ]
rdc.get_field_id_from_name.restype = rdc_field_t
rdc.get_field_id_from_name.argtypes = [ c_char_p ]
