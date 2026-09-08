#include "mainwindow.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QMimeType>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSet>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace {

QLabel *makeMutedLabel(const QString &text, QWidget *parent = nullptr)
{
    auto *label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setStyleSheet(QStringLiteral("color: palette(mid);"));
    return label;
}

QStringList splitMimeTypes(const QString &text)
{
    return ServiceMenuStore::normalizedMimeTypes(text)
        .split(QLatin1Char(';'), Qt::SkipEmptyParts);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    connectUi();
    reloadMenus();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Dolphin Context Menu Manager"));
    resize(1120, 760);
    setMinimumSize(880, 620);

    auto *central = new QWidget(this);
    auto *rootLayout = new QVBoxLayout(central);
    rootLayout->setContentsMargins(18, 16, 18, 16);
    rootLayout->setSpacing(12);

    auto *appTitle = new QLabel(QStringLiteral("Dolphin Context Menu Manager"), central);
    QFont titleFont = appTitle->font();
    titleFont.setPointSize(titleFont.pointSize() + 6);
    titleFont.setBold(true);
    appTitle->setFont(titleFont);
    rootLayout->addWidget(appTitle);
    rootLayout->addWidget(makeMutedLabel(
        QStringLiteral("Create useful right-click actions without editing .desktop files."), central));

    auto *splitter = new QSplitter(Qt::Horizontal, central);
    splitter->setChildrenCollapsible(false);
    rootLayout->addWidget(splitter, 1);

    auto *sidebar = new QWidget(splitter);
    sidebar->setMinimumWidth(270);
    sidebar->setMaximumWidth(390);
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 8, 0);
    sidebarLayout->setSpacing(8);

    auto *listHeaderLayout = new QHBoxLayout;
    auto *listTitle = new QLabel(QStringLiteral("Context actions"), sidebar);
    QFont sectionFont = listTitle->font();
    sectionFont.setBold(true);
    listTitle->setFont(sectionFont);
    listHeaderLayout->addWidget(listTitle);
    listHeaderLayout->addStretch();
    m_refreshButton = new QToolButton(sidebar);
    m_refreshButton->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    m_refreshButton->setToolTip(QStringLiteral("Reload service menus"));
    m_refreshButton->setAutoRaise(true);
    listHeaderLayout->addWidget(m_refreshButton);
    sidebarLayout->addLayout(listHeaderLayout);

    m_searchEdit = new QLineEdit(sidebar);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search actions or file types…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(
        QIcon::fromTheme(QStringLiteral("search")), QLineEdit::LeadingPosition);
    sidebarLayout->addWidget(m_searchEdit);

    m_scopeCombo = new QComboBox(sidebar);
    m_scopeCombo->addItem(QStringLiteral("All actions"), 0);
    m_scopeCombo->addItem(QStringLiteral("My actions"), 1);
    m_scopeCombo->addItem(QStringLiteral("System actions"), 2);
    sidebarLayout->addWidget(m_scopeCombo);

    m_menuTree = new QTreeWidget(sidebar);
    m_menuTree->setColumnCount(2);
    m_menuTree->setHeaderLabels({QStringLiteral("Action"), QStringLiteral("Source")});
    m_menuTree->header()->setStretchLastSection(false);
    m_menuTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_menuTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_menuTree->setRootIsDecorated(false);
    m_menuTree->setAlternatingRowColors(true);
    m_menuTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_menuTree->setUniformRowHeights(true);
    sidebarLayout->addWidget(m_menuTree, 1);

    auto *sidebarButtons = new QHBoxLayout;
    m_newButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
                                  QStringLiteral("New"), sidebar);
    m_duplicateButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-copy")),
                                        QStringLiteral("Duplicate"), sidebar);
    m_deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("user-trash")),
                                     QStringLiteral("Trash"), sidebar);
    sidebarButtons->addWidget(m_newButton);
    sidebarButtons->addWidget(m_duplicateButton);
    sidebarButtons->addWidget(m_deleteButton);
    sidebarLayout->addLayout(sidebarButtons);
    splitter->addWidget(sidebar);

    m_editorPage = new QWidget(splitter);
    auto *editorPageLayout = new QVBoxLayout(m_editorPage);
    editorPageLayout->setContentsMargins(12, 0, 0, 0);
    editorPageLayout->setSpacing(10);

    m_editorTitle = new QLabel(QStringLiteral("Select an action"), m_editorPage);
    m_editorTitle->setFont(titleFont);
    editorPageLayout->addWidget(m_editorTitle);
    m_sourceLabel = makeMutedLabel(QString(), m_editorPage);
    m_sourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    editorPageLayout->addWidget(m_sourceLabel);

    auto *scrollArea = new QScrollArea(m_editorPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    m_editorBody = new QWidget(scrollArea);
    auto *editorLayout = new QVBoxLayout(m_editorBody);
    editorLayout->setContentsMargins(0, 4, 8, 4);
    editorLayout->setSpacing(12);

    auto *whereGroup = new QGroupBox(QStringLiteral("Where it appears"), m_editorBody);
    auto *whereForm = new QFormLayout(whereGroup);
    whereForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    whereForm->setRowWrapPolicy(QFormLayout::WrapLongRows);

    m_enabledCheck = new QCheckBox(QStringLiteral("Show this action in Dolphin"), whereGroup);
    whereForm->addRow(QStringLiteral("Enabled:"), m_enabledCheck);

    auto *typePresetRow = new QWidget(whereGroup);
    auto *typePresetLayout = new QHBoxLayout(typePresetRow);
    typePresetLayout->setContentsMargins(0, 0, 0, 0);
    m_mimePresetCombo = new QComboBox(typePresetRow);
    m_mimePresetCombo->addItem(QStringLiteral("Specific extensions"), QString());
    m_mimePresetCombo->addItem(QStringLiteral("All files"),
                               QStringLiteral("application/octet-stream;"));
    m_mimePresetCombo->addItem(QStringLiteral("Folders"), QStringLiteral("inode/directory;"));
    m_mimePresetCombo->addItem(QStringLiteral("All images"), QStringLiteral("image/*;"));
    m_mimePresetCombo->addItem(QStringLiteral("All videos"), QStringLiteral("video/*;"));
    m_mimePresetCombo->addItem(QStringLiteral("All audio"), QStringLiteral("audio/*;"));
    m_mimePresetCombo->addItem(QStringLiteral("Text files"), QStringLiteral("text/*;"));
    typePresetLayout->addWidget(m_mimePresetCombo, 1);
    m_chooseMimeButton = new QPushButton(QStringLiteral("Advanced MIME…"), typePresetRow);
    m_chooseMimeButton->setToolTip(
        QStringLiteral("Choose MIME types directly when an extension is not enough"));
    typePresetLayout->addWidget(m_chooseMimeButton);
    whereForm->addRow(QStringLiteral("File types:"), typePresetRow);

    m_extensionPanel = new QWidget(whereGroup);
    auto *extensionLayout = new QVBoxLayout(m_extensionPanel);
    extensionLayout->setContentsMargins(0, 0, 0, 0);
    extensionLayout->setSpacing(6);
    m_extensionTable = new QTableWidget(0, 2, m_extensionPanel);
    m_extensionTable->setHorizontalHeaderLabels(
        {QStringLiteral("Extension"), QStringLiteral("Detected KDE file type")});
    m_extensionTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_extensionTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_extensionTable->verticalHeader()->hide();
    m_extensionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_extensionTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_extensionTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_extensionTable->setAlternatingRowColors(true);
    m_extensionTable->setMaximumHeight(125);
    extensionLayout->addWidget(m_extensionTable);

    auto *extensionInputRow = new QHBoxLayout;
    m_extensionEdit = new QLineEdit(m_extensionPanel);
    m_extensionEdit->setPlaceholderText(QStringLiteral(".mp4, .mkv, .webm"));
    m_extensionEdit->setToolTip(QStringLiteral("Enter one or several filename extensions"));
    extensionInputRow->addWidget(m_extensionEdit, 1);
    m_addExtensionButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")),
                                           QStringLiteral("Add"), m_extensionPanel);
    extensionInputRow->addWidget(m_addExtensionButton);
    m_sampleFileButton = new QPushButton(QStringLiteral("From sample…"), m_extensionPanel);
    m_sampleFileButton->setToolTip(QStringLiteral("Detect the type of an example file"));
    extensionInputRow->addWidget(m_sampleFileButton);
    m_removeExtensionButton = new QPushButton(QIcon::fromTheme(QStringLiteral("list-remove")),
                                              QStringLiteral("Remove"), m_extensionPanel);
    extensionInputRow->addWidget(m_removeExtensionButton);
    extensionLayout->addLayout(extensionInputRow);
    extensionLayout->addWidget(makeMutedLabel(
        QStringLiteral("Type familiar extensions; KDE's MIME name is filled in automatically."),
        m_extensionPanel));
    whereForm->addRow(QStringLiteral("Extensions:"), m_extensionPanel);

    m_submenuEdit = new QLineEdit(whereGroup);
    m_submenuEdit->setPlaceholderText(QStringLiteral("Optional, for example “Media tools”"));
    whereForm->addRow(QStringLiteral("Submenu:"), m_submenuEdit);
    m_topLevelCheck = new QCheckBox(
        QStringLiteral("Prefer the main context menu instead of the Actions submenu"), whereGroup);
    whereForm->addRow(QStringLiteral("Placement:"), m_topLevelCheck);
    editorLayout->addWidget(whereGroup);

    auto *actionGroup = new QGroupBox(QStringLiteral("What it does"), m_editorBody);
    auto *actionLayout = new QVBoxLayout(actionGroup);
    actionLayout->addWidget(makeMutedLabel(
        QStringLiteral("A menu can contain one action or a small group of related actions."), actionGroup));

    auto *actionListRow = new QHBoxLayout;
    m_actionList = new QListWidget(actionGroup);
    m_actionList->setMaximumHeight(105);
    m_actionList->setAlternatingRowColors(true);
    actionListRow->addWidget(m_actionList, 1);
    auto *actionListButtons = new QVBoxLayout;
    m_addActionButton = new QToolButton(actionGroup);
    m_addActionButton->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    m_addActionButton->setToolTip(QStringLiteral("Add another action"));
    actionListButtons->addWidget(m_addActionButton);
    m_removeActionButton = new QToolButton(actionGroup);
    m_removeActionButton->setIcon(QIcon::fromTheme(QStringLiteral("list-remove")));
    m_removeActionButton->setToolTip(QStringLiteral("Remove this action"));
    actionListButtons->addWidget(m_removeActionButton);
    actionListButtons->addStretch();
    actionListRow->addLayout(actionListButtons);
    actionLayout->addLayout(actionListRow);

    auto *actionForm = new QFormLayout;
    actionForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    actionForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    m_actionNameEdit = new QLineEdit(actionGroup);
    m_actionNameEdit->setPlaceholderText(QStringLiteral("What users see in Dolphin"));
    actionForm->addRow(QStringLiteral("Name:"), m_actionNameEdit);

    auto *iconRow = new QWidget(actionGroup);
    auto *iconLayout = new QHBoxLayout(iconRow);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    m_iconPreview = new QToolButton(iconRow);
    m_iconPreview->setAutoRaise(true);
    m_iconPreview->setIconSize(QSize(24, 24));
    m_iconPreview->setEnabled(false);
    iconLayout->addWidget(m_iconPreview);
    m_iconCombo = new QComboBox(iconRow);
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("application-x-executable")),
                         QStringLiteral("Application"), QStringLiteral("application-x-executable"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("utilities-terminal")),
                         QStringLiteral("Terminal"), QStringLiteral("utilities-terminal"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("configure")),
                         QStringLiteral("Settings"), QStringLiteral("configure"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("text-x-generic")),
                         QStringLiteral("Document"), QStringLiteral("text-x-generic"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("folder")),
                         QStringLiteral("Folder"), QStringLiteral("folder"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("image-x-generic")),
                         QStringLiteral("Image"), QStringLiteral("image-x-generic"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("video-x-generic")),
                         QStringLiteral("Video"), QStringLiteral("video-x-generic"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("audio-x-generic")),
                         QStringLiteral("Audio"), QStringLiteral("audio-x-generic"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("package-x-generic")),
                         QStringLiteral("Archive"), QStringLiteral("package-x-generic"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("network-workgroup")),
                         QStringLiteral("Network / Share"), QStringLiteral("network-workgroup"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("security-high")),
                         QStringLiteral("Security"), QStringLiteral("security-high"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("tools-wizard")),
                         QStringLiteral("Tools"), QStringLiteral("tools-wizard"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("document-open")),
                         QStringLiteral("Open"), QStringLiteral("document-open"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("document-save")),
                         QStringLiteral("Save / Export"), QStringLiteral("document-save"));
    m_iconCombo->addItem(QIcon::fromTheme(QStringLiteral("preferences-desktop-icons")),
                         QStringLiteral("Custom…"), QStringLiteral("__custom__"));
    iconLayout->addWidget(m_iconCombo);
    m_iconEdit = new QLineEdit(iconRow);
    m_iconEdit->setPlaceholderText(QStringLiteral("Theme icon name or image path"));
    iconLayout->addWidget(m_iconEdit, 1);
    m_chooseIconButton = new QPushButton(QStringLiteral("Browse…"), iconRow);
    iconLayout->addWidget(m_chooseIconButton);
    actionForm->addRow(QStringLiteral("Icon:"), iconRow);

    auto *programRow = new QWidget(actionGroup);
    auto *programLayout = new QHBoxLayout(programRow);
    programLayout->setContentsMargins(0, 0, 0, 0);
    m_programEdit = new QLineEdit(programRow);
    m_programEdit->setPlaceholderText(QStringLiteral("Program name or /absolute/path/to/script"));
    programLayout->addWidget(m_programEdit, 1);
    m_chooseProgramButton = new QPushButton(QStringLiteral("Browse…"), programRow);
    programLayout->addWidget(m_chooseProgramButton);
    actionForm->addRow(QStringLiteral("Program:"), programRow);

    auto *argumentsRow = new QWidget(actionGroup);
    auto *argumentsLayout = new QHBoxLayout(argumentsRow);
    argumentsLayout->setContentsMargins(0, 0, 0, 0);
    m_argumentsEdit = new QLineEdit(argumentsRow);
    m_argumentsEdit->setPlaceholderText(QStringLiteral("Arguments passed to the program"));
    argumentsLayout->addWidget(m_argumentsEdit, 1);
    m_selectionCombo = new QComboBox(argumentsRow);
    m_selectionCombo->addItem(QStringLiteral("One local file"), QStringLiteral("%f"));
    m_selectionCombo->addItem(QStringLiteral("Multiple local files"), QStringLiteral("%F"));
    m_selectionCombo->addItem(QStringLiteral("One file or URL"), QStringLiteral("%u"));
    m_selectionCombo->addItem(QStringLiteral("Multiple files or URLs"), QStringLiteral("%U"));
    argumentsLayout->addWidget(m_selectionCombo);
    actionForm->addRow(QStringLiteral("Arguments:"), argumentsRow);
    auto *placeholderPanel = new QWidget(actionGroup);
    auto *placeholderLayout = new QGridLayout(placeholderPanel);
    placeholderLayout->setContentsMargins(0, 2, 0, 0);
    placeholderLayout->setHorizontalSpacing(6);
    placeholderLayout->setVerticalSpacing(4);
    const QList<QPair<QString, QString>> placeholders{
        {QStringLiteral("%f"), QStringLiteral("One local file")},
        {QStringLiteral("%F"), QStringLiteral("Multiple local files")},
        {QStringLiteral("%u"), QStringLiteral("One file or URL")},
        {QStringLiteral("%U"), QStringLiteral("Multiple files or URLs")},
        {QStringLiteral("%i"), QStringLiteral("Menu icon")},
        {QStringLiteral("%c"), QStringLiteral("Action name")},
        {QStringLiteral("%k"), QStringLiteral("Service-menu file")},
        {QStringLiteral("%%"), QStringLiteral("Literal percent sign")},
    };
    for (int index = 0; index < placeholders.size(); ++index) {
        const QString code = placeholders.at(index).first;
        auto *button = new QPushButton(QStringLiteral("%1  %2").arg(code, placeholders.at(index).second),
                                       placeholderPanel);
        button->setToolTip(QStringLiteral("Insert %1 into the argument list").arg(code));
        connect(button, &QPushButton::clicked, this,
                [this, code] { insertArgumentPlaceholder(code); });
        placeholderLayout->addWidget(button, index / 2, index % 2);
    }
    actionForm->addRow(QStringLiteral("Placeholders:"), placeholderPanel);
    actionForm->addRow(QString(), makeMutedLabel(
        QStringLiteral("Click to insert. Legacy %d, %D, %n, %N, %v, and %m codes are deprecated."),
        actionGroup));

    m_terminalCheck = new QCheckBox(QStringLiteral("Open the command in Konsole"), actionGroup);
    actionForm->addRow(QStringLiteral("Terminal:"), m_terminalCheck);
    actionLayout->addLayout(actionForm);
    editorLayout->addWidget(actionGroup);

    auto *previewGroup = new QGroupBox(QStringLiteral("Generated service menu"), m_editorBody);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_previewEdit = new QPlainTextEdit(previewGroup);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(180);
    m_previewEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_previewEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    previewLayout->addWidget(m_previewEdit);
    editorLayout->addWidget(previewGroup);
    editorLayout->addStretch();

    scrollArea->setWidget(m_editorBody);
    editorPageLayout->addWidget(scrollArea, 1);

    auto *footerLayout = new QHBoxLayout;
    m_statusLabel = new QLabel(m_editorPage);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setMinimumHeight(38);
    m_statusLabel->setContentsMargins(10, 6, 10, 6);
    footerLayout->addWidget(m_statusLabel, 1);
    m_saveButton = new QPushButton(QIcon::fromTheme(QStringLiteral("document-save")),
                                   QStringLiteral("Save action"), m_editorPage);
    m_saveButton->setDefault(true);
    footerLayout->addWidget(m_saveButton);
    editorPageLayout->addLayout(footerLayout);
    splitter->addWidget(m_editorPage);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({320, 800});

    setCentralWidget(central);
}

