#pragma once

#if defined(CODEBEAT_HAS_QT) && __has_include(<QMainWindow>)
#include <QMainWindow>
#include <QString>

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
    QString generateAssistantReply(const QString& text);

    QTextEdit* chatView_{nullptr};
    QLineEdit* input_{nullptr};
    std::vector<QString> notes_{};
    QString last_user_message_{};
    int interaction_count_{0};
};
#else
class MainWindow final {
public:
    explicit MainWindow(void* parent = nullptr) noexcept;
    void show() noexcept {}
};
#endif
