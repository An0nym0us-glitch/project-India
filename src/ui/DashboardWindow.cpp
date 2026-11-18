#include <algorithm>
#include "DashboardWindow.h"
#include "Format.h"
#include "ai/AIContextBuilder.h"
#include "comparison/ComparisonEngine.h"
#include "scenario/ScenarioStore.h"
#include <QApplication>
#include <QChart>
#include <QChartView>
#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QStackedBarSeries>
#include <QBarSet>
#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLegendMarker>
#include <QLineSeries>
#include <QAreaSeries>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTableWidget>
#include <QValueAxis>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

// Validated categorical palette (fixed order; see dataviz reference palette).
static QColor seriesColor(int i, bool dark)
{
    static const char *light[] = {"#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948"};
    static const char *darkc[] = {"#3987e5", "#d95926", "#199e70", "#c98500", "#d55181", "#0ca30c", "#9085e9", "#e66767"};
    return QColor(dark ? darkc[i % 8] : light[i % 8]);
}
static bool isDark() { return qApp->palette().color(QPalette::Window).lightness() < 128; }

static QChartView *makeChartView()
{
    auto *v = new QChartView(new QChart);
    v->setRenderHint(QPainter::Antialiasing);
    v->chart()->setTheme(isDark() ? QChart::ChartThemeDark : QChart::ChartThemeLight);
    v->chart()->setBackgroundRoundness(0);
    v->chart()->legend()->setAlignment(Qt::AlignBottom);
    v->setMinimumHeight(240);
    return v;
}

static void styleAxis(QAbstractAxis *a)
{
    QPen grid(isDark() ? QColor(255, 255, 255, 35) : QColor(0, 0, 0, 25));
    a->setGridLinePen(grid);
    a->setLinePen(QPen(isDark() ? QColor(255, 255, 255, 80) : QColor(0, 0, 0, 80)));
}

DashboardWindow::DashboardWindow(AppController *app, QWidget *parent) : QWidget(parent, Qt::Window), m_app(app)
{
    setWindowTitle(tr("Dashboard — National statistics, global comparison, scenarios"));
    resize(1280, 820);
    auto *lay = new QVBoxLayout(this);
    m_tabs = new QTabWidget;
    m_tabs->addTab(buildStats(), tr("National statistics"));
    m_tabs->addTab(buildComparison(), tr("GLOBAL COUNTRY COMPARISON"));
    m_tabs->addTab(buildScenarios(), tr("Scenarios && projections"));
    m_tabs->addTab(buildAssumptions(), tr("Data sources && assumptions"));
    lay->addWidget(m_tabs);
    connect(app, &AppController::worldUpdated, this, [this]() {
        if (!isVisible()) return;
        if (m_tabs->currentIndex() == 0 && m_app->sim().world().month % 3 == 0) refreshStats();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int i) { if (i == 0) refreshStats(); if (i == 1) refreshComparison(); });
}

void DashboardWindow::showTab(int i)
{
    m_tabs->setCurrentIndex(i);
    if (i == 0) refreshStats();
    if (i == 1) refreshComparison();
    show(); raise(); activateWindow();
}

// Choose a display scale so axes read "2.5 trillion" rather than "2.5e+12".
static std::pair<double, QString> axisScale(double maxAbs)
{
    if (maxAbs >= 1e12) return {1e12, QStringLiteral("trillions")};
    if (maxAbs >= 1e9) return {1e9, QStringLiteral("billions")};
    if (maxAbs >= 1e6) return {1e6, QStringLiteral("millions")};
    if (maxAbs >= 1e4) return {1e3, QStringLiteral("thousands")};
    return {1.0, QString()};
}

