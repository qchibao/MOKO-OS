#pragma once

#include "ptysession.h"

#include <QColor>
#include <QFont>
#include <QPoint>
#include <QQuickPaintedItem>
#include <QVector>

#include <vterm.h>

class TerminalView : public QQuickPaintedItem
{
    Q_OBJECT
    Q_PROPERTY(QString terminalTitle READ terminalTitle NOTIFY terminalTitleChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int scrollOffset READ scrollOffset NOTIFY scrollOffsetChanged)
    Q_PROPERTY(int scrollbackLines READ scrollbackLines NOTIFY scrollbackChanged)

public:
    explicit TerminalView(QQuickItem *parent = nullptr);
    ~TerminalView() override;

    void paint(QPainter *painter) override;
    QString terminalTitle() const;
    QString statusMessage() const;
    bool running() const;
    int scrollOffset() const;
    int scrollbackLines() const;

    Q_INVOKABLE bool startShell();
    Q_INVOKABLE void copySelection();
    Q_INVOKABLE void pasteClipboard();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void scrollToBottom();

signals:
    void terminalTitleChanged();
    void statusMessageChanged();
    void runningChanged();
    void scrollOffsetChanged();
    void scrollbackChanged();
    void shellStarted(qint64 processId);

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    struct Cell {
        QString text;
        QColor foreground;
        QColor background;
        bool bold = false;
        bool underline = false;
        bool italic = false;
        int width = 1;
    };
    using Line = QVector<Cell>;

    static void outputCallback(const char *bytes, size_t length, void *user);
    static int damageCallback(VTermRect rect, void *user);
    static int moveCursorCallback(VTermPos position, VTermPos oldPosition, int visible, void *user);
    static int termPropertyCallback(VTermProp property, VTermValue *value, void *user);
    static int bellCallback(void *user);
    static int resizeCallback(int rows, int columns, void *user);
    static int scrollbackPushCallback(int columns, const VTermScreenCell *cells, void *user);
    static int scrollbackPopCallback(int columns, VTermScreenCell *cells, void *user);
    static int scrollbackClearCallback(void *user);

    Cell convertCell(const VTermScreenCell &cell) const;
    Line screenLine(int row) const;
    Line visibleLine(int displayRow) const;
    QString lineText(const Line &line, int startColumn = 0, int endColumn = -1) const;
    QString selectedText() const;
    QPoint cellAt(const QPointF &position) const;
    bool cellSelected(int column, int displayRow) const;
    void setTerminalTitle(const QString &title);
    void setStatusMessage(const QString &message);
    void updateGridSize();
    void setScrollOffset(int offset);
    void sendKey(VTermKey key, VTermModifier modifiers);
    VTermModifier modifiersFor(const QKeyEvent *event) const;

    PtySession m_session;
    VTerm *m_vterm = nullptr;
    VTermScreen *m_screen = nullptr;
    QFont m_font;
    QVector<Line> m_scrollback;
    int m_maxScrollback = 5000;
    int m_columns = 80;
    int m_rows = 24;
    int m_scrollOffset = 0;
    qreal m_cellWidth = 9;
    qreal m_cellHeight = 18;
    qreal m_padding = 14;
    VTermPos m_cursor{0, 0};
    bool m_cursorVisible = true;
    bool m_selecting = false;
    bool m_hasSelection = false;
    QPoint m_selectionAnchor;
    QPoint m_selectionCurrent;
    QString m_terminalTitle = QStringLiteral("MOKO Terminal");
    QString m_statusMessage = QStringLiteral("Starting shell...");
    QString m_pendingTitle;
};
