#include "MainWindow.h"
#include "AIPanel.h"
#include "AdvisorPanel.h"
#include "CatalogPanel.h"
#include "DashboardWindow.h"
#include "Format.h"
#include "InfoPanel.h"
#include "MapView.h"
#include "PolicyWorkspace.h"
#include "SettingsDialog.h"
#include "TechTreePanel.h"
#include "TimelinePanel.h"
#include "ai/AppSettings.h"
#include "gis/GeoDatabase.h"
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QCompleter>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringListModel>
#include <QTabWidget>
#include <QToolBar>
#include <QMenu>
#include <QToolButton>

namespace isim {
using namespace fmt;

MainWindow::MainWindow(AppController *app, QWidget *parent) : QMainWindow(parent), m_app(app)
{
    setDockNestingEnabled(true);
    m_map = new MapView(app);
    setCentralWidget(m_map);
    buildDocks();
    buildToolbar();
    buildMenus();
    statusBar()->showMessage(tr("Ready. Empty development layer — start building from the Build panel."));
    connect(m_map, &MapView::hoverInfo, this, [this](const QString &t) { statusBar()->showMessage(t); });
    connect(m_map, &MapView::suitabilityPreview, m_info, &InfoPanel::showPreview);
    connect(app, &AppController::worldUpdated, this, &MainWindow::updateTopBar);
    connect(app, &AppController::worldReset, this, &MainWindow::updateTitle);
    connect(app, &AppController::statusMessage, this, [this](const QString &t) { statusBar()->showMessage(t, 5000); });
    connect(app, &AppController::selectionChanged, this, [this]() {
        if (m_app->selection().type != Selection::None) m_rightTabs->setCurrentIndex(0);
    });
    resize(1600, 960);
    const QByteArray geom = AppSettings::s().value("ui/geometry").toByteArray();
    if (!geom.isEmpty()) restoreGeometry(geom);
    const QByteArray state = AppSettings::s().value("ui/state").toByteArray();
    if (!state.isEmpty()) restoreState(state);
    updateTopBar();
    updateTitle();
}

void MainWindow::buildDocks()
{
    // LEFT: Build / Advisor / Policies / Technology
    m_leftTabs = new QTabWidget;
    m_catalog = new CatalogPanel(m_app);
    m_advisor = new AdvisorPanel(m_app);
    m_policies = new PolicyWorkspace(m_app);
    m_tech = new TechTreePanel(m_app);
    m_leftTabs->addTab(m_catalog, tr("Build"));
    m_leftTabs->addTab(m_advisor, tr("Advisor"));
    m_leftTabs->addTab(m_policies, tr("Policies"));
    m_leftTabs->addTab(m_tech, tr("Technology"));
    connect(m_leftTabs, &QTabWidget::currentChanged, this, [this](int i) { if (i == 1) m_advisor->refresh(); if (i == 2) m_policies->refresh(); if (i == 3) m_tech->refresh(); });
    auto *left = new QDockWidget(tr("Planning"), this);
    left->setObjectName("dock.left");
    left->setWidget(m_leftTabs);
    left->setMinimumWidth(360);
    addDockWidget(Qt::LeftDockWidgetArea, left);
    // RIGHT: information + AI
    m_rightTabs = new QTabWidget;
    m_info = new InfoPanel(m_app);
    m_ai = new AIPanel(m_app);
    m_rightTabs->addTab(m_info, tr("Information"));
    m_rightTabs->addTab(m_ai, tr("AI analysis"));
    auto *right = new QDockWidget(tr("Details"), this);
    right->setObjectName("dock.right");
    right->setWidget(m_rightTabs);
    right->setMinimumWidth(400);
    addDockWidget(Qt::RightDockWidgetArea, right);
    // BOTTOM: timeline
    m_timeline = new TimelinePanel(m_app);
    auto *bottom = new QDockWidget(tr("Timeline"), this);
    bottom->setObjectName("dock.bottom");
    bottom->setWidget(m_timeline);
    addDockWidget(Qt::BottomDockWidgetArea, bottom);
    resizeDocks({bottom}, {170}, Qt::Vertical);
    connect(m_info, &InfoPanel::askAI, this, &MainWindow::askAI);
    connect(m_advisor, &AdvisorPanel::askAI, this, &MainWindow::askAI);
    connect(m_policies, &PolicyWorkspace::askAI, this, &MainWindow::askAI);
    connect(m_ai, &AIPanel::openSettings, this, [this]() { SettingsDialog(m_app, this).exec(); });
}

void MainWindow::showDashboardTab(int tab)
{
    if (!m_dashboard) { m_dashboard = new DashboardWindow(m_app, this); connect(m_dashboard, &DashboardWindow::askAI, this, &MainWindow::askAI); }
    m_dashboard->showTab(tab);
}

void MainWindow::setDataLayer(int index) { m_choropleth->setCurrentIndex(index); }

void MainWindow::askAI(const QString &task, const QJsonObject &ctx, const QString &title)
{
    m_rightTabs->setCurrentWidget(m_ai);
    m_ai->ask(task, ctx, title);
}

static QLabel *kpi(const QString &title)
{
    auto *l = new QLabel;
    l->setToolTip(title);
    l->setStyleSheet("QLabel { padding: 2px 10px; border-right: 1px solid palette(mid); }");
    return l;
}

void MainWindow::buildToolbar()
{
    auto *tb = addToolBar(tr("National indicators"));
    tb->setObjectName("toolbar.top");
    tb->setMovable(false);
    auto *country = new QLabel(tr("<b style='font-size:13pt'>India</b>"));
    country->setStyleSheet("padding: 0 10px");
    tb->addWidget(country);
    m_kDate = kpi(tr("Simulation date")); m_kBudget = kpi(tr("Government treasury (debt in brackets)")); m_kGdp = kpi(tr("GDP (nominal, simulated)"));
    m_kPop = kpi(tr("Population")); m_kPower = kpi(tr("Electricity demand served")); m_kScore = kpi(tr("National development score: composite of 9 indicators, equal weights (modelling choice)"));
    for (auto *l : {m_kDate, m_kBudget, m_kGdp, m_kPop, m_kPower, m_kScore}) tb->addWidget(l);
    auto *spacer = new QWidget; spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    auto *sim = addToolBar(tr("Simulation"));
    sim->setObjectName("toolbar.sim");
    m_play = sim->addAction(tr("▶ Run"));
    m_play->setShortcut(Qt::Key_Space);
    m_play->setToolTip(tr("Run / pause (Space)"));
    connect(m_play, &QAction::triggered, this, [this]() { m_app->isRunning() ? m_app->pause() : m_app->play(); });
    sim->addAction(tr("+1 month"), this, [this]() { m_app->stepMonths(1); });
    sim->addAction(tr("+1 year"), this, [this]() { m_app->stepMonths(12); });
    m_speed = new QComboBox;
    for (auto p : QVector<QPair<QString, double>>{{tr("0.5 mo/s"), 0.5}, {tr("1 mo/s"), 1}, {tr("2 mo/s"), 2}, {tr("4 mo/s"), 4}, {tr("12 mo/s"), 12}}) m_speed->addItem(p.first, p.second);
    m_speed->setCurrentIndex(2);
    connect(m_speed, &QComboBox::currentIndexChanged, this, [this]() { m_app->setSpeed(m_speed->currentData().toDouble()); });
    sim->addWidget(m_speed);
    sim->addSeparator();
    sim->addAction(tr("Dashboard"), this, [this]() { if (!m_dashboard) { m_dashboard = new DashboardWindow(m_app, this); connect(m_dashboard, &DashboardWindow::askAI, this, &MainWindow::askAI); } m_dashboard->showTab(0); });
    sim->addAction(tr("Global comparison"), this, [this]() { if (!m_dashboard) { m_dashboard = new DashboardWindow(m_app, this); connect(m_dashboard, &DashboardWindow::askAI, this, &MainWindow::askAI); } m_dashboard->showTab(1); });

    addToolBarBreak();
    auto *maptb = addToolBar(tr("Map"));
    maptb->setObjectName("toolbar.map");
    m_mapSearch = new QLineEdit;
    m_mapSearch->setPlaceholderText(tr("Search map: state, district, river, region…"));
    m_mapSearch->setClearButtonEnabled(true);
    m_mapSearch->setMinimumWidth(280);
    QStringList names;
    const auto &geo = m_app->resources().geo;
    for (const auto &s : geo.states()) names << s.name;
    for (const auto &d : geo.districts()) names << d.name;
    for (const auto &r : geo.rivers()) { const auto n = r.properties.value("name").toString(); if (!n.isEmpty() && !names.contains(n)) names << n; }
    names.removeDuplicates();
    auto *comp = new QCompleter(names, this);
    comp->setCaseSensitivity(Qt::CaseInsensitive);
    comp->setFilterMode(Qt::MatchContains);
    m_mapSearch->setCompleter(comp);
    auto go = [this]() {
        const auto hits = m_app->resources().geo.search(m_mapSearch->text());
        if (hits.isEmpty()) { statusBar()->showMessage(tr("No location matches '%1'").arg(m_mapSearch->text()), 4000); return; }
        const auto &h = hits.first();
        m_map->fitBounds(h.bounds.adjusted(-0.4, -0.4, 0.4, 0.4));
        if (h.kind == SearchHit::StateHit) m_app->select({Selection::State, h.index, {}});
        if (h.kind == SearchHit::DistrictHit) m_app->select({Selection::District, h.index, {}});
        statusBar()->showMessage(h.label, 5000);
    };
    connect(m_mapSearch, &QLineEdit::returnPressed, this, go);
    connect(comp, QOverload<const QString &>::of(&QCompleter::activated), this, go);
    maptb->addWidget(m_mapSearch);
    maptb->addSeparator();
    maptb->addWidget(new QLabel(tr(" Data layer: ")));
    m_choropleth = new QComboBox;
    for (auto c : {Choropleth::None, Choropleth::PopulationDensity, Choropleth::ElectricityServed, Choropleth::WaterServed, Choropleth::Unemployment,
                   Choropleth::HousingShortage, Choropleth::GdpPerCapita, Choropleth::AirQuality, Choropleth::UrbanShare, Choropleth::Literacy2011})
        m_choropleth->addItem(MapView::choroplethName(c), int(c));
    connect(m_choropleth, &QComboBox::currentIndexChanged, this, [this]() { m_map->layers().choropleth = Choropleth(m_choropleth->currentData().toInt()); m_map->invalidate(); });
    maptb->addWidget(m_choropleth);
    auto *layersBtn = new QToolButton;
    layersBtn->setText(tr("Layers ▾"));
    layersBtn->setPopupMode(QToolButton::InstantPopup);
    auto *lm = new QMenu(layersBtn);
    auto addLayer = [&](const QString &name, bool MapLayers::*field) {
        auto *a = lm->addAction(name);
        a->setCheckable(true);
        a->setChecked(m_map->layers().*field);
        connect(a, &QAction::toggled, this, [this, field](bool on) { m_map->layers().*field = on; m_map->invalidate(); });
    };
    addLayer(tr("District boundaries"), &MapLayers::districts);
    addLayer(tr("Rivers"), &MapLayers::rivers);
    addLayer(tr("Lakes"), &MapLayers::lakes);
    addLayer(tr("Mountain ranges, deserts & plains"), &MapLayers::physical);
    addLayer(tr("Neighbouring countries"), &MapLayers::neighbours);
    addLayer(tr("Labels"), &MapLayers::labels);
    lm->addSeparator();
    addLayer(tr("Facilities"), &MapLayers::facilities);
    addLayer(tr("Networks (roads, rail, power, water, fibre)"), &MapLayers::networks);
    addLayer(tr("Unbridged river crossings"), &MapLayers::crossings);
    addLayer(tr("Air pollution grid (PM2.5)"), &MapLayers::pollutionGrid);
    layersBtn->setMenu(lm);
    maptb->addWidget(layersBtn);
    maptb->addAction(tr("Fit India"), m_map, &MapView::fitIndia);
}

void MainWindow::buildMenus()
{
    auto *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&New scenario…"), QKeySequence::New, this, &MainWindow::newScenario);
    file->addAction(tr("&Open scenario…"), QKeySequence::Open, this, &MainWindow::openScenario);
    file->addAction(tr("&Save"), QKeySequence::Save, this, [this]() { saveScenario(false); });
    file->addAction(tr("Save &as…"), QKeySequence::SaveAs, this, [this]() { saveScenario(true); });
    file->addAction(tr("&Duplicate scenario…"), this, &MainWindow::duplicateScenario);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);
    auto *edit = menuBar()->addMenu(tr("&Edit"));
    edit->addAction(tr("Search facilities"), QKeySequence::Find, this, [this]() { m_leftTabs->setCurrentWidget(m_catalog); m_catalog->focusSearch(); });
    edit->addAction(tr("Search map locations"), QKeySequence(tr("Ctrl+L")), this, [this]() { m_mapSearch->setFocus(); m_mapSearch->selectAll(); });
    edit->addAction(tr("Cancel placement"), QKeySequence(Qt::Key_Escape), this, [this]() { m_app->cancelPlacement(); });
    auto *simm = menuBar()->addMenu(tr("&Simulation"));
    simm->addAction(m_play);
    simm->addAction(tr("Advance one month"), QKeySequence(tr("Ctrl+M")), this, [this]() { m_app->stepMonths(1); });
    simm->addAction(tr("Advance one year"), QKeySequence(tr("Ctrl+Y")), this, [this]() { m_app->stepMonths(12); });
    simm->addSeparator();
    simm->addAction(tr("National Development Advisor"), this, [this]() { m_leftTabs->setCurrentWidget(m_advisor); m_advisor->refresh(); });
    simm->addAction(tr("Policy workspace"), this, [this]() { m_leftTabs->setCurrentWidget(m_policies); });
    simm->addAction(tr("Technology tree"), this, [this]() { m_leftTabs->setCurrentWidget(m_tech); });
    auto dash = [this](int tab) { if (!m_dashboard) { m_dashboard = new DashboardWindow(m_app, this); connect(m_dashboard, &DashboardWindow::askAI, this, &MainWindow::askAI); } m_dashboard->showTab(tab); };
    simm->addAction(tr("Projections && scenario comparison"), this, [dash]() { dash(2); });
    auto *view = menuBar()->addMenu(tr("&View"));
    view->addAction(tr("Dashboard: national statistics"), this, [dash]() { dash(0); });
    view->addAction(tr("Global country comparison"), this, [dash]() { dash(1); });
    view->addAction(tr("Data sources && assumptions"), this, [dash]() { dash(3); });
    view->addSeparator();
    for (auto *d : findChildren<QDockWidget *>()) view->addAction(d->toggleViewAction());
    view->addAction(tr("Reset layout"), this, [this]() { AppSettings::s().remove("ui/state"); QMessageBox::information(this, tr("Layout"), tr("The default layout will be used next time the application starts.")); });
    auto *tools = menuBar()->addMenu(tr("&Tools"));
    tools->addAction(tr("Settings — AI providers…"), this, [this]() { SettingsDialog(m_app, this).exec(); });
    auto *help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("Quick guide"), this, [this]() {
        QMessageBox::information(this, tr("Quick guide"), tr(
            "1. Build: search the catalogue (Ctrl+F), select a facility to read its full sheet, then 'Place on map'. While placing, the map colour and the Information panel show the suitability at the cursor.\n"
            "   Lines (roads, rail, power, pipelines, fibre): click points, double-click or Enter to finish. Shift+click keeps placing points.\n"
            "2. Electricity reaches a district only through a substation or distribution network connected to generation. Roads and railways crossing rivers need bridges.\n"
            "3. Run time with Space or the +1 month / +1 year buttons. Construction takes months; costs are paid as it progresses.\n"
            "4. The Advisor lists investments derived from the simulation; accept, save or reject them.\n"
            "5. Policies change model coefficients gradually and with uncertain effectiveness.\n"
            "6. The Dashboard compares simulated India with real-world data and runs 1-50 year projections with uncertainty bands."));
    });
    help->addAction(tr("About"), this, [this]() {
        QMessageBox::about(this, tr("About"), tr("<h3>India National Development &amp; Policy Simulator</h3><p>Version %1</p>"
            "<p>A planning laboratory, not a forecast. Results are model outputs based on documented data and assumptions.</p>"
            "<p>Data: DataMeet (CC BY), Natural Earth (public domain), Census of India 2011, World Bank WDI (CC BY 4.0), CPCB AQI.</p>").arg(QStringLiteral(ISIM_VERSION)));
    });
}