// ------------------------------------------------------------------ statistics
QWidget *DashboardWindow::buildStats()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    m_kpis = new QTableWidget(0, 3);
    m_kpis->setHorizontalHeaderLabels({tr("Indicator"), tr("Value"), tr("Type")});
    m_kpis->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_kpis->verticalHeader()->hide();
    m_kpis->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_kpis->setMinimumWidth(420);
    lay->addWidget(m_kpis, 2);
    auto *right = new QVBoxLayout;
    m_histIndicator = new QComboBox;
    for (const auto &k : ProjectionRunner::defaultIndicators()) m_histIndicator->addItem(k.second, k.first);
    for (const auto &k : QVector<QPair<QString, QString>>{{"exportsCr", "Exports (₹ crore/yr)"}, {"importsCr", "Imports (₹ crore/yr)"}, {"rndShareGdp", "R&D share of GDP (%)"},
                                                       {"roadKm", "Modelled road length (km)"}, {"railKm", "Modelled rail length (km)"}, {"hospitalBedsPer1000", "Hospital beds per 1,000"},
                                                       {"schoolSeatShare", "School seat coverage"}, {"inequality", "Regional inequality (Theil)"}, {"treasuryCr", "Treasury (₹ crore)"}})
        m_histIndicator->addItem(k.second, k.first);
    right->addWidget(m_histIndicator);
    m_histChart = makeChartView();
    right->addWidget(m_histChart, 1);
    auto *ai = new QPushButton(tr("Interpret this report (AI)"));
    right->addWidget(ai);
    lay->addLayout(right, 3);
    connect(m_histIndicator, &QComboBox::currentIndexChanged, this, &DashboardWindow::refreshStats);
    connect(ai, &QPushButton::clicked, this, [this]() { emit askAI("interpret_report", AIContextBuilder::nationalSnapshot(m_app->sim()), tr("National report")); });
    return w;
}

void DashboardWindow::refreshStats()
{
    const auto &w = m_app->sim().world();
    const auto &s = w.stats;
    const auto &b = w.budget;
    QVector<std::tuple<QString, QString, DataKind>> rows = {
        {tr("Date"), w.dateString(), DataKind::Simulated},
        {tr("Population"), people(s.population), DataKind::Simulated},
        {tr("GDP (nominal)"), cr(s.gdpCr) + " ≈ " + usd(s.gdpUsd), DataKind::Simulated},
        {tr("GDP per capita"), usd(s.gdpPerCapitaUsd), DataKind::Simulated},
        {tr("GDP growth (12 months)"), num(s.gdpGrowth, 2) + "%", DataKind::Simulated},
        {tr("Sector shares (agri / industry / services)"), QStringLiteral("%1 / %2 / %3").arg(pct(s.agricultureShare), pct(s.industryShare), pct(s.servicesShare)), DataKind::Simulated},
        {tr("Manufacturing share"), pct(s.manufacturingShare), DataKind::Simulated},
        {tr("Unemployment"), pct(s.unemploymentRate), DataKind::Simulated},
        {tr("Labour force"), people(s.labourForce), DataKind::Simulated},
        {tr("Life expectancy"), num(s.lifeExpectancy, 1) + tr(" years"), DataKind::Simulated},
        {tr("Fertility rate"), num(s.tfr, 2), DataKind::Simulated},
        {tr("Dependency ratio"), num(s.dependencyRatio, 1), DataKind::Simulated},
        {tr("Urban share"), pct(s.urbanShare), DataKind::Simulated},
        {tr("Electricity demand / served"), mw(s.powerDemandMW) + " / " + mw(s.powerServedMW), DataKind::Simulated},
        {tr("Generation capacity (renewable)"), mw(s.generationCapacityMW) + " (" + mw(s.renewableCapacityMW) + ")", DataKind::Simulated},
        {tr("Grid reliability"), pct(s.gridReliability), DataKind::Simulated},
        {tr("Water demand served"), pct(s.waterDemandMLD > 0 ? s.waterSuppliedMLD / s.waterDemandMLD : 0), DataKind::Simulated},
        {tr("Sewage treated"), pct(s.sewageTreatedShare), DataKind::Simulated},
        {tr("Housing shortage"), people(s.housingShortagePersons), DataKind::Simulated},
        {tr("Hospital beds per 1,000"), num(s.hospitalBedsPer1000, 2), DataKind::Simulated},
        {tr("Primary care coverage"), pct(s.primaryCareCoverage), DataKind::Simulated},
        {tr("School seat coverage"), pct(s.schoolSeatShare), DataKind::Simulated},
        {tr("Food supply ratio"), num(s.foodSupplyRatio, 2), DataKind::Simulated},
        {tr("Roads / rail (modelled)"), num(s.roadKm) + " km / " + num(s.railKm) + " km", DataKind::Simulated},
        {tr("Port capacity / airport capacity"), num(s.portCapacityMtpa) + " Mtpa / " + num(s.airportCapacityMpa) + " M pax", DataKind::Simulated},
        {tr("Data-centre IT load"), mw(s.dataCentreMW), DataKind::Simulated},
        {tr("R&D share of GDP"), num(s.rndShareGdp, 2) + "%", DataKind::Simulated},
        {tr("Researchers per million"), num(s.researchersPerMillion), DataKind::Simulated},
        {tr("PM2.5 (population-weighted)"), num(s.pm25PopWeighted, 1) + " µg/m³", DataKind::Simulated},
        {tr("CO₂ emissions"), num(s.co2Mt, 1) + " Mt (" + num(s.co2PerCapita, 2) + " t/person)", DataKind::Simulated},
        {tr("Import dependency (goods)"), pct(s.importDependency), DataKind::Simulated},
        {tr("Exports / imports (goods)"), cr(s.exportsCr) + " / " + cr(s.importsCr), DataKind::Simulated},
        {tr("Regional inequality (Theil)"), num(s.inequalityTheil, 3), DataKind::Simulated},
        {tr("Government revenue"), cr(b.revenueCrYr) + tr("/yr"), DataKind::Simulated},
        {tr("Treasury / debt"), cr(b.treasuryCr) + " / " + cr(b.debtCr), DataKind::Simulated},
        {tr("Private capital available"), cr(b.privateCapitalAvailableCrYr) + tr("/yr"), DataKind::Simulated},
        {tr("National development score"), num(s.developmentScore, 1) + " / 100", DataKind::Assumption},
    };
    m_kpis->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        m_kpis->setItem(i, 0, new QTableWidgetItem(std::get<0>(rows[i])));
        m_kpis->setItem(i, 1, new QTableWidgetItem(std::get<1>(rows[i])));
        m_kpis->setItem(i, 2, new QTableWidgetItem(std::get<2>(rows[i]) == DataKind::Assumption ? tr("Composite index (equal weights)") : dataKindName(std::get<2>(rows[i]))));
    }
    // history chart (single series: title names it, no legend)
    auto *chart = m_histChart->chart();
    chart->removeAllSeries();
    for (auto *a : chart->axes()) chart->removeAxis(a);
    auto *ser = new QLineSeries;
    const QString key = m_histIndicator->currentData().toString();
    double mn = 1e300, mx = -1e300;
    for (const auto &h : w.history) {
        const double v = h.v.value(key);
        ser->append(w.startYear + h.month / 12.0, v);
        mn = std::min(mn, v); mx = std::max(mx, v);
    }
    ser->setPen(QPen(seriesColor(0, isDark()), 2));
    chart->addSeries(ser);
    chart->setTitle(m_histIndicator->currentText() + tr(" — simulated history"));
    chart->legend()->hide();
    auto *ax = new QValueAxis; ax->setLabelFormat("%.0f"); ax->setTitleText(tr("Year"));
    auto *ay = new QValueAxis; ay->setLabelFormat(std::abs(mx) < 10 ? "%.3g" : "%.4g");
    if (mx - mn < 1e-9) { mn -= 1; mx += 1; }
    ay->setRange(std::min(0.0, mn), mx + (mx - mn) * 0.05);
    ax->setRange(w.startYear, std::max(w.startYear + 1.0, w.startYear + w.month / 12.0));
    styleAxis(ax); styleAxis(ay);
    chart->addAxis(ax, Qt::AlignBottom); chart->addAxis(ay, Qt::AlignLeft);
    ser->attachAxis(ax); ser->attachAxis(ay);
}

