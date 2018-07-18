#include "storage_manager.h"

#include "gflags/gflags.h"

DEFINE_string(master_hostname, "localhost", "hostname of storage master");
DEFINE_string(master_port, "50051", "port of storage master");
DEFINE_string(manager_hostname, "localhost", "hostname of storage manager");
DEFINE_string(manager_port, "50052", "port of storage manager");
DEFINE_int32(master_heartbeat_interval_seconds, 5, "interval between heartbeats");

int main(int argc, char** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  StorageManager storage_manager(FLAGS_manager_hostname, FLAGS_manager_port,
                                 FLAGS_master_hostname, FLAGS_master_port,
                                 FLAGS_master_heartbeat_interval_seconds);

  gflags::ShutDownCommandLineFlags();
  return 0;
}