void MainWindow::updateTopBar()
{
    const auto &w = m_app->sim().world();
    const auto &s = w.stats;
    m_kDate->setText(tr("<b>%1</b>").arg(w.dateString()));
    m_kBudget->setText(tr("Treasury <b>%1</b>%2").arg(cr(w.budget.treasuryCr), w.budget.debtCr > 0 ? QStringLiteral(" (debt %1)").arg(cr(w.budget.debtCr)) : QString()));
    m_kGdp->setText(tr("GDP <b>%1</b>").arg(usd(s.gdpUsd)));
    m_kPop->setText(tr("Population <b>%1</b>").arg(people(s.population)));
    m_kPower->setText(tr("Electricity <b>%1</b> of %2").arg(pct(s.electricityAccess, 0), mw(s.powerDemandMW)));
    m_kScore->setText(tr("Development score <b>%1</b>").arg(num(s.developmentScore, 1)));
    m_play->setText(m_app->isRunning() ? tr("⏸ Pause") : tr("▶ Run"));
    if (m_app->isDirty() != windowTitle().contains('*')) updateTitle();
}

void MainWindow::updateTitle()
{
    const QString file = m_app->scenarioPath().isEmpty() ? tr("unsaved") : QFileInfo(m_app->scenarioPath()).fileName();
    setWindowTitle(QStringLiteral("%1 — %2%3 — India National Development & Policy Simulator").arg(m_app->sim().world().name, file, m_app->isDirty() ? "*" : ""));
}

