#pragma once

#if defined(CODEBEAT_HAS_QT) && __has_include(<QMainWindow>)
#include <QMainWindow>
#include <QProcess>
#include <QJsonArray>
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
class QTimer;
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
    QString inferIntentCommand(const QString& text) const;
    bool isWakePhrase(const QString& text) const;
    QString stripWakePrefix(const QString& text) const;
    static bool isInterruptCommand(const QString& text);
    void handleStandbyTranscript(const QString& rawText);
    void scheduleNextStandbyCapture(int delayMs = 250);
    void setStandbyListening(bool enabled);
    void submitVoiceCommand(const QString& command, const QString& rawHeard, bool announceHeard);
    void ensureKnowledgeLoaded();
    void ensurePluginCommandsLoaded();
    QString retrieveKnowledgeReply(const QString& query) const;
    QString learnKnowledgeFact(const QString& rawFact);
    QString pluginStatus() const;
    QString tryHandlePluginCommand(const QString& text, bool& handled);
    static QString normalizeKnowledgeFact(const QString& text);
    static QStringList splitKeywords(const QString& text);
    void runQuickAction(const QString& text);
    QString tryHandleSystemTask(const QString& text, bool& handled);
    void startVoiceCapture();
    void finalizeVoiceCapture(class QProcess* proc, int exitCode, QProcess::ExitStatus exitStatus);
    QString captureVoiceCommand();
    void speakText(const QString& text);
    QString voiceDiagnostics() const;
    QString voiceAuditStatus() const;
    QString voiceAuditSummary() const;
    QString voiceIdentityStatus() const;
    QString evaluateCurrentVoiceIdentity();
    QString ollamaStatus() const;
    QString ollamaReply(const QString& prompt) const;
    void appendVoiceAuditLog(const QString& command, const QString& outcome) const;
    void requestVoiceRoleBadgeRefresh();
    QString enrollOwnerVoiceProfile();
    QString enrollTrustedVoiceProfile();
    void updateVoiceRoleBadge();
    void updateVoiceModeBadge();
    void updateHeardQueueBadge();
    void updateAutoRefreshBadge();
    void refreshUiNow();
    QString normalizeVoiceTranscript(const QString& text) const;
    static bool launchAny(const QStringList& executables, const QStringList& args = {});
    QString generateAssistantReply(const QString& text);
    bool isCommandAllowed(const QString& cmd, bool& needsConfirm) const;
    QString loadSafetyPolicy();
    bool requiresConfirmation(const QString& cmd) const;
    bool isCriticalSystemCommand(const QString& cmd) const;
    bool isCriticalAuthActive() const;
    bool verifyFaceForCriticalAuth(QString& reason) const;

    QTextEdit* chatView_{nullptr};
    QLineEdit* input_{nullptr};
    QPushButton* voiceButton_{nullptr};
    class QLabel* voiceRoleBadge_{nullptr};
    class QLabel* voiceModeBadge_{nullptr};
    class QLabel* heardQueueBadge_{nullptr};
    class QLabel* autoRefreshBadge_{nullptr};
    bool voiceCaptureInProgress_{false};
    QProcess* activeVoiceProcess_{nullptr};
    QProcess* activeSpeechProcess_{nullptr};
    std::vector<QString> notes_{};
    std::vector<QString> knowledge_corpus_{};
    bool knowledge_loaded_{false};
    bool plugins_loaded_{false};
    QString plugin_commands_path_{};
    QJsonArray plugin_commands_{};
    bool concise_mode_{false};
    QString last_user_message_{};
    int interaction_count_{0};
    codebeat::runtime::ConversationMemory memory_{};
    codebeat::runtime::Planner planner_{};
    codebeat::runtime::Tools tools_{};
    QStringList safety_allowlist_{};
    QStringList safety_denylist_{};
    QStringList wake_aliases_{};
    bool safety_mode_enabled_{true};
    QString pending_confirmation_cmd_{};
    QString pending_critical_cmd_{};
    bool require_confirm_for_run_{false};
    bool require_confirm_for_execute_{false};
    bool require_auth_for_critical_{true};
    QString critical_passkey_{};
    int critical_auth_ttl_secs_{120};
    qint64 critical_auth_until_epoch_secs_{0};
    bool standby_listening_enabled_{false};
    bool standby_arm_command_window_{false};
    int wake_command_window_secs_{8};
    qint64 wake_command_until_epoch_secs_{0};
    bool voice_owner_mode_enabled_{true};
    bool voice_owner_profile_required_for_critical_{true};
    QString voice_owner_profile_path_{};
    QString voice_trusted_profile_path_{};
    bool ollama_enabled_{false};
    QString ollama_base_url_{"http://localhost:11434"};
    QString ollama_model_{"llama3.2"};
    bool last_voice_is_owner_{false};
    double last_voice_score_{1.0};
    QString last_voice_label_{"unknown"};
    QString last_voice_role_{"unknown"};
    bool current_send_from_voice_{false};
    bool voice_identity_refresh_in_progress_{false};
    bool voice_identity_refresh_pending_{false};
    bool voice_output_enabled_{true};
    QString tts_engine_{"auto"};
    QString tts_voice_{"en-us"};
    QString tts_piper_model_path_{};
    int tts_rate_{170};
    int tts_pitch_{50};
    int tts_piper_speaker_{0};
    int tts_max_chars_{280};
    int tts_chunk_chars_{150};
    int tts_pause_ms_{90};
    bool voice_speaking_in_progress_{false};
    bool auto_refresh_enabled_{false};
    int auto_refresh_interval_ms_{4000};
    QTimer* auto_refresh_timer_{nullptr};
    QStringList heard_queue_{};
};
#else
class MainWindow final {
public:
    explicit MainWindow(void* parent = nullptr) noexcept;
    void show() noexcept {}
};
#endif
