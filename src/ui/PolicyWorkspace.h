#pragma once
#include "AppController.h"
#include <QWidget>

class QTreeWidget;
class QTextBrowser;
class QSlider;
class QPushButton;
class QLabel;

namespace isim {

// Government policy workspace: enact, adjust and repeal policies with full consequence information.
class PolicyWorkspace : public QWidget {
    Q_OBJECT
public:
    explicit PolicyWorkspace(AppController *app, QWidget *parent = nullptr);
    void refresh();
signals:
    void askAI(const QString &task, const QJsonObject &context, const QString &title);
private:
    void showPolicy();
    QString currentId() const;
    AppController *m_app;
    QTreeWidget *m_tree;
    QTextBrowser *m_details;
    QSlider *m_intensity;
    QLabel *m_intLabel, *m_budget;
    QPushButton *m_enact, *m_repeal, *m_ai;
};

} // namespace isim
