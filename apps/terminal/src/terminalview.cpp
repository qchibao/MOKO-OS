#include "terminalview.h"
#include "livemarker.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr QColor defaultForeground(225, 232, 240);
constexpr QColor defaultBackground(24, 28, 34);
constexpr QColor selectionBackground(58, 104, 176);

QString cellCharacters(const uint32_t *characters)
{
    QString text;
    for (int index = 0; index < VTERM_MAX_CHARS_PER_CELL && characters[index] != 0; ++index) {
        const char32_t character = characters[index];
        text.append(QString::fromUcs4(&character, 1));
    }
    return text.isEmpty() ? QStringLiteral(" ") : text;
}

} // namespace

TerminalView::TerminalView(QQuickItem *parent)
    : QQuickPaintedItem(parent)
    , m_font(QStringLiteral("DejaVu Sans Mono"), 12)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
    setFlag(ItemAcceptsInputMethod, true);
    setFocus(true);
    setAntialiasing(false);
    setOpaquePainting(true);

    m_font.setStyleHint(QFont::Monospace);
    m_font.setFixedPitch(true);
    m_vterm = vterm_new(m_rows, m_columns);
    vterm_set_utf8(m_vterm, 1);
    vterm_output_set_callback(m_vterm, &TerminalView::outputCallback, this);
    m_screen = vterm_obtain_screen(m_vterm);
    static const VTermScreenCallbacks callbacks = {
        &TerminalView::damageCallback,
        nullptr,
        &TerminalView::moveCursorCallback,
        &TerminalView::termPropertyCallback,
        &TerminalView::bellCallback,
        &TerminalView::resizeCallback,
        &TerminalView::scrollbackPushCallback,
        &TerminalView::scrollbackPopCallback,
        &TerminalView::scrollbackClearCallback,
    };
    vterm_screen_set_callbacks(m_screen, &callbacks, this);
    vterm_screen_set_damage_merge(m_screen, VTERM_DAMAGE_ROW);
    vterm_screen_enable_altscreen(m_screen, 1);
    vterm_screen_enable_reflow(m_screen, true);
    VTermColor foreground;
    VTermColor background;
    vterm_color_rgb(&foreground, defaultForeground.red(), defaultForeground.green(), defaultForeground.blue());
    vterm_color_rgb(&background, defaultBackground.red(), defaultBackground.green(), defaultBackground.blue());
    vterm_screen_set_default_colors(m_screen, &foreground, &background);
    vterm_screen_reset(m_screen, 1);

    connect(&m_session, &PtySession::bytesReceived, this, [this](const QByteArray &bytes) {
        vterm_input_write(m_vterm, bytes.constData(), static_cast<size_t>(bytes.size()));
        vterm_screen_flush_damage(m_screen);
        if (m_scrollOffset == 0)
            update();
    });
    connect(&m_session, &PtySession::runningChanged, this, &TerminalView::runningChanged);
    connect(&m_session, &PtySession::started, this, [this](qint64 processId) {
        setStatusMessage(QStringLiteral("Shell running (PID %1)").arg(processId));
        writeMokoLiveEvent(QStringLiteral("MOKO_APP_READY app_id=org.moko.Terminal state=ready detail=pty-shell-running pid=%1")
                               .arg(processId));
        emit shellStarted(processId);
    });
    connect(&m_session, &PtySession::exited, this, [this](int exitCode) {
        setStatusMessage(QStringLiteral("Shell exited with code %1").arg(exitCode));
        update();
    });
    connect(&m_session, &PtySession::errorOccurred, this, &TerminalView::setStatusMessage);
}

TerminalView::~TerminalView()
{
    if (m_vterm)
        vterm_free(m_vterm);
}

