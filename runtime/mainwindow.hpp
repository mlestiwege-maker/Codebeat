#pragma once

#if defined(CODEBEAT_HAS_QT) && __has_include(<QMainWindow>)
#include <QMainWindow>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "runtime/memory.hpp"
#include "runtime/planner.hpp"
#include "runtime/tools.hpp"

#include <vector>

QT_BEGIN_NAMESPACE
class QTextEdit;
class QLineEdit;
class QPushButton;
class QProcess;
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
    void ensureKnowledgeLoaded();
    QString retrieveKnowledgeReply(const QString& query) const;
    QString learnKnowledgeFact(const QString& rawFact);
    static QString normalizeKnowledgeFact(const QString& text);
    static QStringList splitKeywords(const QString& text);
    void runQuickAction(const QString& text);
    QString tryHandleSystemTask(const QString& text, bool& handled);
    void startVoiceCapture();
    void finalizeVoiceCapture(class QProcess* proc, int exitCode, QProcess::ExitStatus exitStatus);
    QString captureVoiceCommand();
    QString voiceDiagnostics() const;
    static QString normalizeVoiceTranscript(const QString& text);
    static bool launchAny(const QStringList& executables, const QStringList& args = {});
    QString generateAssistantReply(const QString& text);
    bool isCommandAllowed(const QString& cmd, bool& needsConfirm) const;
    QString loadSafetyPolicy();
    bool requiresConfirmation(const QString& cmd) const;

    QTextEdit* chatView_{nullptr};
    QLineEdit* input_{nullptr};
    QPushButton* voiceButton_{nullptr};
    bool voiceCaptureInProgress_{false};
    QProcess* activeVoiceProcess_{nullptr};
    std::vector<QString> notes_{};
    std::vector<QString> knowledge_corpus_{};
    bool knowledge_loaded_{false};
    bool concise_mode_{false};
    QString last_user_message_{};
    int interaction_count_{0};
    codebeat::runtime::ConversationMemory memory_{};
    codebeat::runtime::Planner planner_{};
    codebeat::runtime::Tools tools_{};
    QStringList safety_allowlist_{};
    QStringList safety_denylist_{};
    bool safety_mode_enabled_{true};
    QString pending_confirmation_cmd_{};
    bool require_confirm_for_run_{false};
    bool require_confirm_for_execute_{false};
};
#else
class MainWindow final {
public:
    explicit MainWindow(void* parent = nullptr) noexcept;
    void show() noexcept {}
};
#endif
