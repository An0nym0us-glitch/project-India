#pragma once
#include "AppController.h"
#include <QWidget>

class QTreeWidget;
class QTextBrowser;
class QComboBox;
class QPushButton;

namespace isim {

// NATIONAL DEVELOPMENT ADVISOR: recommendations derived from simulation data.
class AdvisorPanel : public QWidget {
    Q_OBJECT
public:
    explicit AdvisorPanel(AppController *app, QWidget *parent = nullptr);
    void refresh();
signals:
    void askAI(const QString &task, const QJsonObject &context, const QString &title);
private:
    void showDetails();
    const Recommendation *current() const;
    AppController *m_app;
    QTreeWidget *m_list;
    QTextBrowser *m_details;
    QComboBox *m_filter;
    QPushButton *m_accept, *m_reject, *m_save, *m_ai, *m_reanalyse;
};

} // namespace isim
