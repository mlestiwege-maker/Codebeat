#include "runtime/mainwindow.hpp"

#if defined(CODEBEAT_HAS_QT) && __has_include(<QWidget>)
#include <QHBoxLayout>
#include <QLineEdit>
#include <QIcon>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTimer>
#include <QRegularExpression>
#include <QFrame>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QProcess>
#include <QStyle>

#include <algorithm>
#include <set>

bool MainWindow::launchAny(const QStringList& executables, const QStringList& args) {
    for (const auto& exe : executables) {
        if (QProcess::startDetached(exe, args)) {
            return true;
        }
    }
    return false;
}

QString MainWindow::normalizeKnowledgeFact(const QString& text) {
    QString out = text;
    out.replace(QRegularExpression("[\r\n]+"), " ");
    out.replace(QRegularExpression("\\s+"), " ");
    out = out.trimmed();
    if (out.endsWith(".")) {
        return out;
    }
    return out + ".";
}

QString MainWindow::learnKnowledgeFact(const QString& rawFact) {
    const auto fact = normalizeKnowledgeFact(rawFact);
    if (fact.isEmpty() || fact == ".") {
        return "Usage: learn: <fact>. Example: learn: Codebeat should ask clarifying questions only when needed.";
    }

    for (const auto& existing : knowledge_corpus_) {
        if (existing.compare(fact, Qt::CaseInsensitive) == 0) {
            return "I already know that fact. No duplicate added.";
        }
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir root(appDir);
    root.cdUp();

    const QString knowledgeDir = root.absoluteFilePath("data/raw/knowledge");
    QDir().mkpath(knowledgeDir);

    const QString learnFile = root.absoluteFilePath("data/raw/knowledge/user_facts.txt");
    const QString corpusFile = root.absoluteFilePath("data/raw/corpus.txt");

    auto appendLine = [&fact](const QString& path) -> bool {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            return false;
        }
        QTextStream out(&f);
        out << fact << "\n";
        return true;
    };

    if (!appendLine(learnFile)) {
        return "I couldn't save that fact to knowledge storage.";
    }
    if (!appendLine(corpusFile)) {
        return "I saved the fact to knowledge file, but updating corpus.txt failed.";
    }

    knowledge_corpus_.push_back(fact);
    return "Learned and saved: \"" + fact + "\"";
}

QStringList MainWindow::splitKeywords(const QString& text) {
    QString cleaned = text.toLower();
    cleaned.replace(QRegularExpression("[^a-z0-9\\s]"), " ");
    cleaned.replace(QRegularExpression("\\s+"), " ");
    cleaned = cleaned.trimmed();

    if (cleaned.isEmpty()) {
        return {};
    }

    const QStringList stopwords = {
        "a", "an", "and", "are", "as", "at", "be", "but", "by", "for", "from",
        "how", "i", "in", "is", "it", "me", "my", "of", "on", "or", "that", "the",
        "this", "to", "we", "what", "when", "where", "who", "why", "with", "you", "your",
        "can", "could", "should", "would", "do", "does", "did", "tell", "explain", "about"
    };

    QStringList out;
    for (const auto& tok : cleaned.split(' ', Qt::SkipEmptyParts)) {
        if (tok.size() >= 2 && !stopwords.contains(tok)) {
            out.push_back(tok);
        }
    }
    return out;
}

void MainWindow::ensureKnowledgeLoaded() {
    if (knowledge_loaded_) {
        return;
    }
    knowledge_loaded_ = true;

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp();
    const QString corpusPath = d.absoluteFilePath("data/raw/corpus.txt");

    QFile f(corpusPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }

    QTextStream in(&f);
    while (!in.atEnd()) {
        const auto line = in.readLine().trimmed();
        if (!line.isEmpty()) {
            knowledge_corpus_.push_back(line);
        }
    }
}

QString MainWindow::retrieveKnowledgeReply(const QString& query) const {
    if (knowledge_corpus_.empty()) {
        return {};
    }

    const auto qk = splitKeywords(query);
    if (qk.isEmpty()) {
        return {};
    }

    int bestScore = 0;
    QString bestLine;
    for (const auto& line : knowledge_corpus_) {
        const auto lk = splitKeywords(line);
        if (lk.isEmpty()) {
            continue;
        }

        int score = 0;
        for (const auto& q : qk) {
            if (lk.contains(q)) {
                ++score;
            }
        }

        if (score > bestScore) {
            bestScore = score;
            bestLine = line;
        }
    }

    if (bestScore <= 0 || bestLine.isEmpty()) {
        return {};
    }

    return "From my local knowledge: " + bestLine;
}

QString MainWindow::normalizeVoiceTranscript(const QString& text) const {
    QString t = text.trimmed();
    if (t.isEmpty()) {
        return t;
    }

    t = t.toLower();
    t.replace(QRegularExpression("^[\\s\\p{P}]+|[\\s\\p{P}]+$"), "");
    t.replace(QRegularExpression("^(codebeat|hey codebeat|okay codebeat|ok codebeat|hello codebeat)\\s*[,;:-]*\\s*"), "");
    t.replace(QRegularExpression("^(and then|then|so|please)\\s+"), "");
    t.replace(QRegularExpression("\\s+"), " ");

    const QStringList fillerOnly = {
        "and", "then", "so", "please", "okay", "ok", "uh", "um",
        "and then", "okay then", "ok then"
    };
    if (fillerOnly.contains(t)) {
        return {};
    }

    // Collapse repeated full command phrases from noisy ASR output.
    t.replace(QRegularExpression("^(open terminal[\\s,]*){2,}$"), "open terminal");
    t.replace(QRegularExpression("^(open chrome[\\s,]*){2,}$"), "open chrome");
    t.replace(QRegularExpression("^(open vs code[\\s,]*){2,}$"), "open vs code");
    t.replace(QRegularExpression("^(voice status[\\s,]*){2,}$"), "voice status");
    t.replace(QRegularExpression("^(status[\\s,]*){2,}$"), "status");

    if (t == "what time is it" || t == "whats the time" || t == "tell me the time") {
        return "time";
    }
    if (t == "microphone status" || t == "voice health" || t == "mic health") {
        return "voice status";
    }

    t.replace(QRegularExpression("^open (google )?chrome( browser)?$"), "open chrome");
    t.replace(QRegularExpression("^open (v\\s*s|vs|visual studio) code$"), "open vs code");
    t.replace(QRegularExpression("^open terminal( app)?$"), "open terminal");
    t.replace(QRegularExpression("^search for "), "search ");

    // Intent extraction from noisy/free-form ASR text.
    const auto has = [&t](const QString& needle) { return t.contains(needle); };

    if ((has("voice") || has("mic") || has("microphone")) && (has("status") || has("check") || has("health"))) {
        return "voice status";
    }
    if (has("lock") || has("relock")) {
        return "lock";
    }
    if (has("time") && (has("what") || has("tell") || has("current"))) {
        return "time";
    }

    if (has("terminal")) {
        return "open terminal";
    }
    if (has("chrome") || (has("browser") && has("open"))) {
        return "open chrome";
    }
    if ((has("vs") && has("code")) || has("visual studio code") || has("vscode")) {
        return "open vs code";
    }

    int sidx = t.indexOf("search ");
    if (sidx >= 0) {
        auto q = t.mid(sidx + 7).trimmed();
        q.replace(QRegularExpression("^[\\p{P}\\s]+|[\\p{P}\\s]+$"), "");
        if (!q.isEmpty()) {
            return "search " + q;
        }
    }

    if (has("status")) {
        return "status";
    }

    if (t.startsWith("open terminal")) return "open terminal";
    if (t.startsWith("open chrome")) return "open chrome";
    if (t.startsWith("open vs code")) return "open vs code";
    if (t.startsWith("voice status")) return "voice status";

    return t.trimmed();
}

QString MainWindow::inferIntentCommand(const QString& text) const {
    QString normalized = text.trimmed().toLower();
    if (normalized.isEmpty()) {
        return {};
    }

    normalized.replace(QRegularExpression(R"([\p{Punct}]+)"), " ");
    normalized.replace(QRegularExpression(R"(\s+)"), " ");
    normalized = normalized.trimmed();

    const auto has = [&normalized](const QString& needle) { return normalized.contains(needle); };
    const bool openAction = has("open") || has("launch") || has("start");

    if (has("write code") || has("open vscode") || has("open vs code") ||
        (has("write") && has("code")) || (has("code") && has("editor")) ||
        (has("i want") && has("code"))) {
        return "open vs code";
    }

    if ((has("browser") && openAction) || has("open chrome") ||
        has("browse the web") || has("search the web") || has("internet browser")) {
        return "open chrome";
    }

    {
        QRegularExpression docsRx(R"(^(?:please\s+)?(?:open|show|find|search)\s+(?:the\s+)?(?:docs|documentation)\s+(?:for\s+)?(.+)$)");
        const auto m = docsRx.match(normalized);
        if (m.hasMatch()) {
            const auto topic = m.captured(1).trimmed();
            if (!topic.isEmpty()) {
                return "search " + topic + " documentation";
            }
        }
    }

    {
        QRegularExpression searchRx(R"(^(?:please\s+)?(?:search|google|look up|lookup|find)\s+(?:for\s+)?(.+)$)");
        const auto m = searchRx.match(normalized);
        if (m.hasMatch()) {
            const auto query = m.captured(1).trimmed();
            if (!query.isEmpty()) {
                return "search " + query;
            }
        }
    }

    if ((has("downloads") || has("download folder") || has("downloads folder")) &&
        (has("open") || has("show") || has("launch") || has("browse"))) {
        return "open downloads";
    }

    if ((has("editor") || has("code editor") || has("text editor")) && openAction) {
        return "open vs code";
    }

    if ((has("terminal") || has("shell") || has("command line") || has("console")) &&
        (openAction || normalized == "terminal" || normalized == "open terminal")) {
        return "open terminal";
    }

    if ((has("close") || has("quit") || has("exit")) &&
        (has("browser") || has("chrome") || has("chromium"))) {
        return "close chrome";
    }

    if (has("voice status") || (has("voice") && has("status")) || (has("microphone") && has("status"))) {
        return "voice status";
    }

    if ((has("voice") || has("speaker") || has("speaking")) &&
        (has("role") || has("identity") || has("who"))) {
        return "voice role";
    }

    if ((has("battery") && (has("status") || has("health") || has("check"))) || has("power status")) {
        return "battery status";
    }

    if (has("running processes") || (has("show") && has("process")) || has("task list")) {
        return "show running processes";
    }

    if (has("screenshot") || has("screen capture") || has("capture the screen")) {
        return "take screenshot";
    }

    if (has("refresh") && has("auto") && (has("on") || has("enable"))) {
        return "refresh auto on";
    }
    if (has("refresh") && has("auto") && (has("off") || has("disable"))) {
        return "refresh auto off";
    }
    if ((has("refresh") && has("auto") && has("status")) ||
        (has("is") && has("auto") && has("refresh") && has("on"))) {
        return "refresh auto status";
    }
    if (has("refresh") && (has("ui") || has("app") || has("screen"))) {
        return "refresh app";
    }

    if ((has("volume") && (has("up") || has("increase") || has("louder") || has("raise"))) ||
        has("turn volume up")) {
        return "volume up";
    }
    if ((has("volume") && (has("down") || has("decrease") || has("quieter") || has("lower"))) ||
        has("turn volume down")) {
        return "volume down";
    }
    if ((has("volume") && has("mute")) || has("mute audio") || has("mute sound")) {
        return "mute";
    }
    if ((has("volume") && has("unmute")) || has("unmute audio") || has("unmute sound")) {
        return "unmute";
    }

    if ((has("create") || has("make") || has("new")) && has("folder")) {
        QString folderName = normalized;
        folderName.replace(QRegularExpression(R"(^(please\s+)?(create|make|new)\s+(a\s+)?folder(\s+called|\s+named|\s+for)?\s*)"), "");
        folderName.replace(QRegularExpression(R"(^(please\s+)?(create|make)\s+(a\s+)?new\s+folder(\s+called|\s+named|\s+for)?\s*)"), "");
        folderName = folderName.trimmed();
        if (folderName.isEmpty()) {
            return "create folder";
        }
        return "create folder " + folderName;
    }

    if (has("standby") && (has("on") || has("enable") || has("start"))) {
        return "voice standby on";
    }
    if (has("standby") && (has("off") || has("disable") || has("stop"))) {
        return "voice standby off";
    }
    if (has("standby") && (has("more sensitive") || has("increase sensitivity") || has("sensitivity up"))) {
        return "voice standby sensitivity up";
    }
    if (has("standby") && (has("less sensitive") || has("decrease sensitivity") || has("sensitivity down"))) {
        return "voice standby sensitivity down";
    }

    if (has("voice") && (has("enroll") || has("train")) && has("owner")) {
        return "voice enroll owner";
    }
    if (has("voice") && (has("enroll") || has("train")) && has("trusted")) {
        return "voice enroll trusted";
    }

    return {};
}

