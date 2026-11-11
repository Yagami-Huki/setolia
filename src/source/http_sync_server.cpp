#include "http_sync_server.hpp"
#include "next_request_policy.hpp"
#include "template_paths.hpp"
#include "ws_frame.hpp"

#include "../../deps/httplib.h"

#include <obs-module.h>
#include <QCryptographicHash>
#include <QByteArray>

#include <chrono>

namespace {

// Each /ws and /events client occupies one worker thread for as long as it is connected, and every template
// browser source keeps such a connection open even while its scene is not shown. httplib's default pool is
// max(8, cores - 1) threads, so a dozen template sources could exhaust it and leave template files, /data and
// /next queued forever. 64 threads leaves plenty of headroom for many sources plus regular requests; idle
// workers just sleep on the task queue.
constexpr size_t kThreadPoolSize = 64;

// /ws and /events send a keep-alive after this much silence. A failed write ends the stream and frees the
// worker thread, so clients that disappeared are detected within one or two intervals.
constexpr auto kKeepAliveInterval = std::chrono::seconds(15);

const char *const kBindAddress = "127.0.0.1";

void configureListenSocket([[maybe_unused]] socket_t sock)
{
#ifdef _WIN32
	// httplib's default sets SO_REUSEADDR, which on Windows lets bind() succeed on a port another
	// application is already listening on. Use exclusive binding so a port conflict fails bind_to_port.
	BOOL exclusive = TRUE;
	setsockopt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char *>(&exclusive),
		   sizeof(exclusive));
#else
	// SO_REUSEADDR only (no SO_REUSEPORT): allows rebinding while old connections are in TIME_WAIT after a
	// restart, but still fails when another socket is listening on the port.
	int yes = 1;
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif
}

} // namespace

HTTPSyncServer::HTTPSyncServer() = default;

HTTPSyncServer::~HTTPSyncServer()
{
	stopServer();
}

std::unique_ptr<httplib::Server> HTTPSyncServer::createBoundServer(int port)
{
	// A stopped httplib::Server cannot be reused, so every start builds a fresh instance.
	auto server = std::make_unique<httplib::Server>();
	server->new_task_queue = [] {
		return new httplib::ThreadPool(kThreadPoolSize);
	};
	server->set_socket_options(configureListenSocket);
	setupRoutes(*server, port);

	// Bind synchronously so a port conflict is reported to the caller right away.
	if (!server->bind_to_port(kBindAddress, port)) {
		blog(LOG_ERROR, "HTTPSyncServer: Failed to bind %s:%d (port in use?)", kBindAddress, port);
		return nullptr;
	}
	return server;
}

void HTTPSyncServer::launch(std::unique_ptr<httplib::Server> server, int port)
{
	srv = std::move(server);
	boundPort = port;
	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		srv_stopped.store(false);
	}

	srv_thread = std::thread([this]() {
		if (!srv->listen_after_bind()) {
			blog(LOG_ERROR, "HTTPSyncServer: Server stopped with an error");
		}
	});

	blog(LOG_INFO, "HTTPSyncServer: Listening on %s:%d", kBindAddress, port);
}

bool HTTPSyncServer::startServer(int port)
{
	stopServer();
	auto server = createBoundServer(port);
	if (!server) {
		return false;
	}
	launch(std::move(server), port);
	return true;
}

bool HTTPSyncServer::switchPort(int port)
{
	// Different ports can be bound at the same time, so the old server stays up until the new one is bound.
	auto server = createBoundServer(port);
	if (!server) {
		return false;
	}
	stopServer();
	launch(std::move(server), port);
	return true;
}

void HTTPSyncServer::stopServer()
{
	// Wake every /ws and /events stream so it returns and releases its worker thread.
	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		srv_stopped.store(true);
	}
	srv_cond.notify_all();

	if (srv) {
		srv->stop();
	}

	// listen_after_bind() returns after the thread pool has joined all worker threads.
	if (srv_thread.joinable()) {
		srv_thread.join();
	}

	srv.reset();
	boundPort = 0;
}

bool HTTPSyncServer::isRunning() const
{
	// srv exists only between a successful bind and stopServer(). (srv->is_running() turns true a moment
	// later, once the listen thread has started, so it is not used here.)
	return srv != nullptr && !srv_stopped.load();
}

void HTTPSyncServer::updateCachedData(const std::string &newData)
{
	std::string oldData;
	{
		std::lock_guard<std::mutex> lock(data_mutex);
		oldData = cachedData;
		cachedData = newData;
	}

	// Only notify if data changed (idempotency)
	if (oldData != newData) {
		{
			std::lock_guard<std::mutex> lock(srv_mutex);
			latestData = newData;
			dataVersion++;
		}
		srv_cond.notify_all();
	}
}

std::string HTTPSyncServer::getCachedData() const
{
	std::lock_guard<std::mutex> lock(data_mutex);
	return cachedData;
}

bool HTTPSyncServer::markOriginLogged(const std::string &origin)
{
	std::lock_guard<std::mutex> lock(loggedOrigins_mutex);
	return loggedOrigins.insert(origin).second;
}

void HTTPSyncServer::notifyNextSongRequested()
{
	if (onNextSongRequested) {
		onNextSongRequested();
	}
}

