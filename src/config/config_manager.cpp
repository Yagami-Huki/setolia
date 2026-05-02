#include "config_manager.hpp"

#include <obs-module.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

ConfigManager::ConfigManager() = default;

QString ConfigManager::configFilePath() const
{
	char *configDir = obs_module_config_path("");
	if (!configDir) {
		blog(LOG_ERROR, "ConfigManager: Failed to get OBS config path");
		return QString();
	}

	QString path = QString::fromUtf8(configDir);
	bfree(configDir);

	if (!QFileInfo::exists(path)) {
		if (!QDir().mkdir(path)) {
			blog(LOG_ERROR, "ConfigManager: Failed to create config directory: %s",
			     path.toUtf8().constData());
			return QString();
		}
	}

	return path + "/setolia-config.json";
}

bool ConfigManager::validateConfig(const Config &config) const
{
	// Validate text lengths (prevent injection/overflow)
	if (config.reserveText.length() > 10000 || config.singingText.length() > 1000 ||
	    config.setlistText.length() > 10000 || config.selectedTemplate.length() > 256) {
		blog(LOG_WARNING, "ConfigManager: Config text exceeds maximum length");
		return false;
	}

	return true;
}

bool ConfigManager::loadConfig(Config &config) const
{
	const QString filePath = configFilePath();
	if (filePath.isEmpty()) {
		return true; // Not an error, just no config
	}

	// File doesn't exist yet - this is OK, will be created on first save
	if (!QFileInfo::exists(filePath)) {
		return true;
	}

	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_ERROR, "ConfigManager: Failed to open config file: %s", filePath.toUtf8().constData());
		return false;
	}

	const QByteArray fileData = file.readAll();
	file.close();

	const QJsonDocument jsonDoc = QJsonDocument::fromJson(fileData);
	if (!jsonDoc.isObject()) {
		blog(LOG_ERROR, "ConfigManager: Invalid JSON in config file");
		return false;
	}

	const QJsonObject jsonObj = jsonDoc.object();

	// Load with defaults
	config.reserveText = jsonObj.value("reserve").toString("");
	config.singingText = jsonObj.value("singing").toString("");
	config.setlistText = jsonObj.value("setlist").toString("");
	config.clockToggled = jsonObj.value("clockToggled").toBool(false);
	config.timestampToggled = jsonObj.value("timestampToggled").toBool(false);
	config.nextToggled = jsonObj.value("nextToggled").toBool(true);
	config.selectedTemplate = jsonObj.value("template").toString("");

	if (!validateConfig(config)) {
		blog(LOG_WARNING, "ConfigManager: Loaded config failed validation");
		return false;
	}

	return true;
}

bool ConfigManager::saveConfig(const Config &config) const
{
	if (!validateConfig(config)) {
		blog(LOG_ERROR, "ConfigManager: Config validation failed before save");
		return false;
	}

	const QString filePath = configFilePath();
	if (filePath.isEmpty()) {
		blog(LOG_ERROR, "ConfigManager: Cannot determine config file path");
		return false;
	}

	QJsonObject jsonObj;
	jsonObj["reserve"] = config.reserveText;
	jsonObj["singing"] = config.singingText;
	jsonObj["setlist"] = config.setlistText;
	jsonObj["clockToggled"] = config.clockToggled;
	jsonObj["timestampToggled"] = config.timestampToggled;
	jsonObj["nextToggled"] = config.nextToggled;
	jsonObj["template"] = config.selectedTemplate;

	const QJsonDocument jsonDoc(jsonObj);

	QFile file(filePath);
	if (!file.open(QIODevice::WriteOnly)) {
		blog(LOG_ERROR, "ConfigManager: Failed to open config file for writing: %s",
		     filePath.toUtf8().constData());
		return false;
	}

	if (file.write(jsonDoc.toJson()) < 0) {
		blog(LOG_ERROR, "ConfigManager: Failed to write config file: %s", filePath.toUtf8().constData());
		file.close();
		return false;
	}

	file.close();
	return true;
}