QString MainWindow::loadSafetyPolicy() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir root(appDir);
    root.cdUp();
    const QString envFile = root.absoluteFilePath(".env");

    qsizetype enabledPos = -1, confirmRunPos = -1, confirmExecPos = -1;
    QString enabledValue = "1", confirmRunValue = "1", confirmExecValue = "1";

    QFile f(envFile);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const auto line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.isEmpty() || line.startsWith("#")) continue;
            if (line.startsWith("CODEBEAT_SAFETY_MODE=")) {
                enabledValue = line.mid(21).trimmed();
            }
            if (line.startsWith("CODEBEAT_CONFIRM_RUN=")) {
                confirmRunValue = line.mid(21).trimmed();
            }
            if (line.startsWith("CODEBEAT_CONFIRM_EXECUTE=")) {
                confirmExecValue = line.mid(25).trimmed();
            }
        }
        f.close();
    }

    safety_mode_enabled_ = (enabledValue != "0" && enabledValue.toLower() != "false");
    require_confirm_for_run_ = (confirmRunValue != "0" && confirmRunValue.toLower() != "false");
    require_confirm_for_execute_ = (confirmExecValue != "0" && confirmExecValue.toLower() != "false");

    return QString("Safety policy loaded: mode=%1, require_run_confirm=%2, require_exec_confirm=%3")
            .arg(safety_mode_enabled_ ? "ON" : "OFF")
            .arg(require_confirm_for_run_ ? "yes" : "no")
            .arg(require_confirm_for_execute_ ? "yes" : "no");
}

bool MainWindow::requiresConfirmation(const QString& cmd) const {
    if (!safety_mode_enabled_) return false;
    
    const auto lowered = cmd.trimmed().toLower();
    if (lowered.startsWith("run ")) return require_confirm_for_run_;
    if (lowered.startsWith("execute ")) return require_confirm_for_execute_;
    return false;
}

bool MainWindow::isCommandAllowed(const QString& cmd, bool& needsConfirm) const {
    needsConfirm = false;
    if (!safety_mode_enabled_) return true;
    
    needsConfirm = requiresConfirmation(cmd);
    return true;
}

QString MainWindow::voiceDiagnostics() const {
    auto hasCommand = [](const QString& cmd) {
        QProcess proc;
        proc.start("bash", {"-lc", "command -v " + cmd + " >/dev/null 2>&1"});
        if (!proc.waitForFinished(3000)) {
            proc.kill();
            return false;
        }
        return proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    };

    const bool hasArecord = hasCommand("arecord");
    const bool hasPwRecord = hasCommand("pw-record");
    const bool hasParec = hasCommand("parec");
    const bool hasFfmpeg = hasCommand("ffmpeg");
    const bool hasWhisperCli = hasCommand("whisper");

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp(); // from build_gui -> project root
    const QString rootDir = d.absolutePath();

    QString pythonBin;
    const QString venvLocal = rootDir + "/.venv/bin/python";
    const QString venvParent = rootDir + "/../.venv/bin/python";

    if (QFileInfo::exists(venvLocal) && QFileInfo(venvLocal).isExecutable()) {
        pythonBin = venvLocal;
    } else if (QFileInfo::exists(venvParent) && QFileInfo(venvParent).isExecutable()) {
        pythonBin = venvParent;
    } else if (hasCommand("python3")) {
        pythonBin = "python3";
    }

    bool hasPythonWhisper = false;
    if (!pythonBin.isEmpty()) {
        QProcess pyProbe;
        pyProbe.start(pythonBin, {"-c", "import whisper"});
        if (pyProbe.waitForFinished(5000) &&
            pyProbe.exitStatus() == QProcess::NormalExit &&
            pyProbe.exitCode() == 0) {
            hasPythonWhisper = true;
        }
    }

    const QString activeRecorder =
        hasArecord ? "arecord" :
        (hasPwRecord ? "pw-record" :
        (hasParec ? "parec" :
        (hasFfmpeg ? "ffmpeg" : "none")));

    const QString activeAsr =
        hasWhisperCli ? "whisper CLI" :
        (hasPythonWhisper ? "python openai-whisper" : "none");

    QString report;
    report += "Voice diagnostics:";
    report += "\n• Recorder backends: arecord=" + QString(hasArecord ? "yes" : "no") +
              ", pw-record=" + QString(hasPwRecord ? "yes" : "no") +
              ", parec=" + QString(hasParec ? "yes" : "no") +
              ", ffmpeg=" + QString(hasFfmpeg ? "yes" : "no");
    report += "\n• ASR backends: whisper_cli=" + QString(hasWhisperCli ? "yes" : "no") +
              ", python_whisper=" + QString(hasPythonWhisper ? "yes" : "no");
    report += "\n• Active recorder candidate: " + activeRecorder;
    report += "\n• Active ASR candidate: " + activeAsr;

    if (!pythonBin.isEmpty()) {
        report += "\n• Python path: " + pythonBin;
    }

    if (activeRecorder == "none") {
        report += "\n• Fix: install one recorder backend (arecord / pw-record / parec / ffmpeg).";
    }
    if (activeAsr == "none") {
        report += "\n• Fix: install whisper CLI or pip package openai-whisper in your venv.";
    }

    report += "\n• Tip: set CODEBEAT_PULSE_SOURCE to choose a specific mic source.";
    return report;
}

void MainWindow::updateVoiceModeBadge() {
    if (voiceModeBadge_ == nullptr) {
        return;
    }

    QString text;
    QString color;
    QString border;
    QString background;

    if (voiceCaptureInProgress_) {
        text = "🟢 VOICE: LISTENING";
        color = "#9effc9";
        border = "#1d8d5e";
        background = "#10251b";
    } else if (voice_speaking_in_progress_) {
        text = "🔵 VOICE: SPEAKING";
        color = "#8fd8ff";
        border = "#276f99";
        background = "#0f2030";
    } else if (standby_listening_enabled_) {
        text = "🟡 VOICE: STANDBY";
        color = "#ffd36e";
        border = "#8f6a18";
        background = "#272012";
    } else {
        text = "⚪ VOICE: READY";
        color = "#9bb0bf";
        border = "#294456";
        background = "#10161f";
    }

    voiceModeBadge_->setText(text);
    voiceModeBadge_->setStyleSheet(
        "QLabel {"
        "  background-color: " + background + ";"
        "  color: " + color + ";"
        "  border: 1px solid " + border + ";"
        "  border-radius: 7px;"
        "  padding: 6px 10px;"
        "  font-family: 'Monospace';"
        "  font-size: 10px;"
        "  font-weight: 700;"
        "}"
    );
}

void MainWindow::speakText(const QString& text) {
    if (!voice_output_enabled_) {
        return;
    }

    QString speakable = text;
    speakable.replace(QRegularExpression("<[^>]+>"), " ");
    speakable.replace(QRegularExpression("[\r\n]+"), " ");
    speakable.replace(QRegularExpression("\\s+"), " ");
    speakable = speakable.trimmed();

    if (speakable.isEmpty()) {
        return;
    }

    if (speakable.size() > 220) {
        speakable = speakable.left(217).trimmed() + "...";
    }

    if (activeSpeechProcess_ && activeSpeechProcess_->state() == QProcess::Running) {
        activeSpeechProcess_->kill();
    }

    auto* speechProc = new QProcess(this);
    activeSpeechProcess_ = speechProc;
    voice_speaking_in_progress_ = true;
    updateVoiceModeBadge();

    QObject::connect(speechProc, &QProcess::finished, this,
                     [this, speechProc](int, QProcess::ExitStatus) {
                         if (speechProc == activeSpeechProcess_) {
                            activeSpeechProcess_ = nullptr;
                            voice_speaking_in_progress_ = false;
                            updateVoiceModeBadge();
                         }
                         speechProc->deleteLater();
                     });

    QObject::connect(speechProc, &QProcess::errorOccurred, this,
                     [this, speechProc](QProcess::ProcessError) {
                         if (speechProc == activeSpeechProcess_) {
                            activeSpeechProcess_ = nullptr;
                            voice_speaking_in_progress_ = false;
                            updateVoiceModeBadge();
                         }
                         speechProc->deleteLater();
                     });

    speechProc->start("spd-say", {"--rate=170", speakable});
    if (!speechProc->waitForStarted(800)) {
        if (speechProc == activeSpeechProcess_) {
            activeSpeechProcess_ = nullptr;
            voice_speaking_in_progress_ = false;
            updateVoiceModeBadge();
        }
        speechProc->deleteLater();
    }
}

void MainWindow::updateHeardQueueBadge() {
    if (heardQueueBadge_ == nullptr) {
        return;
    }

    if (heard_queue_.isEmpty()) {
        heardQueueBadge_->setText("📝 HEARD: (empty)");
    } else {
        QStringList recent;
        const qsizetype start = std::max<qsizetype>(0, heard_queue_.size() - 3);
        for (qsizetype i = start; i < heard_queue_.size(); ++i) {
            recent.push_back(heard_queue_.at(i));
        }
        heardQueueBadge_->setText("📝 HEARD: " + recent.join(" • "));
    }
}

