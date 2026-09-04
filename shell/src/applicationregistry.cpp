#include "applicationregistry.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <optional>
#include <utility>

#include <unistd.h>

namespace {

void writeLaunchEvent(const QString &appId, const QString &state, qint64 processId = 0)
{
    const QString path = qEnvironmentVariable("MOKO_LIVE_LAUNCH_EVENTS");
    if (path.isEmpty())
        return;

    QString safeId = appId;
    for (QChar &character : safeId) {
        if (!character.isLetterOrNumber() && character != u'.' && character != u'-'
            && character != u'_') {
            character = u'_';
        }
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    file.write(QStringLiteral("MOKO_APP_LAUNCH app_id=%1 state=%2 pid=%3 uid=%4\n")
                   .arg(safeId, state)
                   .arg(processId)
                   .arg(geteuid())
                   .toUtf8());
    file.flush();
}

struct DesktopEntryData
{
    QHash<QString, QString> values;

    bool contains(const QString &key) const { return values.contains(key); }
    QString raw(const QString &key) const { return values.value(key).trimmed(); }
};

DesktopEntryData readDesktopEntry(const QString &path)
{
    DesktopEntryData result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine());
        if (line.endsWith(u'\n'))
            line.chop(1);
        if (line.endsWith(u'\r'))
            line.chop(1);

        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(u'#'))
            continue;
        if (trimmed.startsWith(u'[') && trimmed.endsWith(u']')) {
            inDesktopEntry = trimmed == QStringLiteral("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry)
            continue;

        const qsizetype separator = line.indexOf(u'=');
        if (separator <= 0)
            continue;
        const QString key = line.left(separator).trimmed();
        if (!key.isEmpty() && !result.values.contains(key))
            result.values.insert(key, line.mid(separator + 1));
    }
    return result;
}

QChar desktopEscape(QChar character)
{
    switch (character.unicode()) {
    case 's':
        return u' ';
    case 'n':
        return u'\n';
    case 't':
        return u'\t';
    case 'r':
        return u'\r';
    default:
        return character;
    }
}

QString desktopString(const QString &value)
{
    QString result;
    bool escaped = false;
    for (const QChar character : value) {
        if (escaped) {
            result.append(desktopEscape(character));
            escaped = false;
        } else if (character == u'\\') {
            escaped = true;
        } else {
            result.append(character);
        }
    }
    if (escaped)
        result.append(u'\\');
    return result.trimmed();
}

QStringList desktopList(const QString &value)
{
    QStringList result;
    QString current;
    bool escaped = false;

    for (const QChar character : value) {
        if (escaped) {
            current.append(desktopEscape(character));
            escaped = false;
        } else if (character == u'\\') {
            escaped = true;
        } else if (character == u';') {
            if (!current.trimmed().isEmpty())
                result.append(current.trimmed());
            current.clear();
        } else {
            current.append(character);
        }
    }

    if (escaped)
        current.append(u'\\');
    if (!current.trimmed().isEmpty())
        result.append(current.trimmed());
    return result;
}

bool desktopBool(const QVariant &value)
{
    return value.toString().compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0
        || value.toString() == QStringLiteral("1");
}

QString localizedValue(const DesktopEntryData &entry, const QString &key)
{
    QStringList locales = QLocale::system().uiLanguages();
    locales.prepend(QLocale::system().name());

    for (QString locale : std::as_const(locales)) {
        locale.replace(u'-', u'_');
        const QStringList parts = locale.split(u'_');
        const QStringList candidates = parts.size() > 1
            ? QStringList{locale, parts.constFirst()}
            : QStringList{locale};
        for (const QString &candidate : candidates) {
            const QString localizedKey = QStringLiteral("%1[%2]").arg(key, candidate);
            if (entry.contains(localizedKey))
                return desktopString(entry.raw(localizedKey));
        }
    }
    return desktopString(entry.raw(key));
}

QString categoryName(const QStringList &categories)
{
    static const QList<QPair<QString, QString>> names = {
        {QStringLiteral("AudioVideo"), QStringLiteral("Media")},
        {QStringLiteral("Development"), QStringLiteral("Development")},
        {QStringLiteral("Education"), QStringLiteral("Education")},
        {QStringLiteral("Game"), QStringLiteral("Games")},
        {QStringLiteral("Graphics"), QStringLiteral("Graphics")},
        {QStringLiteral("Network"), QStringLiteral("Internet")},
        {QStringLiteral("Office"), QStringLiteral("Office")},
        {QStringLiteral("Science"), QStringLiteral("Science")},
        {QStringLiteral("Settings"), QStringLiteral("Settings")},
        {QStringLiteral("System"), QStringLiteral("System")},
        {QStringLiteral("Utility"), QStringLiteral("Utilities")},
    };

    for (const auto &[desktopCategory, displayCategory] : names) {
        if (categories.contains(desktopCategory))
            return displayCategory;
    }
    return QStringLiteral("Other");
}

QString glyphFor(const QString &id, const QString &name, const QStringList &categories)
{
    const QString haystack = (id + u' ' + name).toLower();
    if (haystack.contains(QStringLiteral("file")))
        return QStringLiteral("files");
    if (haystack.contains(QStringLiteral("terminal")) || haystack.contains(QStringLiteral("foot")))
        return QStringLiteral("terminal");
    if (haystack.contains(QStringLiteral("setting")) || categories.contains(QStringLiteral("Settings")))
        return QStringLiteral("settings");
    if (haystack.contains(QStringLiteral("browser")) || categories.contains(QStringLiteral("Network")))
        return QStringLiteral("browser");
    if (haystack.contains(QStringLiteral("mail")))
        return QStringLiteral("mail");
    if (haystack.contains(QStringLiteral("calendar")))
        return QStringLiteral("calendar");
    if (haystack.contains(QStringLiteral("music")) || categories.contains(QStringLiteral("AudioVideo")))
        return QStringLiteral("music");
    if (haystack.contains(QStringLiteral("camera")))
        return QStringLiteral("camera");
    return QStringLiteral("system");
}

bool isAvailableDesktop(const DesktopEntryData &entry)
{
    if (desktopString(entry.raw(QStringLiteral("Type"))) != QStringLiteral("Application"))
        return false;
    if (desktopBool(entry.raw(QStringLiteral("Hidden")))
        || desktopBool(entry.raw(QStringLiteral("NoDisplay")))) {
        return false;
    }

    const QStringList currentDesktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP", "MOKO")
                                                .split(u':', Qt::SkipEmptyParts);
    const QStringList onlyShowIn = desktopList(entry.raw(QStringLiteral("OnlyShowIn")));
    const QStringList notShowIn = desktopList(entry.raw(QStringLiteral("NotShowIn")));

    if (!onlyShowIn.isEmpty()) {
        bool matches = false;
        for (const QString &desktop : currentDesktops)
            matches = matches || onlyShowIn.contains(desktop, Qt::CaseInsensitive);
        if (!matches)
            return false;
    }
    for (const QString &desktop : currentDesktops) {
        if (notShowIn.contains(desktop, Qt::CaseInsensitive))
            return false;
    }

    const QString tryExec = desktopString(entry.raw(QStringLiteral("TryExec")));
    if (!tryExec.isEmpty()) {
        const bool available = tryExec.contains(u'/')
            ? QFileInfo(tryExec).isExecutable()
            : !QStandardPaths::findExecutable(tryExec).isEmpty();
        if (!available)
            return false;
    }
    return true;
}

std::optional<QString> expandExecToken(const QString &token,
                                       const QString &name,
                                       const QString &desktopFile)
{
    QString result;
    for (qsizetype index = 0; index < token.size(); ++index) {
        const QChar character = token.at(index);
        if (character != u'%') {
            result.append(character);
            continue;
        }
        if (++index >= token.size())
            return std::nullopt;

        switch (token.at(index).unicode()) {
        case '%':
            result.append(u'%');
            break;
        case 'c':
            result.append(name);
            break;
        case 'k':
            result.append(desktopFile);
            break;
        case 'f':
        case 'F':
        case 'u':
        case 'U':
        case 'd':
        case 'D':
        case 'n':
        case 'N':
        case 'v':
        case 'm':
            break;
        default:
            return std::nullopt;
        }
    }
    return result;
}

bool parseExec(ApplicationEntry &entry)
{
    const QStringList tokens = QProcess::splitCommand(entry.executable);
    if (tokens.isEmpty())
        return false;

    QStringList expanded;
    for (const QString &token : tokens) {
        if (token == QStringLiteral("%i")) {
            if (!entry.iconName.isEmpty())
                expanded << QStringLiteral("--icon") << entry.iconName;
            continue;
        }

        const std::optional<QString> value = expandExecToken(token, entry.displayName, entry.desktopFile);
        if (!value.has_value())
            return false;
        if (!value->isEmpty())
            expanded.append(*value);
    }

    if (expanded.isEmpty())
        return false;
    entry.program = expanded.takeFirst();
    entry.arguments = expanded;
    return !entry.program.isEmpty();
}

std::optional<ApplicationEntry> parseDesktopEntry(const QString &path, const QString &desktopId)
{
    const DesktopEntryData desktop = readDesktopEntry(path);
    if (desktop.values.isEmpty() || !isAvailableDesktop(desktop))
        return std::nullopt;

    ApplicationEntry entry;
    entry.id = desktopString(desktop.raw(QStringLiteral("X-MOKO-AppId")));
    if (entry.id.isEmpty())
        entry.id = desktopId;
    entry.displayName = localizedValue(desktop, QStringLiteral("Name"));
    entry.description = localizedValue(desktop, QStringLiteral("Comment"));
    entry.iconName = desktopString(desktop.raw(QStringLiteral("Icon")));
    entry.executable = desktopString(desktop.raw(QStringLiteral("Exec")));
    entry.desktopFile = path;
    entry.workingDirectory = desktopString(desktop.raw(QStringLiteral("Path")));
    entry.terminal = desktopBool(desktop.raw(QStringLiteral("Terminal")));
    entry.pinned = desktopBool(desktop.raw(QStringLiteral("X-MOKO-Pinned")));
    bool pinOrderValid = false;
    entry.pinOrder = desktop.raw(QStringLiteral("X-MOKO-PinOrder")).toInt(&pinOrderValid);
    if (!pinOrderValid)
        entry.pinOrder = 1000;

    const QStringList categories = desktopList(desktop.raw(QStringLiteral("Categories")));
    entry.category = desktopString(desktop.raw(QStringLiteral("X-MOKO-Category")));
    if (entry.category.isEmpty())
        entry.category = categoryName(categories);

    entry.keywords = desktopList(desktop.raw(QStringLiteral("Keywords")));
    entry.keywords.append(categories);
    const QString genericName = localizedValue(desktop, QStringLiteral("GenericName"));
    if (!genericName.isEmpty())
        entry.keywords.append(genericName);
    entry.keywords.removeDuplicates();

    entry.glyph = desktopString(desktop.raw(QStringLiteral("X-MOKO-Glyph")));
    if (entry.glyph.isEmpty())
        entry.glyph = glyphFor(entry.id, entry.displayName, categories);
    const QIcon icon = QFileInfo(entry.iconName).isAbsolute()
        ? QIcon(entry.iconName)
        : QIcon::fromTheme(entry.iconName);
    if (!icon.isNull()) {
        entry.iconSource = QStringLiteral("image://moko-app-icon/")
            + QString::fromLatin1(QUrl::toPercentEncoding(entry.iconName));
    }

    if (entry.id.isEmpty() || entry.displayName.isEmpty() || entry.executable.isEmpty()
        || !parseExec(entry)) {
        return std::nullopt;
    }
    return entry;
}

QVariantMap entryMap(const ApplicationEntry &entry)
{
    return {
        {QStringLiteral("appId"), entry.id},
        {QStringLiteral("displayName"), entry.displayName},
        {QStringLiteral("iconName"), entry.iconName},
        {QStringLiteral("iconSource"), entry.iconSource},
        {QStringLiteral("executable"), entry.executable},
        {QStringLiteral("category"), entry.category},
        {QStringLiteral("description"), entry.description},
        {QStringLiteral("keywords"), entry.keywords},
        {QStringLiteral("glyph"), entry.glyph},
        {QStringLiteral("launchState"), entry.launchState},
        {QStringLiteral("launchMessage"), entry.launchMessage},
        {QStringLiteral("pinned"), entry.pinned},
    };
}

} // namespace

