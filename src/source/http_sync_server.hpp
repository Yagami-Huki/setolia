#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Forward declaration
namespace httplib {
class Server;
class DataSink;
} // namespace httplib

class HTTPSyncServer {
public:
	explicit HTTPSyncServer();
	~HTTPSyncServer();

	// Server lifecycle
	// Binds 127.0.0.1:<port> synchronously and, on success, serves on a background thread.
	// Returns false if the port could not be bound (e.g. already in use) or the server is already running.
	bool startServer(int port);
	void stopServer();
	bool isRunning() const;

	// Data synchronization
	void updateCachedData(const std::string &newData);
	std::string getCachedData() const;

	// External command handling (called from HTTP handler)
	void notifyNextSongRequested();

	// Set callback for when /next is requested
	void setNextSongCallback(std::function<void()> callback) { onNextSongRequested = std::move(callback); }

private:
	void setupRoutes();

	// Streams latestData to one /ws or /events client until the server stops or a write fails.
	// `encode` turns a data payload into the bytes to send; `keepAlive` is sent when nothing changed
	// for kKeepAliveInterval, which also detects clients that went away.
	bool streamUpdates(httplib::DataSink &sink, const std::function<std::string(const std::string &)> &encode,
			   const std::string &keepAlive);

	std::unique_ptr<httplib::Server> srv;
	std::thread srv_thread;

	// Synchronization primitives
	mutable std::mutex srv_mutex; // guards latestData / dataVersion; never held while writing to a socket
	std::condition_variable srv_cond;
	mutable std::mutex data_mutex;

	// State
	std::string latestData;
	std::uint64_t dataVersion = 0;
	std::string cachedData;
	std::atomic<bool> srv_stopped{true};

	// External callback
	std::function<void()> onNextSongRequested;
};
