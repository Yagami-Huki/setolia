#include "http_sync_server.hpp"

#include "../deps/httplib.h"

#include <obs-module.h>

#include <iostream>

#define PORT 8080

HTTPSyncServer::HTTPSyncServer() : srv(std::make_unique<httplib::Server>()), srv_stopped(true), srv_running(false) {}

HTTPSyncServer::~HTTPSyncServer()
{
	if (srv_running || isRunning()) {
		stopServer();
	}
}

void HTTPSyncServer::startServer()
{
	if (srv_running || !srv_stopped) {
		blog(LOG_WARNING, "HTTPSyncServer: Server already running or starting");
		return;
	}

	setupRoutes();

	srv_stopped = false;
	srv_running = true;

	// Start server in background thread
	srv_thread = std::thread([this]() {
		blog(LOG_INFO, "HTTPSyncServer: Starting HTTP server on 127.0.0.1:%d", PORT);
		if (!srv->listen("127.0.0.1", PORT)) {
			blog(LOG_ERROR, "HTTPSyncServer: Failed to start server");
			srv_running = false;
		}
		waitForShutdown();
	});
}

void HTTPSyncServer::stopServer()
{
	if (!srv_running && srv_stopped) {
		return;
	}

	blog(LOG_INFO, "HTTPSyncServer: Stopping server");

	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		srv_stopped = true;
	}
	srv_cond.notify_all();

	if (srv && srv->is_running()) {
		srv->stop();
	}

	if (srv_thread.joinable()) {
		srv_thread.join();
	}

	srv_running = false;
	blog(LOG_INFO, "HTTPSyncServer: Server stopped");
}

bool HTTPSyncServer::isRunning() const
{
	return srv && srv->is_running();
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
		}
		srv_cond.notify_all();
	}
}

std::string HTTPSyncServer::getCachedData() const
{
	std::lock_guard<std::mutex> lock(data_mutex);
	return cachedData;
}

void HTTPSyncServer::notifyNextSongRequested()
{
	if (onNextSongRequested) {
		onNextSongRequested();
	}
}

void HTTPSyncServer::setupRoutes()
{
	if (!srv) {
		blog(LOG_ERROR, "HTTPSyncServer: HTTP server not initialized");
		return;
	}

	// GET /data - Retrieve current state as JSON
	srv->Get("/data", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		std::string data = getCachedData();
		if (data.empty()) {
			// Return empty JSON object if no data
			data = "{}";
		}
		res.set_content(data, "application/json");
	});

	// GET /events - Server-Sent Events stream with auto-reconnect support
	srv->Get("/events", [this](const httplib::Request &, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");
		res.set_header("Content-Type", "text/event-stream");
		res.set_header("Cache-Control", "no-cache");
		res.set_header("Connection", "keep-alive");

		res.set_content_provider("text/event-stream", [this](size_t, httplib::DataSink &sink) -> bool {
			std::string lastSentData = "";
			std::unique_lock<std::mutex> lock(srv_mutex);

			while (true) {
				// Wait for data update or server stop
				srv_cond.wait(lock, [this, &lastSentData] {
					return srv_stopped || (latestData != lastSentData);
				});

				// Check if server is shutting down
				if (srv_stopped) {
					blog(LOG_DEBUG, "HTTPSyncServer: SSE client disconnecting (server stop)");
					break;
				}

				// Send data if changed
				if (latestData != lastSentData) {
					std::string payload = "data: " + latestData + "\n\n";

					if (!sink.write(payload.data(), payload.size())) {
						blog(LOG_DEBUG, "HTTPSyncServer: SSE client disconnect (write failed)");
						break;
					}

					lastSentData = latestData;
				}
			}
			return true;
		});
	});

	// POST /next - External command to trigger nextSong
	srv->Post("/next", [this](const httplib::Request &req, httplib::Response &res) {
		res.set_header("Access-Control-Allow-Origin", "*");

		// Security: Validate request origin if needed
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

	blog(LOG_INFO, "HTTPSyncServer: Routes configured");
}

void HTTPSyncServer::waitForShutdown()
{
	std::unique_lock<std::mutex> lock(srv_mutex);
	srv_cond.wait(lock, [this] { return srv_stopped; });
	blog(LOG_DEBUG, "HTTPSyncServer: Shutdown signal received");
}