ApplicationRegistry::ApplicationRegistry(QObject *parent, QStringList applicationDirectories)
    : QAbstractListModel(parent)
    , m_applicationDirectories(std::move(applicationDirectories))
{
    reload();
}

int ApplicationRegistry::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant ApplicationRegistry::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const ApplicationEntry &entry = m_entries.at(index.row());
    switch (role) {
    case AppIdRole:
        return entry.id;
    case DisplayNameRole:
        return entry.displayName;
    case IconNameRole:
        return entry.iconName;
    case IconSourceRole:
        return entry.iconSource;
    case ExecutableRole:
        return entry.executable;
    case CategoryRole:
        return entry.category;
    case DescriptionRole:
        return entry.description;
    case KeywordsRole:
        return entry.keywords;
    case GlyphRole:
        return entry.glyph;
    case LaunchStateRole:
        return entry.launchState;
    case LaunchMessageRole:
        return entry.launchMessage;
    case PinnedRole:
        return entry.pinned;
    case PinOrderRole:
        return entry.pinOrder;
    default:
        return {};
    }
}

QHash<int, QByteArray> ApplicationRegistry::roleNames() const
{
    return {
        {AppIdRole, "appId"},
        {DisplayNameRole, "displayName"},
        {IconNameRole, "iconName"},
        {IconSourceRole, "iconSource"},
        {ExecutableRole, "executable"},
        {CategoryRole, "category"},
        {DescriptionRole, "description"},
        {KeywordsRole, "keywords"},
        {GlyphRole, "glyph"},
        {LaunchStateRole, "launchState"},
        {LaunchMessageRole, "launchMessage"},
        {PinnedRole, "pinned"},
        {PinOrderRole, "pinOrder"},
    };
}

