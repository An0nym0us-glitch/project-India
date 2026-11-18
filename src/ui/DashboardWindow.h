#pragma once
#include "AppController.h"
#include "scenario/ProjectionRunner.h"
#include <QWidget>

class QTabWidget;
class QTableWidget;
class QComboBox;
class QSpinBox;
class QListWidget;
class QLabel;
class QPushButton;
class QCheckBox;
QT_BEGIN_NAMESPACE
class QChartView;
QT_END_NAMESPACE

namespace isim {

// Separate dashboard window: national statistics, global country comparison,
// scenario projections & comparison, and the assumptions register.
class DashboardWindow : public QWidget {
    Q_OBJECT
public:
    explicit DashboardWindow(AppController *app, QWidget *parent = nullptr);
    void showTab(int i);
signals:
    void askAI(const QString &task, const QJsonObject &context, const QString &title);
private:
    QWidget *buildStats();
    QWidget *buildComparison();
    QWidget *buildScenarios();
    QWidget *buildAssumptions();
    void refreshStats();
    void refreshComparison();
    void refreshComparisonChart();
    void runProjections();
    void refreshProjectionChart();
    AppController *m_app;
    QTabWidget *m_tabs;
    // stats
    QTableWidget *m_kpis;
    QComboBox *m_histIndicator;
    QChartView *m_histChart;
    // comparison
    QComboBox *m_baseYear, *m_group;
    QTableWidget *m_compTable;
    QChartView *m_compChart, *m_trendChart;
    QLabel *m_compSummary;
    // scenarios
    QComboBox *m_horizon, *m_projIndicator;
    QSpinBox *m_members;
    QListWidget *m_scenarioList;
    QChartView *m_projChart;
    QTableWidget *m_projTable;
    QLabel *m_projNotes;
    QVector<ProjectionResult> m_projections;
    QStringList m_extraFiles;
};

} // namespace isim
