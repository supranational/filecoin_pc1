#!/bin/bash

# Copyright Supranational LLC

set -e
set -x

# SPDK should be build one level up and setup should already be called
# https://spdk.io/doc/getting_started.html
# git clone https://github.com/spdk/spdk --recursive
# cd spdk/
# sudo scripts/pkgdep.sh
# ./configure --with-virtio --with-vhost
# make
# sudo ./scripts/setup.sh
SPDK="../spdk-v22.09"

CPPFLAGS="-fno-omit-frame-pointer -g -O2 -Wall -Wextra -Wno-unused-parameter \
          -Wno-missing-field-initializers -fno-strict-aliasing \
          -march=native -Wformat -Wformat-security \
          -D_GNU_SOURCE -fPIC -fstack-protector \
          -fno-common -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2 \
          -DSPDK_GIT_COMMIT=4be6d3043 -pthread"

LDFLAGS="-fno-omit-frame-pointer -Wl,-z,relro,-z,now -Wl,-z,noexecstack -fuse-ld=bfd\
         -L$SPDK/build/lib \
         -Wl,--whole-archive -Wl,--no-as-needed \
         -lspdk_bdev_malloc \
         -lspdk_bdev_null \
         -lspdk_bdev_nvme \
         -lspdk_bdev_passthru \
         -lspdk_bdev_lvol \
         -lspdk_bdev_raid \
         -lspdk_bdev_error \
         -lspdk_bdev_gpt \
         -lspdk_bdev_split \
         -lspdk_bdev_delay \
         -lspdk_bdev_zone_block \
         -lspdk_blobfs_bdev \
         -lspdk_blobfs \
         -lspdk_blob_bdev \
         -lspdk_lvol \
         -lspdk_blob \
         -lspdk_nvme \
         -lspdk_bdev_ftl \
         -lspdk_ftl \
         -lspdk_bdev_aio \
         -lspdk_bdev_virtio \
         -lspdk_virtio \
         -lspdk_vfio_user \
         -lspdk_accel_ioat \
         -lspdk_ioat \
         -lspdk_scheduler_dynamic \
         -lspdk_env_dpdk \
         -lspdk_scheduler_dpdk_governor \
         -lspdk_scheduler_gscheduler \
         -lspdk_sock_posix \
         -lspdk_event \
         -lspdk_event_bdev \
         -lspdk_bdev \
         -lspdk_notify \
         -lspdk_dma \
         -lspdk_event_accel \
         -lspdk_accel \
         -lspdk_event_vmd \
         -lspdk_vmd \
         -lspdk_event_sock \
         -lspdk_init \
         -lspdk_thread \
         -lspdk_trace \
         -lspdk_sock \
         -lspdk_rpc \
         -lspdk_jsonrpc \
         -lspdk_json \
         -lspdk_util \
         -lspdk_log \
         -Wl,--no-whole-archive $SPDK/build/lib/libspdk_env_dpdk.a \
         -Wl,--whole-archive $SPDK/dpdk/build/lib/librte_bus_pci.a \
         $SPDK/dpdk/build/lib/librte_cryptodev.a \
         $SPDK/dpdk/build/lib/librte_dmadev.a \
         $SPDK/dpdk/build/lib/librte_eal.a \
         $SPDK/dpdk/build/lib/librte_ethdev.a \
         $SPDK/dpdk/build/lib/librte_hash.a \
         $SPDK/dpdk/build/lib/librte_kvargs.a \
         $SPDK/dpdk/build/lib/librte_mbuf.a \
         $SPDK/dpdk/build/lib/librte_mempool.a \
         $SPDK/dpdk/build/lib/librte_mempool_ring.a \
         $SPDK/dpdk/build/lib/librte_net.a \
         $SPDK/dpdk/build/lib/librte_pci.a \
         $SPDK/dpdk/build/lib/librte_power.a \
         $SPDK/dpdk/build/lib/librte_rcu.a \
         $SPDK/dpdk/build/lib/librte_ring.a \
         $SPDK/dpdk/build/lib/librte_telemetry.a \
         $SPDK/dpdk/build/lib/librte_vhost.a \
         -Wl,--no-whole-archive \
         -lnuma -ldl \
         -L$SPDK/isa-l/.libs -lisal \
         -pthread -lrt -luuid -lssl -lcrypto -lm -laio"

INCLUDE="-I$SPDK/include -I$SPDK/isa-l/.. -I$SPDK/dpdk/build/include"

rm -fr obj
mkdir -p obj

gcc -c src/sha/sha_ext_mbx2.S -o obj/sha_ext_mbx2.o

c++ $CPPFLAGS $INCLUDE -o obj/pc1.o -c src/sealing/pc1.cpp
c++ $CPPFLAGS $INCLUDE -o obj/debug_helpers.o -c src/util/debug_helpers.cpp
c++ $CPPFLAGS $INCLUDE -o obj/replica_id.o -c src/sealing/replica_id.cpp
c++ -g -o pc1 obj/pc1.o obj/debug_helpers.o obj/replica_id.o \
    obj/sha_ext_mbx2.o $LDFLAGS ../blst/libblst.a

