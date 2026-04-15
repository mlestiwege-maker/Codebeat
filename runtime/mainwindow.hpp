#pragma once

#if defined(CODEBEAT_HAS_QT) && __has_include(<QMainWindow>)
#include <QMainWindow>
#include <QString>

#include "runtime/memory.hpp"
#include "runtime/tools.hpp"

#include <vector>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLineEdit;
QT_END_NAMESPACE

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

signals:
    void lockRequested();

private slots:
    void onSend();

private:
    void runQuickAction(const QString& text);
    QString tryHandleSystemTask(const QString& text, bool& handled);
    QString captureVoiceCommand();
    QString voiceDiagnostics() const;
    static bool launchAny(const QStringList& executables, const QStringList& args = {});
    QString generateAssistantReply(const QString& text);

    QTextEdit* chatView_{nullptr};
    QLineEdit* input_{nullptr};
    std::vector<QString> notes_{};
    QString last_user_message_{};
    int interaction_count_{0};
    codebeat::runtime::ConversationMemory memory_{};
    codebeat::runtime::Tools tools_{};
};
#else
class MainWindow final {
public:
    explicit MainWindow(void* parent = nullptr) noexcept;
    void show() noexcept {}
};
#endif
