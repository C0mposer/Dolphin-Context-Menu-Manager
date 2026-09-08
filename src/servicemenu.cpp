#include "servicemenu.h"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>

namespace {

QString firstCommandToken(const QString &command, QString *remainder)
{
    const QString text = command.trimmed();
    if (text.isEmpty()) {
        if (remainder) {
            remainder->clear();
        }
        return {};
    }

    QString token;
    qsizetype index = 0;
    bool quoted = false;
    QChar quote;

    if (text.front() == QLatin1Char('"') || text.front() == QLatin1Char('\'')) {
        quoted = true;
        quote = text.front();
        ++index;
    }

    bool escaped = false;
    for (; index < text.size(); ++index) {
        const QChar character = text.at(index);
        if (escaped) {
            token.append(character);
            escaped = false;
            continue;
        }
        if (character == QLatin1Char('\\')) {
            escaped = true;
            continue;
        }
        if ((quoted && character == quote) || (!quoted && character.isSpace())) {
            ++index;
            break;
        }
        token.append(character);
    }

    if (escaped) {
        token.append(QLatin1Char('\\'));
    }
    if (remainder) {
        *remainder = text.mid(index).trimmed();
    }
    return token;
}

QString desktopQuote(QString argument)
{
    argument.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    argument.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    argument.replace(QLatin1Char('`'), QStringLiteral("\\`"));
    argument.replace(QLatin1Char('$'), QStringLiteral("\\$"));
    return QStringLiteral("\"") + argument + QStringLiteral("\"");
}

bool isInsideDirectory(const QString &path, const QString &directory)
{
    const QString cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString cleanDirectory = QDir::cleanPath(QFileInfo(directory).absoluteFilePath());
    return cleanPath == cleanDirectory
        || cleanPath.startsWith(cleanDirectory + QDir::separator());
}

} // namespace

IniSection *IniDocument::findSection(const QString &name)
{
    auto it = std::find_if(m_sections.begin(), m_sections.end(), [&](const IniSection &section) {
        return section.name == name;
    });
    return it == m_sections.end() ? nullptr : &(*it);
}

const IniSection *IniDocument::findSection(const QString &name) const
{
    auto it = std::find_if(m_sections.cbegin(), m_sections.cend(), [&](const IniSection &section) {
        return section.name == name;
    });
    return it == m_sections.cend() ? nullptr : &(*it);
}

bool IniDocument::load(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    m_sections.clear();
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    IniSection *currentSection = nullptr;
    qsizetype lineNumber = 0;

    while (!stream.atEnd()) {
        QString line = stream.readLine();
        ++lineNumber;
        if (lineNumber == 1 && !line.isEmpty() && line.front() == QChar::ByteOrderMark) {
            line.removeFirst();
        }

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))
            || trimmed.startsWith(QLatin1Char(';'))) {
            continue;
        }
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            const QString name = trimmed.mid(1, trimmed.size() - 2).trimmed();
            m_sections.append({name, {}});
            currentSection = &m_sections.last();
            continue;
        }

        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (!currentSection || equals <= 0) {
            continue;
        }
        currentSection->entries.append({line.left(equals).trimmed(), line.mid(equals + 1)});
    }
    return true;
}

QString IniDocument::value(const QString &section, const QString &key,
                           const QString &fallback) const
{
    const IniSection *found = findSection(section);
    if (!found) {
        return fallback;
    }
    for (auto it = found->entries.crbegin(); it != found->entries.crend(); ++it) {
        if (it->key == key) {
            return it->value;
        }
    }
    return fallback;
}

void IniDocument::setValue(const QString &section, const QString &key, const QString &value)
{
    IniSection *found = findSection(section);
    if (!found) {
        m_sections.append({section, {}});
        found = &m_sections.last();
    }
    for (IniEntry &entry : found->entries) {
        if (entry.key == key) {
            entry.value = value;
            return;
        }
    }
    found->entries.append({key, value});
}

void IniDocument::removeKey(const QString &section, const QString &key)
{
    IniSection *found = findSection(section);
    if (!found) {
        return;
    }
    found->entries.removeIf([&](const IniEntry &entry) { return entry.key == key; });
}