void MainWindow::connectUi()
{
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this] { applyListFilter(); });
    connect(m_scopeCombo, &QComboBox::currentIndexChanged, this, [this] { applyListFilter(); });
    connect(m_menuTree, &QTreeWidget::itemSelectionChanged, this,
            [this] { menuSelectionChanged(); });
    connect(m_refreshButton, &QToolButton::clicked, this, [this] {
        if (confirmLeaveCurrent()) {
            reloadMenus(m_hasCurrent ? m_current.path : QString());
        }
    });
    connect(m_newButton, &QPushButton::clicked, this, [this] { newMenu(); });
    connect(m_duplicateButton, &QPushButton::clicked, this, [this] { duplicateMenu(); });
    connect(m_deleteButton, &QPushButton::clicked, this, [this] { deleteMenu(); });
    connect(m_saveButton, &QPushButton::clicked, this, [this] {
        if (saveCurrent()) {
            reloadMenus(m_current.path);
        }
    });

    connect(m_enabledCheck, &QCheckBox::toggled, this, [this] { editorChanged(); });
    connect(m_mimePresetCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { mimePresetChanged(index); });
    connect(m_chooseMimeButton, &QPushButton::clicked, this, [this] { chooseMimeTypes(); });
    connect(m_sampleFileButton, &QPushButton::clicked, this, [this] { detectMimeType(); });
    connect(m_addExtensionButton, &QPushButton::clicked, this, [this] { addExtensions(); });
    connect(m_extensionEdit, &QLineEdit::returnPressed, this, [this] { addExtensions(); });
    connect(m_removeExtensionButton, &QPushButton::clicked, this, [this] { removeExtensions(); });
    connect(m_extensionTable, &QTableWidget::itemSelectionChanged, this, [this] {
        m_removeExtensionButton->setEnabled(!m_extensionTable->selectedItems().isEmpty());
    });
    connect(m_submenuEdit, &QLineEdit::textChanged, this, [this] { editorChanged(); });
    connect(m_topLevelCheck, &QCheckBox::toggled, this, [this] { editorChanged(); });

    connect(m_actionList, &QListWidget::currentRowChanged, this,
            [this](int row) { actionSelectionChanged(row); });
    connect(m_addActionButton, &QToolButton::clicked, this, [this] { addAction(); });
    connect(m_removeActionButton, &QToolButton::clicked, this, [this] { removeAction(); });
    connect(m_actionNameEdit, &QLineEdit::textChanged, this, [this] {
        editorChanged();
        if (!m_loading && m_activeActionIndex >= 0
            && m_activeActionIndex < m_actionList->count()) {
            m_actionList->item(m_activeActionIndex)->setText(
                m_actionNameEdit->text().trimmed().isEmpty()
                    ? QStringLiteral("Unnamed action")
                    : m_actionNameEdit->text().trimmed());
        }
    });
    connect(m_iconEdit, &QLineEdit::textChanged, this, [this] {
        updateIconPreview();
        editorChanged();
    });
    connect(m_iconCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { iconPresetChanged(index); });
    connect(m_chooseIconButton, &QPushButton::clicked, this, [this] { chooseIcon(); });
    connect(m_programEdit, &QLineEdit::textChanged, this, [this] { editorChanged(); });
    connect(m_chooseProgramButton, &QPushButton::clicked, this, [this] { chooseProgram(); });
    connect(m_argumentsEdit, &QLineEdit::textChanged, this, [this] {
        if (!m_loading) {
            const QSignalBlocker blocker(m_selectionCombo);
            m_selectionCombo->setCurrentIndex(selectionModeForArguments(m_argumentsEdit->text()));
        }
        editorChanged();
    });
    connect(m_selectionCombo, &QComboBox::currentIndexChanged, this,
            [this](int index) { selectionModeChanged(index); });
    connect(m_terminalCheck, &QCheckBox::toggled, this, [this] { editorChanged(); });
}

