#include "spdk/env.h"
#include "spdk/nvme_spec.h"
#include <cstddef>
#include <cstring>
#include <spdk/nvme.h>
#include <string>
#include <vector>

using std::string;
using std::vector;

// https://github.com/spdk/spdk/blob/master/examples/nvme/hello_world/hello_world.c

constexpr int64_t kPageSize = 4096;
constexpr int64_t kSectorSize = 512;
constexpr int64_t kDataLength = kPageSize * 20;

char write_stage[15] = "write";
char read_stage[15] = "read";

vector<spdk_nvme_ctrlr *> ns_ctrlrs;
vector<spdk_nvme_ns *> ns_list;
vector<string> ns_traddr;

char *write_buf; // 显然需要对齐 sector
char *read_buf;  // 显然需要对齐 sector

bool finished;
bool probe_cb(void *ctx, const struct spdk_nvme_transport_id *trid,
              struct spdk_nvme_ctrlr_opts *opts) {
  printf("probed type %s, addr %s\n", trid->trstring, trid->traddr);
  return true;
}

void attach_cb(void *ctx, const struct spdk_nvme_transport_id *trid,
               struct spdk_nvme_ctrlr *ctrlr,
               const struct spdk_nvme_ctrlr_opts *opts) {
  printf("attach type %s, addr %s\n", trid->trstring, trid->traddr);
  int ns_cnt = spdk_nvme_ctrlr_get_num_ns(ctrlr);
  printf("namespace cnt %d\n", ns_cnt);
  for (int i = 1; i <= ns_cnt; i++) {
    struct spdk_nvme_ns *ns = spdk_nvme_ctrlr_get_ns(ctrlr, i);
    if (!spdk_nvme_ns_is_active(ns)) {
      continue;
    }
    uint64_t ns_size = spdk_nvme_ns_get_size(ns);
    uint64_t sector_size = spdk_nvme_ns_get_sector_size(ns);
    uint64_t sector_num = spdk_nvme_ns_get_num_sectors(ns);
    printf("  Namespace ID: %d size: %.2f GB, %.2fGiB, sector size %lu, sector "
           "num %lu\n",
           spdk_nvme_ns_get_id(ns), ns_size / 1000.0 / 1000.0 / 1000.0,
           ns_size / 1024.0 / 1024.0 / 1024.0, sector_size, sector_num);
    ns_ctrlrs.push_back(ctrlr);
    ns_list.push_back(ns);
    ns_traddr.push_back(trid->traddr);
  }
}

void server_spdk_cmd_cb(void *arg, const struct spdk_nvme_cpl *cpl) {
  if (cpl == nullptr || !spdk_nvme_cpl_is_success(cpl)) {
    printf("error %s\n", spdk_nvme_cpl_get_status_type_string(&cpl->status));
  }
  printf("stage %s success\n", (char *)arg);
  finished = true;
}

int main() {
  int ret;
  struct spdk_env_opts opts;
  spdk_env_opts_init(&opts);
  // 如果有配置，就配置。配置完了初始化环境
  ret = spdk_env_init(&opts);
  if (ret < 0) {
    printf("Unable to initialize SPDK env\n");
    return 1;
  }
  // numa id 随便填了
  write_buf = (char *)spdk_malloc(kDataLength, kPageSize, nullptr,
                                  SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
  read_buf = (char *)spdk_malloc(kDataLength, kPageSize, nullptr,
                                 SPDK_ENV_SOCKET_ID_ANY, SPDK_MALLOC_DMA);
  if (write_buf == nullptr || read_buf == nullptr) {
    printf("read/write buf spdk_malloc failed\n");
    return 1;
  }
  for (int64_t i = 0; i < kDataLength;) {
    int64_t len = std::min(int64_t(3191), kDataLength - i);
    memset(write_buf + i, 'a' + (i % 26), len);
    i += len;
  }
  // 检测设备
  ret = spdk_nvme_probe(nullptr, nullptr, probe_cb, attach_cb, nullptr);
  if (ret < 0) {
    printf("spdk nvme probe failed\n");
    return 1;
  }
  if (ns_ctrlrs.empty()) {
    printf("no spdk device!\n");
    return 1;
  }
  // 创建 QP
  struct spdk_nvme_io_qpair_opts qp_opts;
  spdk_nvme_ctrlr_get_default_io_qpair_opts(ns_ctrlrs[0], &qp_opts,
                                            sizeof(qp_opts));

  qp_opts.io_queue_size = 256;
  qp_opts.io_queue_requests = 256 * 168;
  struct spdk_nvme_qpair *ssd_qp =
      spdk_nvme_ctrlr_alloc_io_qpair(ns_ctrlrs[0], &qp_opts, sizeof(qp_opts));
  if (ssd_qp == nullptr) {
    printf("ssd qp is nullptr\n");
    return 1;
  }
  finished = false;
  spdk_nvme_ns_cmd_write(ns_list[0], ssd_qp, write_buf, 0,
                         kDataLength / kSectorSize, server_spdk_cmd_cb,
                         write_stage, 0);
  while (!finished) {
    spdk_nvme_qpair_process_completions(ssd_qp, 0);
  }
  finished = false;
  spdk_nvme_ns_cmd_read(ns_list[0], ssd_qp, read_buf, 0,
                        kDataLength / kSectorSize, server_spdk_cmd_cb,
                        read_stage, 0);
  while (!finished) {
    spdk_nvme_qpair_process_completions(ssd_qp, 0);
  }
  printf("memcmp %d\n", memcmp(write_buf, read_buf, kDataLength));

  for (int64_t i = 0; i < kDataLength;) {
    printf("read c %c %c\n", write_buf[i], read_buf[i]);
    int64_t len = std::min(int64_t(3191), kDataLength - i);
    i += len;
  }
  spdk_free(write_buf);
  spdk_free(read_buf);
  spdk_nvme_ctrlr_free_io_qpair(ssd_qp);
  for (auto &x : ns_ctrlrs) {
    spdk_nvme_detach(x);
  }
  spdk_env_fini();
  return 0;
}