void IniDocument::removeSection(const QString &section)
{
    m_sections.removeIf([&](const IniSection &item) { return item.name == section; });
}

QStringList IniDocument::sectionNames() const
{
    QStringList result;
    for (const IniSection &section : m_sections) {
        result.append(section.name);
    }
    return result;
}

QString IniDocument::serialize() const
{
    QString result;
    QTextStream stream(&result);
    for (qsizetype sectionIndex = 0; sectionIndex < m_sections.size(); ++sectionIndex) {
        const IniSection &section = m_sections.at(sectionIndex);
        stream << '[' << section.name << "]\n";
        for (const IniEntry &entry : section.entries) {
            stream << entry.key << '=' << entry.value << '\n';
        }
        if (sectionIndex + 1 < m_sections.size()) {
            stream << '\n';
        }
    }
    return result;
}

QString ServiceMenu::displayName() const
{
    if (!submenu.trimmed().isEmpty()) {
        return submenu.trimmed();
    }
    if (!actions.isEmpty() && !actions.first().name.trimmed().isEmpty()) {
        if (actions.size() == 1) {
            return actions.first().name.trimmed();
        }
        return QStringLiteral("%1 (+%2 more)")
            .arg(actions.first().name.trimmed())
            .arg(actions.size() - 1);
    }
    if (!path.isEmpty()) {
        return QFileInfo(path).completeBaseName();
    }
    return QStringLiteral("New context action");
}

ServiceMenuStore::ServiceMenuStore(QString userDirectory)
{
    m_refreshEnabled = userDirectory.isEmpty();
    if (userDirectory.isEmpty()) {
        userDirectory = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
            + QStringLiteral("/kio/servicemenus");
    }
    m_userDirectory = QDir::cleanPath(userDirectory);
}

QString ServiceMenuStore::userDirectory() const
{
    return m_userDirectory;
}

QStringList ServiceMenuStore::dataLocations() const
{
    QStringList result{m_userDirectory};
    const QStringList roots = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &root : roots) {
        const QString directory = QDir::cleanPath(root + QStringLiteral("/kio/servicemenus"));
        if (!result.contains(directory)) {
            result.append(directory);
        }
    }
    return result;
}

QList<ServiceMenu> ServiceMenuStore::loadMenus() const
{
    QList<ServiceMenu> result;
    for (const QString &directoryPath : dataLocations()) {
        QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        const bool userDirectory = QDir::cleanPath(directory.absolutePath())
            == QDir::cleanPath(m_userDirectory);
        QStringList filters{QStringLiteral("*.desktop")};
        if (userDirectory) {
            filters.append(QStringLiteral("*.desktop.disabled"));
        }

        const QFileInfoList files = directory.entryInfoList(
            filters, QDir::Files | QDir::Readable, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo &file : files) {
            ServiceMenu menu = loadMenu(file.absoluteFilePath(), userDirectory);
            if (menu.document.value(QStringLiteral("Desktop Entry"), QStringLiteral("Type"))
                    != QStringLiteral("Service")) {
                continue;
            }
            if (menu.actions.isEmpty() && menu.loadError.isEmpty()) {
                continue;
            }
            result.append(std::move(menu));
        }
    }

    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::sort(result.begin(), result.end(), [&](const ServiceMenu &left, const ServiceMenu &right) {
        if (left.userOwned != right.userOwned) {
            return left.userOwned;
        }
        return collator.compare(left.displayName(), right.displayName()) < 0;
    });
    return result;
}

