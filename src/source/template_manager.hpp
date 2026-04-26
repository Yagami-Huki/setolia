#pragma once

#include <QString>
#include <QStringList>

class TemplateManager {
public:
	QStringList listTemplateNames() const;
	bool openTemplateDirectory() const;
	bool addTemplateToCurrentScene(const QString &templateName) const;
};