void TerminalView::paint(QPainter *painter)
{
    painter->fillRect(boundingRect(), defaultBackground);
    painter->setFont(m_font);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const QFontMetricsF metrics(m_font);
    for (int displayRow = 0; displayRow < m_rows; ++displayRow) {
        const Line line = visibleLine(displayRow);
        for (int column = 0; column < std::min(m_columns, static_cast<int>(line.size())); ++column) {
            const Cell &cell = line.at(column);
            if (cell.width == 0)
                continue;
            const QRectF cellRect(m_padding + column * m_cellWidth,
                                  m_padding + displayRow * m_cellHeight,
                                  m_cellWidth * qMax(1, cell.width),
                                  m_cellHeight);
            QColor background = cell.background.isValid() ? cell.background : defaultBackground;
            QColor foreground = cell.foreground.isValid() ? cell.foreground : defaultForeground;
            if (cellSelected(column, displayRow))
                background = selectionBackground;
            if (background != defaultBackground)
                painter->fillRect(cellRect, background);

            QFont cellFont = m_font;
            cellFont.setBold(cell.bold);
            cellFont.setItalic(cell.italic);
            cellFont.setUnderline(cell.underline);
            painter->setFont(cellFont);
            painter->setPen(foreground);
            painter->drawText(cellRect.left(),
                              cellRect.top() + metrics.ascent() + (m_cellHeight - metrics.height()) / 2,
                              cell.text);
        }
    }

    if (m_scrollOffset == 0 && m_cursorVisible && hasActiveFocus()) {
        const QRectF cursorRect(m_padding + m_cursor.col * m_cellWidth,
                                m_padding + m_cursor.row * m_cellHeight,
                                qMax<qreal>(2, m_cellWidth * .16),
                                m_cellHeight);
        painter->fillRect(cursorRect, QColor(120, 184, 255));
    }
}

QString TerminalView::terminalTitle() const
{
    return m_terminalTitle;
}

QString TerminalView::statusMessage() const
{
    return m_statusMessage;
}

bool TerminalView::running() const
{
    return m_session.running();
}

int TerminalView::scrollOffset() const
{
    return m_scrollOffset;
}

int TerminalView::scrollbackLines() const
{
    return m_scrollback.size();
}

bool TerminalView::startShell()
{
    if (m_session.running())
        return true;
    if (qEnvironmentVariableIsSet("MOKO_TERMINAL_SMOKE")) {
        setStatusMessage(QStringLiteral("Smoke-test mode"));
        return true;
    }
    const QString shell = qEnvironmentVariable("SHELL", QStringLiteral("/bin/sh"));
    m_session.resize(m_columns, m_rows);
    return m_session.start(shell);
}

void TerminalView::copySelection()
{
    const QString text = selectedText();
    if (text.isEmpty()) {
        setStatusMessage(QStringLiteral("Nothing selected"));
        return;
    }
    QGuiApplication::clipboard()->setText(text, QClipboard::Clipboard);
    setStatusMessage(QStringLiteral("Copied %1 character%2")
                         .arg(text.size())
                         .arg(text.size() == 1 ? QString() : QStringLiteral("s")));
}

void TerminalView::pasteClipboard()
{
    const QString text = QGuiApplication::clipboard()->text(QClipboard::Clipboard);
    if (text.isEmpty())
        return;
    const QByteArray bytes = text.toUtf8();
    vterm_keyboard_start_paste(m_vterm);
    m_session.writeBytes(bytes);
    vterm_keyboard_end_paste(m_vterm);
    clearSelection();
}

void TerminalView::clearSelection()
{
    if (!m_hasSelection)
        return;
    m_hasSelection = false;
    m_selecting = false;
    update();
}

void TerminalView::scrollToBottom()
{
    setScrollOffset(0);
}

void TerminalView::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickPaintedItem::geometryChange(newGeometry, oldGeometry);
    updateGridSize();
}