// ------------------------------------------------------------------ comparison
QWidget *DashboardWindow::buildComparison()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    auto *top = new QHBoxLayout;
    top->addWidget(new QLabel(tr("Baseline year (real-world data):")));
    m_baseYear = new QComboBox;
    for (int y = 2024; y >= 2000; --y) m_baseYear->addItem(QString::number(y), y);
    m_baseYear->setCurrentIndex(m_baseYear->findData(2023));
    top->addWidget(m_baseYear);
    top->addWidget(new QLabel(tr("Group:")));
    m_group = new QComboBox;
    m_group->addItems({tr("All"), "Economy", "Population", "Infrastructure", "Technology", "Human development", "Environment"});
    top->addWidget(m_group);
    top->addStretch();
    lay->addLayout(top);
    m_compSummary = new QLabel;
    m_compSummary->setWordWrap(true);
    lay->addWidget(m_compSummary);
    auto *split = new QSplitter(Qt::Vertical);
    m_compTable = new QTableWidget;
    m_compTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_compTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_compTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_compTable->verticalHeader()->hide();
    split->addWidget(m_compTable);
    auto *charts = new QSplitter;
    m_compChart = makeChartView();
    m_trendChart = makeChartView();
    charts->addWidget(m_compChart);
    charts->addWidget(m_trendChart);
    split->addWidget(charts);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    lay->addWidget(split, 1);
    connect(m_baseYear, &QComboBox::currentIndexChanged, this, &DashboardWindow::refreshComparison);
    connect(m_group, &QComboBox::currentIndexChanged, this, &DashboardWindow::refreshComparison);
    connect(m_compTable, &QTableWidget::itemSelectionChanged, this, &DashboardWindow::refreshComparisonChart);
    return w;
}