ServiceMenu ServiceMenuStore::loadMenu(const QString &path, bool userOwned) const
{
    ServiceMenu menu;
    menu.path = QFileInfo(path).absoluteFilePath();
    menu.userOwned = userOwned;
    menu.enabled = !path.endsWith(QStringLiteral(".disabled"));

    if (!menu.document.load(path, &menu.loadError)) {
        return menu;
    }

    const QString desktopEntry = QStringLiteral("Desktop Entry");
    const auto managedValue = [&menu](const QString &section, const QString &name) {
        const QString currentKey = QStringLiteral("X-DolphinContextMenuManager-") + name;
        const QString legacyKey = QStringLiteral("X-DolphinContextManager-") + name;
        const QString currentValue = menu.document.value(section, currentKey);
        return currentValue.isEmpty() ? menu.document.value(section, legacyKey) : currentValue;
    };
    menu.mimeTypes = normalizedMimeTypes(menu.document.value(
        desktopEntry, QStringLiteral("MimeType"), QStringLiteral("application/octet-stream;")));
    menu.submenu = menu.document.value(desktopEntry, QStringLiteral("X-KDE-Submenu"));
    menu.topLevel = menu.document.value(desktopEntry, QStringLiteral("X-KDE-Priority"))
        == QStringLiteral("TopLevel");
    menu.extensions = managedValue(desktopEntry, QStringLiteral("Extensions"))
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (menu.extensions.isEmpty() && !menu.mimeTypes.contains(QLatin1Char('*'))
        && menu.mimeTypes != QStringLiteral("application/octet-stream;")
        && menu.mimeTypes != QStringLiteral("inode/directory;")) {
        QMimeDatabase database;
        const QStringList types = menu.mimeTypes.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &type : types) {
            const QString suffix = database.mimeTypeForName(type).preferredSuffix();
            if (!suffix.isEmpty()) {
                const QString extension = QLatin1Char('.') + suffix.toLower();
                if (!menu.extensions.contains(extension)) {
                    menu.extensions.append(extension);
                }
            }
        }
    }

    const QStringList actionIds = menu.document.value(desktopEntry, QStringLiteral("Actions"))
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    for (const QString &rawId : actionIds) {
        ServiceAction action;
        action.id = rawId.trimmed();
        const QString section = QStringLiteral("Desktop Action ") + action.id;
        action.name = menu.document.value(section, QStringLiteral("Name"), action.id);
        action.icon = menu.document.value(
            section, QStringLiteral("Icon"), QStringLiteral("application-x-executable"));

        const QString managedProgram = managedValue(section, QStringLiteral("Program"));
        if (!managedProgram.isEmpty()) {
            action.program = managedProgram;
            action.arguments = managedValue(section, QStringLiteral("Arguments"));
            action.runInTerminal = managedValue(section, QStringLiteral("Terminal"))
                == QStringLiteral("true");
        } else {
            QString remainder;
            action.program = firstCommandToken(
                menu.document.value(section, QStringLiteral("Exec")), &remainder);
            action.arguments = remainder;
        }
        menu.actions.append(std::move(action));
    }
    return menu;
}

ServiceMenu ServiceMenuStore::createBlank() const
{
    ServiceMenu menu;
    ServiceAction action;
    action.id = QStringLiteral("newAction");
    action.name = QStringLiteral("New context action");
    menu.actions.append(action);
    return menu;
}

QString ServiceMenuStore::normalizedMimeTypes(const QString &mimeTypes)
{
    QStringList result;
    const QStringList candidates = mimeTypes.split(
        QRegularExpression(QStringLiteral("[;,\\s]+")), Qt::SkipEmptyParts);
    for (const QString &candidate : candidates) {
        const QString cleaned = candidate.trimmed();
        if (!cleaned.isEmpty() && !result.contains(cleaned)) {
            result.append(cleaned);
        }
    }
    if (result.isEmpty()) {
        return {};
    }
    return result.join(QLatin1Char(';')) + QLatin1Char(';');
}

QString ServiceMenuStore::makeActionId(const QString &name, const QStringList &existingIds)
{
    QString base = name.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
    const QStringList words = base.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.isEmpty()) {
        base = QStringLiteral("action");
    } else {
        base = words.first();
        for (qsizetype index = 1; index < words.size(); ++index) {
            QString word = words.at(index);
            word[0] = word.at(0).toUpper();
            base += word;
        }
    }

    QString candidate = base;
    int suffix = 2;
    while (existingIds.contains(candidate)) {
        candidate = base + QString::number(suffix++);
    }
    return candidate;
}

