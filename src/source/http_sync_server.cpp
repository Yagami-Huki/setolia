#include "http_sync_server.hpp"

#include "../../deps/httplib.h"

#include <obs-module.h>
#include <QCryptographicHash>
#include <QByteArray>

#include <iostream>

#define PORT 8080

HTTPSyncServer::HTTPSyncServer() : srv(std::make_unique<httplib::Server>()) {}

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

	srv_stopped.store(false);
	srv_running.store(true);

	// Start server in background thread
	srv_thread = std::thread([this]() {
		if (!srv->listen("127.0.0.1", PORT)) {
			blog(LOG_ERROR, "HTTPSyncServer: Failed to start server");
			srv_running.store(false);
		}
		waitForShutdown();
	});
}

void HTTPSyncServer::stopServer()
{
	if (!srv_running && srv_stopped) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(srv_mutex);
		srv_stopped.store(true);
	}
	srv_cond.notify_all();

	if (srv && srv->is_running()) {
		srv->stop();
	}

	if (srv_thread.joinable()) {
		srv_thread.join();
	}

	srv_running.store(false);
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
					return srv_stopped.load() || (latestData != lastSentData);
				});

				// Check if server is shutting down
				if (srv_stopped.load()) {
					break;
				}

				// Send data if changed
				if (latestData != lastSentData) {
					std::string payload = "data: " + latestData + "\n\n";

					if (!sink.write(payload.data(), payload.size())) {
						break;
					}

					lastSentData = latestData;
				}
			}
			return true;
		});
	});

	// GET /ws - WebSocket endpoint for real-time updates
	srv->Get("/ws", [this](const httplib::Request &req, httplib::Response &res) {
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
		QByteArray hash = QCryptographicHash::hash(QByteArray::fromStdString(combined), QCryptographicHash::Sha1);
		std::string accept_key = hash.toBase64().toStdString();

		res.status = 101;
		res.set_header("Upgrade", "websocket");
		res.set_header("Connection", "Upgrade");
		res.set_header("Sec-WebSocket-Accept", accept_key);

		res.set_content_provider("text/plain", [this](size_t, httplib::DataSink &sink) -> bool {
			std::string lastSentData = "";
			std::unique_lock<std::mutex> lock(srv_mutex);

			// Send initial payload if available
			if (!latestData.empty()) {
				std::string text = latestData;
				size_t len = text.size();
				std::string frame;
				frame.push_back((char)0x81); // FIN + Text Opcode
				if (len <= 125) {
					frame.push_back((char)len);
				} else if (len <= 65535) {
					frame.push_back((char)126);
					frame.push_back((char)((len >> 8) & 0xFF));
					frame.push_back((char)(len & 0xFF));
				} else {
					frame.push_back((char)127);
					for (int i = 7; i >= 0; i--) {
						frame.push_back((char)((len >> (i * 8)) & 0xFF));
					}
				}
				frame += text;
				if (!sink.write(frame.data(), frame.size())) return false;
				lastSentData = latestData;
			}

			while (true) {
				srv_cond.wait(lock, [this, &lastSentData] {
					return srv_stopped.load() || (latestData != lastSentData);
				});

				if (srv_stopped.load()) {
					break;
				}

				if (latestData != lastSentData) {
					std::string text = latestData;
					size_t len = text.size();
					std::string frame;
					frame.push_back((char)0x81);
					if (len <= 125) {
						frame.push_back((char)len);
					} else if (len <= 65535) {
						frame.push_back((char)126);
						frame.push_back((char)((len >> 8) & 0xFF));
						frame.push_back((char)(len & 0xFF));
					} else {
						frame.push_back((char)127);
						for (int i = 7; i >= 0; i--) {
							frame.push_back((char)((len >> (i * 8)) & 0xFF));
						}
					}
					frame += text;

					if (!sink.write(frame.data(), frame.size())) {
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
}

void HTTPSyncServer::waitForShutdown()
{
	std::unique_lock<std::mutex> lock(srv_mutex);
	srv_cond.wait(lock, [this] { return srv_stopped.load(); });
}