bool MainWindow::maybeSave()
{
    if (!m_app->isDirty()) return true;
    const auto r = QMessageBox::question(this, tr("Unsaved changes"), tr("Save the current scenario first?"), QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (r == QMessageBox::Cancel) return false;
    if (r == QMessageBox::Save) return saveScenario(false);
    return true;
}

void MainWindow::newScenario()
{
    if (!maybeSave()) return;
    QDialog d(this);
    d.setWindowTitle(tr("New scenario"));
    auto *form = new QFormLayout(&d);
    auto *name = new QLineEdit(tr("New development plan"));
    auto *year = new QSpinBox; year->setRange(2024, 2100); year->setValue(2025);
    auto *fund = new QSpinBox; fund->setRange(0, 100000000); fund->setSingleStep(50000); fund->setValue(500000); fund->setSuffix(tr(" ₹ crore"));
    form->addRow(tr("Name:"), name); form->addRow(tr("Start year:"), year); form->addRow(tr("Initial development fund:"), fund);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(bb);
    connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
    if (d.exec() != QDialog::Accepted) return;
    ScenarioSettings s; s.name = name->text(); s.startYear = year->value(); s.initialFundCr = fund->value();
    m_app->newScenario(s);
    m_map->fitIndia();
}

void MainWindow::openScenario()
{
    if (!maybeSave()) return;
    const QString f = QFileDialog::getOpenFileName(this, tr("Open scenario"), AppSettings::lastScenarioDir(), tr("Scenario files (*.indiasim)"));
    if (f.isEmpty()) return;
    QString err;
    if (!m_app->loadScenario(f, &err)) { QMessageBox::warning(this, tr("Could not open scenario"), err); return; }
    AppSettings::setLastScenarioDir(QFileInfo(f).absolutePath());
}

bool MainWindow::saveScenario(bool saveAs)
{
    QString f = m_app->scenarioPath();
    if (saveAs || f.isEmpty()) {
        f = QFileDialog::getSaveFileName(this, tr("Save scenario"), AppSettings::lastScenarioDir() + "/" + m_app->sim().world().name + ".indiasim", tr("Scenario files (*.indiasim)"));
        if (f.isEmpty()) return false;
        if (!f.endsWith(".indiasim")) f += ".indiasim";
    }
    QString err;
    if (!m_app->saveScenario(f, &err)) { QMessageBox::warning(this, tr("Could not save"), err); return false; }
    AppSettings::setLastScenarioDir(QFileInfo(f).absolutePath());
    updateTitle();
    return true;
}

void MainWindow::duplicateScenario()
{
    Simulation copy(m_app->sim());
    copy.world().name = m_app->sim().world().name + tr(" (copy)");
    m_app->installSimulation(copy);
    statusBar()->showMessage(tr("Scenario duplicated. Save it under a new name to keep both."), 6000);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (!maybeSave()) { e->ignore(); return; }
    AppSettings::s().setValue("ui/geometry", saveGeometry());
    AppSettings::s().setValue("ui/state", saveState());
    for (auto *p : m_app->ai()->providers()) p->stop();
    e->accept();
}

} // namespace isim