void DashboardWindow::refreshComparison()
{
    const auto &ref = m_app->resources().reference;
    if (!ref.isLoaded()) { m_compSummary->setText(tr("<b>Reference data is unavailable.</b> The bundled World Bank snapshot could not be loaded.")); return; }
    const int year = m_baseYear->currentData().toInt();
    const auto rows = ComparisonEngine::compare(m_app->sim(), ref, year);
    const QString group = m_group->currentIndex() == 0 ? QString() : m_group->currentText();
    QStringList headers{tr("Indicator"), tr("Unit"), tr("Simulated India\n(%1)").arg(m_app->sim().world().dateString())};
    for (const auto &c : ref.countries()) headers << ref.countryName(c) + (c == "IND" ? tr("\n(real baseline)") : QString());
    headers << tr("Sim. rank\n(of 6)") << tr("Source");
    m_compTable->clear();
    m_compTable->setColumnCount(headers.size());
    m_compTable->setHorizontalHeaderLabels(headers);
    int r = 0;
    QStringList strengths, gaps;
    m_compTable->setRowCount(0);
    for (const auto &row : rows) {
        if (!group.isEmpty() && row.group != group) continue;
        m_compTable->insertRow(r);
        auto *lab = new QTableWidgetItem(row.label);
        lab->setData(Qt::UserRole, row.code);
        lab->setToolTip(row.code);
        m_compTable->setItem(r, 0, lab);
        m_compTable->setItem(r, 1, new QTableWidgetItem(row.unit));
        auto cell = [&](const ComparisonCell &c, const QString &note) {
            auto *it = new QTableWidgetItem(c.available ? general(c.value, QString()) : tr("unavailable"));
            if (!c.available) it->setForeground(QColor(140, 140, 140));
            QString tip = note;
            if (!c.note.isEmpty()) tip = c.note + (tip.isEmpty() ? QString() : "\n" + tip);
            if (c.available && c.year && !c.note.isEmpty()) it->setText(it->text() + QStringLiteral(" (%1)").arg(c.year));
            it->setToolTip(tip);
            return it;
        };
        auto *sim = cell(row.simulated, row.simulatedNote);
        if (!row.simulated.available) sim->setText(tr("not simulated"));
        sim->setBackground(isDark() ? QColor(42, 60, 90) : QColor(225, 236, 250));
        m_compTable->setItem(r, 2, sim);
        for (int c = 0; c < row.countries.size(); ++c) m_compTable->setItem(r, 3 + c, cell(row.countries[c], {}));
        m_compTable->setItem(r, 3 + row.countries.size(), new QTableWidgetItem(row.simulatedRank ? QString::number(row.simulatedRank) : QStringLiteral("—")));
        auto *src = new QTableWidgetItem(row.source);
        src->setToolTip(row.source);
        m_compTable->setItem(r, 4 + row.countries.size(), src);
        // gap vs real India baseline
        if (row.simulated.available && !row.countries.isEmpty() && row.countries[0].available && row.higherIsBetter.has_value() && row.countries[0].value != 0) {
            const double ratio = row.simulated.value / row.countries[0].value;
            const bool better = *row.higherIsBetter ? ratio > 1.05 : ratio < 0.95;
            const bool worse = *row.higherIsBetter ? ratio < 0.8 : ratio > 1.25;
            if (better) strengths << row.label;
            if (worse) gaps << row.label;
        }
        ++r;
    }
    m_compTable->resizeColumnsToContents();
    m_compTable->setColumnWidth(0, 260);
    m_compTable->setColumnWidth(m_compTable->columnCount() - 1, 260);
    m_compSummary->setText(tr("<b>Baseline year %1.</b> Real-world values: %2 (%3, retrieved %4). Where %1 is missing the latest earlier year within 5 years is used and labelled; "
                              "otherwise the value is shown as <i>unavailable</i>. Simulated values are model outputs for the current simulation date and use the same definition where marked "
                              "(hover for notes). Nominal and PPP GDP, and total vs per-capita emissions, are separate rows and never mixed.<br>"
                              "<b>Ahead of real-India baseline:</b> %5<br><b>Largest development gaps vs real-India baseline:</b> %6")
                               .arg(year).arg(esc(ref.source()), esc(ref.license()), esc(ref.retrieved()))
                               .arg(strengths.isEmpty() ? tr("none yet") : esc(strengths.join(", ")), gaps.isEmpty() ? tr("none") : esc(gaps.join(", "))));
    if (m_compTable->rowCount() > 0 && m_compTable->selectedItems().isEmpty()) m_compTable->selectRow(0);
    else refreshComparisonChart();
}

