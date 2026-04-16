#include "runtime/splash.hpp"

#if defined(CODEBEAT_HAS_QT) && __has_include(<QWidget>)
#include <QApplication>
#include <QFont>
#include <QFontMetrics>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QCoreApplication>
#include <QDir>
#include <QInputDialog>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <cmath>
#include <chrono>
#include <vector>

bool SplashScreen::faceOnlyMode() const {
    return qEnvironmentVariableIntValue("CODEBEAT_FACE_ONLY") != 0;
}

SplashScreen::SplashScreen(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::SplashScreen | Qt::FramelessWindowHint);
    setFixedSize(900, 500);

    // Center on screen
    auto* screen = QApplication::primaryScreen();
    auto geom = screen->availableGeometry();
    move(geom.center() - rect().center());

    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this]() {
        animation_frame_ = (animation_frame_ + 1) % 4;
        update();
    });
    timer->start(200);

    setupAuthUi();
}

void SplashScreen::prepareForAuth() {
    updateStatus("Initializing Codebeat...   ");
    if (auth_input_ != nullptr) {
        auth_input_->clear();
        auth_input_->setFocus();
    }
    if (feedback_label_ != nullptr) {
        feedback_label_->clear();
    }

    if (face_only_mode_) {
        QTimer::singleShot(350, this, [this]() { onBiometricAuthenticate(); });
    }
}

void SplashScreen::setupAuthUi() {
    face_only_mode_ = faceOnlyMode();

    const auto os_user = qEnvironmentVariable("USER", "operator");
    prompt_label_ = new QLabel(
        face_only_mode_
            ? ("Welcome " + os_user + " • Face unlock only")
            : ("Welcome " + os_user + " • Authenticate with passkey or biometrics"),
        this);
    prompt_label_->setAlignment(Qt::AlignCenter);
    prompt_label_->setStyleSheet(
        "QLabel {"
        " color: #c8f5ff;"
        " font-family: 'Segoe UI', 'Calibri';"
        " font-size: 13px;"
        " font-weight: 600;"
        " background: transparent;"
        "}"
    );
    prompt_label_->setGeometry(150, 320, 600, 24);

    auth_input_ = new QLineEdit(this);
    auth_input_->setPlaceholderText("Enter wake word or passkey...");
    auth_input_->setEchoMode(QLineEdit::Password);
    auth_input_->setGeometry(250, 350, 300, 40);
    auth_input_->setStyleSheet(
        "QLineEdit {"
        "  background-color: rgba(20, 10, 45, 220);"
        "  color: #a8eeff;"
        "  border: 2px solid #5f2db1;"
        "  border-radius: 8px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 12px;"
        "  padding: 0 12px;"
        "}"
        "QLineEdit:focus {"
        "  border: 2px solid #00d5ff;"
        "}"
    );

    if (face_only_mode_) {
        auth_input_->hide();
    }

    unlock_button_ = new QPushButton("Unlock", this);
    unlock_button_->setGeometry(540, 350, 85, 40);
    unlock_button_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7b35ff, stop:1 #00b8e6);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #8c4dff, stop:1 #19c7f3);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #5c21cf, stop:1 #0090b3);"
        "}"
    );

    if (face_only_mode_) {
        unlock_button_->hide();
    }

    face_auth_button_ = new QPushButton("Face Auth", this);
    face_auth_button_->setGeometry(630, 350, 100, 40);
    face_auth_button_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #16a085, stop:1 #1abc9c);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1cb99a, stop:1 #2ad3af);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #0f7f69, stop:1 #13a88b);"
        "}"
    );

    if (face_only_mode_) {
        face_auth_button_->setText("Face Unlock");
        face_auth_button_->setGeometry(585, 350, 140, 40);
    }

    feedback_label_ = new QLabel(this);
    feedback_label_->setAlignment(Qt::AlignCenter);
    feedback_label_->setWordWrap(true);
    feedback_label_->setStyleSheet(
        "QLabel {"
        " color: #ff86b6;"
        " font-family: 'Segoe UI', 'Calibri';"
        " font-size: 11px;"
        " background: transparent;"
        "}"
    );
    feedback_label_->setGeometry(180, 396, 540, 46);

    enroll_face_button_ = new QPushButton("Enroll Face", this);
    enroll_face_button_->setGeometry(735, 350, 110, 40);
    enroll_face_button_->setStyleSheet(
        "QPushButton {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #845EC2, stop:1 #6F42C1);"
        "  color: #ffffff;"
        "  border: none;"
        "  border-radius: 8px;"
        "  font-family: 'Segoe UI', 'Calibri';"
        "  font-size: 12px;"
        "  font-weight: 700;"
        "}"
        "QPushButton:hover {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #9569cf, stop:1 #7f57cf);"
        "}"
        "QPushButton:pressed {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #7049ab, stop:1 #5f37a8);"
        "}"
    );

    connect(unlock_button_, &QPushButton::clicked, this, [this]() { onAuthenticate(); });
    connect(face_auth_button_, &QPushButton::clicked, this, [this]() { onBiometricAuthenticate(); });
    connect(enroll_face_button_, &QPushButton::clicked, this, [this]() { onEnrollFace(); });
    connect(auth_input_, &QLineEdit::returnPressed, this, [this]() { onAuthenticate(); });
    auth_input_->setFocus();

    if (face_only_mode_) {
        feedback_label_->setStyleSheet(
            "QLabel { color: #8befff; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Face-only mode enabled. Scanning your face now...");
        QTimer::singleShot(450, this, [this]() { onBiometricAuthenticate(); });
    }
}