QString ServiceMenuStore::commandForAction(const ServiceAction &action)
{
    QString command = desktopQuote(action.program.trimmed());
    if (!action.arguments.trimmed().isEmpty()) {
        command += QLatin1Char(' ') + action.arguments.trimmed();
    }
    if (action.runInTerminal) {
        command = QStringLiteral("konsole -e ") + command;
    }
    return command;
}

ValidationResult ServiceMenuStore::validate(const ServiceMenu &menu) const
{
    ValidationResult result;
    const QString mimeTypes = normalizedMimeTypes(menu.mimeTypes);
    if (mimeTypes.isEmpty()) {
        result.errors.append(QStringLiteral("Choose at least one file type."));
    } else {
        const QRegularExpression mimePattern(
            QStringLiteral("^[A-Za-z0-9.+-]+/(?:[A-Za-z0-9.+_-]+|\\*)$"));
        const QStringList types = mimeTypes.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &type : types) {
            if (!mimePattern.match(type).hasMatch()
                && type != QStringLiteral("all/allfiles")
                && type != QStringLiteral("all/all")) {
                result.errors.append(QStringLiteral("Invalid MIME type: %1").arg(type));
            }
        }
    }

    if (menu.actions.isEmpty()) {
        result.errors.append(QStringLiteral("Add at least one action."));
    }

    QStringList ids;
    for (const ServiceAction &action : menu.actions) {
        if (action.name.trimmed().isEmpty()) {
            result.errors.append(QStringLiteral("Every action needs a visible name."));
        }
        if (action.id.isEmpty() || ids.contains(action.id)) {
            result.errors.append(QStringLiteral("Action identifiers must be unique."));
        }
        ids.append(action.id);

        const QString program = action.program.trimmed();
        if (program.isEmpty()) {
            result.errors.append(QStringLiteral("%1 needs a program or script.")
                                     .arg(action.name.isEmpty() ? QStringLiteral("The action")
                                                                : action.name));
        } else if (QDir::isAbsolutePath(program)) {
            const QFileInfo info(program);
            if (!info.exists()) {
                result.errors.append(QStringLiteral("Program does not exist: %1").arg(program));
            } else if (!info.isExecutable()) {
                result.errors.append(QStringLiteral("Program is not executable: %1").arg(program));
            }
        } else if (program.contains(QLatin1Char('/'))) {
            result.warnings.append(
                QStringLiteral("Use an absolute program path for predictable results: %1").arg(program));
        } else if (QStandardPaths::findExecutable(program).isEmpty()) {
            result.errors.append(QStringLiteral("Program was not found in PATH: %1").arg(program));
        }

        const QRegularExpression fileCodePattern(QStringLiteral("%[fFuU]"));
        int fileCodeCount = 0;
        auto fileCodeMatches = fileCodePattern.globalMatch(action.arguments);
        while (fileCodeMatches.hasNext()) {
            fileCodeMatches.next();
            ++fileCodeCount;
        }
        if (fileCodeCount == 0) {
            result.warnings.append(QStringLiteral("%1 does not pass the selected file to its command.")
                                       .arg(action.name.isEmpty() ? QStringLiteral("The action")
                                                                  : action.name));
        } else if (fileCodeCount > 1) {
            result.errors.append(QStringLiteral(
                "%1 contains more than one file-selection placeholder.")
                                     .arg(action.name.isEmpty() ? QStringLiteral("The action")
                                                                : action.name));
        }

        const QString allowedCodes = QStringLiteral("fFuUick%");
        const QString deprecatedCodes = QStringLiteral("dDnNvm");
        QSet<QString> reportedCodes;
        auto fieldCodes = QRegularExpression(QStringLiteral("%([A-Za-z%])"))
                              .globalMatch(action.arguments);
        while (fieldCodes.hasNext()) {
            const QString code = fieldCodes.next().captured(1);
            if (reportedCodes.contains(code) || allowedCodes.contains(code)) {
                continue;
            }
            reportedCodes.insert(code);
            if (deprecatedCodes.contains(code)) {
                result.warnings.append(QStringLiteral("%%%1 is deprecated and should be replaced.")
                                           .arg(code));
            } else {
                result.warnings.append(QStringLiteral("%%%1 is not a recognized argument placeholder.")
                                           .arg(code));
            }
        }
        if (action.runInTerminal && QStandardPaths::findExecutable(QStringLiteral("konsole")).isEmpty()) {
            result.errors.append(QStringLiteral("Konsole is required for terminal actions."));
        }
    }
    return result;
}