void DashboardWindow::refreshComparisonChart()
{
    const auto &ref = m_app->resources().reference;
    const auto sel = m_compTable->selectedItems();
    if (sel.isEmpty()) return;
    const QString code = m_compTable->item(sel.first()->row(), 0)->data(Qt::UserRole).toString();
    const auto *ind = ref.indicator(code);
    if (!ind) return;
    const int year = m_baseYear->currentData().toInt();
    const bool dark = isDark();
    // bar chart: one bar per country, fixed colour per entity
    auto *chart = m_compChart->chart();
    chart->removeAllSeries();
    for (auto *a : chart->axes()) chart->removeAxis(a);
    auto *bars = new QStackedBarSeries; // one set per entity, zero elsewhere: full-width bars with a fixed colour per country
    QStringList cats;
    QString note;
    const auto simV = ComparisonEngine::simulatedValue(m_app->sim(), code, &note);
    QVector<std::pair<QString, std::optional<double>>> entries;
    entries.append({tr("Simulated India"), simV});
    for (const auto &c : ref.countries()) {
        const auto v = ref.valueAt(code, c, year, 5);
        entries.append({ref.countryName(c), v.available ? std::optional<double>(v.value) : std::nullopt});
    }
    double mx = 0;
    for (const auto &e : entries) if (e.second) mx = std::max(mx, std::abs(*e.second));
    const auto [k, kname] = axisScale(mx);
    mx = 0;
    for (int i = 0; i < entries.size(); ++i) {
        auto *set = new QBarSet(entries[i].first + (entries[i].second ? QString() : tr(" (unavailable)")));
        // one value per set at its own category index -> each entity keeps its own colour
        for (int j = 0; j < entries.size(); ++j) *set << (j == i && entries[i].second ? *entries[i].second / k : 0.0);
        set->setColor(seriesColor(i, dark));
        set->setBorderColor(chart->backgroundBrush().color());
        bars->append(set);
        if (entries[i].second) mx = std::max(mx, *entries[i].second / k);
        cats << entries[i].first;
    }
    bars->setBarWidth(0.9);
    bars->setLabelsVisible(false);
    chart->addSeries(bars);
    auto *ax = new QBarCategoryAxis; ax->append(cats);
    auto *ay = new QValueAxis; ay->setRange(0, mx > 0 ? mx * 1.08 : 1); ay->setLabelFormat("%.4g");
    ay->setTitleText(kname.isEmpty() ? ind->unit : ind->unit + " (" + kname + ")");
    styleAxis(ax); styleAxis(ay);
    chart->addAxis(ax, Qt::AlignBottom); chart->addAxis(ay, Qt::AlignLeft);
    bars->attachAxis(ax); bars->attachAxis(ay);
    chart->setTitle(QStringLiteral("%1 — %2 (baseline %3)").arg(ind->label, ind->unit).arg(year));
    chart->legend()->setVisible(true);
    // trend chart: historical series for the 5 countries + simulated India
    auto *tc = m_trendChart->chart();
    tc->removeAllSeries();
    for (auto *a : tc->axes()) tc->removeAxis(a);
    double lo = 1e300, hi = -1e300; int y0 = 3000, y1 = 0;
    QVector<QLineSeries *> all;
    const auto &h = m_app->sim().world().history;
    double trendMax = simV ? std::abs(*simV) : 0;
    for (const auto &c : ref.countries()) for (double v : ind->series.value(c)) trendMax = std::max(trendMax, std::abs(v));
    const auto [tk, tkname] = axisScale(trendMax);
    if (simV) {
        auto *s = new QLineSeries; s->setName(tr("Simulated India"));
        // map simulated indicator through history where a key exists
        static const QHash<QString, QString> keyFor = {{"SP.POP.TOTL", "population"}, {"NY.GDP.MKTP.CD", "gdpUsd"}, {"NY.GDP.PCAP.CD", "gdpPerCapitaUsd"},
                                                      {"SP.DYN.LE00.IN", "lifeExpectancy"}, {"SP.DYN.TFRT.IN", "tfr"}, {"EN.GHG.CO2.MT.CE.AR5", "co2Mt"}};
        const QString k = keyFor.value(code);
        if (!k.isEmpty()) for (const auto &p : h) { const double yy = m_app->sim().world().startYear + p.month / 12.0; const double v = p.v.value(k) / tk; s->append(yy, v); lo = std::min(lo, v); hi = std::max(hi, v); y0 = std::min(y0, int(yy)); y1 = std::max(y1, int(std::ceil(yy))); }
        else { const double yy = m_app->sim().world().year(); const double v = *simV / tk; s->append(yy, v); lo = std::min(lo, v); hi = std::max(hi, v); y0 = std::min(y0, int(yy)); y1 = std::max(y1, int(yy)); }
        QPen pen(seriesColor(0, dark), 2, Qt::DashLine); s->setPen(pen);
        all << s;
    }
    for (int i = 0; i < ref.countries().size(); ++i) {
        const auto ser = ind->series.value(ref.countries()[i]);
        if (ser.isEmpty()) continue;
        auto *s = new QLineSeries; s->setName(ref.countryName(ref.countries()[i]));
        for (auto it = ser.begin(); it != ser.end(); ++it) { const double v = it.value() / tk; s->append(it.key(), v); lo = std::min(lo, v); hi = std::max(hi, v); y0 = std::min(y0, it.key()); y1 = std::max(y1, it.key()); }
        s->setPen(QPen(seriesColor(i + 1, dark), 2));
        all << s;
    }
    for (auto *s : all) tc->addSeries(s);
    if (!all.isEmpty()) {
        auto *tx = new QValueAxis; tx->setLabelFormat("%.0f"); tx->setRange(y0, std::max(y0 + 1, y1));
        auto *ty = new QValueAxis; ty->setLabelFormat("%.4g"); ty->setRange(std::min(0.0, lo), hi * 1.05 + 1e-9);
        ty->setTitleText(tkname.isEmpty() ? ind->unit : ind->unit + " (" + tkname + ")");
        styleAxis(tx); styleAxis(ty);
        tc->addAxis(tx, Qt::AlignBottom); tc->addAxis(ty, Qt::AlignLeft);
        for (auto *s : all) { s->attachAxis(tx); s->attachAxis(ty); }
    }
    tc->setTitle(tr("Historical trend (World Bank) and simulated India (dashed)"));
}

