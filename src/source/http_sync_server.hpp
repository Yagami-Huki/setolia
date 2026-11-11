#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
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
	// Moves a running server to another port without a gap in which neither port is served: the new port is
	// bound first and the old server is stopped only if that succeeded. On failure the old server keeps
	// running untouched and false is returned. Works like startServer() when no server is running.
	bool switchPort(int port);
	void stopServer();
	bool isRunning() const;
	// Port the running server is bound to (0 when stopped).
	int port() const { return isRunning() ? boundPort : 0; }

	// Data synchronization
	void updateCachedData(const std::string &newData);
	std::string getCachedData() const;

	// External command handling (called from HTTP handler)
	void notifyNextSongRequested();

	// When true, POST /next is accepted only without an Origin header or from the plugin's own origin
	// (setolia::isNextRequestAllowed). Can be changed while the server runs.
	void setRestrictNextOrigin(bool restrict) { restrictNextOrigin.store(restrict); }

	// Set callback for when /next is requested
	void setNextSongCallback(std::function<void()> callback) { onNextSongRequested = std::move(callback); }

private:
	// Creates a server with all routes and binds it to 127.0.0.1:<port>. Returns nullptr if the bind fails.
	std::unique_ptr<httplib::Server> createBoundServer(int port);
	// Takes ownership of a bound server and serves it on srv_thread. No server may be running.
	void launch(std::unique_ptr<httplib::Server> server, int port);
	// port: the port the server will be bound to (used by the /next origin check).
	void setupRoutes(httplib::Server &server, int port);

	// Streams latestData to one /ws or /events client until the server stops or a write fails.
	// `encode` turns a data payload into the bytes to send; `keepAlive` is sent when nothing changed
	// for kKeepAliveInterval, which also detects clients that went away.
	bool streamUpdates(httplib::DataSink &sink, const std::function<std::string(const std::string &)> &encode,
			   const std::string &keepAlive);

	std::unique_ptr<httplib::Server> srv;
	std::thread srv_thread;
	int boundPort = 0;

	// Synchronization primitives
	mutable std::mutex srv_mutex; // guards latestData / dataVersion; never held while writing to a socket
	std::condition_variable srv_cond;
	mutable std::mutex data_mutex;

	// State
	std::string latestData;
	std::uint64_t dataVersion = 0;
	std::string cachedData;
	std::atomic<bool> srv_stopped{true};
	std::atomic<bool> restrictNextOrigin{false};

	// Origins of /next requests that were already logged, keyed with the decision (each origin is logged once
	// per OBS session when accepted and once when rejected).
	std::mutex loggedOrigins_mutex;
	std::set<std::string> loggedOrigins;
	// Returns true the first time a key is seen.
	bool markOriginLogged(const std::string &origin);

	// External callback
	std::function<void()> onNextSongRequested;
};
