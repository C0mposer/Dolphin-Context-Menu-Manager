#pragma once

#include <QList>
#include <QString>
#include <QStringList>

struct IniEntry {
    QString key;
    QString value;
};

struct IniSection {
    QString name;
    QList<IniEntry> entries;
};

class IniDocument {
public:
    bool load(const QString &path, QString *errorMessage = nullptr);
    QString value(const QString &section, const QString &key,
                  const QString &fallback = {}) const;
    void setValue(const QString &section, const QString &key, const QString &value);
    void removeKey(const QString &section, const QString &key);
    void removeSection(const QString &section);
    QStringList sectionNames() const;
    QString serialize() const;

private:
    IniSection *findSection(const QString &name);
    const IniSection *findSection(const QString &name) const;

    QList<IniSection> m_sections;
};

struct ServiceAction {
    QString id;
    QString name;
    QString icon = QStringLiteral("application-x-executable");
    QString program;
    QString arguments = QStringLiteral("%f");
    bool runInTerminal = false;
};

struct ServiceMenu {
    QString path;
    bool userOwned = true;
    bool enabled = true;
    bool topLevel = true;
    QString mimeTypes = QStringLiteral("application/octet-stream;");
    QStringList extensions;
    QString submenu;
    QList<ServiceAction> actions;
    IniDocument document;
    QString loadError;

    QString displayName() const;
};

struct ValidationResult {
    QStringList errors;
    QStringList warnings;

    bool isValid() const { return errors.isEmpty(); }
};

class ServiceMenuStore {
public:
    explicit ServiceMenuStore(QString userDirectory = {});

    QString userDirectory() const;
    QList<ServiceMenu> loadMenus() const;
    ServiceMenu loadMenu(const QString &path, bool userOwned) const;
    ServiceMenu createBlank() const;
    ValidationResult validate(const ServiceMenu &menu) const;
    QString serialize(const ServiceMenu &menu) const;

    bool save(ServiceMenu *menu, QString *errorMessage = nullptr) const;
    bool moveToTrash(const ServiceMenu &menu, QString *errorMessage = nullptr) const;
    void refreshKdeCache() const;

    static QString normalizedMimeTypes(const QString &mimeTypes);
    static QString makeActionId(const QString &name, const QStringList &existingIds = {});
    static QString commandForAction(const ServiceAction &action);

private:
    QString uniquePathFor(const ServiceMenu &menu) const;
    QStringList dataLocations() const;

    QString m_userDirectory;
    bool m_refreshEnabled = true;
};
