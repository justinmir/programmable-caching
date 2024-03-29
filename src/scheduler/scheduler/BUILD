load("@org_pubref_rules_protobuf//cpp:rules.bzl", "GRPC_COMPILE_DEPS")
package(default_visibility = ["//visibility:public"])

cc_library(
	name = "scheduler",
	srcs = ["scheduler.cc"],
	hdrs = ["scheduler.h"],
	deps = [
		":job_view",
		":scheduler_common",
		":serverless_system_view",
		"//execution_engine/rpc_interfaces:execution_engine_interface",
		"//external:glog",
		"//storage_service/rpc_interfaces:storage_master_interface",
		"//proto:scheduler_proto",
	] + GRPC_COMPILE_DEPS
)

cc_binary(
	name = "basic_scheduler_binary",
	srcs = ["basic_scheduler_binary.cc"],
	deps = [
		":scheduler"
	]
)

cc_library(
	name = "serverless_system_view",
	srcs = ["serverless_system_view.cc"],
	hdrs = ["serverless_system_view.h"],
	deps = [
		":scheduler_common",
		"//external:glog",
		"//proto:scheduler_proto",
		"//proto:storage_service_proto"
	]
)

cc_test(
	name = "serverless_system_view_test",
	srcs = ["serverless_system_view_test.cc"],
  copts = [
  	"-Iexternal/gtest",
  ],
	deps = [
		":scheduler_common",
		"//proto:scheduler_proto",
		"//proto:storage_service_proto",
		":serverless_system_view",
		"//external:gtest"
	]
)

cc_library(
	name = "scheduler_common",
	hdrs = ["scheduler_common.h"],
)

cc_library(
	name = "job_view",
	srcs = ["job_view.cc"],
	hdrs = ["job_view.h"],
	deps = [
		":scheduler_common",
		"//proto:scheduler_proto"
	]
)

cc_test(
	name = "scheduler_test",
	srcs = ["scheduler_test.cc"],
  copts = [
  	"-Iexternal/gtest",
  ],
	deps = [
		"//scheduler/rpc_interfaces:scheduler_interface",
		":scheduler",
		"//storage_service/storage_master:storage_master",
		"//proto:scheduler_proto",
		"//external:gtest"
	]
)