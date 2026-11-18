#pragma once
#include "AppController.h"
#include <QWidget>
class QTreeWidget;
class QTextBrowser;
namespace isim {
// Technology maturity tree: domains, levels, research rates and blockers.
class TechTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit TechTreePanel(AppController *app, QWidget *parent = nullptr);
    void refresh();
private:
    AppController *m_app;
    QTreeWidget *m_tree;
    QTextBrowser *m_details;
};
}
