cmake_minimum_required(VERSION 3.11.0)
project(melonfs VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g -ggdb -fno-omit-frame-pointer -fno-inline-functions")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")

### configurable options end ###
set(CMAKE_PREFIX_PATH "/home/cyx/spdk/install")
include_directories("/home/cyx/spdk/install/include")
link_directories("/home/cyx/spdk/install/lib")

add_executable(spdk_exercise spdk_exercise.cc)
target_link_libraries(spdk_exercise
    "-Wl,--no-as-needed" # 需要这一句
    spdk_nvme
    spdk_sock
    spdk_sock_posix
    spdk_trace
    spdk_rpc
    spdk_jsonrpc
    spdk_json
    spdk_vfio_user
    spdk_env_dpdk
    spdk_util
    spdk_log
    rte_eal
    rte_mempool
    rte_ring
    rte_mbuf
    rte_bus_pci
    rte_pci
    rte_mempool_ring
    rte_telemetry
    rte_kvargs
    rte_power
    rte_ethdev
    rte_vhost
    rte_net
    rte_dmadev
    rte_cryptodev
    rte_hash
    rte_rcu
    isal
    isal_crypto
    "-Wl,--as-needed" # 需要这一句
)