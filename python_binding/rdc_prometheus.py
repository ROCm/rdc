import argparse
from RdcReader import RdcReader
from RdcUtil import RdcUtil
from rdc_bootstrap import *
from prometheus_client import start_http_server, Gauge, REGISTRY, PROCESS_COLLECTOR, PLATFORM_COLLECTOR

default_field_ids = [
        rdc_field_t.RDC_FI_GPU_MEMORY_USAGE,
        rdc_field_t.RDC_FI_GPU_MEMORY_TOTAL,
        rdc_field_t.RDC_FI_POWER_USAGE,
        rdc_field_t.RDC_FI_GPU_CLOCK,
        rdc_field_t.RDC_FI_GPU_UTIL,
        rdc_field_t.RDC_FI_GPU_TEMP
]

class PrometheusReader(RdcReader):
    def __init__(self, rdc_ip_port, field_ids, update_freq, max_keep_age, max_keep_samples,
            gpu_indexes, rdc_unauth, enable_plugin_monitoring, enable_kubernetes_integration):
        group_name = "rdc_prometheus_plugin_group"
        field_group_name = "rdc_prometheus_plugin_fieldgroup"
        if rdc_unauth:
            RdcReader.__init__(self, ip_port = rdc_ip_port, field_ids = field_ids, update_freq=update_freq,
                    max_keep_age = max_keep_age, max_keep_samples = max_keep_samples,
                    gpu_indexes = gpu_indexes, field_group_name = field_group_name, gpu_group_name = group_name, root_ca = None)
        else:
             RdcReader.__init__(self, ip_port = rdc_ip_port, field_ids = field_ids, update_freq=update_freq,
                    max_keep_age = max_keep_age, max_keep_samples = max_keep_samples,
                    gpu_indexes = gpu_indexes, field_group_name = field_group_name, gpu_group_name = group_name)

        # Supress internal metrics from prometheus_client
        if enable_plugin_monitoring == False:
            REGISTRY.unregister(PROCESS_COLLECTOR)
            REGISTRY.unregister(PLATFORM_COLLECTOR)

        self.enable_kubernetes_integration = enable_kubernetes_integration

        # Create the gauges
        self.gauges = {}
        for fid in self.field_ids:
            field_name = self.rdc_util.field_id_string(fid).lower()
            if enable_kubernetes_integration:
                self.gauges[fid] = Gauge(field_name, field_name, labelnames=['gpu_index', 'pod', 'namespace', 'container'])
            else:
                self.gauges[fid] = Gauge(field_name, field_name, labelnames=['gpu_index'])

        if enable_kubernetes_integration:
            import sys, os
            sys.path.append('/opt/rocm/bin')
            from rocm_smi import getBus, initializeRsmi
            from kuryr_kubernetes.pod_resources.client import PodResourcesClient

            # Create kubelet client for podresources api to get pcie bus address of attached gpu
            self.pr_client = PodResourcesClient(os.getenv('RDC_KUBERNETES_KUBELET_PATH','/var/lib/kubelet'))

            self.empty_label_value = os.getenv('RDC_KUBERNETES_EMPTY_LABEL_VALUE','')

            initializeRsmi()
            
            # Cache mapping between gpu indexes and PCIe bus addresses, assumes no hotplug of gpus
            self.index_to_bus_addr = {}
            for item in self.gpu_indexes:
                self.index_to_bus_addr[item] = getBus(item)

    def process(self):
        # Make sure no other thread collects metrics before we are fully finished with them
        with REGISTRY._lock:
            if self.enable_kubernetes_integration:
                from google.protobuf.json_format import MessageToDict
                # Get list of all pods and their containers with devices attached to them
                self.pod_list = MessageToDict(self.pr_client.list())
                # Clear the labels and populate them later again
                for fid in self.field_ids:
                    self.gauges[fid].clear()
            RdcReader.process(self)

    def handle_field(self, gpu_index, value):
        fid = value.field_id.value
        if fid in self.gauges:
            if self.enable_kubernetes_integration:
                gpu_bus_addr = self.index_to_bus_addr[gpu_index]
                # Check if currently processed gpu is attached to any container, single gpu can only be attached to a single container
                container_data = self.findContainer(gpu_bus_addr)
                if container_data:
                    self.gauges[fid].labels(gpu_index=gpu_index, pod=container_data['pod'], namespace=container_data['namespace'], container=container_data['container']).set(value.value.l_int)
                else:
                    self.gauges[fid].labels(gpu_index=gpu_index, pod=self.empty_label_value, namespace=self.empty_label_value, container=self.empty_label_value).set(value.value.l_int)
            else:
                self.gauges[fid].labels(gpu_index).set(value.value.l_int)

    def findContainer(self,dev_id):
        container_dict = {}
        for pod in self.pod_list['podResources']:
            for container in pod['containers']:
                if 'devices' in container:
                    for device in container['devices']:
                        if device['resourceName'] == 'amd.com/gpu':
                            if device['deviceIds'][0] == dev_id:
                                container_dict['container'] = container['name']
                                container_dict['pod'] = pod['name']
                                container_dict['namespace'] = pod['namespace']
                                return container_dict
        return container_dict


