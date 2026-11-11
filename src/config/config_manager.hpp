#pragma once

#include "../source/template_paths.hpp"

#include <QString>
#include <memory>

class Config {
public:
	// Text content
	QString reserveText;
	QString singingText;
	QString setlistText;

	// Toggles
	bool clockToggled = false;
	bool timestampToggled = false;
	bool nextToggled = true;

	// Template
	QString selectedTemplate;

	// HTTP server port (setolia::kMinHttpServerPort..kMaxHttpServerPort)
	int port = setolia::kDefaultHttpServerPort;

	// Reject POST /next from web pages (requests with a foreign Origin header). Off by default.
	bool restrictNextOrigin = false;
};

class ConfigManager {
public:
	explicit ConfigManager();

	// Load configuration from OBS config path
	// Returns true if successfully loaded (or config didn't exist)
	bool loadConfig(Config &config) const;

	// Save configuration to OBS config path
	// Returns true if successfully saved
	bool saveConfig(const Config &config) const;

private:
	QString configFilePath() const;
	bool validateConfig(const Config &config) const;
};