void MainWindow::reloadMenus(const QString &selectPath)
{
    m_menus = m_store.loadMenus();
    m_dirty = false;
    populateMenuList(selectPath);
}

void MainWindow::populateMenuList(const QString &selectPath)
{
    m_loading = true;
    m_menuTree->clear();
    int targetIndex = -1;

    for (int index = 0; index < m_menus.size(); ++index) {
        const ServiceMenu &menu = m_menus.at(index);
        const QString source = menu.userOwned
            ? (menu.enabled ? QStringLiteral("Mine") : QStringLiteral("Mine · Off"))
            : QStringLiteral("System");
        auto *item = new QTreeWidgetItem({menu.displayName(), source});
        item->setData(0, Qt::UserRole, index);
        const QString iconName = !menu.actions.isEmpty() ? menu.actions.first().icon : QString();
        item->setIcon(0, iconName.isEmpty() ? QIcon::fromTheme(QStringLiteral("configure"))
                                            : QIcon::fromTheme(iconName, QIcon(iconName)));
        item->setToolTip(0, QStringLiteral("%1\n%2").arg(menu.path, menu.mimeTypes));
        item->setToolTip(1, menu.path);
        if (!menu.enabled) {
            QFont font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            item->setForeground(0, palette().brush(QPalette::Disabled, QPalette::Text));
        }
        m_menuTree->addTopLevelItem(item);
        if (!selectPath.isEmpty()
            && QDir::cleanPath(menu.path) == QDir::cleanPath(selectPath)) {
            targetIndex = index;
        }
    }
    m_loading = false;
    applyListFilter();

    if (targetIndex < 0) {
        for (int row = 0; row < m_menuTree->topLevelItemCount(); ++row) {
            auto *item = m_menuTree->topLevelItem(row);
            if (!item->isHidden()) {
                targetIndex = item->data(0, Qt::UserRole).toInt();
                break;
            }
        }
    }
    if (targetIndex >= 0) {
        selectListIndex(targetIndex);
        m_selectedMenuIndex = targetIndex;
        m_current = m_menus.at(targetIndex);
        m_hasCurrent = true;
        loadCurrentIntoEditor();
    } else {
        clearEditor();
    }
}