void MainWindow::updateAutoRefreshBadge() {
    if (autoRefreshBadge_ == nullptr) {
        return;
    }

    const QString text = auto_refresh_enabled_
        ? QString("↻ AUTO REFRESH: ON (%1s)").arg(QString::number(auto_refresh_interval_ms_ / 1000.0, 'f', 1))
        : "↻ AUTO REFRESH: OFF";

    const QString color = auto_refresh_enabled_ ? "#9effc9" : "#9bb0bf";
    const QString border = auto_refresh_enabled_ ? "#1d8d5e" : "#294456";
    const QString background = auto_refresh_enabled_ ? "#10251b" : "#10161f";

    autoRefreshBadge_->setText(text);
    autoRefreshBadge_->setStyleSheet(
        "QLabel {"
        "  background-color: " + background + ";"
        "  color: " + color + ";"
        "  border: 1px solid " + border + ";"
        "  border-radius: 7px;"
        "  padding: 6px 10px;"
        "  font-family: 'Monospace';"
        "  font-size: 9px;"
        "  font-weight: 700;"
        "}"
    );
}

void MainWindow::refreshUiNow() {
    updateVoiceModeBadge();
    updateHeardQueueBadge();
    updateAutoRefreshBadge();
    if (input_ != nullptr) {
        input_->update();
    }
    if (chatView_ != nullptr) {
        chatView_->viewport()->update();
    }
    this->update();
    QCoreApplication::processEvents();
}

QString MainWindow::captureVoiceCommand() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp(); // from build_gui -> project root
    const QString scriptPath = d.absoluteFilePath("voice_recognize.sh");

    QProcess proc;
    proc.start("bash", {"-lc", "\"" + scriptPath + "\""});

    if (!proc.waitForFinished(300000)) {
        proc.kill();
        return "__VOICE_ERROR__Voice capture timed out";
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const auto err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QString reason;
        if (!err.isEmpty()) {
            const auto lines = err.split('\n', Qt::SkipEmptyParts);
            reason = lines.isEmpty() ? err : lines.back().trimmed();
        } else if (!out.isEmpty()) {
            const auto lines = out.split('\n', Qt::SkipEmptyParts);
            reason = lines.isEmpty() ? out : lines.back().trimmed();
        } else {
            reason = "Voice recognition failed";
        }
        return "__VOICE_ERROR__" + reason.left(170);
    }

    const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return out;
}

void MainWindow::finalizeVoiceCapture(QProcess* proc, int exitCode, QProcess::ExitStatus exitStatus) {
    if (proc == nullptr || proc != activeVoiceProcess_) {
        if (proc != nullptr) {
            proc->deleteLater();
        }
        return;
    }

    activeVoiceProcess_ = nullptr;

    if (voiceButton_ != nullptr) {
        voiceButton_->setEnabled(true);
        voiceButton_->setText("🎙 VOICE");
    }
    voiceCaptureInProgress_ = false;
    updateVoiceModeBadge();

    if (exitStatus != QProcess::NormalExit || exitCode != 0) {
        const auto err = QString::fromUtf8(proc->readAllStandardError()).trimmed();
        const auto out = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        QString reason;
        if (!err.isEmpty()) {
            const auto lines = err.split('\n', Qt::SkipEmptyParts);
            reason = lines.isEmpty() ? err : lines.back().trimmed();
        } else if (!out.isEmpty()) {
            const auto lines = out.split('\n', Qt::SkipEmptyParts);
            reason = lines.isEmpty() ? out : lines.back().trimmed();
        } else {
            reason = "Voice recognition failed";
        }
        chatView_->append(QString("<span style='color:#ffcc66;'>Codebeat:</span> %1").arg(reason.left(220)));
        proc->deleteLater();
        return;
    }

    const auto heard = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
    proc->deleteLater();

    if (heard.isEmpty()) {
        chatView_->append("<span style='color:#ffcc66;'>Codebeat:</span> Voice recognition unavailable or no speech captured.");
        return;
    }

    const auto normalized = normalizeVoiceTranscript(heard);
    if (normalized.trimmed().isEmpty()) {
        chatView_->append("<span style='color:#ffcc66;'>Codebeat:</span> I heard audio but couldn't extract a command. Try saying: open terminal, open chrome, open vs code, status, or voice status.");
        return;
    }

    input_->setText(normalized);
    if (normalized != heard.trimmed()) {
        chatView_->append(QString("<span style='color:#7fffcf;'>Codebeat:</span> Heard: %1 → %2").arg(heard, normalized));
    } else {
        chatView_->append(QString("<span style='color:#7fffcf;'>Codebeat:</span> Heard: %1").arg(heard));
    }
    chatView_->append(QString("<span style='color:#89c7ff;'>Codebeat transcript:</span> %1").arg(normalized));

    heard_queue_.push_back(normalized);
    while (heard_queue_.size() > 3) {
        heard_queue_.pop_front();
    }
    updateHeardQueueBadge();

    onSend();
}