QString ServiceMenuStore::serialize(const ServiceMenu &menu) const
{
    IniDocument document = menu.document;
    const QString desktopEntry = QStringLiteral("Desktop Entry");
    document.setValue(desktopEntry, QStringLiteral("Type"), QStringLiteral("Service"));
    document.setValue(desktopEntry, QStringLiteral("MimeType"), normalizedMimeTypes(menu.mimeTypes));

    QStringList actionIds;
    for (const ServiceAction &action : menu.actions) {
        actionIds.append(action.id);
    }
    document.setValue(desktopEntry, QStringLiteral("Actions"),
                      actionIds.join(QLatin1Char(';')) + QLatin1Char(';'));
    document.setValue(desktopEntry, QStringLiteral("X-DolphinContextMenuManager-Version"),
                      QStringLiteral("1"));
    document.removeKey(desktopEntry, QStringLiteral("X-DolphinContextManager-Version"));
    if (menu.extensions.isEmpty()) {
        document.removeKey(desktopEntry, QStringLiteral("X-DolphinContextMenuManager-Extensions"));
    } else {
        document.setValue(desktopEntry, QStringLiteral("X-DolphinContextMenuManager-Extensions"),
                          menu.extensions.join(QLatin1Char(';')) + QLatin1Char(';'));
    }
    document.removeKey(desktopEntry, QStringLiteral("X-DolphinContextManager-Extensions"));

    if (menu.submenu.trimmed().isEmpty()) {
        document.removeKey(desktopEntry, QStringLiteral("X-KDE-Submenu"));
    } else {
        document.setValue(desktopEntry, QStringLiteral("X-KDE-Submenu"), menu.submenu.trimmed());
    }
    if (menu.topLevel) {
        document.setValue(desktopEntry, QStringLiteral("X-KDE-Priority"), QStringLiteral("TopLevel"));
    } else {
        document.removeKey(desktopEntry, QStringLiteral("X-KDE-Priority"));
    }

    const bool acceptsMultiple = std::any_of(
        menu.actions.cbegin(), menu.actions.cend(), [](const ServiceAction &action) {
            return action.arguments.contains(QStringLiteral("%F"))
                || action.arguments.contains(QStringLiteral("%U"));
        });
    if (acceptsMultiple) {
        document.removeKey(desktopEntry, QStringLiteral("X-KDE-MaxNumberOfUrls"));
    } else {
        document.setValue(desktopEntry, QStringLiteral("X-KDE-MaxNumberOfUrls"), QStringLiteral("1"));
    }

    const QStringList existingSections = document.sectionNames();
    for (const QString &section : existingSections) {
        if (section.startsWith(QStringLiteral("Desktop Action "))) {
            const QString id = section.mid(QStringLiteral("Desktop Action ").size());
            if (!actionIds.contains(id)) {
                document.removeSection(section);
            }
        }
    }

    for (const ServiceAction &action : menu.actions) {
        const QString section = QStringLiteral("Desktop Action ") + action.id;
        document.setValue(section, QStringLiteral("Name"), action.name.trimmed());
        if (action.icon.trimmed().isEmpty()) {
            document.removeKey(section, QStringLiteral("Icon"));
        } else {
            document.setValue(section, QStringLiteral("Icon"), action.icon.trimmed());
        }
        document.setValue(section, QStringLiteral("Exec"), commandForAction(action));
        document.setValue(section, QStringLiteral("X-DolphinContextMenuManager-Program"),
                          action.program.trimmed());
        document.setValue(section, QStringLiteral("X-DolphinContextMenuManager-Arguments"),
                          action.arguments.trimmed());
        document.setValue(section, QStringLiteral("X-DolphinContextMenuManager-Terminal"),
                          action.runInTerminal ? QStringLiteral("true") : QStringLiteral("false"));
        document.removeKey(section, QStringLiteral("X-DolphinContextManager-Program"));
        document.removeKey(section, QStringLiteral("X-DolphinContextManager-Arguments"));
        document.removeKey(section, QStringLiteral("X-DolphinContextManager-Terminal"));
    }
    return document.serialize();
}