void MainWindow::applyListFilter()
{
    const QString search = m_searchEdit->text().trimmed();
    const int scope = m_scopeCombo->currentData().toInt();
    for (int row = 0; row < m_menuTree->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_menuTree->topLevelItem(row);
        const int index = item->data(0, Qt::UserRole).toInt();
        const ServiceMenu &menu = m_menus.at(index);
        const bool scopeMatches = scope == 0 || (scope == 1 && menu.userOwned)
            || (scope == 2 && !menu.userOwned);
        const QString haystack = menu.displayName() + QLatin1Char(' ') + menu.mimeTypes
            + QLatin1Char(' ') + menu.extensions.join(QLatin1Char(' '))
            + QLatin1Char(' ') + menu.path;
        item->setHidden(!scopeMatches || !haystack.contains(search, Qt::CaseInsensitive));
    }
}

void MainWindow::selectListIndex(int index)
{
    const QSignalBlocker blocker(m_menuTree);
    for (int row = 0; row < m_menuTree->topLevelItemCount(); ++row) {
        QTreeWidgetItem *item = m_menuTree->topLevelItem(row);
        if (item->data(0, Qt::UserRole).toInt() == index) {
            m_menuTree->setCurrentItem(item);
            return;
        }
    }
    m_menuTree->clearSelection();
}

bool MainWindow::confirmLeaveCurrent()
{
    if (!m_dirty) {
        return true;
    }
    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, QStringLiteral("Unsaved changes"),
        QStringLiteral("Save your changes to “%1” before continuing?").arg(m_current.displayName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel) {
        return false;
    }
    if (choice == QMessageBox::Save) {
        return saveCurrent();
    }
    m_dirty = false;
    return true;
}