void MainWindow::startVoiceCapture() {
    if (voiceCaptureInProgress_) {
        chatView_->append("<span style='color:#ffcc66;'>Codebeat:</span> Voice capture already in progress...");
        return;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp();
    const QString scriptPath = d.absoluteFilePath("voice_recognize.sh");

    auto* proc = new QProcess(this);
    activeVoiceProcess_ = proc;
    voiceCaptureInProgress_ = true;

    if (voiceButton_ != nullptr) {
        voiceButton_->setEnabled(false);
        voiceButton_->setText("🎙 ...");
    }
    updateVoiceModeBadge();

    chatView_->append("<span style='color:#7fffcf;'>Codebeat:</span> Listening for voice command...");

    QObject::connect(proc, &QProcess::finished, this,
                     [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
                         if (proc != activeVoiceProcess_ && !voiceCaptureInProgress_) {
                             proc->deleteLater();
                             return;
                         }
                         finalizeVoiceCapture(proc, exitCode, exitStatus);
                     });

    QObject::connect(proc, &QProcess::errorOccurred, this,
                     [this, proc](QProcess::ProcessError) {
                         if (!voiceCaptureInProgress_ || proc != activeVoiceProcess_) {
                             proc->deleteLater();
                             return;
                         }
                         finalizeVoiceCapture(proc, 1, QProcess::CrashExit);
                     });

    // Hard watchdog to avoid indefinite "Listening..." state on stuck backend.
    QTimer::singleShot(90000, this, [this, proc]() {
        if (!voiceCaptureInProgress_ || proc != activeVoiceProcess_) {
            return;
        }
        if (proc->state() == QProcess::Running) {
            proc->kill();
            finalizeVoiceCapture(proc, 1, QProcess::CrashExit);
        }
    });

    proc->start("bash", {"-lc", "\"" + scriptPath + "\""});
}

QString MainWindow::tryHandleSystemTask(const QString& text, bool& handled) {
    handled = false;
    const auto lowered = text.trimmed().toLower();
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir rootDir(appDir);
    rootDir.cdUp();

    const auto isLikelyDirectTarget = [](const QString& target) {
        const auto t = target.trimmed().toLower();
        if (t.isEmpty() || t.size() > 96) {
            return false;
        }

        if (t.contains(" and ") || t.contains(" then ") || t.contains(" because ") ||
            t.contains(" please ") || t.contains(" maybe ") || t.contains(" i think ")) {
            return false;
        }

        static const QRegularExpression targetRx(R"(^[a-z0-9._/+:~\-]+(?:\s+[a-z0-9._/+:~\-]+){0,2}$)");
        return targetRx.match(t).hasMatch();
    };

    if (lowered == "open chrome" || lowered == "open google chrome") {
        handled = true;
        const bool ok = launchAny({"google-chrome", "google-chrome-stable", "chromium", "chromium-browser"});
        return ok ? "Opening Chrome." : "Could not launch Chrome. Check if it is installed.";
    }

    if (lowered == "open vs code" || lowered == "open vscode" || lowered == "open code") {
        handled = true;
        const bool ok = launchAny({"code", "codium"});
        return ok ? "Opening VS Code." : "Could not launch VS Code. Check if 'code' is on PATH.";
    }

    if (lowered == "open terminal") {
        handled = true;
        const bool ok = launchAny({"gnome-terminal", "konsole", "xfce4-terminal", "xterm"});
        return ok ? "Opening terminal." : "Could not launch a terminal app.";
    }

    if (lowered == "open downloads" || lowered == "open downloads folder" || lowered == "open my downloads folder") {
        handled = true;
        const QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
        const bool ok = QProcess::startDetached("xdg-open", {downloadsPath.isEmpty() ? QDir::homePath() : downloadsPath});
        return ok ? "Opening Downloads folder." : "Could not open Downloads folder.";
    }

    if (lowered == "close chrome" || lowered == "close browser" || lowered == "quit browser" || lowered == "exit browser") {
        handled = true;
        const bool ok = QProcess::startDetached("bash", {"-lc", "pkill -f '(chrome|chromium)'"});
        return ok ? "Attempting to close browser." : "Could not start close-browser command.";
    }

    if (lowered.startsWith("open ") && lowered.size() > 5) {
        const auto maybeUrl = lowered.mid(5).trimmed();
        if (maybeUrl.startsWith("http://") || maybeUrl.startsWith("https://") || maybeUrl.startsWith("www.")) {
            handled = true;
            const auto url = maybeUrl.startsWith("www.") ? ("https://" + maybeUrl) : maybeUrl;
            const bool ok = QProcess::startDetached("xdg-open", {url});
            return ok ? ("Opening URL: " + url) : "Could not open URL.";
        }

        handled = true;
        const auto app = lowered.mid(5).trimmed();
        if (!isLikelyDirectTarget(app)) {
            return "That sounds ambiguous. Use a direct target: open firefox, open code, open ~/Downloads, or open https://example.com.";
        }
        const bool ok = QProcess::startDetached(app, {});
        return ok ? ("Opening " + app + ".") : ("I couldn't open '" + app + "'.");
    }

    if (lowered.startsWith("search ") && lowered.size() > 7) {
        handled = true;
        const auto query = text.mid(7).trimmed();
        const auto encoded = QString(query).replace(" ", "+");
        const auto url = "https://www.google.com/search?q=" + encoded;
        const bool ok = QProcess::startDetached("xdg-open", {url});
        return ok ? ("Searching web for: " + query) : "Could not launch web search.";
    }

    if (lowered.startsWith("close ") && lowered.size() > 6) {
        handled = true;
        const auto app = lowered.mid(6).trimmed();
        if (!isLikelyDirectTarget(app)) {
            return "That close target looks ambiguous. Try: close chrome, close code, or close <app_name>.";
        }
        const bool ok = QProcess::startDetached("bash", {"-lc", "pkill -f \"" + app + "\""});
        return ok ? ("Attempting to close: " + app) : "Could not start close command.";
    }

    if (lowered == "create folder" || lowered == "make folder") {
        handled = true;
        return "Usage: create folder <name>. Example: create folder project-notes";
    }

    if ((lowered.startsWith("create folder ") && lowered.size() > 14) ||
        (lowered.startsWith("make folder ") && lowered.size() > 12)) {
        handled = true;
        const auto folderName = lowered.startsWith("create folder ") ? text.mid(14).trimmed() : text.mid(12).trimmed();
        if (folderName.isEmpty()) {
            return "Usage: create folder <name>. Example: create folder project-notes";
        }

        const QString homeDir = QDir::homePath();
        const QString targetPath = QDir(homeDir).absoluteFilePath(folderName);
        const bool ok = QDir().mkpath(targetPath);
        return ok ? ("Created folder: " + targetPath) : ("Could not create folder: " + targetPath);
    }

    if ((lowered.startsWith("run ") && lowered.size() > 4) ||
        (lowered.startsWith("execute ") && lowered.size() > 8)) {
        handled = true;
        const auto cmd = lowered.startsWith("execute ") ? text.mid(8).trimmed() : text.mid(4).trimmed();
        
        bool needsConfirm = false;
        if (!isCommandAllowed(text, needsConfirm)) {
            return "This command is not allowed by safety policy.";
        }
        
        if (needsConfirm && pending_confirmation_cmd_.isEmpty()) {
            pending_confirmation_cmd_ = cmd;
            return "Execute: " + cmd + "?\n\nReply 'yes' to confirm or 'no' to cancel.";
        }
        
        if (needsConfirm && pending_confirmation_cmd_ == cmd) {
            pending_confirmation_cmd_.clear();
        }
        
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? ("Running command: " + cmd) : "Command execution failed to start.";
    }

    if (lowered == "voice status" || lowered == "voice check" || lowered == "mic status") {
        handled = true;
        return voiceDiagnostics();
    }

    if (lowered == "voice identity status") {
        handled = true;
        return QString("Voice role: %1\nScore: %2")
            .arg(last_voice_role_)
            .arg(QString::number(last_voice_score_, 'f', 3));
    }

    if (lowered == "voice role" || lowered == "voice identity" || lowered == "voice profile status" ||
        lowered == "who is speaking" || lowered == "who am i by voice") {
        handled = true;
        return QString("Voice role: %1\nScore: %2")
            .arg(last_voice_role_)
            .arg(QString::number(last_voice_score_, 'f', 3));
    }

    if (lowered == "voice audit status") {
        handled = true;
        const QString logPath = rootDir.absoluteFilePath("data/logs/voice_commands.log");
        QFileInfo fi(logPath);
        if (!fi.exists()) {
            return "Voice audit log not found yet. Trigger at least one voice command first.";
        }
        return QString("Voice audit log: %1\nSize: %2 bytes\nLast update: %3")
            .arg(logPath)
            .arg(fi.size())
            .arg(fi.lastModified().toString(Qt::ISODate));
    }

    if (lowered == "voice audit summary") {
        handled = true;
        const QString logPath = rootDir.absoluteFilePath("data/logs/voice_commands.log");
        QFile f(logPath);
        if (!f.exists() || !f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return "Voice audit summary unavailable (no log file yet).";
        }

        int lines = 0;
        QString lastLine;
        QTextStream in(&f);
        while (!in.atEnd()) {
            lastLine = in.readLine();
            ++lines;
        }
        return QString("Voice audit entries: %1\nLast entry: %2")
            .arg(lines)
            .arg(lastLine.isEmpty() ? QString("(none)") : lastLine.left(220));
    }

    if (lowered == "open voice log" || lowered == "voice audit open") {
        handled = true;
        const QString logPath = rootDir.absoluteFilePath("data/logs/voice_commands.log");
        const bool ok = QProcess::startDetached("xdg-open", {logPath});
        return ok ? "Opening voice audit log." : "Couldn't open voice audit log.";
    }

    if (lowered == "open logs folder" || lowered == "voice audit folder") {
        handled = true;
        const QString logsPath = rootDir.absoluteFilePath("data/logs");
        const bool ok = QProcess::startDetached("xdg-open", {logsPath});
        return ok ? "Opening logs folder." : "Couldn't open logs folder.";
    }

    if (lowered == "list exports" || lowered == "voice audit list exports") {
        handled = true;
        const QString logsPath = rootDir.absoluteFilePath("data/logs");
        QDir logs(logsPath);
        const QStringList files = logs.entryList({"voice_commands_export_*.log"}, QDir::Files, QDir::Time);
        if (files.isEmpty()) {
            return "No voice audit exports found yet.";
        }
        QString out = "Recent voice exports:";
        const int cap = std::min(5, static_cast<int>(files.size()));
        for (int i = 0; i < cap; ++i) {
            out += "\n• " + files[i];
        }
        return out;
    }

    if (lowered == "open latest export" || lowered == "voice audit open latest") {
        handled = true;
        const QString logsPath = rootDir.absoluteFilePath("data/logs");
        QDir logs(logsPath);
        const QStringList files = logs.entryList({"voice_commands_export_*.log"}, QDir::Files, QDir::Time);
        if (files.isEmpty()) {
            return "No exported voice audit snapshot found yet.";
        }
        const QString latest = logs.absoluteFilePath(files.first());
        const bool ok = QProcess::startDetached("xdg-open", {latest});
        return ok ? ("Opening latest export: " + latest) : "Couldn't open latest export.";
    }

    if (lowered == "copy latest export path" || lowered == "voice audit copy latest") {
        handled = true;
        const QString logsPath = rootDir.absoluteFilePath("data/logs");
        QDir logs(logsPath);
        const QStringList files = logs.entryList({"voice_commands_export_*.log"}, QDir::Files, QDir::Time);
        if (files.isEmpty()) {
            return "No exported voice audit snapshot found yet.";
        }
        const QString latest = logs.absoluteFilePath(files.first());
        return "Latest export path: " + latest;
    }

    if (lowered == "voice audit clear") {
        handled = true;
        const QString logPath = rootDir.absoluteFilePath("data/logs/voice_commands.log");
        QFile f(logPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            return "Couldn't clear voice audit log.";
        }
        return "Voice audit log cleared.";
    }

    if (lowered == "voice audit export") {
        handled = true;
        const QString logsPath = rootDir.absoluteFilePath("data/logs");
        QDir().mkpath(logsPath);
        const QString src = rootDir.absoluteFilePath("data/logs/voice_commands.log");
        if (!QFileInfo::exists(src)) {
            return "No voice audit log exists yet to export.";
        }
        const QString stamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
        const QString dst = QDir(logsPath).absoluteFilePath("voice_commands_export_" + stamp + ".log");
        QFile::remove(dst);
        const bool ok = QFile::copy(src, dst);
        return ok ? ("Voice audit exported: " + dst) : "Voice audit export failed.";
    }

    if (lowered == "voice standby on" || lowered == "standby on" || lowered == "listening mode on") {
        handled = true;
        standby_listening_enabled_ = true;
        updateVoiceModeBadge();
        return "Continuous listening is now ON.";
    }

    if (lowered == "voice standby off" || lowered == "standby off" || lowered == "sleep mode") {
        handled = true;
        standby_listening_enabled_ = false;
        wake_command_until_epoch_secs_ = 0;
        updateVoiceModeBadge();
        return "Continuous listening is now OFF.";
    }

    if (lowered == "stop listening" || lowered == "go to sleep" || lowered == "quiet mode" || lowered == "never mind") {
        handled = true;
        if (activeVoiceProcess_ && activeVoiceProcess_->state() == QProcess::Running) {
            activeVoiceProcess_->terminate();
            QTimer::singleShot(500, this, [this]() {
                if (activeVoiceProcess_ && activeVoiceProcess_->state() == QProcess::Running) {
                    activeVoiceProcess_->kill();
                }
            });
        }
        voiceCaptureInProgress_ = false;
        standby_listening_enabled_ = false;
        updateVoiceModeBadge();
        return "Voice capture interrupted. Standby mode OFF.";
    }

    if (lowered == "voice standby status" || lowered == "standby status" || lowered == "listening mode status") {
        handled = true;
        const auto standbyState = standby_listening_enabled_ ? "ON" : "OFF";
        return QString("Standby status: %1\nWake command window: %2s")
            .arg(standbyState)
            .arg(wake_command_window_secs_);
    }

    if (lowered == "voice standby sensitivity up" || lowered == "standby sensitivity up" || lowered == "wake window shorter") {
        handled = true;
        wake_command_window_secs_ = std::max(3, wake_command_window_secs_ - 1);
        return QString("Wake window shortened to %1s.").arg(wake_command_window_secs_);
    }

    if (lowered == "voice standby sensitivity down" || lowered == "standby sensitivity down" || lowered == "wake window longer") {
        handled = true;
        wake_command_window_secs_ = std::min(20, wake_command_window_secs_ + 1);
        return QString("Wake window extended to %1s.").arg(wake_command_window_secs_);
    }

    if (lowered == "volume up" || lowered == "sound up") {
        handled = true;
        const QString cmd =
            "(pactl set-sink-volume @DEFAULT_SINK@ +5% "
            "|| wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%+ "
            "|| amixer -q sset Master 5%+) >/dev/null 2>&1";
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? "Volume increased." : "Could not increase volume.";
    }

    if (lowered == "volume down" || lowered == "sound down") {
        handled = true;
        const QString cmd =
            "(pactl set-sink-volume @DEFAULT_SINK@ -5% "
            "|| wpctl set-volume @DEFAULT_AUDIO_SINK@ 5%- "
            "|| amixer -q sset Master 5%-) >/dev/null 2>&1";
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? "Volume decreased." : "Could not decrease volume.";
    }

    if (lowered == "mute" || lowered == "mute volume" || lowered == "mute audio") {
        handled = true;
        const QString cmd =
            "(pactl set-sink-mute @DEFAULT_SINK@ 1 "
            "|| wpctl set-mute @DEFAULT_AUDIO_SINK@ 1 "
            "|| amixer -q sset Master mute) >/dev/null 2>&1";
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? "Audio muted." : "Could not mute audio.";
    }

    if (lowered == "unmute" || lowered == "unmute volume" || lowered == "unmute audio") {
        handled = true;
        const QString cmd =
            "(pactl set-sink-mute @DEFAULT_SINK@ 0 "
            "|| wpctl set-mute @DEFAULT_AUDIO_SINK@ 0 "
            "|| amixer -q sset Master unmute) >/dev/null 2>&1";
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? "Audio unmuted." : "Could not unmute audio.";
    }

    if (lowered == "voice enroll owner") {
        handled = true;
        const bool ok = QProcess::startDetached("bash", {"-lc", "cd \"" + rootDir.absolutePath() + "\" && python3 runtime/voice_profile.py owner"});
        return ok ? "Started owner voice enrollment." : "Failed to start owner enrollment.";
    }

    if (lowered == "voice enroll trusted") {
        handled = true;
        const bool ok = QProcess::startDetached("bash", {"-lc", "cd \"" + rootDir.absolutePath() + "\" && python3 runtime/voice_profile.py trusted"});
        return ok ? "Started trusted voice enrollment." : "Failed to start trusted enrollment.";
    }

    if (lowered == "check battery" || lowered == "battery status") {
        handled = true;
        QProcess proc;
        proc.start("bash", {"-lc", "upower -i $(upower -e | grep BAT | head -1) 2>/dev/null | grep -E 'state|percentage|time to empty|time to full'"});
        if (!proc.waitForFinished(5000)) {
            proc.kill();
            return "Battery check timed out.";
        }
        const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        return out.isEmpty() ? "Battery details unavailable on this system." : out;
    }

    if (lowered == "show running processes") {
        handled = true;
        QProcess proc;
        proc.start("bash", {"-lc", "ps aux --sort=-%cpu | head -20"});
        if (!proc.waitForFinished(5000)) {
            proc.kill();
            return "Process listing timed out.";
        }
        const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        return out.isEmpty() ? "No process output available." : out;
    }

    if (lowered == "take screenshot") {
        handled = true;
        const QString shotPath = rootDir.absoluteFilePath("data/processed/screenshot_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".png");
        const bool ok = QProcess::startDetached("bash", {"-lc", "(gnome-screenshot -f \"" + shotPath + "\" || grim \"" + shotPath + "\" || scrot \"" + shotPath + "\") >/dev/null 2>&1"});
        return ok ? ("Screenshot command triggered. Target: " + shotPath) : "Could not start screenshot command.";
    }

    if (lowered == "refresh app" || lowered == "refresh ui" || lowered == "reload ui" || lowered == "refresh") {
        handled = true;
        refreshUiNow();
        if (input_ != nullptr) {
            input_->setFocus();
        }
        return "App UI refreshed.";
    }

    if (lowered == "refresh auto on") {
        handled = true;
        auto_refresh_enabled_ = true;
        if (auto_refresh_timer_ != nullptr && !auto_refresh_timer_->isActive()) {
            auto_refresh_timer_->start(auto_refresh_interval_ms_);
        }
        refreshUiNow();
        return QString("Auto-refresh enabled (%1s interval).")
            .arg(QString::number(auto_refresh_interval_ms_ / 1000.0, 'f', 1));
    }

    if (lowered == "refresh auto off") {
        handled = true;
        auto_refresh_enabled_ = false;
        if (auto_refresh_timer_ != nullptr) {
            auto_refresh_timer_->stop();
        }
        updateAutoRefreshBadge();
        return "Auto-refresh disabled.";
    }

    if (lowered == "refresh auto status" || lowered == "auto refresh status") {
        handled = true;
        return QString("Auto-refresh: %1 (%2s interval)")
            .arg(auto_refresh_enabled_ ? "ON" : "OFF")
            .arg(QString::number(auto_refresh_interval_ms_ / 1000.0, 'f', 1));
    }

    if (lowered == "what can you control" || lowered == "apps") {
        handled = true;
        return "I can open apps and run tasks. Try: open chrome, open vs code, open terminal, open downloads, search <query>, google <query>, open docs for <topic>, close browser, close <app>, run <command>, voice status, voice role, voice audit status, voice standby on, refresh auto status, volume up, volume down, mute, unmute, battery status, show running processes, take screenshot.";
    }

    return {};
}

void MainWindow::runQuickAction(const QString& text) {
    input_->setText(text);
    onSend();
}

QString MainWindow::generateAssistantReply(const QString& text) {
    ensureKnowledgeLoaded();
    const auto lowered = text.trimmed().toLower();
    const auto inferredIntent = inferIntentCommand(text);

    if (lowered == "mode concise") {
        concise_mode_ = true;
        return "Response mode set to concise.";
    }
    if (lowered == "mode detailed") {
        concise_mode_ = false;
        return "Response mode set to detailed.";
    }
    if (lowered == "mode status") {
        return QString("Current response mode: %1").arg(concise_mode_ ? "concise" : "detailed");
    }

    if (lowered.startsWith("learn:") || lowered.startsWith("learn ")) {
        const auto fact = lowered.startsWith("learn:")
                              ? text.mid(text.indexOf(':') + 1)
                              : text.mid(6);
        return learnKnowledgeFact(fact);
    }

    if (lowered == "knowledge status" || lowered == "knowledge count") {
        return "Knowledge entries loaded: " + QString::number(static_cast<int>(knowledge_corpus_.size())) +
               ". Add more with: learn: <fact>.";
    }

    if (!pending_confirmation_cmd_.isEmpty()) {
        if (lowered == "yes" || lowered == "y" || lowered == "confirm") {
            const auto cmdCopy = pending_confirmation_cmd_;
            pending_confirmation_cmd_.clear();
            const bool ok = QProcess::startDetached("bash", {"-lc", cmdCopy});
            return ok ? ("Executing: " + cmdCopy) : "Command execution failed.";
        }
        if (lowered == "no" || lowered == "n" || lowered == "cancel") {
            pending_confirmation_cmd_.clear();
            return "Command canceled.";
        }
        return "Waiting for confirmation. Reply 'yes' or 'no'.";
    }

    bool handled = false;
    const auto taskReply = tryHandleSystemTask(inferredIntent.isEmpty() ? text : inferredIntent, handled);
    if (handled) {
        if (inferredIntent.isEmpty() || inferredIntent.compare(lowered, Qt::CaseInsensitive) == 0) {
            return taskReply;
        }
        return QString("Intent: %1\n%2").arg(inferredIntent, taskReply);
    }

    if (lowered.startsWith("plan ") && lowered.size() > 5) {
        const auto goal = text.mid(5).trimmed();
        if (goal.isEmpty()) {
            return "Usage: plan <goal>. Example: plan build and test codebeat and push changes.";
        }

        QStringList chunks = goal.split(QRegularExpression("\\b(and then|then|and|->|,)\\b"), Qt::SkipEmptyParts);
        for (auto& c : chunks) {
            c = c.trimmed();
        }
        chunks.erase(std::remove_if(chunks.begin(), chunks.end(), [](const QString& s) { return s.isEmpty(); }), chunks.end());

        QString out = "Plan for: " + goal + "\n";
        int step = 1;
        if (chunks.size() >= 2) {
            for (const auto& c : chunks) {
                out += QString::number(step++) + ". " + c + "\n";
            }
            out += QString::number(step++) + ". validate results\n";
            out += QString::number(step) + ". iterate on weak spots";
        } else {
            out += "1. understand requirements\n";
            out += "2. design small implementation steps\n";
            out += "3. implement incrementally\n";
            out += "4. test and validate\n";
            out += "5. refine and ship";
        }

        const auto plannerHint = QString::fromStdString(planner_.plan(goal.toStdString()));
        out += "\n\nPlanner hint: " + plannerHint;
        return out;
    }

    if (lowered.startsWith("brainstorm ") || lowered.startsWith("ideas ")) {
        const auto topic = lowered.startsWith("brainstorm ") ? text.mid(11).trimmed() : text.mid(6).trimmed();
        if (topic.isEmpty()) {
            return "Usage: brainstorm <topic> or ideas <topic>.";
        }
        return "Ideas for " + topic + ":\n"
               "• Build a minimal MVP with one killer workflow.\n"
               "• Add a fast feedback loop with clear diagnostics.\n"
               "• Introduce automation for repetitive tasks.\n"
               "• Add a premium UX layer for key actions.\n"
               "• Measure outcomes and iterate weekly.";
    }

    if (lowered.startsWith("compare ") && lowered.size() > 8) {
        const auto body = text.mid(8).trimmed();
        int vsIdx = body.toLower().indexOf(" vs ");
        if (vsIdx < 0) {
            return "Usage: compare <option A> vs <option B>.";
        }
        const auto left = body.left(vsIdx).trimmed();
        const auto right = body.mid(vsIdx + 4).trimmed();
        if (left.isEmpty() || right.isEmpty()) {
            return "Usage: compare <option A> vs <option B>.";
        }
        return "Comparison: " + left + " vs " + right + "\n"
               "• Strengths: " + left + " may optimize simplicity; " + right + " may optimize flexibility.\n"
               "• Risks: " + left + " can limit edge-cases; " + right + " can increase complexity.\n"
               "• Cost: evaluate implementation time, maintenance, and reliability.\n"
               "• Recommendation: pick the option that matches your immediate priority and test it with a small pilot.";
    }

    if (lowered.startsWith("rewrite:") || lowered.startsWith("rewrite ")) {
        const auto payload = lowered.startsWith("rewrite:") ? text.mid(8).trimmed() : text.mid(8).trimmed();
        int sep = payload.indexOf("::");
        if (sep < 0) {
            return "Usage: rewrite: <style>::<text>. Styles: concise, professional, friendly, technical.";
        }
        const auto style = payload.left(sep).trimmed().toLower();
        QString source = payload.mid(sep + 2).trimmed();
        source.replace(QRegularExpression("\\s+"), " ");
        if (source.isEmpty()) {
            return "Please provide text to rewrite.";
        }

        if (style == "concise") {
            if (source.size() > 160) {
                source = source.left(157).trimmed() + "...";
            }
            return "Concise rewrite: " + source;
        }
        if (style == "professional") {
            return "Professional rewrite: " + source.left(1).toUpper() + source.mid(1);
        }
        if (style == "friendly") {
            return "Friendly rewrite: Hey! " + source;
        }
        if (style == "technical") {
            return "Technical rewrite: " + source + " (validated through incremental testing and observable outputs).";
        }
        return "Unknown style. Use: concise, professional, friendly, or technical.";
    }

    if (lowered.startsWith("summarize:") || lowered.startsWith("summarize ")) {
        QString payload = lowered.startsWith("summarize:") ? text.mid(10).trimmed() : text.mid(10).trimmed();
        if (payload.isEmpty()) {
            payload = last_user_message_.trimmed();
        }
        if (payload.isEmpty()) {
            return "I don’t have text to summarize yet. Use: summarize: <text>.";
        }

        const auto sentences = payload.split(QRegularExpression("(?<=[.!?])\\s+"), Qt::SkipEmptyParts);
        QString core;
        if (!sentences.isEmpty()) {
            core = sentences.first().trimmed();
            if (sentences.size() > 1) {
                core += " " + sentences.at(1).trimmed();
            }
        } else {
            core = payload;
        }
        if (core.size() > 220) {
            core = core.left(217).trimmed() + "...";
        }

        std::set<QString> uniq;
        QStringList keywords;
        for (const auto& k : splitKeywords(payload)) {
            if (uniq.insert(k).second) {
                keywords.push_back(k);
            }
            if (keywords.size() >= 6) {
                break;
            }
        }

        QString keyLine = keywords.isEmpty() ? "none" : keywords.join(", ");
        return "Summary: " + core + "\nKey terms: " + keyLine;
    }

    if (lowered == "lock" || lowered == "lock app" || lowered == "relock") {
        return "Locking now...";
    }

    if (lowered.startsWith("remember ")) {
        const auto note = text.mid(9).trimmed();
        if (!note.isEmpty()) {
            notes_.push_back(note);
            return "Got it. I’ll remember: \"" + note + "\".";
        }
        return "Tell me what to remember, for example: remember I prefer dark mode.";
    }

    if (lowered.contains("what did i tell you") || lowered.contains("what do you remember")) {
        const auto recent = memory_.recent(6);
        if (recent.empty() && notes_.empty()) {
            return "You haven’t asked me to remember anything yet.";
        }

        QString summary = "Here’s my recent memory:";
        for (const auto& item : recent) {
            summary += "\n• " + QString::fromStdString(item);
        }

        if (!notes_.empty()) {
            summary += "\nStored notes:";
            const int start = std::max(0, static_cast<int>(notes_.size()) - 3);
            for (int i = start; i < static_cast<int>(notes_.size()); ++i) {
                summary += "\n• " + notes_[static_cast<std::size_t>(i)];
            }
        }
        return summary;
    }

    if (lowered.contains("help")) {
        return "I can chat naturally, remember context, summarize, answer questions, and control your system. Try saying: I want to write some code, open the browser, open my downloads folder, google qt signals slots, open docs for cmake, close browser, create folder project-notes, show running processes, battery status, voice status, voice role, refresh auto status, volume up, volume down, mute, unmute, learn: Codebeat should be concise, knowledge status, plan ship release this week, brainstorm onboarding flow, compare vscode vs vim, rewrite: concise::your text, mode concise, status, time.";
    }

    if (lowered.contains("status")) {
        const auto tool_info = tools_.capabilities();
        return "All systems nominal. Interaction count: " + QString::number(interaction_count_) +
               " | Notes stored: " + QString::number(static_cast<int>(notes_.size())) +
               " | " + QString::fromStdString(tool_info.output) + ".";
    }

    if (lowered == "time" || lowered.contains("what time") || lowered.contains("current time")) {
        const auto result = tools_.now_time();
        return QString::fromStdString(result.output);
    }

    if (lowered.contains("summarize")) {
        if (last_user_message_.isEmpty()) {
            return "I don’t have prior context yet. Tell me something first, then ask me to summarize.";
        }
        return "Summary of your previous message: \"" + last_user_message_ + "\".";
    }

    if (lowered.contains("hi") || lowered.contains("hello") || lowered.contains("hey")) {
        return "Hey — I’m online and ready. What should we build or solve next?";
    }

    if (lowered.contains("thank")) {
        return "Always. Keep going — we’re making great progress.";
    }

    if (text.trimmed().endsWith('?')) {
        const auto grounded = retrieveKnowledgeReply(text);
        if (!grounded.isEmpty()) {
            return grounded;
        }

        if (lowered.contains("who are you")) {
            return "I’m your Codebeat assistant. I can help you think through ideas, code tasks, and keep context while we chat.";
        }
        if (lowered.contains("what can you do")) {
            return "I can answer questions, track short notes, summarize context, and help with coding decisions step-by-step.";
        }
        if (lowered.contains("how") || lowered.contains("why")) {
            return "Great question. Here’s the practical approach: split the problem into steps, validate each part, and iterate with quick feedback.";
        }
        return "Good question. Based on our context, I’d solve it incrementally and verify each step. If you want, I can do that with you now.";
    }

    const auto grounded = retrieveKnowledgeReply(text);
    if (!grounded.isEmpty()) {
        return grounded;
    }

    if (concise_mode_) {
        return "Understood: " + text;
    }

    return "Understood. My take: " + text + "\nIf you want, I can break that into concrete next steps.";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    ensureKnowledgeLoaded();
    loadSafetyPolicy();
    bool voiceOutputOk = false;
    const auto voiceOutputRaw = qEnvironmentVariableIntValue("CODEBEAT_VOICE_OUTPUT", &voiceOutputOk);
    voice_output_enabled_ = !voiceOutputOk || voiceOutputRaw != 0;

    auto_refresh_timer_ = new QTimer(this);
    auto_refresh_timer_->setInterval(auto_refresh_interval_ms_);
    QObject::connect(auto_refresh_timer_, &QTimer::timeout, this, [this]() {
        if (auto_refresh_enabled_) {
            refreshUiNow();
        }
    });
    // Apply dark modern theme stylesheet
    setStyleSheet(
        "QMainWindow { background-color: #050505; }"
        "QWidget { background-color: #050505; }"
    );

    auto* central = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setContentsMargins(0, 0, 0, 0);
    main_layout->setSpacing(0);

    // Header bar with gradient effect (simulated)
    auto* header = new QFrame(central);
    header->setFixedHeight(50);
    header->setStyleSheet(
        "QFrame {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
        "    stop:0 #0b0b0b, stop:0.5 #121212, stop:1 #0b0b0b);"
        "  border-bottom: 1px solid #00d8ff;"
        "}"
    );
    
    auto* header_layout = new QHBoxLayout(header);
    auto* title_label = new QLabel("⚡ CODEBEAT // Neural Assistant");
    title_label->setStyleSheet(
        "color: #00e5ff; font-family: 'Monospace'; font-size: 14px; font-weight: bold;"
    );
    header_layout->addWidget(title_label);
    header_layout->addStretch();
    
    auto* status_label = new QLabel("● ONLINE");
    status_label->setStyleSheet("color: #00ff88; font-family: 'Monospace'; font-size: 11px;");
    header_layout->addWidget(status_label);

    auto* lockBtn = new QPushButton("🔒 LOCK", header);
    lockBtn->setFixedHeight(28);
    lockBtn->setStyleSheet(
        "QPushButton {"
        "  background-color: #1a1a1a;"
        "  color: #ff7ab8;"
        "  border: 1px solid #ff4da0;"
        "  border-radius: 6px;"
        "  padding: 2px 10px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 10px;"
        "  font-weight: 600;"
        "}"
        "QPushButton:hover { background-color: #24111d; }"
        "QPushButton:pressed { background-color: #13090f; }"
    );
    header_layout->addWidget(lockBtn);
    header_layout->setContentsMargins(15, 0, 15, 0);

    main_layout->addWidget(header);

    auto* content_frame = new QFrame(central);
    content_frame->setStyleSheet("QFrame { background-color: #050505; }");
    auto* content_layout = new QHBoxLayout(content_frame);
    content_layout->setContentsMargins(10, 10, 10, 10);
    content_layout->setSpacing(10);

    auto* hud_panel = new QFrame(content_frame);
    hud_panel->setFixedWidth(290);
    hud_panel->setStyleSheet(
        "QFrame {"
        "  background-color: #090c11;"
        "  border: 1px solid #133042;"
        "  border-radius: 10px;"
        "}"
    );
    auto* hud_layout = new QVBoxLayout(hud_panel);
    hud_layout->setContentsMargins(14, 14, 14, 14);
    hud_layout->setSpacing(10);

    auto* hud_title = new QLabel("TACTICAL PANEL", hud_panel);
    hud_title->setStyleSheet("color:#69e9ff; font-family:'Monospace'; font-size:12px; font-weight:700; border:none;");
    hud_layout->addWidget(hud_title);

    auto makeStatCard = [hud_panel](const QString& name, const QString& value, const QString& accent) {
        auto* card = new QFrame(hud_panel);
        card->setStyleSheet(
            "QFrame {"
            "  background-color: #10161f;"
            "  border: 1px solid #1c3d52;"
            "  border-radius: 8px;"
            "}"
        );
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(10, 8, 10, 8);
        l->setSpacing(2);

        auto* n = new QLabel(name, card);
        n->setStyleSheet("color:#93a7b7; font-family:'Monospace'; font-size:10px; border:none;");
        l->addWidget(n);

        auto* v = new QLabel(value, card);
        v->setStyleSheet("color:" + accent + "; font-family:'Monospace'; font-size:12px; font-weight:700; border:none;");
        l->addWidget(v);
        return card;
    };

    hud_layout->addWidget(makeStatCard("SYSTEM", "ONLINE", "#6bffb4"));
    hud_layout->addWidget(makeStatCard("MODEL", "TINY-LOCAL", "#8ad8ff"));
    hud_layout->addWidget(makeStatCard("AUTH", "ACTIVE", "#ff9cd0"));

    auto makeHudBtn = [hud_panel](const QString& txt) {
        auto* b = new QPushButton(txt, hud_panel);
        b->setFixedHeight(30);
        b->setStyleSheet(
            "QPushButton {"
            "  background-color:#131b24;"
            "  color:#d8f7ff;"
            "  border:1px solid #28485f;"
            "  border-radius:6px;"
            "  font-family:'Segoe UI';"
            "  font-size:10px;"
            "  text-align:left;"
            "  padding-left:10px;"
            "}"
            "QPushButton:hover { border:1px solid #00d8ff; background-color:#162435; }"
        );
        return b;
    };

    auto setThemedIcon = [this](QPushButton* btn, const QString& themeName, QStyle::StandardPixmap fallback) {
        QIcon icon = QIcon::fromTheme(themeName);
        if (icon.isNull()) {
            icon = style()->standardIcon(fallback);
        }
        btn->setIcon(icon);
    };

    auto makeSectionToggle = [hud_panel](const QString& title, bool expanded) {
        auto* b = new QPushButton((expanded ? "v " : "> ") + title, hud_panel);
        b->setCheckable(true);
        b->setChecked(expanded);
        b->setProperty("sectionTitle", title);
        b->setFixedHeight(26);
        b->setStyleSheet(
            "QPushButton {"
            "  background-color:#0f1722;"
            "  color:#7ec7dd;"
            "  border:1px solid #23445a;"
            "  border-radius:6px;"
            "  text-align:left;"
            "  padding-left:8px;"
            "  font-family:'Segoe UI';"
            "  font-size:11px;"
            "  font-weight:600;"
            "}"
            "QPushButton:hover { border:1px solid #00d8ff; }"
        );
        return b;
    };

    auto* quick_toggle = makeSectionToggle("Quick Control", true);
    auto* quick_content = new QFrame(hud_panel);
    quick_content->setStyleSheet("QFrame { border:none; background: transparent; }");
    auto* quick_content_layout = new QVBoxLayout(quick_content);
    quick_content_layout->setContentsMargins(0, 4, 0, 0);
    quick_content_layout->setSpacing(6);

    auto* cmd_open_chrome = makeHudBtn("open chrome");
    auto* cmd_open_code = makeHudBtn("open vs code");
    auto* cmd_search_cpp = makeHudBtn("search c++ qt tutorial");
    auto* cmd_status = makeHudBtn("status");
    setThemedIcon(cmd_open_chrome, "internet-web-browser", QStyle::SP_DriveNetIcon);
    setThemedIcon(cmd_open_code, "applications-development", QStyle::SP_DesktopIcon);
    setThemedIcon(cmd_search_cpp, "edit-find", QStyle::SP_FileDialogContentsView);
    setThemedIcon(cmd_status, "dialog-information", QStyle::SP_MessageBoxInformation);

    quick_content_layout->addWidget(cmd_open_chrome);
    quick_content_layout->addWidget(cmd_open_code);
    quick_content_layout->addWidget(cmd_search_cpp);
    quick_content_layout->addWidget(cmd_status);

    auto* voice_toggle = makeSectionToggle("Voice Access", false);
    auto* voice_content = new QFrame(hud_panel);
    voice_content->setStyleSheet("QFrame { border:none; background: transparent; }");
    auto* voice_content_layout = new QVBoxLayout(voice_content);
    voice_content_layout->setContentsMargins(0, 4, 0, 0);
    voice_content_layout->setSpacing(6);
    voice_content->setVisible(false);

    auto* cmd_voice_identity = makeHudBtn("voice identity status");
    auto* cmd_voice_audit_status = makeHudBtn("voice audit status");
    auto* cmd_voice_audit_summary = makeHudBtn("audit summary");
    auto* cmd_open_voice_log = makeHudBtn("open voice log");
    auto* cmd_open_logs_folder = makeHudBtn("open logs folder");
    auto* cmd_open_latest_export = makeHudBtn("open latest export");
    auto* cmd_copy_latest_export = makeHudBtn("copy latest export path");
    auto* cmd_list_exports = makeHudBtn("list exports");
    auto* cmd_voice_audit_clear = makeHudBtn("clear voice log");
    auto* cmd_voice_audit_export = makeHudBtn("export voice log");
    auto* cmd_standby_on = makeHudBtn("voice standby on");
    auto* cmd_standby_off = makeHudBtn("voice standby off");
    auto* cmd_standby_status = makeHudBtn("standby status");
    auto* cmd_interrupt = makeHudBtn("stop listening");
    auto* cmd_enroll_owner = makeHudBtn("voice enroll owner");
    auto* cmd_enroll_trusted = makeHudBtn("voice enroll trusted");
    setThemedIcon(cmd_voice_identity, "preferences-desktop-user", QStyle::SP_DirHomeIcon);
    setThemedIcon(cmd_voice_audit_status, "view-list-details", QStyle::SP_FileDialogDetailedView);
    setThemedIcon(cmd_voice_audit_summary, "dialog-information", QStyle::SP_MessageBoxInformation);
    setThemedIcon(cmd_open_voice_log, "text-x-log", QStyle::SP_FileIcon);
    setThemedIcon(cmd_open_logs_folder, "folder-open", QStyle::SP_DirOpenIcon);
    setThemedIcon(cmd_open_latest_export, "document-open-recent", QStyle::SP_DirOpenIcon);
    setThemedIcon(cmd_copy_latest_export, "edit-copy", QStyle::SP_FileDialogNewFolder);
    setThemedIcon(cmd_list_exports, "view-list-text", QStyle::SP_FileDialogListView);
    setThemedIcon(cmd_voice_audit_clear, "edit-clear", QStyle::SP_DialogResetButton);
    setThemedIcon(cmd_voice_audit_export, "document-save-as", QStyle::SP_DialogSaveButton);
    setThemedIcon(cmd_standby_on, "media-playback-start", QStyle::SP_MediaPlay);
    setThemedIcon(cmd_standby_off, "media-playback-stop", QStyle::SP_MediaStop);
    setThemedIcon(cmd_standby_status, "dialog-information", QStyle::SP_MessageBoxInformation);
    setThemedIcon(cmd_interrupt, "process-stop", QStyle::SP_MediaStop);
    setThemedIcon(cmd_enroll_owner, "emblem-favorite", QStyle::SP_DialogApplyButton);
    setThemedIcon(cmd_enroll_trusted, "emblem-ok", QStyle::SP_DialogYesButton);

    voice_content_layout->addWidget(cmd_voice_identity);
    voice_content_layout->addWidget(cmd_voice_audit_status);
    voice_content_layout->addWidget(cmd_voice_audit_summary);
    voice_content_layout->addWidget(cmd_open_voice_log);
    voice_content_layout->addWidget(cmd_open_logs_folder);
    voice_content_layout->addWidget(cmd_open_latest_export);
    voice_content_layout->addWidget(cmd_copy_latest_export);
    voice_content_layout->addWidget(cmd_list_exports);
    voice_content_layout->addWidget(cmd_voice_audit_clear);
    voice_content_layout->addWidget(cmd_voice_audit_export);
    voice_content_layout->addWidget(cmd_standby_on);
    voice_content_layout->addWidget(cmd_standby_off);
    voice_content_layout->addWidget(cmd_standby_status);
    voice_content_layout->addWidget(cmd_interrupt);
    voice_content_layout->addWidget(cmd_enroll_owner);
    voice_content_layout->addWidget(cmd_enroll_trusted);

    auto* system_toggle = makeSectionToggle("System Info", false);
    auto* system_content = new QFrame(hud_panel);
    system_content->setStyleSheet("QFrame { border:none; background: transparent; }");
    auto* system_content_layout = new QVBoxLayout(system_content);
    system_content_layout->setContentsMargins(0, 4, 0, 0);
    system_content_layout->setSpacing(6);
    system_content->setVisible(false);

    auto* cmd_battery = makeHudBtn("check battery");
    auto* cmd_processes = makeHudBtn("show running processes");
    auto* cmd_screenshot = makeHudBtn("take screenshot");
    auto* cmd_refresh_app = makeHudBtn("refresh app");
    setThemedIcon(cmd_battery, "battery-good", QStyle::SP_DriveHDIcon);
    setThemedIcon(cmd_processes, "utilities-system-monitor", QStyle::SP_ComputerIcon);
    setThemedIcon(cmd_screenshot, "camera-photo", QStyle::SP_DesktopIcon);
    setThemedIcon(cmd_refresh_app, "view-refresh", QStyle::SP_BrowserReload);
    system_content_layout->addWidget(cmd_battery);
    system_content_layout->addWidget(cmd_processes);
    system_content_layout->addWidget(cmd_screenshot);
    system_content_layout->addWidget(cmd_refresh_app);

    auto bindSection = [this](QPushButton* toggle, QFrame* content) {
        QObject::connect(toggle, &QPushButton::clicked, this, [toggle, content](bool checked) {
            content->setVisible(checked);
            const QString title = toggle->property("sectionTitle").toString();
            toggle->setText((checked ? "v " : "> ") + title);
        });
    };

    bindSection(quick_toggle, quick_content);
    bindSection(voice_toggle, voice_content);
    bindSection(system_toggle, system_content);

    hud_layout->addWidget(quick_toggle);
    hud_layout->addWidget(quick_content);
    hud_layout->addWidget(voice_toggle);
    hud_layout->addWidget(voice_content);
    hud_layout->addWidget(system_toggle);
    hud_layout->addWidget(system_content);
    hud_layout->addStretch();

    auto* terminal_hint = new QLabel("Tip: type `what can you control` or try `I want to write some code` for natural-language control.", hud_panel);
    terminal_hint->setWordWrap(true);
    terminal_hint->setStyleSheet("color:#7a8b97; font-family:'Monospace'; font-size:10px; border:none;");
    hud_layout->addWidget(terminal_hint);

    auto* right_panel = new QFrame(content_frame);
    right_panel->setStyleSheet(
        "QFrame {"
        "  background-color: #07090d;"
        "  border: 1px solid #13202b;"
        "  border-radius: 10px;"
        "}"
    );
    auto* right_layout = new QVBoxLayout(right_panel);
    right_layout->setContentsMargins(8, 8, 8, 8);
    right_layout->setSpacing(8);

    // Chat view with enhanced styling
    chatView_ = new QTextEdit(right_panel);
    chatView_->setReadOnly(true);
    chatView_->setPlaceholderText("Messages appear here...");
    chatView_->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0a0a0a;"
        "  color: #d7f9ff;"
        "  border: 1px solid #152736;"
        "  font-family: 'Fira Code', 'Monospace';"
        "  font-size: 12px;"
        "  padding: 15px;"
        "  margin: 0px;"
        "  selection-background-color: #22445a;"
        "  border-radius: 8px;"
        "}"
        "QTextEdit:focus { outline: none; }"
        "QScrollBar:vertical { width: 8px; background-color: #121212; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background-color: #1d4f66; border-radius: 4px; }"
    );
    right_layout->addWidget(chatView_, 1);

    // Input section with gradient border
    auto* input_section = new QFrame(right_panel);
    input_section->setStyleSheet(
        "QFrame {"
        "  background-color: #050505;"
        "  border-top: 1px solid #00d8ff;"
        "  padding: 10px;"
        "}"
    );
    
    auto* input_layout = new QVBoxLayout(input_section);
    input_layout->setContentsMargins(10, 10, 10, 10);
    input_layout->setSpacing(8);

    // Modern input field with icon placeholder
    input_ = new QLineEdit(input_section);
    input_->setPlaceholderText("🔮 Ask Codebeat...");
    input_->setMinimumHeight(40);
    input_->setStyleSheet(
        "QLineEdit {"
        "  background-color: #121212;"
        "  color: #d8f7ff;"
        "  border: 1px solid #1d4f66;"
        "  border-radius: 8px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 13px;"
        "  padding: 8px 15px;"
        "  selection-background-color: #1d4f66;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid #00d8ff;"
        "  background-color: #0d0d0d;"
        "}"
        "QLineEdit::placeholder { color: #5f6d74; }"
    );

    auto* input_row = new QHBoxLayout();
    input_row->addWidget(input_, 1);

    auto* voiceBtn = new QPushButton("🎙 VOICE", input_section);
    voiceBtn->setFixedWidth(110);
    voiceBtn->setMinimumHeight(40);
    voiceBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #18a56f, stop:1 #0f7f54);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-family: 'Monospace';"
        "  font-weight: bold;"
        "  font-size: 11px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #20be80, stop:1 #149b66);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0d6a46, stop:1 #0b5a3b);"
        "}"
    );
    input_row->addWidget(voiceBtn);
    voiceButton_ = voiceBtn;

    // Modern send button with hover effects
    auto* sendBtn = new QPushButton("▶ SEND", input_section);
    sendBtn->setFixedWidth(100);
    sendBtn->setMinimumHeight(40);
    sendBtn->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "    stop:0 #00a8d0, stop:1 #006f8a);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-family: 'Monospace';"
        "  font-weight: bold;"
        "  font-size: 12px;"
        "  padding: 0px 20px;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "    stop:0 #19bde4, stop:1 #0085a6);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, "
        "    stop:0 #007ea1, stop:1 #00566a);"
        "}"
    );
    
    input_row->addWidget(sendBtn);
    input_layout->addLayout(input_row);

    auto* voice_mode_row = new QHBoxLayout();
    voice_mode_row->setContentsMargins(0, 0, 0, 0);
    voice_mode_row->setSpacing(0);

    auto* voice_mode_badge = new QLabel(input_section);
    voice_mode_badge->setText("VOICE: READY");
    voiceModeBadge_ = voice_mode_badge;
    updateVoiceModeBadge();
    voice_mode_row->addWidget(voice_mode_badge);

    auto* auto_refresh_badge = new QLabel(input_section);
    autoRefreshBadge_ = auto_refresh_badge;
    updateAutoRefreshBadge();
    voice_mode_row->addStretch();
    voice_mode_row->addWidget(auto_refresh_badge);
    input_layout->addLayout(voice_mode_row);

    auto* heard_queue_row = new QHBoxLayout();
    heard_queue_row->setContentsMargins(0, 0, 0, 0);
    heard_queue_row->setSpacing(0);

    auto* heard_queue_badge = new QLabel(input_section);
    heard_queue_badge->setStyleSheet(
        "QLabel {"
        "  background-color: #10161f;"
        "  color: #a8d8ff;"
        "  border: 1px solid #2d4f66;"
        "  border-radius: 7px;"
        "  padding: 5px 8px;"
        "  font-family: 'Monospace';"
        "  font-size: 9px;"
        "  font-weight: 600;"
        "}"
    );
    heardQueueBadge_ = heard_queue_badge;
    updateHeardQueueBadge();
    heard_queue_row->addWidget(heard_queue_badge);
    heard_queue_row->addStretch();
    input_layout->addLayout(heard_queue_row);

    // Quick action buttons
    auto* quick_btns_layout = new QHBoxLayout();
    quick_btns_layout->setSpacing(8);
    
    QStringList quick_actions = {
        "Help", "Status", "Learn", "Voice IO", "Clear",
        "Chrome", "VS Code", "Terminal"
    };
    auto btn_style = [](const QString& gradient_start, const QString& gradient_end) -> QString {
        return QString(
            "QPushButton {"
            "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
            "    stop:0 %1, stop:1 %2);"
            "  color: #ffffff;"
            "  border: 1px solid #1d4f66;"
            "  border-radius: 6px;"
            "  font-family: 'Segoe UI', 'Calibri';"
            "  font-size: 10px;"
            "  padding: 5px 12px;"
            "}"
            "QPushButton:hover { border: 1px solid #00d8ff; }"
        ).arg(gradient_start, gradient_end);
    };
    
    std::vector<std::pair<QString, QString>> colors = {
        {"#111111", "#1a1a1a"},
        {"#0f161a", "#1d4f66"},
        {"#151515", "#2a2a2a"},
        {"#141414", "#222222"},
        {"#1a1117", "#2a1620"},
        {"#0f1a15", "#145a3f"},
        {"#10141f", "#1e3c78"},
        {"#1a1a1a", "#404040"}
    };
    
    std::vector<QPushButton*> quickButtons;
    for (int i = 0; i < static_cast<int>(quick_actions.size()); ++i) {
        auto* btn = new QPushButton(quick_actions[i], input_section);
        btn->setFixedHeight(28);
        btn->setStyleSheet(btn_style(colors[i].first, colors[i].second));
        quick_btns_layout->addWidget(btn);
        quickButtons.push_back(btn);
    }

    if (quickButtons.size() >= 8) {
        setThemedIcon(quickButtons[0], "help-browser", QStyle::SP_MessageBoxQuestion);
        setThemedIcon(quickButtons[1], "dialog-information", QStyle::SP_MessageBoxInformation);
        setThemedIcon(quickButtons[2], "applications-education", QStyle::SP_DialogHelpButton);
        setThemedIcon(quickButtons[3], "audio-input-microphone", QStyle::SP_MediaVolume);
        setThemedIcon(quickButtons[4], "edit-clear", QStyle::SP_DialogResetButton);
        setThemedIcon(quickButtons[5], "internet-web-browser", QStyle::SP_DriveNetIcon);
        setThemedIcon(quickButtons[6], "applications-development", QStyle::SP_DesktopIcon);
        setThemedIcon(quickButtons[7], "utilities-terminal", QStyle::SP_ComputerIcon);
    }
    
    quick_btns_layout->addStretch();
    input_layout->addLayout(quick_btns_layout);

    right_layout->addWidget(input_section);

    content_layout->addWidget(hud_panel);
    content_layout->addWidget(right_panel, 1);
    main_layout->addWidget(content_frame, 1);

    setCentralWidget(central);
    setWindowTitle("Codebeat • Neural Assistant");
    setWindowIcon(QIcon());
    resize(1200, 800);

    QObject::connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSend);
    QObject::connect(input_, &QLineEdit::returnPressed, this, &MainWindow::onSend);
    QObject::connect(cmd_open_chrome, &QPushButton::clicked, this, [this]() { runQuickAction("open chrome"); });
    QObject::connect(cmd_open_code, &QPushButton::clicked, this, [this]() { runQuickAction("open vs code"); });
    QObject::connect(cmd_search_cpp, &QPushButton::clicked, this, [this]() { runQuickAction("search c++ qt tutorial"); });
    QObject::connect(cmd_status, &QPushButton::clicked, this, [this]() { runQuickAction("status"); });
    QObject::connect(cmd_voice_identity, &QPushButton::clicked, this, [this]() { runQuickAction("voice identity status"); });
    QObject::connect(cmd_voice_audit_status, &QPushButton::clicked, this, [this]() { runQuickAction("voice audit status"); });
    QObject::connect(cmd_voice_audit_summary, &QPushButton::clicked, this, [this]() { runQuickAction("voice audit summary"); });
    QObject::connect(cmd_open_voice_log, &QPushButton::clicked, this, [this]() { runQuickAction("open voice log"); });
    QObject::connect(cmd_open_logs_folder, &QPushButton::clicked, this, [this]() { runQuickAction("open logs folder"); });
    QObject::connect(cmd_open_latest_export, &QPushButton::clicked, this, [this]() { runQuickAction("open latest export"); });
    QObject::connect(cmd_copy_latest_export, &QPushButton::clicked, this, [this]() { runQuickAction("copy latest export path"); });
    QObject::connect(cmd_list_exports, &QPushButton::clicked, this, [this]() { runQuickAction("list exports"); });
    QObject::connect(cmd_voice_audit_clear, &QPushButton::clicked, this, [this]() { runQuickAction("voice audit clear"); });
    QObject::connect(cmd_voice_audit_export, &QPushButton::clicked, this, [this]() { runQuickAction("voice audit export"); });
    QObject::connect(cmd_standby_on, &QPushButton::clicked, this, [this]() { runQuickAction("voice standby on"); });
    QObject::connect(cmd_standby_off, &QPushButton::clicked, this, [this]() { runQuickAction("voice standby off"); });
    QObject::connect(cmd_standby_status, &QPushButton::clicked, this, [this]() { runQuickAction("voice standby status"); });
    QObject::connect(cmd_interrupt, &QPushButton::clicked, this, [this]() { runQuickAction("stop listening"); });
    QObject::connect(cmd_enroll_owner, &QPushButton::clicked, this, [this]() { runQuickAction("voice enroll owner"); });
    QObject::connect(cmd_enroll_trusted, &QPushButton::clicked, this, [this]() { runQuickAction("voice enroll trusted"); });
    QObject::connect(cmd_battery, &QPushButton::clicked, this, [this]() { runQuickAction("check battery"); });
    QObject::connect(cmd_processes, &QPushButton::clicked, this, [this]() { runQuickAction("show running processes"); });
    QObject::connect(cmd_screenshot, &QPushButton::clicked, this, [this]() { runQuickAction("take screenshot"); });
    QObject::connect(cmd_refresh_app, &QPushButton::clicked, this, [this]() { runQuickAction("refresh app"); });
    QObject::connect(voiceBtn, &QPushButton::clicked, this, [this]() { startVoiceCapture(); });
    QObject::connect(lockBtn, &QPushButton::clicked, this, [this]() { emit lockRequested(); });

    if (quickButtons.size() >= 8) {
        QObject::connect(quickButtons[0], &QPushButton::clicked, this, [this]() { runQuickAction("help"); });
        QObject::connect(quickButtons[1], &QPushButton::clicked, this, [this]() { runQuickAction("status"); });
        QObject::connect(quickButtons[2], &QPushButton::clicked, this, [this]() { runQuickAction("how can i learn faster?"); });
        QObject::connect(quickButtons[3], &QPushButton::clicked, this, [this]() { runQuickAction("voice status"); });
        QObject::connect(quickButtons[4], &QPushButton::clicked, this, [this]() { runQuickAction("clear"); });
        QObject::connect(quickButtons[5], &QPushButton::clicked, this, [this]() { runQuickAction("open chrome"); });
        QObject::connect(quickButtons[6], &QPushButton::clicked, this, [this]() { runQuickAction("open vs code"); });
        QObject::connect(quickButtons[7], &QPushButton::clicked, this, [this]() { runQuickAction("open terminal"); });
    }

    // Welcome with styled text
    chatView_->append("<span style='color: #ff6b9d; font-weight: bold;'>[⚡ CODEBEAT]</span>");
    chatView_->append("<span style='color: #00ffff;'>System initialized and live. Good to see you.</span>");
    chatView_->append("<span style='color: #8b3dff;'>Try: I want to write some code, create folder project-notes, open chrome, voice status, refresh auto on, or use 🎙 VOICE.</span>");
    chatView_->append("");
}

