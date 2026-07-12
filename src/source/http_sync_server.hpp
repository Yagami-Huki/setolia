#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>

// Forward declaration
namespace httplib {
class Server;
}

class HTTPSyncServer {
public:
	explicit HTTPSyncServer();
	~HTTPSyncServer();

	// Server lifecycle
	void startServer();
	void stopServer();
	bool isRunning() const;

	// Data synchronization
	void updateCachedData(const std::string &newData);
	std::string getCachedData() const;

	// External command handling (called from HTTP handler)
	void notifyNextSongRequested();

private:
	void setupRoutes();
	void waitForShutdown();

	std::unique_ptr<httplib::Server> srv;
	std::thread srv_thread;

	// Synchronization primitives
	mutable std::mutex srv_mutex;
	std::condition_variable srv_cond;
	mutable std::mutex data_mutex;

	// State
	std::string latestData;
	std::string cachedData;
	std::atomic<bool> srv_stopped{true};
	std::atomic<bool> srv_running{false};

	// External callback
	std::function<void()> onNextSongRequested;

public:
	// Set callback for when /next is requested
	void setNextSongCallback(std::function<void()> callback) { onNextSongRequested = callback; }
};
