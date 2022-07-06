FROM almalinux:8

ADD rocm.repo /etc/yum.repos.d/
ADD protobuf.repo /etc/yum.repos.d/
RUN yum install net-tools procps-ng gcc-c++ rdc protobuf python3-devel -y
ENV LD_LIBRARY_PATH /opt/rocm/rdc/lib:/opt/rocm/rdc/grpc/lib/
ENV PATH /root/.local/bin:$PATH
RUN pip3 install --user -U pip
RUN pip3 install --user protobuf==3.11.3
RUN pip3 install --user prometheus_client kuryr_kubernetes
RUN ln -s /lib64/libprotobuf.so.22 /lib64/libprotobuf.so.3.11.2.0