void TerminalView::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() & Qt::ControlModifier)
        && (event->modifiers() & Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_C) {
            copySelection();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_V) {
            pasteClipboard();
            event->accept();
            return;
        }
    }
    if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_PageUp) {
        setScrollOffset(m_scrollOffset + qMax(1, m_rows - 2));
        event->accept();
        return;
    }
    if ((event->modifiers() & Qt::ShiftModifier) && event->key() == Qt::Key_PageDown) {
        setScrollOffset(m_scrollOffset - qMax(1, m_rows - 2));
        event->accept();
        return;
    }

    const VTermModifier modifiers = modifiersFor(event);
    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        sendKey(VTERM_KEY_ENTER, modifiers);
        break;
    case Qt::Key_Tab:
        sendKey(VTERM_KEY_TAB, modifiers);
        break;
    case Qt::Key_Backspace:
        sendKey(VTERM_KEY_BACKSPACE, modifiers);
        break;
    case Qt::Key_Escape:
        sendKey(VTERM_KEY_ESCAPE, modifiers);
        break;
    case Qt::Key_Up:
        sendKey(VTERM_KEY_UP, modifiers);
        break;
    case Qt::Key_Down:
        sendKey(VTERM_KEY_DOWN, modifiers);
        break;
    case Qt::Key_Left:
        sendKey(VTERM_KEY_LEFT, modifiers);
        break;
    case Qt::Key_Right:
        sendKey(VTERM_KEY_RIGHT, modifiers);
        break;
    case Qt::Key_Insert:
        sendKey(VTERM_KEY_INS, modifiers);
        break;
    case Qt::Key_Delete:
        sendKey(VTERM_KEY_DEL, modifiers);
        break;
    case Qt::Key_Home:
        sendKey(VTERM_KEY_HOME, modifiers);
        break;
    case Qt::Key_End:
        sendKey(VTERM_KEY_END, modifiers);
        break;
    case Qt::Key_PageUp:
        sendKey(VTERM_KEY_PAGEUP, modifiers);
        break;
    case Qt::Key_PageDown:
        sendKey(VTERM_KEY_PAGEDOWN, modifiers);
        break;
    default:
        if (event->key() >= Qt::Key_F1 && event->key() <= Qt::Key_F35) {
            sendKey(static_cast<VTermKey>(VTERM_KEY_FUNCTION(event->key() - Qt::Key_F1 + 1)),
                    modifiers);
        } else if (!event->text().isEmpty()) {
            const QList<uint> characters = event->text().toUcs4();
            for (const uint character : characters)
                vterm_keyboard_unichar(m_vterm, character, modifiers);
        } else {
            QQuickPaintedItem::keyPressEvent(event);
            return;
        }
    }
    clearSelection();
    setScrollOffset(0);
    event->accept();
}

void TerminalView::mousePressEvent(QMouseEvent *event)
{
    forceActiveFocus();
    if (event->button() == Qt::LeftButton) {
        m_selectionAnchor = cellAt(event->position());
        m_selectionCurrent = m_selectionAnchor;
        m_selecting = true;
        m_hasSelection = false;
        update();
        event->accept();
        return;
    }
    QQuickPaintedItem::mousePressEvent(event);
}

void TerminalView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_selecting) {
        m_selectionCurrent = cellAt(event->position());
        m_hasSelection = m_selectionCurrent != m_selectionAnchor;
        update();
        event->accept();
        return;
    }
    QQuickPaintedItem::mouseMoveEvent(event);
}

void TerminalView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_selecting && event->button() == Qt::LeftButton) {
        m_selectionCurrent = cellAt(event->position());
        m_hasSelection = m_selectionCurrent != m_selectionAnchor;
        m_selecting = false;
        update();
        event->accept();
        return;
    }
    QQuickPaintedItem::mouseReleaseEvent(event);
}

void TerminalView::wheelEvent(QWheelEvent *event)
{
    const int steps = event->angleDelta().y() / 120;
    if (steps != 0) {
        setScrollOffset(m_scrollOffset + steps * 3);
        clearSelection();
        event->accept();
        return;
    }
    QQuickPaintedItem::wheelEvent(event);
}

void TerminalView::focusInEvent(QFocusEvent *event)
{
    vterm_state_focus_in(vterm_obtain_state(m_vterm));
    update();
    QQuickPaintedItem::focusInEvent(event);
}

void TerminalView::focusOutEvent(QFocusEvent *event)
{
    vterm_state_focus_out(vterm_obtain_state(m_vterm));
    update();
    QQuickPaintedItem::focusOutEvent(event);
}

void TerminalView::outputCallback(const char *bytes, size_t length, void *user)
{
    static_cast<TerminalView *>(user)->m_session.writeBytes(
        QByteArray(bytes, static_cast<qsizetype>(length)));
}

int TerminalView::damageCallback(VTermRect, void *user)
{
    static_cast<TerminalView *>(user)->update();
    return 1;
}

int TerminalView::moveCursorCallback(VTermPos position, VTermPos, int visible, void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    view->m_cursor = position;
    view->m_cursorVisible = visible != 0;
    view->update();
    return 1;
}

