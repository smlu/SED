#include "main_window/common.hpp"

#include "app_logging.hpp"

#include <QDebug>

namespace sed
{

namespace
{

constexpr int kMessageSeverityRole = Qt::UserRole + 31;
constexpr std::array<const char*, 7> kMessageSeverityLabels{
    "Verbose", "Trace", "Debug", "Info", "Warning", "Error", "Fatal"
};
constexpr std::array<const char*, 7> kMessageSeveritySettingNames{
    "verbose", "trace", "debug", "info", "warning", "error", "fatal"
};

QString messageSeveritySettingKey(const int severityIndex)
{
    if (severityIndex < 0
        || severityIndex >= static_cast<int>(kMessageSeveritySettingNames.size()))
    {
        return {};
    }
    return QStringLiteral("messages/severity_visible/%1")
        .arg(QString::fromLatin1(kMessageSeveritySettingNames[severityIndex]));
}

} // namespace

void MainWindow::copySelectedMessagesToClipboard()
{
    if (!messages_)
    {
        return;
    }

    QList<QTreeWidgetItem*> selection = messages_->selectedItems();
    if (selection.isEmpty() && messages_->currentItem())
    {
        selection.push_back(messages_->currentItem());
    }

    QStringList lines{};
    lines.reserve(selection.size());
    for (const QTreeWidgetItem* item : selection)
    {
        if (!item)
        {
            continue;
        }

        const QString severity = item->text(0);
        const QString msg = item->text(1);
        if (!severity.isEmpty())
        {
            lines.push_back(QString("%1\t%2").arg(severity, msg));
        }
        else
        {
            lines.push_back(msg);
        }
    }

    if (lines.isEmpty())
    {
        return;
    }

    if (QClipboard* clipboard = QApplication::clipboard())
    {
        clipboard->setText(lines.join("\n"));
    }
}

void MainWindow::showMessageDetails(const QTreeWidgetItem* item)
{
    if (!item)
    {
        return;
    }

    auto* dialog = new QDialog(this);
    dialog->setWindowTitle("Message");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(900, 320);

    auto* layout = new QVBoxLayout(dialog);

    const QString severity = item->text(0);
    const QString message = item->text(1);

    if (!severity.isEmpty())
    {
        auto* header = new QLabel(QString("Severity: %1").arg(severity), dialog);
        layout->addWidget(header);
    }

    auto* edit = new QPlainTextEdit(dialog);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    edit->setPlainText(message);
    layout->addWidget(edit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);

    dialog->open();
}

void MainWindow::logMessage(
    const MessageSeverity severity,
    const QString& message,
    const MessageLogCategory category)
{
    if (category == MessageLogCategory::resources)
    {
        switch (severity)
        {
            case MessageSeverity::verbose:
            case MessageSeverity::trace:
            case MessageSeverity::debug:
                qCDebug(logResources).noquote() << message;
                break;
            case MessageSeverity::info:
                qCInfo(logResources).noquote() << message;
                break;
            case MessageSeverity::warning:
                qCWarning(logResources).noquote() << message;
                break;
            case MessageSeverity::error:
                qCCritical(logResources).noquote() << message;
                break;
            case MessageSeverity::fatal:
                break;
        }
    }
    else
    {
        switch (severity)
        {
            case MessageSeverity::verbose:
            case MessageSeverity::trace:
            case MessageSeverity::debug:
                qCDebug(logApplication).noquote() << message;
                break;
            case MessageSeverity::info:
                qCInfo(logApplication).noquote() << message;
                break;
            case MessageSeverity::warning:
                qCWarning(logApplication).noquote() << message;
                break;
            case MessageSeverity::error:
                qCCritical(logApplication).noquote() << message;
                break;
            case MessageSeverity::fatal:
                break;
        }
    }

    if (!messages_)
    {
        if (severity == MessageSeverity::fatal)
        {
            qFatal("%s", message.toUtf8().constData());
        }
        return;
    }

    auto* item = new QTreeWidgetItem(messages_);
    switch (severity)
    {
        case MessageSeverity::verbose:
        {
            item->setText(0, "Verbose");
            item->setForeground(0, QBrush(QColor(100, 100, 100)));
            break;
        }
        case MessageSeverity::trace:
        {
            item->setText(0, "Trace");
            item->setForeground(0, QBrush(QColor(120, 120, 120)));
            break;
        }
        case MessageSeverity::debug:
        {
            item->setText(0, "Debug");
            item->setForeground(0, QBrush(QColor(150, 150, 150)));
            break;
        }
        case MessageSeverity::info:
        {
            item->setText(0, "Info");
            item->setForeground(0, QBrush(QColor(180, 200, 255)));
            break;
        }
        case MessageSeverity::warning:
        {
            item->setText(0, "Warning");
            item->setForeground(0, QBrush(QColor(255, 210, 120)));
            break;
        }
        case MessageSeverity::error:
        {
            item->setText(0, "Error");
            item->setForeground(0, QBrush(QColor(255, 120, 120)));
            break;
        }
        case MessageSeverity::fatal:
        {
            item->setText(0, "Fatal");
            item->setForeground(0, QBrush(QColor(255, 80, 80)));
            break;
        }
    }

    item->setData(0, kMessageSeverityRole, static_cast<int>(severity));
    item->setText(1, message);
    item->setHidden(!messageSeverityVisible(severity));
    if (!item->isHidden())
    {
        messages_->scrollToBottom();
    }

    if (severity == MessageSeverity::fatal)
    {
        qFatal("%s", message.toUtf8().constData());
    }
}

bool MainWindow::messageSeverityVisible(const MessageSeverity severity) const
{
    const QString key = messageSeveritySettingKey(static_cast<int>(severity));
    if (key.isEmpty())
    {
        return true;
    }
    return settingsStore().value(key, true).toBool();
}

void MainWindow::refreshMessageSeverityVisibility()
{
    if (!messages_)
    {
        return;
    }

    for (int row = 0; row < messages_->topLevelItemCount(); ++row)
    {
        QTreeWidgetItem* item = messages_->topLevelItem(row);
        if (!item)
        {
            continue;
        }
        const QVariant storedSeverity = item->data(0, kMessageSeverityRole);
        if (!storedSeverity.isValid())
        {
            continue;
        }
        const int severityIndex = storedSeverity.toInt();
        if (severityIndex < 0
            || severityIndex >= static_cast<int>(kMessageSeverityLabels.size()))
        {
            continue;
        }
        item->setHidden(!messageSeverityVisible(
            static_cast<MessageSeverity>(severityIndex)));
    }
}

void MainWindow::showMessageSeverityPopup(QWidget* anchor)
{
    if (!anchor)
    {
        return;
    }

    if (QWidget* existing = findChild<QWidget*>(
            QStringLiteral("messagesSeverityPopup"), Qt::FindDirectChildrenOnly))
    {
        existing->close();
    }

    auto* popup = new QFrame(this, Qt::Popup | Qt::FramelessWindowHint);
    popup->setObjectName(QStringLiteral("messagesSeverityPopup"));
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::StyledPanel);
    popup->setStyleSheet(QStringLiteral(R"(
QFrame#messagesSeverityPopup {
    background-color: #2b2d31;
    border: 1px solid #545861;
    border-radius: 4px;
}
QLabel#messagesSeverityPopupTitle {
    color: #d9dce2;
    padding: 2px 3px 4px 3px;
}
QToolButton {
    background-color: #383b41;
    border: 1px solid #50545c;
    border-radius: 3px;
    color: #8f949d;
    min-width: 112px;
    padding: 5px 9px;
    text-align: left;
}
QToolButton:hover {
    background-color: #444850;
    color: #dfe2e8;
}
QToolButton:checked {
    background-color: #516c88;
    border-color: #7896b4;
    color: #ffffff;
}
)"));

    auto* popupLayout = new QVBoxLayout(popup);
    popupLayout->setContentsMargins(7, 7, 7, 7);
    popupLayout->setSpacing(4);
    auto* title = new QLabel(QStringLiteral("Message severities"), popup);
    title->setObjectName(QStringLiteral("messagesSeverityPopupTitle"));
    popupLayout->addWidget(title);

    for (int severityIndex = 0;
         severityIndex < static_cast<int>(kMessageSeverityLabels.size());
         ++severityIndex)
    {
        const MessageSeverity severity = static_cast<MessageSeverity>(severityIndex);
        auto* button = new QToolButton(popup);
        button->setObjectName(QStringLiteral("messagesSeverityButton.%1")
            .arg(QString::fromLatin1(kMessageSeveritySettingNames[severityIndex])));
        button->setText(QString::fromLatin1(kMessageSeverityLabels[severityIndex]));
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setCheckable(true);
        button->setChecked(messageSeverityVisible(severity));
        button->setAccessibleName(QStringLiteral("Show %1 messages")
            .arg(QString::fromLatin1(kMessageSeverityLabels[severityIndex])));
        button->setToolTip(QStringLiteral("Show or hide %1 messages")
            .arg(QString::fromLatin1(kMessageSeverityLabels[severityIndex]).toLower()));
        connect(button, &QToolButton::toggled, this,
            [this, severityIndex](const bool visible)
            {
                QSettings settings = settingsStore();
                settings.setValue(messageSeveritySettingKey(severityIndex), visible);
                settings.sync();
                refreshMessageSeverityVisibility();
            });
        popupLayout->addWidget(button);
    }

    popup->adjustSize();
    const QPoint anchorBottomRight = anchor->mapToGlobal(
        QPoint(anchor->width(), anchor->height()));
    popup->move(anchorBottomRight.x() - popup->width(), anchorBottomRight.y());
    popup->show();
}

} // namespace sed
