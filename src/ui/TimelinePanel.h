#pragma once
#include "AppController.h"
#include <QWidget>
class QTableWidget;
class QTabWidget;
class QListWidget;
namespace isim {
// Bottom panel: notifications, construction progress, economic events, environmental alerts.
class TimelinePanel : public QWidget {
    Q_OBJECT
public:
    explicit TimelinePanel(AppController *app, QWidget *parent = nullptr);
private:
    void refresh();
    AppController *m_app;
    QTabWidget *m_tabs;
    QListWidget *m_all, *m_econ, *m_env;
    QTableWidget *m_construction;
    int m_logSize = -1;
};
}