int TerminalView::termPropertyCallback(VTermProp property, VTermValue *value, void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    if (property == VTERM_PROP_TITLE) {
        if (value->string.initial)
            view->m_pendingTitle.clear();
        view->m_pendingTitle.append(QString::fromUtf8(value->string.str,
                                                      static_cast<qsizetype>(value->string.len)));
        if (value->string.final)
            view->setTerminalTitle(view->m_pendingTitle);
    } else if (property == VTERM_PROP_CURSORVISIBLE) {
        view->m_cursorVisible = value->boolean != 0;
        view->update();
    }
    return 1;
}

int TerminalView::bellCallback(void *user)
{
    static_cast<TerminalView *>(user)->setStatusMessage(QStringLiteral("Bell"));
    return 1;
}

int TerminalView::resizeCallback(int rows, int columns, void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    view->m_rows = rows;
    view->m_columns = columns;
    view->update();
    return 1;
}

int TerminalView::scrollbackPushCallback(int columns,
                                         const VTermScreenCell *cells,
                                         void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    Line line;
    line.reserve(columns);
    for (int column = 0; column < columns; ++column)
        line.append(view->convertCell(cells[column]));
    view->m_scrollback.append(std::move(line));
    while (view->m_scrollback.size() > view->m_maxScrollback)
        view->m_scrollback.removeFirst();
    if (view->m_scrollOffset > 0)
        view->setScrollOffset(view->m_scrollOffset + 1);
    emit view->scrollbackChanged();
    return 1;
}

int TerminalView::scrollbackPopCallback(int columns, VTermScreenCell *cells, void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    if (view->m_scrollback.isEmpty())
        return 0;
    const Line line = view->m_scrollback.takeLast();
    for (int column = 0; column < columns; ++column) {
        VTermScreenCell &target = cells[column];
        target = {};
        if (column >= line.size())
            continue;
        const Cell &source = line.at(column);
        const QList<uint> chars = source.text.toUcs4();
        for (int index = 0; index < std::min<int>(chars.size(), VTERM_MAX_CHARS_PER_CELL - 1); ++index)
            target.chars[index] = chars.at(index);
        target.width = static_cast<char>(source.width);
        target.attrs.bold = source.bold;
        target.attrs.underline = source.underline ? VTERM_UNDERLINE_SINGLE : VTERM_UNDERLINE_OFF;
        target.attrs.italic = source.italic;
        vterm_color_rgb(&target.fg, source.foreground.red(), source.foreground.green(), source.foreground.blue());
        vterm_color_rgb(&target.bg, source.background.red(), source.background.green(), source.background.blue());
    }
    emit view->scrollbackChanged();
    return 1;
}

int TerminalView::scrollbackClearCallback(void *user)
{
    auto *view = static_cast<TerminalView *>(user);
    view->m_scrollback.clear();
    view->setScrollOffset(0);
    emit view->scrollbackChanged();
    return 1;
}

TerminalView::Cell TerminalView::convertCell(const VTermScreenCell &source) const
{
    Cell cell;
    cell.text = cellCharacters(source.chars);
    cell.bold = source.attrs.bold;
    cell.underline = source.attrs.underline != VTERM_UNDERLINE_OFF;
    cell.italic = source.attrs.italic;
    cell.width = source.width;
    VTermColor foreground = source.fg;
    VTermColor background = source.bg;
    vterm_screen_convert_color_to_rgb(m_screen, &foreground);
    vterm_screen_convert_color_to_rgb(m_screen, &background);
    cell.foreground = QColor(foreground.rgb.red, foreground.rgb.green, foreground.rgb.blue);
    cell.background = QColor(background.rgb.red, background.rgb.green, background.rgb.blue);
    if (source.attrs.reverse)
        std::swap(cell.foreground, cell.background);
    if (source.attrs.conceal)
        cell.foreground = cell.background;
    return cell;
}

TerminalView::Line TerminalView::screenLine(int row) const
{
    Line line;
    line.reserve(m_columns);
    for (int column = 0; column < m_columns; ++column) {
        VTermScreenCell cell {};
        if (vterm_screen_get_cell(m_screen, VTermPos{row, column}, &cell))
            line.append(convertCell(cell));
        else
            line.append(Cell{});
    }
    return line;
}