// ------------------------------------------------------------------ scenarios
QWidget *DashboardWindow::buildScenarios()
{
    auto *w = new QWidget;
    auto *lay = new QHBoxLayout(w);
    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel(tr("<b>Scenarios to project</b>")));
    m_scenarioList = new QListWidget;
    auto *cur = new QListWidgetItem(tr("Current scenario (as is)"));
    cur->setFlags(cur->flags() | Qt::ItemIsUserCheckable); cur->setCheckState(Qt::Checked); cur->setData(Qt::UserRole, "current");
    m_scenarioList->addItem(cur);
    for (const auto &p : strategyPresets()) {
        auto *it = new QListWidgetItem(tr("Current + strategy %1").arg(p.name));
        it->setToolTip(p.description);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable); it->setCheckState(Qt::Unchecked); it->setData(Qt::UserRole, "preset:" + p.id);
        m_scenarioList->addItem(it);
    }
    left->addWidget(m_scenarioList, 1);
    auto *addFile = new QPushButton(tr("Add saved scenario file…"));
    left->addWidget(addFile);
    auto *form = new QHBoxLayout;
    form->addWidget(new QLabel(tr("Horizon:")));
    m_horizon = new QComboBox;
    for (int y : {1, 5, 10, 25, 50}) m_horizon->addItem(tr("%1 year(s)").arg(y), y);
    m_horizon->setCurrentIndex(2);
    form->addWidget(m_horizon);
    form->addWidget(new QLabel(tr("Ensemble:")));
    m_members = new QSpinBox; m_members->setRange(1, 64); m_members->setValue(12);
    form->addWidget(m_members);
    left->addLayout(form);
    auto *run = new QPushButton(tr("Run projections"));
    run->setDefault(true);
    left->addWidget(run);
    auto *ai = new QPushButton(tr("Summarise comparison (AI)"));
    left->addWidget(ai);
    m_projNotes = new QLabel;
    m_projNotes->setWordWrap(true);
    left->addWidget(m_projNotes);
    lay->addLayout(left, 1);
    auto *right = new QVBoxLayout;
    m_projIndicator = new QComboBox;
    for (const auto &k : ProjectionRunner::defaultIndicators()) m_projIndicator->addItem(k.second, k.first);
    right->addWidget(m_projIndicator);
    m_projChart = makeChartView();
    right->addWidget(m_projChart, 3);
    m_projTable = new QTableWidget;
    m_projTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_projTable->verticalHeader()->hide();
    right->addWidget(m_projTable, 2);
    lay->addLayout(right, 3);
    connect(run, &QPushButton::clicked, this, &DashboardWindow::runProjections);
    connect(m_projIndicator, &QComboBox::currentIndexChanged, this, &DashboardWindow::refreshProjectionChart);
    connect(addFile, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, tr("Add scenario"), QString(), tr("Scenario files (*.indiasim)"));
        if (f.isEmpty()) return;
        auto *it = new QListWidgetItem(QFileInfo(f).completeBaseName());
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable); it->setCheckState(Qt::Checked); it->setData(Qt::UserRole, "file:" + f);
        m_scenarioList->addItem(it);
    });
    connect(ai, &QPushButton::clicked, this, [this]() {
        if (m_projections.isEmpty()) { QMessageBox::information(this, tr("No projections"), tr("Run projections first.")); return; }
        emit askAI("compare_scenarios", AIContextBuilder::projections(m_projections), tr("Scenario comparison"));
    });
    return w;
}

