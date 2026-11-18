#pragma once
#include "AppController.h"
#include <QMainWindow>

class QLabel;
class QComboBox;
class QLineEdit;
class QAction;
class QTabWidget;

namespace isim {
class MapView;
class CatalogPanel;
class InfoPanel;
class AdvisorPanel;
class PolicyWorkspace;
class TechTreePanel;
class TimelinePanel;
class AIPanel;
class DashboardWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(AppController *app, QWidget *parent = nullptr);
public slots:
    void showDashboardTab(int tab);
    void setDataLayer(int index);
protected:
    void closeEvent(QCloseEvent *e) override;
private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void updateTopBar();
    void updateTitle();
    bool maybeSave();
    void newScenario();
    void openScenario();
    bool saveScenario(bool saveAs);
    void duplicateScenario();
    void askAI(const QString &task, const QJsonObject &ctx, const QString &title);
    AppController *m_app;
    MapView *m_map;
    CatalogPanel *m_catalog;
    InfoPanel *m_info;
    AdvisorPanel *m_advisor;
    PolicyWorkspace *m_policies;
    TechTreePanel *m_tech;
    TimelinePanel *m_timeline;
    AIPanel *m_ai;
    DashboardWindow *m_dashboard = nullptr;
    QTabWidget *m_leftTabs, *m_rightTabs;
    QLabel *m_kDate, *m_kBudget, *m_kGdp, *m_kPop, *m_kPower, *m_kScore;
    QAction *m_play;
    QComboBox *m_speed, *m_choropleth;
    QLineEdit *m_mapSearch;
};

} // namespace isim
