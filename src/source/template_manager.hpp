#pragma once

#include <QString>
#include <QStringList>

class TemplateManager {
public:
	QStringList listTemplateNames() const;
	bool openTemplateDirectory() const;
	bool addTemplateToCurrentScene(const QString &templateName) const;

	// Points template browser sources at the plugin's HTTP server on the current port:
	//  - is_local_file=true and local_file is a SETOLIA template (current folder or an old install
	//    location, see setolia::templateRelativePathFromLocalFile) -> http URL
	//  - http://127.0.0.1|localhost:<other port>/templates/... -> same URL on the current port
	// Returns the number of converted sources.
	int migrateTemplateSources() const;
};