void DashboardWindow::runProjections()
{
    const int years = m_horizon->currentData().toInt();
    const int members = m_members->value();
    std::vector<Simulation> sims;
    QStringList names;
    for (int i = 0; i < m_scenarioList->count(); ++i) {
        auto *it = m_scenarioList->item(i);
        if (it->checkState() != Qt::Checked) continue;
        const QString key = it->data(Qt::UserRole).toString();
        Simulation s(m_app->sim());
        if (key.startsWith("preset:")) {
            for (const auto &p : strategyPresets()) if ("preset:" + p.id == key) {
                for (const auto &pol : p.policies) s.enactPolicy(pol.first, pol.second);
                s.world().name = p.name;
            }
        } else if (key.startsWith("file:")) {
            WorldState w; QString err;
            if (!ScenarioStore::load(key.mid(5), w, &err)) { QMessageBox::warning(this, tr("Load failed"), err); continue; }
            s.setWorld(w);
        } else s.world().name = tr("Current: %1").arg(m_app->sim().world().name);
        sims.push_back(s);
    }
    if (sims.empty()) return;
    QProgressDialog prog(tr("Running %1 scenario(s) × %2 ensemble members for %3 years…").arg(int(sims.size())).arg(members).arg(years), tr("Cancel"), 0, int(sims.size()), this);
    prog.setWindowModality(Qt::WindowModal);
    prog.setMinimumDuration(0);
    m_projections.clear();
    for (int i = 0; i < int(sims.size()); ++i) {
        prog.setValue(i);
        QApplication::processEvents();
        if (prog.wasCanceled()) break;
        m_projections << ProjectionRunner::run(sims[i], years, members, 1000 + i);
    }
    prog.setValue(int(sims.size()));
    if (!m_projections.isEmpty()) m_projNotes->setText("<small>" + esc(m_projections.first().notes.join(" ")) + "</small>");
    refreshProjectionChart();
}

