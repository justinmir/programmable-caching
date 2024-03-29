#include "serverless_system_view.h"
#include "proto/scheduler.grpc.pb.h"
#include "proto/storage_service.grpc.pb.h"
#include "scheduler_common.h"

#include <vector>
#include <string>
#include <glog/logging.h>

namespace {

// Hackiest.
std::string uri_to_hostname(const std::string& uri) {
	return std::string(uri.begin(), uri.begin() + uri.find(':'));
}

std::string name_to_storage_name(const std::string& name) {
	auto first = name.find(':');
	std::string sliced = std::string(name.begin() + first + 1, name.end());
	auto second = sliced.find(':');
	return std::string(sliced.begin(), sliced.begin() + second);
}

}

ServerlessSystemView::ServerlessSystemView(StorageName ephemeral,
    StorageName persistant): ephemeral_(ephemeral), persistant_(persistant)
{ }

ServerlessSystemView::ServerlessSystemView(const ServerlessSystemView& view) {
	// Lock the other view.
	std::lock_guard<std::mutex> lock(view.system_mutex);

	ephemeral_ = view.ephemeral_;
	persistant_ = view.persistant_;
	client_uri_to_engine = view.client_uri_to_engine;

	// Currently running engines
	engine_to_state = view.engine_to_state;
	hostname_to_engine = view.hostname_to_engine;

	// Manager to keys
	mgr_to_keys = view.mgr_to_keys;
	persistant_key_to_mgr = view.persistant_key_to_mgr;
}



void ServerlessSystemView::AddExecutionEngine(const std::string& exec_uri,
    const std::string& client_uri,
    int maximum_tasks) {
	std::lock_guard<std::mutex> lock(system_mutex);
	std::string hostname = uri_to_hostname(exec_uri);

	hostname_to_engine[hostname] = exec_uri;
	engine_to_state[exec_uri] = EngineState(exec_uri, maximum_tasks);
	client_uri_to_engine[client_uri] = exec_uri;
}

std::vector<std::string> ServerlessSystemView::GetEphemeralStorageView(
  const std::string& engine_name)
const {
	return mgr_to_keys.at(engine_to_state.at(engine_name).ephemeral_name);
}

// Changes the storage file view based off of the received GetView. Uses the
// hostname of ephemeral managers to determine the client on the same machine.
// Assumes 1 per host.
void ServerlessSystemView::ParseGetViewReply(const GetViewReply& master_view) {
	std::lock_guard<std::mutex> lock(system_mutex);

	// Engines are created once the storage client and manager has been mapped.
	for (const auto& view : master_view.view()) {
		std::string hostname = uri_to_hostname(view.uri());
		std::string engine_name = hostname_to_engine[hostname];

		StorageName s_name;
		bool success = StorageName_Parse(name_to_storage_name(view.name()), &s_name);
		assert(success);

		if (s_name == ephemeral_) {
			// TODO(justinmiron): unnecessary for engines that are fully defined.
			EngineState& state = engine_to_state[engine_name];
			state.ephemeral_name = view.name();
		} else { // s_name == persistant_
			for (const auto& key : view.key()) {
				persistant_key_to_mgr[key] = view.name();
			}
		}

		// TODO(justinmiron): Change the deltas instead
		mgr_to_keys[view.name()] = 	std::vector<std::string>(view.key().begin(),
		                            view.key().end());
	}

	for (const auto& client : master_view.client()) {
		std::string hostname = uri_to_hostname(client.uri());
		std::string engine_name = hostname_to_engine[hostname];

		EngineState& state = engine_to_state[engine_name];
		state.client_name = client.name();
	}
}

void ServerlessSystemView::TasksScheduled(const SchedulingDecisions&
    decisions) {
	std::lock_guard<std::mutex> lock(system_mutex);
	for(const auto& decision : decisions.decisions) {
		EngineState & e = engine_to_state[decision.engine];
		e.running_tasks++;
	}
}


void ServerlessSystemView::GetSchedulableEngines(std::vector<std::string>*
    schedulable_engines) const {
	std::lock_guard<std::mutex> lock(system_mutex);
	schedulable_engines->clear();
	for (const auto& name_state_pair : engine_to_state) {
		if (name_state_pair.second.AvailableToSchedule()) {
			schedulable_engines->push_back(name_state_pair.first);
		}
	}
}

// Returns the client on the same machine as the engine.
EngineState ServerlessSystemView::GetEngineState(
  const std::string& engine_name) const {
	std::lock_guard<std::mutex> lock(system_mutex);
	return engine_to_state.at(engine_name);
}

// Returns the persistent manager that has a file. Allows scheduler
// to make rules for retrieval from persistent storage.
std::string ServerlessSystemView::GetPersistentMgrForFile(
  const std::string& file_name) const {
	std::lock_guard<std::mutex> lock(system_mutex);
	return persistant_key_to_mgr.at(file_name);
}


std::string ServerlessSystemView::GetPersistentMgrForPut(const std::string& file_name) const {
	std::lock_guard<std::mutex> lock(system_mutex);
	// TODO(justinmiron): Implement
	// Currently returns the mgr of the first key found
	return persistant_key_to_mgr.at(persistant_key_to_mgr.begin()->first);
}