QStringList ApplicationRegistry::defaultApplicationDirectories() const
{
    const QString dataHome = qEnvironmentVariable(
        "XDG_DATA_HOME", QDir::home().filePath(QStringLiteral(".local/share")));
    const QString dataDirs = qEnvironmentVariable(
        "XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"));

    QStringList directories{QDir(dataHome).filePath(QStringLiteral("applications"))};
    for (const QString &dataDirectory : dataDirs.split(QDir::listSeparator(), Qt::SkipEmptyParts))
        directories.append(QDir(dataDirectory).filePath(QStringLiteral("applications")));
    directories.removeDuplicates();
    return directories;
}

void ApplicationRegistry::reload()
{
    const QStringList directories = m_applicationDirectories.isEmpty()
        ? defaultApplicationDirectories()
        : m_applicationDirectories;
    QVector<ApplicationEntry> entries;
    QSet<QString> seenDesktopIds;
    QSet<QString> seenApplicationIds;

    for (const QString &directoryPath : directories) {
        const QDir directory(directoryPath);
        if (!directory.exists())
            continue;

        QStringList paths;
        QDirIterator iterator(directoryPath,
                              {QStringLiteral("*.desktop")},
                              QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext())
            paths.append(iterator.next());
        std::sort(paths.begin(), paths.end());

        for (const QString &path : std::as_const(paths)) {
            QString desktopId = directory.relativeFilePath(path);
            desktopId.replace(u'/', u'-');
            if (desktopId.endsWith(QStringLiteral(".desktop")))
                desktopId.chop(8);
            if (seenDesktopIds.contains(desktopId))
                continue;
            seenDesktopIds.insert(desktopId);

            std::optional<ApplicationEntry> entry = parseDesktopEntry(path, desktopId);
            if (!entry.has_value() || seenApplicationIds.contains(entry->id))
                continue;
            seenApplicationIds.insert(entry->id);
            entries.append(std::move(*entry));
        }
    }

    std::sort(entries.begin(), entries.end(), [](const ApplicationEntry &left, const ApplicationEntry &right) {
        return QString::localeAwareCompare(left.displayName, right.displayName) < 0;
    });

    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
    emit countChanged();
}

