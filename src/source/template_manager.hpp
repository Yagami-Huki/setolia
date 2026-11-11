#pragma once

#include <QString>
#include <QStringList>

class TemplateManager {
public:
	// Templates of the user folder and the bundled folder, each name once (see setolia::mergeTemplateNames).
	// If overridden is given, it receives the user templates that hide a bundled template of the same name.
	QStringList listTemplateNames(QStringList *overridden = nullptr) const;
	// Opens the user template folder (created if missing; a README.txt is placed in it while it is empty).
	bool openTemplateDirectory() const;
	// Creates the user template folder and copies custom templates that older versions kept in the bundled
	// folder into it (copy, not move: the bundled folder may not be writable). Call once at startup.
	void prepareUserTemplateDirectory() const;
	bool addTemplateToCurrentScene(const QString &templateName) const;

	// Points template browser sources at the plugin's HTTP server on the current port:
	//  - is_local_file=true and local_file is a SETOLIA template (current folder or an old install
	//    location, see setolia::templateRelativePathFromLocalFile) -> http URL
	//  - http://127.0.0.1|localhost:<other port>/templates/... -> same URL on the current port
	// Returns the number of converted sources.
	int migrateTemplateSources() const;
};
