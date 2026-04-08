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
#include <QFrame>

#include <algorithm>

void MainWindow::runQuickAction(const QString& text) {
    input_->setText(text);
    onSend();
}

QString MainWindow::generateAssistantReply(const QString& text) {
    const auto lowered = text.trimmed().toLower();

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
        return "I can chat naturally, remember context, summarize, answer questions, and run built-in tools. Try: remember ..., status, time, summarize, what do you remember.";
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

    // Chat view with enhanced styling
    chatView_ = new QTextEdit(central);
    chatView_->setReadOnly(true);
    chatView_->setPlaceholderText("Messages appear here...");
    chatView_->setStyleSheet(
        "QTextEdit {"
        "  background-color: #0a0a0a;"
        "  color: #d7f9ff;"
        "  border: none;"
        "  font-family: 'Fira Code', 'Monospace';"
        "  font-size: 12px;"
        "  padding: 15px;"
        "  margin: 5px;"
        "  selection-background-color: #22445a;"
        "  border-radius: 0px;"
        "}"
        "QTextEdit:focus { outline: none; }"
        "QScrollBar:vertical { width: 8px; background-color: #121212; border-radius: 4px; }"
        "QScrollBar::handle:vertical { background-color: #1d4f66; border-radius: 4px; }"
    );

    main_layout->addWidget(chatView_, 1);

    // Input section with gradient border
    auto* input_section = new QFrame(central);
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
    
    QStringList quick_actions = {"💡 Help", "📊 Status", "🧠 Learn", "⚙️ Config", "🧹 Clear"};
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
        {"#1a1117", "#2a1620"}
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

    main_layout->addWidget(input_section);

    setCentralWidget(central);
    setWindowTitle("Codebeat • Neural Assistant");
    setWindowIcon(QIcon());
    resize(1200, 800);

    QObject::connect(sendBtn, &QPushButton::clicked, this, &MainWindow::onSend);
    QObject::connect(input_, &QLineEdit::returnPressed, this, &MainWindow::onSend);
    QObject::connect(lockBtn, &QPushButton::clicked, this, [this]() { emit lockRequested(); });

    if (quickButtons.size() >= 5) {
        QObject::connect(quickButtons[0], &QPushButton::clicked, this, [this]() { runQuickAction("help"); });
        QObject::connect(quickButtons[1], &QPushButton::clicked, this, [this]() { runQuickAction("status"); });
        QObject::connect(quickButtons[2], &QPushButton::clicked, this, [this]() { runQuickAction("how can i learn faster?"); });
        QObject::connect(quickButtons[3], &QPushButton::clicked, this, [this]() { runQuickAction("what can you do?"); });
        QObject::connect(quickButtons[4], &QPushButton::clicked, this, [this]() { runQuickAction("clear"); });
    }

    // Welcome with styled text
    chatView_->append("<span style='color: #ff6b9d; font-weight: bold;'>[⚡ CODEBEAT]</span>");
    chatView_->append("<span style='color: #00ffff;'>System initialized and ready.</span>");
    chatView_->append("<span style='color: #8b3dff;'>Type 'help' for available commands.</span>");
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