int ApplicationRegistry::indexOfApplication(const QString &appId) const
{
    for (int index = 0; index < m_entries.size(); ++index) {
        if (m_entries.at(index).id == appId)
            return index;
    }
    return -1;
}

QVariantMap ApplicationRegistry::application(const QString &appId) const
{
    const int row = indexOfApplication(appId);
    return row >= 0 ? entryMap(m_entries.at(row)) : QVariantMap{};
}

void ApplicationRegistry::setLaunchState(const QString &appId,
                                         const QString &state,
                                         const QString &message)
{
    const int row = indexOfApplication(appId);
    if (row < 0)
        return;

    ApplicationEntry &entry = m_entries[row];
    if (entry.launchState == state && entry.launchMessage == message)
        return;
    entry.launchState = state;
    entry.launchMessage = message;
    emit dataChanged(index(row), index(row), {LaunchStateRole, LaunchMessageRole});
    if (state == QStringLiteral("launching") || state == QStringLiteral("failed"))
        writeLaunchEvent(appId, state);
}

bool ApplicationRegistry::launch(const QString &appId)
{
    const int row = indexOfApplication(appId);
    if (row < 0) {
        emit applicationFailed(appId, appId, QStringLiteral("Application is not installed."));
        return false;
    }

    const ApplicationEntry entry = m_entries.at(row);
    if (const QPointer<QProcess> existing = m_processes.value(appId);
        existing && existing->state() != QProcess::NotRunning) {
        setLaunchState(appId, QStringLiteral("running"), QStringLiteral("Already running"));
        emit applicationRunning(appId, entry.displayName);
        return true;
    }

    QString program = entry.program;
    QStringList arguments = entry.arguments;
    if (entry.terminal) {
        QString terminal = QStandardPaths::findExecutable(QStringLiteral("foot"));
        if (terminal.isEmpty())
            terminal = QStandardPaths::findExecutable(QStringLiteral("x-terminal-emulator"));
        if (terminal.isEmpty()) {
            const QString message = QStringLiteral("No terminal emulator is available.");
            setLaunchState(appId, QStringLiteral("failed"), message);
            emit applicationFailed(appId, entry.displayName, message);
            return false;
        }
        arguments.prepend(program);
        arguments.prepend(QStringLiteral("-e"));
        program = terminal;
    }

    auto *process = new QProcess(this);
    process->setProgram(program);
    process->setArguments(arguments);
    process->setProcessChannelMode(QProcess::ForwardedChannels);
    if (!entry.workingDirectory.isEmpty() && QDir(entry.workingDirectory).exists())
        process->setWorkingDirectory(entry.workingDirectory);
    m_processes.insert(appId, process);

    connect(process, &QProcess::started, this, [this, appId, name = entry.displayName, process]() {
        setLaunchState(appId,
                       QStringLiteral("running"),
                       QStringLiteral("Running (PID %1)").arg(process->processId()));
        writeLaunchEvent(appId, QStringLiteral("running"), process->processId());
        emit applicationRunning(appId, name);
    });
    connect(process,
            &QProcess::errorOccurred,
            this,
            [this, appId, name = entry.displayName, process](QProcess::ProcessError error) {
                const QString message = process->errorString();
                setLaunchState(appId, QStringLiteral("failed"), message);
                emit applicationFailed(appId, name, message);
                if (error == QProcess::FailedToStart) {
                    m_processes.remove(appId);
                    process->deleteLater();
                }
            });
    connect(process,
            qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this,
            [this, appId, name = entry.displayName, process](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::CrashExit || exitCode != 0) {
                    const QString message = QStringLiteral("Exited with code %1").arg(exitCode);
                    setLaunchState(appId, QStringLiteral("failed"), message);
                    emit applicationFailed(appId, name, message);
                } else {
                    setLaunchState(appId, QStringLiteral("ready"), QStringLiteral("Exited normally"));
                }
                if (m_processes.value(appId) == process)
                    m_processes.remove(appId);
                process->deleteLater();
            });

    setLaunchState(appId, QStringLiteral("launching"), QStringLiteral("Starting..."));
    emit applicationLaunching(appId, entry.displayName);
    process->start();
    return true;
}

