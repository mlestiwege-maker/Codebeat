#pragma once

#if defined(CODEBEAT_HAS_QT) && __has_include(<QWidget>)
#include <QWidget>
#include <QString>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class SplashScreen final : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(QWidget* parent = nullptr);
    void prepareForAuth();

protected:
    void paintEvent(class QPaintEvent* event) override;

signals:
    void finished();

private:
    void setupAuthUi();
    [[nodiscard]] bool isValidAccessKey(const QString& text) const;
    void onAuthenticate();
    void onBiometricAuthenticate();
    void updateStatus(const QString& status);
    QString status_text_{"Initializing Codebeat...   "};
    int animation_frame_{0};
    QLabel* prompt_label_{nullptr};
    QLineEdit* auth_input_{nullptr};
    QPushButton* unlock_button_{nullptr};
    QPushButton* face_auth_button_{nullptr};
    QLabel* feedback_label_{nullptr};
};
#else
class SplashScreen final {
public:
    explicit SplashScreen(void* = nullptr) {}
    void show() {}
    void finish(QWidget* = nullptr) {}
};
#endif