bool SplashScreen::isValidAccessKey(const QString& text) const {
    const auto token = text.trimmed().toLower();
    return token == "code10111011" ||
           token == "code1011011" ||
           token == "codebeat" ||
           token == "open codebeat" ||
           token == "activate" ||
           token == "activate codebeat";
}

void SplashScreen::onAuthenticate() {
    if (face_only_mode_) {
        feedback_label_->setStyleSheet(
            "QLabel { color: #ffcc66; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Face-only mode is enabled. Use Face Unlock or Enroll Face.");
        return;
    }

    const auto entered = auth_input_->text();
    if (isValidAccessKey(entered)) {
        updateStatus("Access granted • entering system...");
        feedback_label_->setStyleSheet(
            "QLabel { color: #7bffba; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Access granted");
        emit finished();
        return;
    }

    feedback_label_->setStyleSheet(
        "QLabel { color: #ff86b6; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
    );
    feedback_label_->setText("Invalid wake word/passkey. Try again.");
    auth_input_->selectAll();
    auth_input_->setFocus();
}

void SplashScreen::onBiometricAuthenticate() {
    updateStatus("Biometric scan in progress...");

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp(); // from build_gui -> project root
    const QString scriptPath = d.absoluteFilePath("face_auth.sh");

    // Linux face auth hook:
    // 1) howdy test if available
    // 2) OpenCV python fallback
    QProcess proc;
    proc.start("bash", {"-lc", "\"" + scriptPath + "\""});
    const bool done = proc.waitForFinished(18000);

    if (done && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        updateStatus("Biometric authentication successful • entering system...");
        feedback_label_->setStyleSheet(
            "QLabel { color: #7bffba; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Biometric ID confirmed");
        emit finished();
        return;
    }

    const auto err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    QString shortErr = err.isEmpty() ? QString("Face authentication unavailable/failed") : err;
    if (shortErr.contains("No enrolled owner face profile found", Qt::CaseInsensitive)) {
        shortErr = "No face profile yet. Run ./face_auth.sh --enroll, then try Face Auth again.";
    } else {
        const auto lines = shortErr.split('\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            shortErr = lines.back().trimmed();
        }
    }

    updateStatus("Biometric check unavailable or failed");
    feedback_label_->setStyleSheet(
        "QLabel { color: #ffcc66; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
    );
    feedback_label_->setText(shortErr + (face_only_mode_ ? " • re-enroll or fix camera" : " • use passkey unlock"));
    auth_input_->setFocus();
}

void SplashScreen::onEnrollFace() {
    bool ok = false;
    const QString pass = QInputDialog::getText(
        this,
        "Authorize Face Enrollment",
        "Enter passkey to enroll face:",
        QLineEdit::Password,
        QString(),
        &ok);

    if (!ok) {
        return;
    }

    if (!isValidAccessKey(pass)) {
        feedback_label_->setStyleSheet(
            "QLabel { color: #ff86b6; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Invalid passkey. Face enrollment denied.");
        return;
    }

    updateStatus("Face enrollment started... look at camera");

    const QString appDir = QCoreApplication::applicationDirPath();
    QDir d(appDir);
    d.cdUp();
    const QString scriptPath = d.absoluteFilePath("face_auth.sh");

    QProcess proc;
    proc.start("bash", {"-lc", "\"" + scriptPath + "\" --enroll"});
    const bool done = proc.waitForFinished(45000);

    if (done && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0) {
        updateStatus("Face enrollment complete. You can now use Face Auth.");
        feedback_label_->setStyleSheet(
            "QLabel { color: #7bffba; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
        );
        feedback_label_->setText("Enrollment complete. Click Face Auth to unlock.");
        return;
    }

    const auto err = QString::fromUtf8(proc.readAllStandardError()).trimmed();
    QString shortErr = err.isEmpty() ? QString("Face enrollment failed") : err;
    const auto lines = shortErr.split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        shortErr = lines.back().trimmed();
    }

    if (shortErr.contains("already enrolled", Qt::CaseInsensitive) || proc.exitCode() == 15) {
        shortErr = "Face enrollment limit reached (2/2). Face unlock is active; no more enrollment.";
    }

    updateStatus("Face enrollment unavailable or failed");
    feedback_label_->setStyleSheet(
        "QLabel { color: #ffcc66; font-family: 'Segoe UI', 'Calibri'; font-size: 11px; background: transparent; }"
    );
    feedback_label_->setText(shortErr + " • you can still use passkey unlock");
}