TerminalView::Line TerminalView::visibleLine(int displayRow) const
{
    const int totalLines = m_scrollback.size() + m_rows;
    const int firstLine = qMax(0, totalLines - m_rows - m_scrollOffset);
    const int lineIndex = firstLine + displayRow;
    if (lineIndex < m_scrollback.size())
        return m_scrollback.at(lineIndex);
    return screenLine(lineIndex - m_scrollback.size());
}

QString TerminalView::lineText(const Line &line, int startColumn, int endColumn) const
{
    if (endColumn < 0)
        endColumn = line.size() - 1;
    QString text;
    for (int column = qMax(0, startColumn); column <= endColumn && column < line.size(); ++column) {
        if (line.at(column).width != 0)
            text.append(line.at(column).text);
    }
    while (text.endsWith(u' '))
        text.chop(1);
    return text;
}

QString TerminalView::selectedText() const
{
    if (!m_hasSelection)
        return {};
    QPoint start = m_selectionAnchor;
    QPoint end = m_selectionCurrent;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x()))
        std::swap(start, end);
    QStringList lines;
    for (int row = start.y(); row <= end.y(); ++row) {
        const Line line = visibleLine(row);
        lines.append(lineText(line,
                              row == start.y() ? start.x() : 0,
                              row == end.y() ? end.x() : m_columns - 1));
    }
    return lines.join(u'\n');
}

QPoint TerminalView::cellAt(const QPointF &position) const
{
    const int column = qBound(0,
                              static_cast<int>((position.x() - m_padding) / m_cellWidth),
                              m_columns - 1);
    const int row = qBound(0,
                           static_cast<int>((position.y() - m_padding) / m_cellHeight),
                           m_rows - 1);
    return {column, row};
}

bool TerminalView::cellSelected(int column, int displayRow) const
{
    if (!m_hasSelection)
        return false;
    QPoint start = m_selectionAnchor;
    QPoint end = m_selectionCurrent;
    if (start.y() > end.y() || (start.y() == end.y() && start.x() > end.x()))
        std::swap(start, end);
    if (displayRow < start.y() || displayRow > end.y())
        return false;
    if (start.y() == end.y())
        return column >= start.x() && column <= end.x();
    if (displayRow == start.y())
        return column >= start.x();
    if (displayRow == end.y())
        return column <= end.x();
    return true;
}

void TerminalView::setTerminalTitle(const QString &title)
{
    const QString value = title.trimmed().isEmpty() ? QStringLiteral("MOKO Terminal") : title.trimmed();
    if (m_terminalTitle == value)
        return;
    m_terminalTitle = value;
    emit terminalTitleChanged();
}

void TerminalView::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

void TerminalView::updateGridSize()
{
    const QFontMetricsF metrics(m_font);
    m_cellWidth = qMax<qreal>(1, metrics.horizontalAdvance(QLatin1Char('M')));
    m_cellHeight = qMax<qreal>(1, qCeil(metrics.height() + 2));
    const int columns = qMax(2, static_cast<int>((width() - 2 * m_padding) / m_cellWidth));
    const int rows = qMax(2, static_cast<int>((height() - 2 * m_padding) / m_cellHeight));
    if (columns == m_columns && rows == m_rows)
        return;
    m_columns = columns;
    m_rows = rows;
    vterm_set_size(m_vterm, rows, columns);
    m_session.resize(columns, rows);
    setScrollOffset(m_scrollOffset);
    update();
}

void TerminalView::setScrollOffset(int offset)
{
    const int bounded = qBound(0, offset, m_scrollback.size());
    if (m_scrollOffset == bounded)
        return;
    m_scrollOffset = bounded;
    emit scrollOffsetChanged();
    update();
}

void TerminalView::sendKey(VTermKey key, VTermModifier modifiers)
{
    vterm_keyboard_key(m_vterm, key, modifiers);
}

VTermModifier TerminalView::modifiersFor(const QKeyEvent *event) const
{
    int modifiers = VTERM_MOD_NONE;
    if (event->modifiers() & Qt::ShiftModifier)
        modifiers |= VTERM_MOD_SHIFT;
    if (event->modifiers() & Qt::ControlModifier)
        modifiers |= VTERM_MOD_CTRL;
    if (event->modifiers() & Qt::AltModifier)
        modifiers |= VTERM_MOD_ALT;
    return static_cast<VTermModifier>(modifiers);
}
