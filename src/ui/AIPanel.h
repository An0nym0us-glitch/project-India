#pragma once
#include "AppController.h"
#include <QJsonObject>
#include <QWidget>
class QComboBox;
class QLabel;
class QTextBrowser;
class QLineEdit;
class QPushButton;
namespace isim {
// AI analysis panel: provider status, model control, structured validated answers.
class AIPanel : public QWidget {
    Q_OBJECT
public:
    explicit AIPanel(AppController *app, QWidget *parent = nullptr);
    void ask(const QString &task, const QJsonObject &context, const QString &title);
signals:
    void openSettings();
private:
    void updateStatus();
    void showAnswer(const AIAnswer &a);
    AppController *m_app;
    QComboBox *m_provider;
    QLabel *m_status;
    QPushButton *m_startStop;
    QTextBrowser *m_out;
    QLineEdit *m_question;
    QHash<QString, QString> m_titles;
    QString m_html;
};
}