QString ServiceMenuStore::uniquePathFor(const ServiceMenu &menu) const
{
    QString base = menu.displayName().toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    base = base.trimmed();
    while (base.startsWith(QLatin1Char('-'))) {
        base.removeFirst();
    }
    while (base.endsWith(QLatin1Char('-'))) {
        base.chop(1);
    }
    if (base.isEmpty()) {
        base = QStringLiteral("context-action");
    }
    base.prepend(QStringLiteral("dcm-"));

    QString path = m_userDirectory + QLatin1Char('/') + base + QStringLiteral(".desktop");
    int suffix = 2;
    while (QFileInfo::exists(path) || QFileInfo::exists(path + QStringLiteral(".disabled"))) {
        path = m_userDirectory + QLatin1Char('/') + base + QLatin1Char('-')
            + QString::number(suffix++) + QStringLiteral(".desktop");
    }
    return path;
}

bool ServiceMenuStore::save(ServiceMenu *menu, QString *errorMessage) const
{
    if (!menu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No service menu was supplied.");
        }
        return false;
    }
    if (!menu->path.isEmpty()
        && (!menu->userOwned || !isInsideDirectory(menu->path, m_userDirectory))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("System service menus are read-only. Duplicate it first.");
        }
        return false;
    }

    const ValidationResult validation = validate(*menu);
    if (!validation.isValid()) {
        if (errorMessage) {
            *errorMessage = validation.errors.join(QLatin1Char('\n'));
        }
        return false;
    }
    if (!QDir().mkpath(m_userDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not create %1").arg(m_userDirectory);
        }
        return false;
    }

    const QString oldPath = menu->path;
    QString targetPath = oldPath.isEmpty() ? uniquePathFor(*menu) : oldPath;
    if (menu->enabled && targetPath.endsWith(QStringLiteral(".disabled"))) {
        targetPath.chop(QStringLiteral(".disabled").size());
    } else if (!menu->enabled && !targetPath.endsWith(QStringLiteral(".disabled"))) {
        targetPath += QStringLiteral(".disabled");
    }

    if (targetPath != oldPath && QFileInfo::exists(targetPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("A service menu already exists at %1").arg(targetPath);
        }
        return false;
    }

    QSaveFile output(targetPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) {
            *errorMessage = output.errorString();
        }
        return false;
    }
    const QByteArray contents = serialize(*menu).toUtf8();
    if (output.write(contents) != contents.size() || !output.commit()) {
        if (errorMessage) {
            *errorMessage = output.errorString();
        }
        return false;
    }

    const QFile::Permissions permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner
        | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::ExeOther;
    if (!QFile::setPermissions(targetPath, permissions)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Saved the file but could not mark it executable.");
        }
        return false;
    }

    if (!oldPath.isEmpty() && oldPath != targetPath && QFileInfo::exists(oldPath)
        && !QFile::remove(oldPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Saved the new file but could not remove %1").arg(oldPath);
        }
        return false;
    }

    menu->path = targetPath;
    menu->userOwned = true;
    menu->document = IniDocument();
    menu->document.load(targetPath);
    refreshKdeCache();
    return true;
}

bool ServiceMenuStore::moveToTrash(const ServiceMenu &menu, QString *errorMessage) const
{
    if (menu.path.isEmpty() || !menu.userOwned || !isInsideDirectory(menu.path, m_userDirectory)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Only personal service menus can be removed.");
        }
        return false;
    }
    QString trashedPath;
    if (!QFile::moveToTrash(menu.path, &trashedPath)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Could not move %1 to Trash.").arg(menu.path);
        }
        return false;
    }
    refreshKdeCache();
    return true;
}

void ServiceMenuStore::refreshKdeCache() const
{
    if (!m_refreshEnabled) {
        return;
    }
    const QString executable = QStandardPaths::findExecutable(QStringLiteral("kbuildsycoca6"));
    if (!executable.isEmpty()) {
        QProcess::startDetached(executable, {});
    }
}
