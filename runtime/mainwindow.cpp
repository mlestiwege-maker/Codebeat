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
#include <QRegularExpression>
#include <QFrame>

#include <algorithm>

bool MainWindow::launchAny(const QStringList& executables, const QStringList& args) {
    for (const auto& exe : executables) {
        if (QProcess::startDetached(exe, args)) {
            return true;
        }
    }
    return false;
}

QString MainWindow::captureVoiceCommand() {
    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp(); // from build_gui -> project root
    const QString scriptPath = d.absoluteFilePath("voice_recognize.sh");

    QProcess proc;
    proc.start("bash", {"-lc", "\"" + scriptPath + "\""});

    if (!proc.waitForFinished(35000)) {
        proc.kill();
        return "__VOICE_ERROR__Voice capture timed out";
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const auto err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
        const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        const auto reason = !err.isEmpty() ? err : (!out.isEmpty() ? out : QString("Voice recognition failed"));
        return "__VOICE_ERROR__" + reason.left(170);
    }

    const auto out = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    return out;
}

QString MainWindow::tryHandleSystemTask(const QString& text, bool& handled) {
    handled = false;
    const auto lowered = text.trimmed().toLower();

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
        const bool ok = QProcess::startDetached("bash", {"-lc", "pkill -f \"" + app + "\""});
        return ok ? ("Attempting to close: " + app) : "Could not start close command.";
    }

    if ((lowered.startsWith("run ") && lowered.size() > 4) ||
        (lowered.startsWith("execute ") && lowered.size() > 8)) {
        handled = true;
        const auto cmd = lowered.startsWith("execute ") ? text.mid(8).trimmed() : text.mid(4).trimmed();
        const bool ok = QProcess::startDetached("bash", {"-lc", cmd});
        return ok ? ("Running command: " + cmd) : "Command execution failed to start.";
    }

    if (lowered == "what can you control" || lowered == "apps") {
        handled = true;
        return "I can open apps and run tasks. Try: open chrome, open vs code, open terminal, open <app>, open https://..., search <query>, close <app>, run <command>.";
    }

    return {};
}

void MainWindow::runQuickAction(const QString& text) {
    input_->setText(text);
    onSend();
}

QString MainWindow::generateAssistantReply(const QString& text) {
    const auto lowered = text.trimmed().toLower();

    bool handled = false;
    const auto taskReply = tryHandleSystemTask(text, handled);
    if (handled) {
        return taskReply;
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
        return "I can chat naturally, remember context, summarize, answer questions, and control your system. Try: open chrome, open vs code, open terminal, search linux c++, run ls, close chrome, remember ..., status, time.";
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

    return "Understood. My take: " + text + "\nIf you want, I can break that into concrete next steps.";
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
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

    auto* quick_label = new QLabel("Quick Control", hud_panel);
    quick_label->setStyleSheet("color:#7ec7dd; font-family:'Segoe UI'; font-size:11px; font-weight:600; border:none;");
    hud_layout->addWidget(quick_label);

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

    auto* cmd_open_chrome = makeHudBtn("▶ open chrome");
    auto* cmd_open_code = makeHudBtn("▶ open vs code");
    auto* cmd_search_cpp = makeHudBtn("▶ search c++ qt tutorial");
    auto* cmd_status = makeHudBtn("▶ status");

    hud_layout->addWidget(cmd_open_chrome);
    hud_layout->addWidget(cmd_open_code);
    hud_layout->addWidget(cmd_search_cpp);
    hud_layout->addWidget(cmd_status);
    hud_layout->addStretch();

    auto* terminal_hint = new QLabel("Tip: type `what can you control` for full command list.", hud_panel);
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

    // Quick action buttons
    auto* quick_btns_layout = new QHBoxLayout();
    quick_btns_layout->setSpacing(8);
    
    QStringList quick_actions = {
        "💡 Help", "📊 Status", "🧠 Learn", "⚙️ Config", "🧹 Clear",
        "🌐 Chrome", "🧩 VS Code", "🖥 Terminal"
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
    QObject::connect(voiceBtn, &QPushButton::clicked, this, [this]() {
        chatView_->append("<span style='color:#7fffcf;'>Codebeat:</span> Listening for voice command...");
        const auto heard = captureVoiceCommand();
        if (heard.startsWith("__VOICE_ERROR__")) {
            const auto reason = heard.mid(QString("__VOICE_ERROR__").size());
            chatView_->append(QString("<span style='color:#ffcc66;'>Codebeat:</span> %1").arg(reason));
            return;
        }
        if (heard.isEmpty()) {
            chatView_->append("<span style='color:#ffcc66;'>Codebeat:</span> Voice recognition unavailable or no speech captured.");
            return;
        }

        input_->setText(heard);
        chatView_->append(QString("<span style='color:#7fffcf;'>Codebeat:</span> Heard: %1").arg(heard));
        onSend();
    });
    QObject::connect(lockBtn, &QPushButton::clicked, this, [this]() { emit lockRequested(); });

    if (quickButtons.size() >= 8) {
        QObject::connect(quickButtons[0], &QPushButton::clicked, this, [this]() { runQuickAction("help"); });
        QObject::connect(quickButtons[1], &QPushButton::clicked, this, [this]() { runQuickAction("status"); });
        QObject::connect(quickButtons[2], &QPushButton::clicked, this, [this]() { runQuickAction("how can i learn faster?"); });
        QObject::connect(quickButtons[3], &QPushButton::clicked, this, [this]() { runQuickAction("what can you do?"); });
        QObject::connect(quickButtons[4], &QPushButton::clicked, this, [this]() { runQuickAction("clear"); });
        QObject::connect(quickButtons[5], &QPushButton::clicked, this, [this]() { runQuickAction("open chrome"); });
        QObject::connect(quickButtons[6], &QPushButton::clicked, this, [this]() { runQuickAction("open vs code"); });
        QObject::connect(quickButtons[7], &QPushButton::clicked, this, [this]() { runQuickAction("open terminal"); });
    }

    // Welcome with styled text
    chatView_->append("<span style='color: #ff6b9d; font-weight: bold;'>[⚡ CODEBEAT]</span>");
    chatView_->append("<span style='color: #00ffff;'>System initialized and live. Good to see you.</span>");
    chatView_->append("<span style='color: #8b3dff;'>Try: open chrome, open vs code, open terminal, run ls, help, or use 🎙 VOICE.</span>");
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
    input_->clear();
    input_->setFocus();
}
#else
MainWindow::MainWindow(void* /*parent*/) noexcept {}
#endif