bool MainWindow::saveCurrent()
{
    if (!m_hasCurrent) {
        return false;
    }
    syncCurrentFromEditor();
    QString error;
    if (!m_store.save(&m_current, &error)) {
        QMessageBox::critical(this, QStringLiteral("Could not save action"), error);
        updatePreviewAndValidation();
        return false;
    }
    m_dirty = false;
    updatePreviewAndValidation();
    statusBar()->showMessage(QStringLiteral("Saved %1").arg(m_current.displayName()), 3500);
    return true;
}

void MainWindow::loadCurrentIntoEditor()
{
    if (!m_hasCurrent) {
        clearEditor();
        return;
    }
    m_loading = true;
    m_editorPage->setEnabled(true);
    m_editorTitle->setText(m_current.displayName());
    if (m_current.path.isEmpty()) {
        m_sourceLabel->setText(
            QStringLiteral("New personal action · will be saved in %1").arg(m_store.userDirectory()));
    } else if (m_current.userOwned) {
        m_sourceLabel->setText(QStringLiteral("Personal action%1 · %2")
                                   .arg(m_current.enabled ? QString() : QStringLiteral(" (disabled)"),
                                        m_current.path));
    } else {
        m_sourceLabel->setText(
            QStringLiteral("System action (read-only) · %1").arg(m_current.path));
    }

    m_enabledCheck->setChecked(m_current.enabled);
    int presetIndex = 0;
    const QString normalized = ServiceMenuStore::normalizedMimeTypes(m_current.mimeTypes);
    for (int index = 1; index < m_mimePresetCombo->count(); ++index) {
        if (m_mimePresetCombo->itemData(index).toString() == normalized) {
            presetIndex = index;
            break;
        }
    }
    m_mimePresetCombo->setCurrentIndex(presetIndex);
    rebuildExtensionTable();
    m_submenuEdit->setText(m_current.submenu);
    m_topLevelCheck->setChecked(m_current.topLevel);
    rebuildActionList(0);
    m_loading = false;

    const bool editable = (m_current.path.isEmpty() || m_current.userOwned)
        && m_current.loadError.isEmpty();
    m_editorBody->setEnabled(editable);
    m_duplicateButton->setEnabled(true);
    m_deleteButton->setEnabled(m_current.userOwned && !m_current.path.isEmpty());
    updatePreviewAndValidation();
}

