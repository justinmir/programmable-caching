#ifndef SERVERLESS_SYSTEM_VIEW_
#define SERVERLESS_SYSTEM_VIEW_

#include "proto/scheduler.grpc.pb.h"
#include "proto/storage_service.grpc.pb.h"

#include <vector>
#include <string>

// Presents a view of the storage service as a two layer hierarchy.
//
// 			  Persistent Storage
//  					     |
//			-----------------------
//		Eph   	Eph	  	Eph	  	Eph
//		 |		   |	  	 |	  	 |
//	Client  Client	Client	Client
//     |			 |			 |			 |
//	 Exec		 Exec		 Exec		 Exec
//


class ServerlessSystemView {
 public:
	class EngineState {
	 public:
	 	EngineState(): name(""),
			client_name(""), ephemeral_name(""), running_tasks(0),
			maximum_tasks(-1){ }
		EngineState(std::string _name, int _maximum_tasks): name(_name),
			client_name(""), ephemeral_name(""), running_tasks(0),
			maximum_tasks(_maximum_tasks)
		{ }

		bool AvailableToSchedule() const {
			return (running_tasks < maximum_tasks &&
			        name != "" && client_name != "" && ephemeral_name != "");
		}

		std::string name;
		std::string client_name;
		std::string ephemeral_name;
		int running_tasks;
		int maximum_tasks;
	};

	// Engines with unknown client names: Mapping from client uri -> engine name.
	// Once view reply comes in we can move engine to maps.

	// Maps: Engine -> Client
	// Maps: Engine -> Ephemeral mgr

	ServerlessSystemView(StorageName ephemeral,
	                     StorageName persistent);

	/* System view management API. */

	/* Functions that construct the serverless system view */
	// Add execution engine
	void AddExecutionEngine(const std::string& exec_uri,
	                        const std::string& client_uri, int maximum_tasks);

	// Changes the storage file view based off of the received GetView. Uses the
	// hostname of ephemeral managers to determine the client on the same machine.
	// Assumes 1 per host.
	void ParseGetViewReply(const GetViewReply& master_view);

	// Returns a mapping of client to files, this allows scheduler to know what
	// files are cached. Assume if not cached, it is in persistent.
	std::vector<std::string> GetEphemeralStorageView(const std::string& engine_name)
	const;

	/* Scheduler API. */
	void GetSchedulableEngines(std::vector<std::string>* schedulable_engines) const;

	// Returns the client on the same machine as the engine.
	EngineState GetEngineState(const std::string& engine_name) const;

	// Returns the persistent manager that has a file. Allows scheduler
	// to make rules for retrieval from persistent storage.
	std::string GetPersistentMgrForFile(const std::string& file_name) const;

 private:
 	mutable std::mutex system_mutex;

 	StorageName ephemeral_;
 	StorageName persistant_;

	// Map from engine_to_client_uri
	std::unordered_map<std::string, std::string> client_uri_to_engine;

	// Currently running engines
	std::unordered_map<std::string, EngineState> engine_to_state;
	std::unordered_map<std::string, std::string> hostname_to_engine;

	// Manager to keys
	std::unordered_map<std::string, std::vector<std::string>> mgr_to_keys;
	std::unordered_map<std::string, std::string> persistant_key_to_mgr;
};

#endif