#include "gtest/gtest.h"
#include "storage_master.h"
#include "storage_service/storage_client/storage_client.h"
#include "storage_service/rpc_interfaces/storage_master_interface.h"

#include <grpcpp/grpcpp.h>
#include <thread>
#include <chrono>
#include <set>

using grpc::Status;

class StorageMasterTest : public ::testing::Test {
 public:
	StorageMasterTest():
		master_(hostname, port) { }

	void SetUp() override {
		master_.Start();

		// Delay for startup
		std::this_thread::sleep_for(std::chrono::milliseconds(50));

		master_interface_.reset(new StorageMasterInterface(hostname, port));
	}

	void TearDown() override {
		master_.Stop();

		// Delay for shutdown
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	const std::string hostname = "localhost";
	const std::string port = "50051";
	StorageMaster master_;
	std::unique_ptr<StorageMasterInterface> master_interface_;
};

TEST_F(StorageMasterTest, StartAndStop) {
}

//
TEST_F(StorageMasterTest, UpdateToDateGlobalView) {

	// Introduce RPC from storage manager with files
	IntroduceRequest request;
	IntroduceRequest::StorageManagerIntroduce* introduce =
	  request.mutable_storage_manager();
	introduce->set_storage_type(StorageType::MANAGED);
	introduce->set_storage_name(StorageName::EPHEMERAL);
	introduce->set_rpc_hostname("hostname");
	introduce->set_rpc_port("1234");

	std::set<std::string> manager_files = {"f1", "f2"};
	for (const auto& f : manager_files) {
		IntroduceRequest::StorageManagerIntroduce::FileId* id = introduce->add_file();
		id->set_key(f);
	}

	IntroduceReply reply;
	Status s = master_interface_->Introduce(request, &reply);
	EXPECT_TRUE(s.ok());

	// Retrieve the manager name for later delta
	std::string mgr_name = reply.name();


	// Check that the view is equal to the initial set of manager files in
	// introduce.
	GetViewReply view_reply;
	master_interface_->GetView(&view_reply);
	EXPECT_EQ(view_reply.view_size(), 1);

	const GetViewReply::ManagerView& mgr_view = view_reply.view(0);
	EXPECT_EQ(mgr_view.name(), mgr_name);
	EXPECT_EQ(manager_files.size(), mgr_view.key_size());

	// Each manager file actually exists in the view.
	for (const auto& key : mgr_view.key()) {
		EXPECT_NE(manager_files.find(key), manager_files.end());
	}

	// Remove f2 from mgr and add f3.
	StorageChangeRequest change_request;
	Action::PutAction* put_action = change_request.add_storage_change()->mutable_put_action();
	put_action->set_key("f3");
	put_action->mutable_mgr()->set_name(mgr_name);

	Action::RemoveAction* remove_action = change_request.add_storage_change()->mutable_remove_action();
	remove_action->set_key("f2");
	remove_action->mutable_mgr()->set_name(mgr_name);


	// Issue storage change to the master
	master_interface_->StorageChange(change_request);

	std::set<std::string> new_set_of_files = {"f1", "f3"};
	GetViewReply new_view_reply;
	master_interface_->GetView(&new_view_reply);
	EXPECT_EQ(new_view_reply.view_size(), 1);

	const GetViewReply::ManagerView& new_mgr_view = new_view_reply.view(0);
	EXPECT_EQ(new_mgr_view.name(), mgr_name);
	EXPECT_EQ(new_set_of_files.size(), new_mgr_view.key_size());

	// Each manager file actually exists in the view.
	for (const auto& key : new_mgr_view.key()) {
		EXPECT_NE(new_set_of_files.find(key), new_set_of_files.end());
	}
}

// TODO(justinmiron): Replace storage client with a fake.
class StorageMasterWithClientTest : public ::testing::Test {
 public:
	StorageMasterWithClientTest():
		master_(master_hostname, master_port),
		client_(client_hostname, client_port,
		        master_hostname, master_port) { }

	void SetUp() override {
		master_.Start();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		client_.Start();
		master_interface_.reset(new StorageMasterInterface(master_hostname,
		                        master_port));
	}

	void TearDown() override {
		client_.Stop();
		master_.Stop();
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	const std::string master_hostname = "localhost";
	const std::string master_port = "50055";
	const std::string client_hostname = "localhost";
	const std::string client_port = "50056";
	StorageMaster master_;
	StorageClient client_;
	std::unique_ptr<StorageMasterInterface> master_interface_;
};


// TODO(justinmiron): Currently doesn't test anything new, move to an e2e
// test once infrastructure is up.
TEST_F(StorageMasterWithClientTest, FillInRulePopulatesMgrRpcUri) {

	// Introduce manager
	IntroduceRequest request;
	IntroduceRequest::StorageManagerIntroduce* introduce =
	  request.mutable_storage_manager();
	introduce->set_storage_type(StorageType::MANAGED);
	introduce->set_storage_name(StorageName::EPHEMERAL);
	introduce->set_rpc_hostname("hostname");
	introduce->set_rpc_port("1234");

	IntroduceReply reply;
	Status s = master_interface_->Introduce(request, &reply);
	EXPECT_TRUE(s.ok());

	std::string mgr_uri = "hostname:1234";

	// Retrieve the manager name for later delta
	std::string mgr_name = reply.name();

	// TODO(justinmiron): Replace with call to GetView once implemented fully.
	// Get the client name in a hacky way, replace with GetView once
	// implemented with extended client.
	IntroduceRequest mimic_request;
	IntroduceRequest::StorageClientIntroduce* i =
	  mimic_request.mutable_storage_client();
	i->set_rpc_port(client_port);
	i->set_rpc_hostname(client_hostname);

	std::string client_name = StorageMaster::GenerateName(mimic_request);

	InstallRuleRequest rule_request;
	rule_request.set_client(client_name);

	Rule* rule = rule_request.mutable_rule();
	Action::GetAction * get = rule->add_action()->mutable_get_action();

	get->set_key("KEY");
	get->mutable_mgr()->set_name(mgr_name);

	s = master_interface_->InstallRule(rule_request);
	EXPECT_TRUE(s.ok());
}