ApplicationFilterModel::ApplicationFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setSortCaseSensitivity(Qt::CaseInsensitive);
    sort(0);
    connect(this, &QAbstractItemModel::modelReset, this, &ApplicationFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsInserted, this, &ApplicationFilterModel::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ApplicationFilterModel::countChanged);
}

QString ApplicationFilterModel::searchText() const
{
    return m_searchText;
}

void ApplicationFilterModel::setSearchText(const QString &searchText)
{
    const QString normalized = searchText.simplified();
    if (m_searchText == normalized)
        return;
    m_searchText = normalized;
    invalidateFilter();
    emit searchTextChanged();
    emit countChanged();
}

bool ApplicationFilterModel::pinnedOnly() const
{
    return m_pinnedOnly;
}

void ApplicationFilterModel::setPinnedOnly(bool pinnedOnly)
{
    if (m_pinnedOnly == pinnedOnly)
        return;
    m_pinnedOnly = pinnedOnly;
    invalidateFilter();
    invalidate();
    emit pinnedOnlyChanged();
    emit countChanged();
}

bool ApplicationFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    const QModelIndex sourceIndex = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_pinnedOnly && !sourceIndex.data(ApplicationRegistry::PinnedRole).toBool())
        return false;
    if (m_searchText.isEmpty())
        return true;

    const QString haystack = QStringList{
        sourceIndex.data(ApplicationRegistry::DisplayNameRole).toString(),
        sourceIndex.data(ApplicationRegistry::CategoryRole).toString(),
        sourceIndex.data(ApplicationRegistry::DescriptionRole).toString(),
        sourceIndex.data(ApplicationRegistry::KeywordsRole).toStringList().join(u' '),
    }.join(u' ').toCaseFolded();

    for (const QString &term : m_searchText.toCaseFolded().split(u' ', Qt::SkipEmptyParts)) {
        if (!haystack.contains(term))
            return false;
    }
    return true;
}

bool ApplicationFilterModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    if (m_pinnedOnly) {
        const int leftOrder = left.data(ApplicationRegistry::PinOrderRole).toInt();
        const int rightOrder = right.data(ApplicationRegistry::PinOrderRole).toInt();
        if (leftOrder != rightOrder)
            return leftOrder < rightOrder;
    }
    return QString::localeAwareCompare(left.data(ApplicationRegistry::DisplayNameRole).toString(),
                                       right.data(ApplicationRegistry::DisplayNameRole).toString()) < 0;
}