void MainWindow::clearEditor()
{
    m_hasCurrent = false;
    m_selectedMenuIndex = -1;
    m_activeActionIndex = -1;
    m_editorTitle->setText(QStringLiteral("No context actions found"));
    m_sourceLabel->setText(QStringLiteral("Create a personal action to get started."));
    m_editorBody->setEnabled(false);
    m_previewEdit->clear();
    m_statusLabel->clear();
    m_saveButton->setEnabled(false);
    m_duplicateButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

void MainWindow::syncCurrentFromEditor()
{
    if (!m_hasCurrent || m_loading) {
        return;
    }
    syncActiveActionFromEditor();
    m_current.enabled = m_enabledCheck->isChecked();
    if (m_mimePresetCombo->currentIndex() > 0) {
        m_current.mimeTypes = m_mimePresetCombo->currentData().toString();
        m_current.extensions.clear();
    } else {
        syncFileTypesFromTable();
    }
    m_current.submenu = m_submenuEdit->text().trimmed();
    m_current.topLevel = m_topLevelCheck->isChecked();
}

void MainWindow::syncActiveActionFromEditor()
{
    if (!m_hasCurrent || m_activeActionIndex < 0
        || m_activeActionIndex >= m_current.actions.size()) {
        return;
    }
    ServiceAction &action = m_current.actions[m_activeActionIndex];
    action.name = m_actionNameEdit->text().trimmed();
    const QString iconChoice = m_iconCombo->currentData().toString();
    action.icon = iconChoice == QStringLiteral("__custom__")
        ? m_iconEdit->text().trimmed()
        : iconChoice;
    action.program = m_programEdit->text().trimmed();
    action.arguments = m_argumentsEdit->text().trimmed();
    action.runInTerminal = m_terminalCheck->isChecked();
}

void MainWindow::loadActionIntoEditor(int index)
{
    const bool previousLoading = m_loading;
    m_loading = true;
    m_activeActionIndex = index;
    if (index < 0 || index >= m_current.actions.size()) {
        m_actionNameEdit->clear();
        m_iconEdit->clear();
        m_programEdit->clear();
        m_argumentsEdit->clear();
        m_terminalCheck->setChecked(false);
        m_loading = previousLoading;
        return;
    }

    const ServiceAction &action = m_current.actions.at(index);
    m_actionNameEdit->setText(action.name);
    m_iconEdit->setText(action.icon);
    int iconIndex = m_iconCombo->findData(action.icon);
    if (iconIndex < 0) {
        iconIndex = m_iconCombo->findData(QStringLiteral("__custom__"));
    }
    m_iconCombo->setCurrentIndex(iconIndex);
    const bool customIcon = m_iconCombo->currentData().toString() == QStringLiteral("__custom__");
    m_iconEdit->setVisible(customIcon);
    m_chooseIconButton->setVisible(customIcon);
    m_programEdit->setText(action.program);
    m_argumentsEdit->setText(action.arguments);
    m_selectionCombo->setCurrentIndex(selectionModeForArguments(action.arguments));
    m_terminalCheck->setChecked(action.runInTerminal);
    m_removeActionButton->setEnabled(m_current.actions.size() > 1);
    updateIconPreview();
    m_loading = previousLoading;
}

void MainWindow::rebuildActionList(int selectedIndex)
{
    const bool previousLoading = m_loading;
    m_loading = true;
    m_actionList->clear();
    for (const ServiceAction &action : std::as_const(m_current.actions)) {
        auto *item = new QListWidgetItem(
            QIcon::fromTheme(action.icon, QIcon(action.icon)),
            action.name.trimmed().isEmpty() ? QStringLiteral("Unnamed action") : action.name);
        m_actionList->addItem(item);
    }
    if (!m_current.actions.isEmpty()) {
        selectedIndex = std::clamp(
            selectedIndex, 0, static_cast<int>(m_current.actions.size()) - 1);
        m_actionList->setCurrentRow(selectedIndex);
        loadActionIntoEditor(selectedIndex);
    } else {
        loadActionIntoEditor(-1);
    }
    m_loading = previousLoading;
}

void MainWindow::editorChanged()
{
    if (m_loading || !m_hasCurrent || !m_editorBody->isEnabled()) {
        return;
    }
    syncCurrentFromEditor();
    m_dirty = true;
    m_editorTitle->setText(m_current.displayName());
    updatePreviewAndValidation();
}

void MainWindow::updatePreviewAndValidation()
{
    if (!m_hasCurrent) {
        return;
    }
    if (!m_loading && m_editorBody->isEnabled()) {
        syncCurrentFromEditor();
    }
    m_previewEdit->setPlainText(m_store.serialize(m_current));

    const bool editable = (m_current.path.isEmpty() || m_current.userOwned)
        && m_current.loadError.isEmpty();
    const ValidationResult validation = m_store.validate(m_current);
    QString message;
    QString color;
    if (!m_current.loadError.isEmpty()) {
        message = QStringLiteral("Could not read this service menu: %1").arg(m_current.loadError);
        color = QStringLiteral("#da4453");
    } else if (!editable) {
        message = QStringLiteral("System actions are protected. Duplicate this action to customize it.");
        color = QStringLiteral("#3daee9");
    } else if (!validation.errors.isEmpty()) {
        message = QStringLiteral("Fix before saving: %1").arg(validation.errors.first());
        color = QStringLiteral("#da4453");
    } else if (!validation.warnings.isEmpty()) {
        message = QStringLiteral("Ready, with a note: %1").arg(validation.warnings.first());
        color = QStringLiteral("#f39c12");
    } else if (m_dirty) {
        message = QStringLiteral("Ready to save. Dolphin's service cache will refresh automatically.");
        color = QStringLiteral("#27ae60");
    } else {
        message = QStringLiteral("Saved and ready to use in Dolphin.");
        color = QStringLiteral("#27ae60");
    }
    m_statusLabel->setText(message);
    m_statusLabel->setStyleSheet(
        QStringLiteral("border: 1px solid %1; border-radius: 5px;").arg(color));
    m_saveButton->setEnabled(editable && validation.isValid() && m_dirty);
    m_removeActionButton->setEnabled(editable && m_current.actions.size() > 1);
}

void MainWindow::updateIconPreview()
{
    const QString selection = m_iconCombo->currentData().toString();
    const QString icon = selection == QStringLiteral("__custom__")
        ? m_iconEdit->text().trimmed()
        : selection;
    m_iconPreview->setIcon(icon.isEmpty() ? QIcon::fromTheme(QStringLiteral("configure"))
                                          : QIcon::fromTheme(icon, QIcon(icon)));
    if (!m_loading && m_activeActionIndex >= 0
        && m_activeActionIndex < m_actionList->count()) {
        m_actionList->item(m_activeActionIndex)->setIcon(m_iconPreview->icon());
    }
}

void MainWindow::rebuildExtensionTable()
{
    const bool custom = m_mimePresetCombo->currentIndex() == 0;
    m_extensionPanel->setEnabled(custom);
    m_extensionTable->setRowCount(0);
    m_removeExtensionButton->setEnabled(false);
    if (!custom) {
        return;
    }

    QMimeDatabase database;
    const QStringList mimeTypes = splitMimeTypes(m_current.mimeTypes);
    QSet<QString> representedTypes;
    auto appendRow = [this](const QString &extension, const QString &mimeType) {
        const int row = m_extensionTable->rowCount();
        m_extensionTable->insertRow(row);
        m_extensionTable->setItem(row, 0, new QTableWidgetItem(extension));
        m_extensionTable->setItem(row, 1, new QTableWidgetItem(mimeType));
    };

    for (QString extension : std::as_const(m_current.extensions)) {
        extension = extension.trimmed().toLower();
        if (extension.isEmpty()) {
            continue;
        }
        if (!extension.startsWith(QLatin1Char('.'))) {
            extension.prepend(QLatin1Char('.'));
        }
        const QString mimeType = database
            .mimeTypeForFile(QStringLiteral("example") + extension, QMimeDatabase::MatchExtension)
            .name();
        appendRow(extension, mimeType);
        const QString canonical = database.mimeTypeForName(mimeType).name();
        representedTypes.insert(canonical.isEmpty() ? mimeType : canonical);
    }

    for (const QString &mimeType : mimeTypes) {
        const QString canonical = database.mimeTypeForName(mimeType).name();
        if (representedTypes.contains(canonical.isEmpty() ? mimeType : canonical)) {
            continue;
        }
        const QString suffix = database.mimeTypeForName(mimeType).preferredSuffix();
        const QString extension = suffix.isEmpty() ? QStringLiteral("—")
                                                   : QLatin1Char('.') + suffix.toLower();
        appendRow(extension, mimeType);
    }
}

void MainWindow::syncFileTypesFromTable()
{
    QStringList mimeTypes;
    QStringList extensions;
    for (int row = 0; row < m_extensionTable->rowCount(); ++row) {
        const QString extension = m_extensionTable->item(row, 0)->text();
        const QString mimeType = m_extensionTable->item(row, 1)->text();
        if (!mimeType.isEmpty() && !mimeTypes.contains(mimeType)) {
            mimeTypes.append(mimeType);
        }
        if (extension.startsWith(QLatin1Char('.')) && !extensions.contains(extension)) {
            extensions.append(extension);
        }
    }
    m_current.mimeTypes = ServiceMenuStore::normalizedMimeTypes(
        mimeTypes.join(QLatin1Char(';')));
    m_current.extensions = extensions;
}

void MainWindow::newMenu()
{
    if (!confirmLeaveCurrent()) {
        return;
    }
    {
        const QSignalBlocker blocker(m_menuTree);
        m_menuTree->clearSelection();
        m_menuTree->setCurrentItem(nullptr);
    }
    m_current = m_store.createBlank();
    m_hasCurrent = true;
    m_selectedMenuIndex = -1;
    m_dirty = true;
    loadCurrentIntoEditor();
    m_dirty = true;
    updatePreviewAndValidation();
    m_actionNameEdit->selectAll();
    m_actionNameEdit->setFocus();
}

void MainWindow::duplicateMenu()
{
    if (!m_hasCurrent || !confirmLeaveCurrent()) {
        return;
    }
    syncCurrentFromEditor();
    m_current.path.clear();
    m_current.userOwned = true;
    m_current.enabled = true;
    m_current.document = IniDocument();
    m_current.loadError.clear();
    if (!m_current.submenu.isEmpty()) {
        m_current.submenu += QStringLiteral(" (Copy)");
    } else if (!m_current.actions.isEmpty()) {
        m_current.actions.first().name += QStringLiteral(" (Copy)");
    }
    {
        const QSignalBlocker blocker(m_menuTree);
        m_menuTree->clearSelection();
        m_menuTree->setCurrentItem(nullptr);
    }
    m_selectedMenuIndex = -1;
    m_dirty = true;
    loadCurrentIntoEditor();
    m_dirty = true;
    updatePreviewAndValidation();
}

void MainWindow::deleteMenu()
{
    if (!m_hasCurrent || m_current.path.isEmpty() || !m_current.userOwned) {
        return;
    }
    if (QMessageBox::question(
            this, QStringLiteral("Move action to Trash"),
            QStringLiteral("Move “%1” to Trash? This can be undone from Dolphin.")
                .arg(m_current.displayName()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_store.moveToTrash(m_current, &error)) {
        QMessageBox::critical(this, QStringLiteral("Could not remove action"), error);
        return;
    }
    m_dirty = false;
    reloadMenus();
}

void MainWindow::menuSelectionChanged()
{
    if (m_loading) {
        return;
    }
    QTreeWidgetItem *item = m_menuTree->currentItem();
    if (!item) {
        return;
    }
    const int newIndex = item->data(0, Qt::UserRole).toInt();
    if (m_hasCurrent && newIndex == m_selectedMenuIndex) {
        return;
    }
    if (!confirmLeaveCurrent()) {
        selectListIndex(m_selectedMenuIndex);
        return;
    }
    m_selectedMenuIndex = newIndex;
    m_current = m_menus.at(newIndex);
    m_hasCurrent = true;
    m_dirty = false;
    loadCurrentIntoEditor();
}

void MainWindow::actionSelectionChanged(int row)
{
    if (m_loading || row == m_activeActionIndex) {
        return;
    }
    syncActiveActionFromEditor();
    loadActionIntoEditor(row);
    updatePreviewAndValidation();
}

void MainWindow::addAction()
{
    if (!m_hasCurrent || !m_editorBody->isEnabled()) {
        return;
    }
    syncCurrentFromEditor();
    QStringList ids;
    for (const ServiceAction &action : std::as_const(m_current.actions)) {
        ids.append(action.id);
    }
    ServiceAction action;
    action.name = QStringLiteral("New action");
    action.id = ServiceMenuStore::makeActionId(action.name, ids);
    m_current.actions.append(action);
    rebuildActionList(m_current.actions.size() - 1);
    m_dirty = true;
    updatePreviewAndValidation();
    m_actionNameEdit->selectAll();
    m_actionNameEdit->setFocus();
}

void MainWindow::removeAction()
{
    if (!m_hasCurrent || !m_editorBody->isEnabled() || m_current.actions.size() <= 1
        || m_activeActionIndex < 0) {
        return;
    }
    const int removedIndex = m_activeActionIndex;
    m_current.actions.removeAt(removedIndex);
    rebuildActionList(
        std::min(removedIndex, static_cast<int>(m_current.actions.size()) - 1));
    m_dirty = true;
    updatePreviewAndValidation();
}

void MainWindow::chooseMimeTypes()
{
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Choose file types"));
    dialog.resize(680, 560);
    auto *layout = new QVBoxLayout(&dialog);
    layout->addWidget(makeMutedLabel(
        QStringLiteral("Select the file types that should show this context action."), &dialog));
    auto *filterEdit = new QLineEdit(&dialog);
    filterEdit->setPlaceholderText(QStringLiteral("Search MIME types or descriptions…"));
    filterEdit->setClearButtonEnabled(true);
    layout->addWidget(filterEdit);
    auto *list = new QListWidget(&dialog);
    list->setAlternatingRowColors(true);
    list->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(list, 1);

    syncCurrentFromEditor();
    const QStringList selected = splitMimeTypes(m_current.mimeTypes);
    QMimeDatabase database;
    QList<QMimeType> types = database.allMimeTypes();
    std::sort(types.begin(), types.end(), [](const QMimeType &left, const QMimeType &right) {
        return left.name() < right.name();
    });
    for (const QMimeType &mime : std::as_const(types)) {
        const QString label = mime.comment().isEmpty()
            ? mime.name()
            : QStringLiteral("%1 — %2").arg(mime.name(), mime.comment());
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, mime.name());
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(selected.contains(mime.name()) ? Qt::Checked : Qt::Unchecked);
    }
    connect(filterEdit, &QLineEdit::textChanged, &dialog, [list](const QString &text) {
        for (int row = 0; row < list->count(); ++row) {
            list->item(row)->setHidden(!list->item(row)->text().contains(text, Qt::CaseInsensitive));
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QStringList chosen;
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->checkState() == Qt::Checked) {
            chosen.append(list->item(row)->data(Qt::UserRole).toString());
        }
    }
    m_current.mimeTypes = ServiceMenuStore::normalizedMimeTypes(chosen.join(QLatin1Char(';')));
    m_current.extensions.clear();
    for (const QString &mimeType : std::as_const(chosen)) {
        const QString suffix = database.mimeTypeForName(mimeType).preferredSuffix();
        if (!suffix.isEmpty()) {
            const QString extension = QLatin1Char('.') + suffix.toLower();
            if (!m_current.extensions.contains(extension)) {
                m_current.extensions.append(extension);
            }
        }
    }
    {
        const QSignalBlocker blocker(m_mimePresetCombo);
        m_mimePresetCombo->setCurrentIndex(0);
    }
    rebuildExtensionTable();
    editorChanged();
}

void MainWindow::detectMimeType()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose an example file"), QStandardPaths::writableLocation(
                                                           QStandardPaths::HomeLocation));
    if (path.isEmpty()) {
        return;
    }
    const QString suffix = QFileInfo(path).suffix();
    if (suffix.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("No filename extension"),
            QStringLiteral("That file has no extension. Use Advanced MIME to select its type."));
        return;
    }
    m_extensionEdit->setText(QLatin1Char('.') + suffix);
    addExtensions();
}