def get_field_ids(args):
    field_ids = []

    field_id_str=[]
    if args.rdc_fields:
        field_id_str=args.rdc_fields
    elif args.rdc_fields_file:
        try:
            with open(args.rdc_fields_file) as fi:
                content = fi.readlines()
            field_id_str = [x.strip() for x in content]
        except Exception as e:
             print("Fail to read " + args.rdc_fields_file + ":" + str(e))

    if len(field_id_str)> 0 :
        for f in field_id_str:
            field_id = rdc.get_field_id_from_name(f)
            if field_id.value == rdc_field_t.RDC_FI_INVALID:
                print("Invalid field '%s' will be ignored." % (f))
            else:
                field_ids.append(field_id.value)
        return field_ids

    return default_field_ids


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='RDC Prometheus plugin.')
    parser.add_argument('--listen_port', default=5000, type=int, help='The listen port of the plugin (default: 5000)')
    parser.add_argument('--rdc_embedded', default=False, action='store_true', help='Run RDC in embedded mode (default: standalone mode)')
    parser.add_argument('--rdc_ip_port' , default='localhost:50051', help='The rdcd IP and port in standalone mode (default: localhost:50051)')
    parser.add_argument('--rdc_unauth', default=False, action='store_true', help='Set this option if the rdcd is running with unauth in standalone mode (default: false)')
    parser.add_argument('--rdc_update_freq', default=10, help='The fields update frequency in seconds (default: 10)')
    parser.add_argument('--rdc_max_keep_age', default=3600, help='The max keep age of the fields in seconds (default: 3600)')
    parser.add_argument('--rdc_max_keep_samples', default=1000, help='The max samples to keep for each field in the cache (default: 1000)')
    parser.add_argument('--rdc_fields', default=None, nargs='+', help='The list of fields name needs to be watched, for example, " --rdc_fields RDC_FI_GPU_TEMP RDC_FI_POWER_USAGE " (default: predefined fields in the plugin)')
    parser.add_argument('--rdc_fields_file', default=None, help='The list of fields name can also be read from a file with each field name in a separated line (default: None)')
    parser.add_argument('--rdc_gpu_indexes',  default=None, nargs='+', help='The list of GPUs to be watched (default: All GPUs)')
    parser.add_argument('--enable_plugin_monitoring', default=False, action='store_true', help = 'Set this option to collect process metrics of the plugin itself (default: false)')
    parser.add_argument('--enable_kubernetes_integration', default=False, action='store_true', help='Set this option if you want per pod gpu monitoring in kubernetes (default: false)')
    
    args = parser.parse_args()

    field_ids = get_field_ids(args)
    rdc_ip_port = args.rdc_ip_port
    if args.rdc_embedded:
        rdc_ip_port = None
    if args.rdc_gpu_indexes != None:
        for i in range(0, len(args.rdc_gpu_indexes)):
            args.rdc_gpu_indexes[i] = int(args.rdc_gpu_indexes[i])

    reader = PrometheusReader(rdc_ip_port, field_ids, args.rdc_update_freq*1000000,
            args.rdc_max_keep_age, args.rdc_max_keep_samples,
            args.rdc_gpu_indexes, args.rdc_unauth, args.enable_plugin_monitoring, args.enable_kubernetes_integration)
    start_http_server(args.listen_port)
    print("The RDC Prometheus plugin listen at port %d" % (args.listen_port))
    time.sleep(3)
    while True:
        reader.process()
        time.sleep(1)