void MainWindow::onSend() {
    const auto text = input_->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    chatView_->append(QString("<span style='color: #00ffff; font-weight: bold;'>You:</span> %1").arg(text));
    memory_.push("user: " + text.toStdString());

    if (text.trimmed().compare("lock", Qt::CaseInsensitive) == 0 ||
        text.trimmed().compare("lock app", Qt::CaseInsensitive) == 0 ||
        text.trimmed().compare("relock", Qt::CaseInsensitive) == 0) {
        chatView_->append("<span style='color: #ff7ab8; font-weight: bold;'>Codebeat:</span> Locking and returning to access screen...");
        input_->clear();
        emit lockRequested();
        return;
    }

    ++interaction_count_;
    QString response = "[processing] ";
    QString response_color = "#00ffff";
    
    if (text.toLower().contains("analyze")) {
        response = "🧠 Analysis mode: Send code or text for intelligent analysis.";
        response_color = "#ff6b9d";
    } else if (text.toLower().contains("clear")) {
        chatView_->clear();
        input_->clear();
        input_->setFocus();
        return;
    } else {
        response = generateAssistantReply(text);
        response_color = "#8b3dff";
    }

    chatView_->append(QString("<span style='color: %1; font-weight: bold;'>Codebeat:</span> %2")
                      .arg(response_color, response));
    memory_.push("assistant: " + response.toStdString());
    last_user_message_ = text;
    speakText(response);
    input_->clear();
    input_->setFocus();
}
#else
MainWindow::MainWindow(void* /*parent*/) noexcept {}
#endif