void MainWindow::addExtensions()
{
    if (!m_hasCurrent || !m_editorBody->isEnabled()) {
        return;
    }
    const QStringList candidates = m_extensionEdit->text().split(
        QRegularExpression(QStringLiteral("[,;\\s]+")), Qt::SkipEmptyParts);
    if (candidates.isEmpty()) {
        return;
    }

    if (m_mimePresetCombo->currentIndex() != 0) {
        const QSignalBlocker blocker(m_mimePresetCombo);
        m_mimePresetCombo->setCurrentIndex(0);
        m_current.mimeTypes.clear();
        m_current.extensions.clear();
        rebuildExtensionTable();
    }

    QMimeDatabase database;
    QStringList rejected;
    for (QString extension : candidates) {
        extension = extension.trimmed().toLower();
        while (extension.startsWith(QStringLiteral("*."))) {
            extension.removeFirst();
        }
        if (!extension.startsWith(QLatin1Char('.'))) {
            extension.prepend(QLatin1Char('.'));
        }
        if (!QRegularExpression(QStringLiteral("^\\.[a-z0-9][a-z0-9.+_-]*$"))
                 .match(extension)
                 .hasMatch()) {
            rejected.append(extension);
            continue;
        }

        const QString mimeType = database
            .mimeTypeForFile(QStringLiteral("example") + extension, QMimeDatabase::MatchExtension)
            .name();
        if (mimeType.isEmpty() || mimeType == QStringLiteral("application/octet-stream")) {
            rejected.append(extension);
            continue;
        }

        bool duplicate = false;
        for (int row = 0; row < m_extensionTable->rowCount(); ++row) {
            if (m_extensionTable->item(row, 0)->text() == extension) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            const int row = m_extensionTable->rowCount();
            m_extensionTable->insertRow(row);
            m_extensionTable->setItem(row, 0, new QTableWidgetItem(extension));
            m_extensionTable->setItem(row, 1, new QTableWidgetItem(mimeType));
        }
    }

    syncFileTypesFromTable();
    m_extensionEdit->clear();
    m_dirty = true;
    updatePreviewAndValidation();
    if (!rejected.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Unknown extension"),
            QStringLiteral("KDE does not have a specific MIME type for: %1\n\n"
                           "Register that file type first, or use Advanced MIME if you know its type.")
                .arg(rejected.join(QStringLiteral(", "))));
    }
}