bool HTTPSyncServer::streamUpdates(httplib::DataSink &sink,
				   const std::function<std::string(const std::string &)> &encode,
				   const std::string &keepAlive)
{
	// srv_mutex is only held to copy the shared state; socket writes happen without it so a slow
	// client can never block updateCachedData() (called from the OBS UI thread).
	std::string payload;
	std::uint64_t sentVersion = 0;
	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		if (srv_stopped.load()) {
			return false;
		}
		payload = latestData;
		sentVersion = dataVersion;
	}

	// Send the current state right away so a newly connected client does not wait for the next change.
	if (!payload.empty()) {
		const std::string bytes = encode(payload);
		if (!sink.write(bytes.data(), bytes.size())) {
			return false;
		}
	}

	while (true) {
		bool changed = false;
		{
			std::unique_lock<std::mutex> lock(srv_mutex);
			changed = srv_cond.wait_for(lock, kKeepAliveInterval, [this, sentVersion] {
				return srv_stopped.load() || dataVersion != sentVersion;
			});
			if (srv_stopped.load()) {
				break;
			}
			if (changed) {
				payload = latestData;
				sentVersion = dataVersion;
			}
		}

		const std::string bytes = changed ? encode(payload) : keepAlive;
		if (!sink.write(bytes.data(), bytes.size())) {
			break; // client went away
		}
	}

	// Returning false ends the response and closes the connection. Returning true would make httplib
	// call the provider again for as long as the server is not yet shutting down.
	return false;
}

void HTTPSyncServer::setupRoutes(httplib::Server &server, int port)
{

	// GET /templates/* - Serve template files so browser sources load them over http://127.0.0.1
	// instead of as local files (OBS 33 / CEF blocks fetch from file:// pages to 127.0.0.1).
	// The user template folder is mounted first: httplib tries mount points in registration order, so a
	// file in the user folder wins over the bundled file with the same path.
	// no-cache: edits to templates are picked up on the next reload.
	for (const QString &templatesDir : {setolia::userTemplatesDirectoryPath(), setolia::templatesDirectoryPath()}) {
		if (templatesDir.isEmpty() ||
		    !server.set_mount_point(setolia::kTemplatesMountPoint, templatesDir.toUtf8().toStdString(),
					    {{"Cache-Control", "no-cache"}})) {
			blog(LOG_WARNING, "HTTPSyncServer: Failed to mount templates directory: %s",
			     templatesDir.toUtf8().constData());
		}
	}

	// GET /data - Retrieve current state as JSON
	server.Get("/data", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		std::string data = getCachedData();
		if (data.empty()) {
			// Return empty JSON object if no data
			data = "{}";
		}
		res.set_content(data, "application/json");
	});

	// GET /events - Server-Sent Events stream with auto-reconnect support
	server.Get("/events", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		res.set_header("Content-Type", "text/event-stream");
		res.set_header("Cache-Control", "no-cache");
		res.set_header("Connection", "keep-alive");

		res.set_content_provider("text/event-stream", [this](size_t, httplib::DataSink &sink) -> bool {
			return streamUpdates(
				sink, [](const std::string &data) { return "data: " + data + "\n\n"; }, ": ping\n\n");
		});
	});

	// GET /ws - WebSocket endpoint for real-time updates
	server.Get("/ws", [this](const httplib::Request &req, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");

		if (req.get_header_value("Upgrade") != "websocket") {
			res.status = 400;
			res.set_content("Bad Request: Expected WebSocket upgrade", "text/plain");
			return;
		}

		std::string ws_key = req.get_header_value("Sec-WebSocket-Key");
		if (ws_key.empty()) {
			res.status = 400;
			res.set_content("Bad Request: Missing Sec-WebSocket-Key", "text/plain");
			return;
		}

		std::string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
		std::string combined = ws_key + magic;
		QByteArray hash =
			QCryptographicHash::hash(QByteArray::fromStdString(combined), QCryptographicHash::Sha1);
		std::string accept_key = hash.toBase64().toStdString();

		res.status = 101;
		res.set_header("Upgrade", "websocket");
		res.set_header("Connection", "Upgrade");
		res.set_header("Sec-WebSocket-Accept", accept_key);

		res.set_content_provider("text/plain", [this](size_t, httplib::DataSink &sink) -> bool {
			return streamUpdates(sink, setolia::buildWsTextFrame, setolia::buildWsPingFrame());
		});
	});

	// POST /next - External command to trigger nextSong
	server.Post("/next", [this, port](const httplib::Request &req, httplib::Response &res) {
		const bool restrict = restrictNextOrigin.load();
		const std::string origin = req.get_header_value("Origin");
		const bool allowed = setolia::isNextRequestAllowed(origin, port, restrict);

		// Log each Origin once per session (whether or not the restriction is on), so users can find out
		// whether their Stream Deck plugin sends one before enabling the restriction.
		if (!origin.empty() && markOriginLogged((allowed ? "accepted " : "rejected ") + origin)) {
			blog(allowed ? LOG_INFO : LOG_WARNING,
			     "HTTPSyncServer: POST /next with Origin '%s' %s (restriction %s; further such requests from "
			     "this origin are not logged)",
			     origin.c_str(), allowed ? "accepted" : "rejected with 403", restrict ? "on" : "off");
		}

		if (!restrict) {
			// Unrestricted (default): unchanged from earlier versions.
			res.set_header("Access-Control-Allow-Origin", "*");
		}
		if (!allowed) {
			res.status = 403;
			res.set_content("Forbidden", "text/plain");
			return;
		}

		std::string body = req.body;
		if (body.length() > 1024) {
			res.status = 413; // Payload Too Large
			res.set_content("Payload too large", "text/plain");
			return;
		}

		try {
			// Trigger next song callback
			notifyNextSongRequested();
			res.status = 200;
			res.set_content("OK", "text/plain");
		} catch (const std::exception &e) {
			blog(LOG_ERROR, "HTTPSyncServer: Error handling POST /next: %s", e.what());
			res.status = 500;
			res.set_content("Internal server error", "text/plain");
		}
	});
}
