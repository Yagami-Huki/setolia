#pragma once

#include <QString>
#include <QStringList>

class TemplateManager {
public:
	QStringList listTemplateNames() const;
	bool openTemplateDirectory() const;
	bool addTemplateToCurrentScene(const QString &templateName) const;

	// Rewrites browser sources that load a template as a local file (is_local_file=true,
	// local_file under the template folder) to load it from the plugin's HTTP server.
	// Returns the number of converted sources.
	int migrateLocalFileSources() const;
};