void DashboardWindow::refreshProjectionChart()
{
    auto *chart = m_projChart->chart();
    chart->removeAllSeries();
    for (auto *a : chart->axes()) chart->removeAxis(a);
    if (m_projections.isEmpty()) return;
    const QString key = m_projIndicator->currentData().toString();
    const bool dark = isDark();
    auto *ax = new QValueAxis; ax->setLabelFormat("%.0f");
    auto *ay = new QValueAxis; ay->setLabelFormat("%.4g");
    double lo = 1e300, hi = -1e300; int y0 = 3000, y1 = 0;
    QVector<QAbstractSeries *> series;
    for (int i = 0; i < m_projections.size(); ++i) {
        const auto *s = m_projections[i].find(key);
        if (!s) continue;
        const QColor c = seriesColor(i, dark);
        auto *upper = new QLineSeries, *lower = new QLineSeries, *mid = new QLineSeries;
        for (int k = 0; k < s->years.size(); ++k) {
            upper->append(s->years[k], s->p90[k]); lower->append(s->years[k], s->p10[k]); mid->append(s->years[k], s->p50[k]);
            lo = std::min(lo, s->p10[k]); hi = std::max(hi, s->p90[k]); y0 = std::min(y0, s->years[k]); y1 = std::max(y1, s->years[k]);
        }
        auto *band = new QAreaSeries(upper, lower);
        QColor bc = c; bc.setAlpha(55);
        band->setBrush(bc); band->setPen(Qt::NoPen);
        band->setName(m_projections[i].scenarioName + tr(" (10–90%)"));
        mid->setName(m_projections[i].scenarioName + tr(" (median)"));
        mid->setPen(QPen(c, 2));
        series << band << mid;
    }
    for (auto *s : series) chart->addSeries(s);
    // hide band entries from the legend (one entry per scenario)
    for (auto *m : chart->legend()->markers()) if (qobject_cast<QAreaSeries *>(m->series())) m->setVisible(false);
    if (hi < lo) return;
    ax->setRange(y0 - 1, y1);
    ay->setRange(std::min(0.0, lo), hi + (hi - lo) * 0.05 + 1e-9);
    styleAxis(ax); styleAxis(ay);
    chart->addAxis(ax, Qt::AlignBottom); chart->addAxis(ay, Qt::AlignLeft);
    for (auto *s : series) { s->attachAxis(ax); s->attachAxis(ay); }
    chart->setTitle(m_projIndicator->currentText() + tr(" — projection (median and 10–90% band; not a forecast)"));
    // table at horizon milestones
    QVector<int> marks;
    const auto *first = m_projections.first().find(key);
    for (int y : {1, 5, 10, 25, 50}) if (first && y <= first->years.size()) marks << y;
    m_projTable->clear();
    m_projTable->setColumnCount(1 + marks.size());
    QStringList hdr{tr("Scenario")};
    for (int y : marks) hdr << tr("+%1 yr: median [10%–90%]").arg(y);
    m_projTable->setHorizontalHeaderLabels(hdr);
    m_projTable->setRowCount(m_projections.size());
    for (int i = 0; i < m_projections.size(); ++i) {
        m_projTable->setItem(i, 0, new QTableWidgetItem(m_projections[i].scenarioName));
        const auto *s = m_projections[i].find(key);
        for (int c = 0; s && c < marks.size(); ++c) {
            const int k = marks[c] - 1;
            m_projTable->setItem(i, c + 1, new QTableWidgetItem(QStringLiteral("%1 [%2 – %3]").arg(general(s->p50[k], {}), general(s->p10[k], {}), general(s->p90[k], {}))));
        }
    }
    m_projTable->resizeColumnsToContents();
}

// ------------------------------------------------------------------ assumptions
QWidget *DashboardWindow::buildAssumptions()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);
    auto *intro = new QLabel(tr("<b>Data sources</b>: State/UT boundaries — DataMeet India (CC BY 4.0); district boundaries — DataMeet Census 2011 districts (CC BY 2.5 IN); "
                                "rivers, lakes, coastline, physical regions, neighbouring countries — Natural Earth (public domain); district demography — Census of India 2011; "
                                "country indicators — World Bank WDI (CC BY 4.0); AQI breakpoints — CPCB (2014). "
                                "Every model coefficient is listed below with its status. Projections sample the low–high ranges."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *t = new QTableWidget;
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->hide();
    t->setColumnCount(7);
    t->setHorizontalHeaderLabels({tr("Parameter"), tr("Value"), tr("Range"), tr("Unit"), tr("Status"), tr("Description"), tr("Source / basis")});
    const auto &p = m_app->sim().params();
    const auto keys = p.keys();
    for (const auto &k : keys) {
        if (k.startsWith('_')) continue;
        const auto *pp = p.find(k);
        const int r = t->rowCount();
        t->insertRow(r);
        t->setItem(r, 0, new QTableWidgetItem(k));
        t->setItem(r, 1, new QTableWidgetItem(QString::number(pp->value, 'g', 6)));
        t->setItem(r, 2, new QTableWidgetItem(pp->high > pp->low ? QStringLiteral("%1 – %2").arg(pp->low, 0, 'g', 4).arg(pp->high, 0, 'g', 4) : QStringLiteral("fixed")));
        t->setItem(r, 3, new QTableWidgetItem(pp->unit));
        auto *st = new QTableWidgetItem(dataKindName(pp->provenance.kind));
        st->setForeground(pp->provenance.kind == DataKind::Measured ? QColor(31, 111, 67) : pp->provenance.kind == DataKind::Assumption ? QColor(160, 100, 0) : QColor(36, 76, 138));
        t->setItem(r, 4, st);
        t->setItem(r, 5, new QTableWidgetItem(pp->description));
        t->setItem(r, 6, new QTableWidgetItem(pp->provenance.source));
    }
    t->resizeColumnsToContents();
    t->setColumnWidth(5, 380);
    t->setColumnWidth(6, 380);
    lay->addWidget(t, 1);
    return w;
}

} // namespace isim
