#include "template_paths.hpp"

#include <obs-module.h>

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QUrl>

namespace setolia {

QString templatesDirectoryPath()
{
	char *templatesDir = obs_module_file("templates");
	if (!templatesDir) {
		return QString();
	}
	const QString path = QString::fromUtf8(templatesDir);
	bfree(templatesDir);
	// obs_module_file may return a path containing "../" (e.g. on Windows); normalize it.
	return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString templateUrl(const QString &relativePath)
{
	QStringList encoded;
	for (const QString &segment : relativePath.split('/', Qt::SkipEmptyParts)) {
		encoded.append(QString::fromUtf8(QUrl::toPercentEncoding(segment)));
	}
	return QStringLiteral("http://127.0.0.1:%1%2/%3")
		.arg(kHttpServerPort)
		.arg(QString::fromLatin1(kTemplatesMountPoint))
		.arg(encoded.join('/'));
}

} // namespace setolia