void MainWindow::removeExtensions()
{
    QSet<int> selectedRows;
    for (const QTableWidgetItem *item : m_extensionTable->selectedItems()) {
        selectedRows.insert(item->row());
    }
    QList<int> rows = selectedRows.values();
    std::sort(rows.begin(), rows.end(), std::greater<>());
    for (int row : std::as_const(rows)) {
        m_extensionTable->removeRow(row);
    }
    if (!rows.isEmpty()) {
        syncFileTypesFromTable();
        m_dirty = true;
        updatePreviewAndValidation();
    }
}

void MainWindow::chooseProgram()
{
    const QString start = m_programEdit->text().startsWith(QLatin1Char('/'))
        ? m_programEdit->text()
        : QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose a program or script"), start);
    if (!path.isEmpty()) {
        m_programEdit->setText(path);
    }
}

void MainWindow::chooseIcon()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose an icon"), QStringLiteral("/usr/share/icons"),
        QStringLiteral("Images (*.png *.svg *.svgz *.xpm);;All files (*)"));
    if (!path.isEmpty()) {
        const QSignalBlocker blocker(m_iconCombo);
        m_iconCombo->setCurrentIndex(m_iconCombo->findData(QStringLiteral("__custom__")));
        m_iconEdit->setVisible(true);
        m_chooseIconButton->setVisible(true);
        m_iconEdit->setText(path);
    }
}

void MainWindow::mimePresetChanged(int index)
{
    if (m_loading) {
        return;
    }
    if (index == 0) {
        m_current.mimeTypes.clear();
        m_current.extensions.clear();
    } else {
        m_current.mimeTypes = m_mimePresetCombo->itemData(index).toString();
        m_current.extensions.clear();
    }
    rebuildExtensionTable();
    editorChanged();
}

void MainWindow::iconPresetChanged(int index)
{
    if (index < 0) {
        return;
    }
    const bool custom = m_iconCombo->itemData(index).toString() == QStringLiteral("__custom__");
    m_iconEdit->setVisible(custom);
    m_chooseIconButton->setVisible(custom);
    updateIconPreview();
    editorChanged();
}

void MainWindow::selectionModeChanged(int index)
{
    if (m_loading) {
        return;
    }
    QString arguments = m_argumentsEdit->text();
    const QString placeholder = placeholderForSelectionMode(index);
    const QRegularExpression expression(QStringLiteral("%[fFuU]"));
    if (arguments.contains(expression)) {
        arguments.replace(expression, placeholder);
    } else if (arguments.trimmed().isEmpty()) {
        arguments = placeholder;
    } else {
        arguments = arguments.trimmed() + QLatin1Char(' ') + placeholder;
    }
    m_argumentsEdit->setText(arguments);
}

void MainWindow::insertArgumentPlaceholder(const QString &placeholder)
{
    QString arguments = m_argumentsEdit->text().trimmed();
    if (!arguments.isEmpty()) {
        arguments += QLatin1Char(' ');
    }
    arguments += placeholder;
    m_argumentsEdit->setText(arguments);
    m_argumentsEdit->setFocus();
    m_argumentsEdit->setCursorPosition(arguments.size());
}

QString MainWindow::mimeSummary(const QString &mimeTypes)
{
    const QString normalized = ServiceMenuStore::normalizedMimeTypes(mimeTypes);
    if (normalized == QStringLiteral("application/octet-stream;")) {
        return QStringLiteral("All files");
    }
    if (normalized == QStringLiteral("inode/directory;")) {
        return QStringLiteral("Folders");
    }
    if (normalized == QStringLiteral("image/*;")) {
        return QStringLiteral("All images");
    }
    if (normalized == QStringLiteral("video/*;")) {
        return QStringLiteral("All videos");
    }
    if (normalized == QStringLiteral("audio/*;")) {
        return QStringLiteral("All audio");
    }
    const QStringList types = splitMimeTypes(normalized);
    return types.size() <= 2 ? types.join(QStringLiteral(", "))
                             : QStringLiteral("%1 file types").arg(types.size());
}

int MainWindow::selectionModeForArguments(const QString &arguments)
{
    if (arguments.contains(QStringLiteral("%U"))) {
        return 3;
    }
    if (arguments.contains(QStringLiteral("%u"))) {
        return 2;
    }
    if (arguments.contains(QStringLiteral("%F"))) {
        return 1;
    }
    return 0;
}

QString MainWindow::placeholderForSelectionMode(int index)
{
    static const QStringList placeholders{
        QStringLiteral("%f"), QStringLiteral("%F"), QStringLiteral("%u"), QStringLiteral("%U")};
    return placeholders.value(index, QStringLiteral("%f"));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (confirmLeaveCurrent()) {
        event->accept();
    } else {
        event->ignore();
    }
}
