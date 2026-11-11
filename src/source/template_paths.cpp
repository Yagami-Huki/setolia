#include "template_paths.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <atomic>

namespace setolia {

namespace {

std::atomic<int> g_httpServerPort{kDefaultHttpServerPort};

// Accepts both separators regardless of the platform (scene collections may come from Windows).
QString normalizeSeparators(const QString &path)
{
	QString normalized = path;
	normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
	return QDir::cleanPath(normalized);
}

} // namespace

bool isValidHttpServerPort(int port)
{
	return port >= kMinHttpServerPort && port <= kMaxHttpServerPort;
}

int httpServerPort()
{
	return g_httpServerPort.load();
}

void setHttpServerPort(int port)
{
	if (isValidHttpServerPort(port)) {
		g_httpServerPort.store(port);
	}
}

namespace {

bool containsName(const QStringList &names, const QString &name, Qt::CaseSensitivity caseSensitivity)
{
	for (const QString &candidate : names) {
		if (QString::compare(candidate, name, caseSensitivity) == 0) {
			return true;
		}
	}
	return false;
}

} // namespace

MergedTemplateNames mergeTemplateNames(const QStringList &userNames, const QStringList &bundledNames,
				       Qt::CaseSensitivity caseSensitivity)
{
	MergedTemplateNames merged;
	for (const QString &name : userNames) {
		if (!containsName(merged.names, name, caseSensitivity)) {
			merged.names.append(name);
		}
		if (containsName(bundledNames, name, caseSensitivity) &&
		    !containsName(merged.overridden, name, caseSensitivity)) {
			merged.overridden.append(name);
		}
	}
	for (const QString &name : bundledNames) {
		if (!containsName(merged.names, name, caseSensitivity)) {
			merged.names.append(name);
		}
	}
	// Same order as QDir::entryList's default (name, ignoring case), which the dock used before.
	merged.names.sort(Qt::CaseInsensitive);
	merged.overridden.sort(Qt::CaseInsensitive);
	return merged;
}

QStringList customTemplatesToCopy(const QStringList &bundledNames, const QStringList &builtinNames,
				  const QStringList &userNames, Qt::CaseSensitivity caseSensitivity)
{
	QStringList result;
	for (const QString &name : bundledNames) {
		if (!containsName(builtinNames, name, caseSensitivity) &&
		    !containsName(userNames, name, caseSensitivity)) {
			result.append(name);
		}
	}
	return result;
}

bool copyDirectoryRecursively(const QString &source, const QString &destination)
{
	const QDir sourceDir(source);
	if (!sourceDir.exists() || !QDir().mkpath(destination)) {
		return false;
	}

	bool ok = true;
	QDirIterator it(source, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
			QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString path = it.next();
		const QFileInfo info = it.fileInfo();
		const QString target = QDir(destination).filePath(sourceDir.relativeFilePath(path));
		if (info.isDir()) {
			ok = QDir().mkpath(target) && ok;
		} else if (!QFileInfo::exists(target)) {
			ok = QDir().mkpath(QFileInfo(target).absolutePath()) && QFile::copy(path, target) && ok;
		}
	}
	return ok;
}

QString templateUrl(const QString &relativePath, int port)
{
	QStringList encoded;
	for (const QString &segment : relativePath.split('/', Qt::SkipEmptyParts)) {
		encoded.append(QString::fromUtf8(QUrl::toPercentEncoding(segment)));
	}
	return QStringLiteral("http://127.0.0.1:%1%2/%3")
		.arg(port)
		.arg(QString::fromLatin1(kTemplatesMountPoint))
		.arg(encoded.join('/'));
}

QString templateUrl(const QString &relativePath)
{
	return templateUrl(relativePath, httpServerPort());
}

Qt::CaseSensitivity platformPathCaseSensitivity()
{
#if defined(_WIN32)
	return Qt::CaseInsensitive;
#else
	return Qt::CaseSensitive;
#endif
}

const char *templateMigrationRuleName(TemplateMigrationRule rule)
{
	switch (rule) {
	case TemplateMigrationRule::CurrentTemplatesDir:
		return "current template folder";
	case TemplateMigrationRule::LegacyTemplatesPath:
		return "legacy template path";
	case TemplateMigrationRule::PortChange:
		return "port change";
	}
	return "unknown";
}

std::optional<TemplateLocalFileMatch> templateRelativePathFromLocalFile(const QString &localFile,
									const QString &templatesDir,
									const QStringList &templateNames,
									Qt::CaseSensitivity caseSensitivity)
{
	if (localFile.trimmed().isEmpty()) {
		return std::nullopt;
	}
	const QString file = normalizeSeparators(localFile);

	// (a) Under the current template folder.
	if (!templatesDir.isEmpty()) {
		const QString prefix = normalizeSeparators(templatesDir) + QLatin1Char('/');
		if (file.startsWith(prefix, caseSensitivity)) {
			const QString relativePath = file.mid(prefix.size());
			if (!relativePath.isEmpty()) {
				return TemplateLocalFileMatch{relativePath, TemplateMigrationRule::CurrentTemplatesDir};
			}
		}
	}

	// (b) .../setolia/templates/<name>/<file> of a template that exists now. Covers the OBS 32 data layout
	// (<OBS>/data/obs-plugins/setolia/templates, incl. old Program Files installs) and, for good measure,
	// the OBS 33 plugin layout (.../plugins/setolia/data/templates) of another install location.
	static const QString markers[] = {QStringLiteral("/setolia/templates/"),
					  QStringLiteral("/setolia/data/templates/")};
	qsizetype restPos = -1;
	for (const QString &marker : markers) {
		const qsizetype pos = file.lastIndexOf(marker, -1, caseSensitivity);
		if (pos >= 0 && pos + marker.size() > restPos) {
			restPos = pos + marker.size();
		}
	}
	if (restPos < 0) {
		return std::nullopt;
	}
	const QString rest = file.mid(restPos);
	const qsizetype slash = rest.indexOf(QLatin1Char('/'));
	if (slash <= 0 || slash == rest.size() - 1) {
		return std::nullopt; // no "<name>/<file>"
	}
	const QString name = rest.left(slash);
	const QString remainder = rest.mid(slash + 1);
	for (const QString &templateName : templateNames) {
		if (QString::compare(templateName, name, caseSensitivity) == 0) {
			return TemplateLocalFileMatch{templateName + QLatin1Char('/') + remainder,
						      TemplateMigrationRule::LegacyTemplatesPath};
		}
	}
	return std::nullopt;
}

std::optional<QString> rewriteTemplateUrlPort(const QString &url, int port)
{
	static const QRegularExpression pattern(
		QStringLiteral("^(http://(?:127\\.0\\.0\\.1|localhost)):(\\d{1,5})(/templates/.*)$"),
		QRegularExpression::CaseInsensitiveOption);

	const QRegularExpressionMatch match = pattern.match(url.trimmed());
	if (!match.hasMatch()) {
		return std::nullopt;
	}
	if (match.captured(2).toInt() == port) {
		return std::nullopt;
	}
	return match.captured(1) + QLatin1Char(':') + QString::number(port) + match.captured(3);
}

} // namespace setolia
