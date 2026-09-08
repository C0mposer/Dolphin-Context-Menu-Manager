#pragma once

#include "servicemenu.h"

#include <QMainWindow>

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTableWidget;
class QWidget;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void buildUi();
    void connectUi();
    void reloadMenus(const QString &selectPath = {});
    void populateMenuList(const QString &selectPath = {});
    void applyListFilter();
    void selectListIndex(int index);

    bool confirmLeaveCurrent();
    bool saveCurrent();
    void loadCurrentIntoEditor();
    void clearEditor();
    void syncCurrentFromEditor();
    void syncActiveActionFromEditor();
    void loadActionIntoEditor(int index);
    void rebuildActionList(int selectedIndex);
    void editorChanged();
    void updatePreviewAndValidation();
    void updateIconPreview();
    void rebuildExtensionTable();
    void syncFileTypesFromTable();

    void newMenu();
    void duplicateMenu();
    void deleteMenu();
    void menuSelectionChanged();
    void actionSelectionChanged(int row);
    void addAction();
    void removeAction();
    void chooseMimeTypes();
    void detectMimeType();
    void addExtensions();
    void removeExtensions();
    void chooseProgram();
    void chooseIcon();
    void mimePresetChanged(int index);
    void iconPresetChanged(int index);
    void selectionModeChanged(int index);
    void insertArgumentPlaceholder(const QString &placeholder);

    static QString mimeSummary(const QString &mimeTypes);
    static int selectionModeForArguments(const QString &arguments);
    static QString placeholderForSelectionMode(int index);

    ServiceMenuStore m_store;
    QList<ServiceMenu> m_menus;
    ServiceMenu m_current;
    bool m_hasCurrent = false;
    bool m_dirty = false;
    bool m_loading = false;
    int m_selectedMenuIndex = -1;
    int m_activeActionIndex = -1;

    QWidget *m_editorPage = nullptr;
    QWidget *m_editorBody = nullptr;
    QLabel *m_editorTitle = nullptr;
    QLabel *m_sourceLabel = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_scopeCombo = nullptr;
    QTreeWidget *m_menuTree = nullptr;
    QPushButton *m_newButton = nullptr;
    QPushButton *m_duplicateButton = nullptr;
    QPushButton *m_deleteButton = nullptr;
    QToolButton *m_refreshButton = nullptr;

    QCheckBox *m_enabledCheck = nullptr;
    QComboBox *m_mimePresetCombo = nullptr;
    QPushButton *m_chooseMimeButton = nullptr;
    QWidget *m_extensionPanel = nullptr;
    QTableWidget *m_extensionTable = nullptr;
    QLineEdit *m_extensionEdit = nullptr;
    QPushButton *m_addExtensionButton = nullptr;
    QPushButton *m_removeExtensionButton = nullptr;
    QPushButton *m_sampleFileButton = nullptr;
    QLineEdit *m_submenuEdit = nullptr;
    QCheckBox *m_topLevelCheck = nullptr;

    QListWidget *m_actionList = nullptr;
    QToolButton *m_addActionButton = nullptr;
    QToolButton *m_removeActionButton = nullptr;
    QLineEdit *m_actionNameEdit = nullptr;
    QComboBox *m_iconCombo = nullptr;
    QLineEdit *m_iconEdit = nullptr;
    QToolButton *m_iconPreview = nullptr;
    QPushButton *m_chooseIconButton = nullptr;
    QLineEdit *m_programEdit = nullptr;
    QPushButton *m_chooseProgramButton = nullptr;
    QLineEdit *m_argumentsEdit = nullptr;
    QComboBox *m_selectionCombo = nullptr;
    QCheckBox *m_terminalCheck = nullptr;

    QPlainTextEdit *m_previewEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_saveButton = nullptr;
};