void SplashScreen::updateStatus(const QString& status) {
    status_text_ = status;
    update();
}

void SplashScreen::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    // Gradient background: deep purple to dark blue
    QLinearGradient bg(0, 0, width(), height());
    bg.setColorAt(0.0, QColor(15, 10, 40));
    bg.setColorAt(0.5, QColor(20, 15, 50));
    bg.setColorAt(1.0, QColor(10, 20, 45));
    p.fillRect(rect(), bg);

    // Animated outer aura (purple glow)
    float pulse = 0.5f + 0.5f * std::sin(animation_frame_ * 0.5f);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(200, 100, 255, static_cast<int>(30 * pulse)));
    p.drawEllipse(rect().center(), 250, 250);

    // Mid-frequency aura (cyan)
    p.setBrush(QColor(0, 200, 255, static_cast<int>(50 * pulse)));
    p.drawEllipse(rect().center(), 180, 180);

    // Inner core (bright gold/yellow)
    p.setBrush(QColor(255, 200, 100, static_cast<int>(60 * pulse)));
    p.drawEllipse(rect().center(), 100, 100);

    // Logo with gradient effect
    QFont logo_font("Monospace", 56, QFont::Bold);
    p.setFont(logo_font);
    
    // Create text path for gradient fill
    QLinearGradient text_grad(rect().left(), rect().top(), rect().right(), rect().bottom());
    text_grad.setColorAt(0.0, QColor(0, 255, 200));
    text_grad.setColorAt(0.5, QColor(100, 200, 255));
    text_grad.setColorAt(1.0, QColor(255, 150, 100));
    
    p.setPen(QPen(QBrush(text_grad), 2));
    p.drawText(rect().adjusted(0, 60, 0, 0), Qt::AlignHCenter | Qt::AlignTop, "CODEBEAT");

    // Subtitle with modern styling
    QFont sub_font("Segoe UI, Calibri", 16, QFont::Normal);
    p.setFont(sub_font);
    p.setPen(QColor(200, 255, 255, 220));
    p.drawText(rect().adjusted(0, 150, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               "Intelligent Assistant");
    
    QFont tag_font("Segoe UI, Calibri", 12, QFont::Normal);
    p.setFont(tag_font);
    p.setPen(QColor(150, 200, 200, 180));
    p.drawText(rect().adjusted(0, 179, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
               "From-Scratch C++ Neural AI");

    // Animated loading bars
    int bar_y = 280;
    p.setPen(Qt::NoPen);
    for (int i = 0; i < 20; ++i) {
        float phase = (i / 20.0f + animation_frame_ * 0.1f);
        float brightness = 0.3f + 0.7f * (0.5f + 0.5f * std::sin(phase * 3.14159f * 2.0f));
        
        QColor bar_col = (i % 2 == 0) 
            ? QColor(100, 255, 200, static_cast<int>(255 * brightness))
            : QColor(200, 100, 255, static_cast<int>(255 * brightness));
        
        p.fillRect(100 + i * 30, bar_y, 20, 6, bar_col);
    }

    // Status text with modern font
    p.setFont(QFont("Segoe UI, Calibri", 11, QFont::Normal));
    p.setPen(QColor(150, 200, 220, 200));
    p.drawText(rect().adjusted(0, 330, 0, 0), Qt::AlignHCenter | Qt::AlignTop, status_text_);

    // Modern separator line with gradient
    QLinearGradient line_grad(100, height() - 70, width() - 100, height() - 70);
    line_grad.setColorAt(0.0, QColor(100, 200, 255, 0));
    line_grad.setColorAt(0.5, QColor(200, 100, 255, 200));
    line_grad.setColorAt(1.0, QColor(100, 200, 255, 0));
    p.setPen(QPen(QBrush(line_grad), 3));
    p.drawLine(100, height() - 70, width() - 100, height() - 70);

    // System status boxes
    p.setFont(QFont("Monospace", 9, QFont::Normal));
    struct StatusBox {
        int x, y, w, h;
        QString text;
        QColor color;
    };
    
    std::vector<StatusBox> boxes = {
        {30, height() - 50, 120, 25, "ML ENGINE", QColor(100, 255, 200)},
        {170, height() - 50, 120, 25, "SAFETY ACTIVE", QColor(255, 200, 100)},
        {310, height() - 50, 120, 25, "TOKENIZER", QColor(200, 100, 255)}
    };
    
    for (const auto& box : boxes) {
        p.setPen(QPen(box.color, 1));
        p.setBrush(QColor(box.color.red(), box.color.green(), box.color.blue(), 20));
        p.drawRect(box.x, box.y, box.w, box.h);
        p.setPen(box.color);
        p.drawText(box.x + 5, box.y, box.w - 10, box.h, Qt::AlignVCenter | Qt::AlignHCenter, box.text);
    }
}

#endif
