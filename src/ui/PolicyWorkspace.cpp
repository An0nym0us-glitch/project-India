#include "PolicyWorkspace.h"
#include "Format.h"
#include "ai/AIContextBuilder.h"
#include "engines/PolicyEngine.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

PolicyWorkspace::PolicyWorkspace(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QHBoxLayout(this);
    auto *split = new QSplitter;
    auto *left = new QWidget;
    auto *ll = new QVBoxLayout(left);
    ll->setContentsMargins(0, 0, 0, 0);
    m_budget = new QLabel;
    m_budget->setWordWrap(true);
    ll->addWidget(m_budget);
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Policy"), tr("Status")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    ll->addWidget(m_tree, 1);
    split->addWidget(left);
    auto *right = new QWidget;
    auto *rl = new QVBoxLayout(right);
    rl->setContentsMargins(0, 0, 0, 0);
    m_details = new QTextBrowser;
    rl->addWidget(m_details, 1);
    auto *ctl = new QHBoxLayout;
    ctl->addWidget(new QLabel(tr("Intensity:")));
    m_intensity = new QSlider(Qt::Horizontal);
    m_intensity->setRange(0, 100);
    m_intensity->setValue(100);
    m_intLabel = new QLabel("100%");
    m_intLabel->setMinimumWidth(40);
    ctl->addWidget(m_intensity, 1);
    ctl->addWidget(m_intLabel);
    m_enact = new QPushButton(tr("Enact"));
    m_repeal = new QPushButton(tr("Repeal"));
    m_ai = new QPushButton(tr("Explain consequences (AI)"));
    ctl->addWidget(m_enact); ctl->addWidget(m_repeal); ctl->addWidget(m_ai);
    rl->addLayout(ctl);
    split->addWidget(right);
    split->setStretchFactor(1, 2);
    lay->addWidget(split);

    connect(m_tree, &QTreeWidget::currentItemChanged, this, &PolicyWorkspace::showPolicy);
    connect(m_intensity, &QSlider::valueChanged, this, [this](int v) { m_intLabel->setText(QString::number(v) + "%"); });
    connect(m_enact, &QPushButton::clicked, this, [this]() {
        const QString id = currentId();
        const auto *p = m_app->resources().policies.find(id);
        if (!p) return;
        if (p->isCoercive()) {
            if (QMessageBox::warning(this, tr("Coercive policy"),
                                     tr("%1\n\n%2\n\nThe simulator models its social harms. Enact anyway?").arg(p->name, p->humanRightsConcern),
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        }
        m_app->enactPolicy(id, m_intensity->value() / 100.0);
        refresh();
    });
    connect(m_repeal, &QPushButton::clicked, this, [this]() { m_app->repealPolicy(currentId()); refresh(); });
    connect(m_ai, &QPushButton::clicked, this, [this]() {
        if (const auto *p = m_app->resources().policies.find(currentId())) emit askAI("explain_policy", AIContextBuilder::policy(m_app->sim(), *p), p->name);
    });
    connect(app, &AppController::worldReset, this, &PolicyWorkspace::refresh);
    refresh();
}

QString PolicyWorkspace::currentId() const
{
    auto *it = m_tree->currentItem();
    return it ? it->data(0, Qt::UserRole).toString() : QString();
}

void PolicyWorkspace::refresh()
{
    const QString keep = currentId();
    m_tree->clear();
    const auto &lib = m_app->resources().policies;
    QTreeWidgetItem *sel = nullptr;
    for (const auto &cat : lib.categories()) {
        auto *g = new QTreeWidgetItem(m_tree, {cat});
        QFont f = g->font(0); f.setBold(true); g->setFont(0, f);
        g->setFlags(Qt::ItemIsEnabled);
        for (const auto &p : lib.all()) {
            if (p.category != cat) continue;
            auto *it = new QTreeWidgetItem(g, {p.name});
            it->setData(0, Qt::UserRole, p.id);
            for (const auto &inst : m_app->sim().world().policies)
                if (inst.id == p.id) { it->setText(1, QStringLiteral("active %1%").arg(int(inst.intensity * 100))); it->setForeground(1, QColor(42, 157, 75)); }
            if (p.isCoercive()) it->setForeground(0, QColor(192, 57, 43));
            if (p.id == keep) sel = it;
        }
        g->setExpanded(true);
    }
    if (sel) m_tree->setCurrentItem(sel);
    const auto &b = m_app->sim().world().budget;
    auto ctx = m_app->sim().context();
    m_budget->setText(tr("<b>Fiscal position</b><br>Revenue %1/yr · Policy spending %2/yr<br>Treasury %3 · Debt %4")
                          .arg(cr(b.revenueCrYr), cr(PolicyEngine::annualCostCr(ctx)), cr(b.treasuryCr), cr(b.debtCr)));
}

static QString ul(const QStringList &l)
{
    if (l.isEmpty()) return QStringLiteral("—");
    QString s = "<ul style='margin-top:0'>";
    for (const auto &x : l) s += "<li>" + x.toHtmlEscaped() + "</li>";
    return s + "</ul>";
}

void PolicyWorkspace::showPolicy()
{
    const auto *p = m_app->resources().policies.find(currentId());
    if (!p) { m_details->clear(); return; }
    const double gdp = m_app->sim().world().stats.gdpCr;
    const PolicyInstance *inst = nullptr;
    for (const auto &i : m_app->sim().world().policies) if (i.id == p->id) inst = &i;
    QString h = QStringLiteral("<h2>%1</h2><div><b>%2</b></div><p>%3</p>").arg(esc(p->name), esc(p->category), esc(p->description));
    if (!p->humanRightsConcern.isEmpty()) h += QStringLiteral("<div style='border:2px solid #c0392b;padding:6px;color:#c0392b'><b>Human-rights concern.</b> %1</div>").arg(esc(p->humanRightsConcern));
    h += "<table>";
    auto row = [&](const QString &k, const QString &v) { h += QStringLiteral("<tr><td style='color:#888;padding-right:8px' valign='top'>%1</td><td>%2</td></tr>").arg(k, v); };
    row(tr("Intensity scale"), esc(p->intensityLabel));
    const double cost = p->costCrPerYearAtFull + p->costPctGdpAtFull * gdp;
    row(tr("Implementation cost"), cost > 0 ? cr(cost) + tr(" per year at 100% (scales with intensity)") : tr("No direct spending"));
    QString rev;
    for (const auto &e : p->effects) if (e.key == "tax.rate") rev = QStringLiteral("%1 percentage points of GDP at 100% ≈ %2/yr").arg(e.full * 100, 0, 'f', 1).arg(cr(e.full * gdp));
    row(tr("Government revenue impact"), rev.isEmpty() ? (p->revenueNote.isEmpty() ? tr("Indirect only") : esc(p->revenueNote)) : rev);
    row(tr("Expected benefits"), ul(p->benefits));
    row(tr("Possible unintended consequences"), ul(p->unintended));
    row(tr("Short-term effects"), esc(p->shortTerm));
    row(tr("Long-term effects"), esc(p->longTerm));
    row(tr("Affected population groups"), ul(p->affectedGroups));
    row(tr("State-level differences"), esc(p->stateDifferences.isEmpty() ? tr("Not differentiated in the model") : p->stateDifferences));
    row(tr("Implementation challenges"), ul(p->challenges));
    row(tr("Uncertainty"), esc(p->uncertainty) + QStringLiteral("<br>Modelled effectiveness: %1 (low) – %2 (central) – %3 (high). Effects start after %4 months and ramp in over %5 months. %6")
                                                  .arg(pct(p->effLow, 0), pct(p->effCentral, 0), pct(p->effHigh, 0)).arg(p->lagMonths).arg(p->rampMonths).arg(badge(DataKind::Assumption)));
    QStringList eff;
    for (const auto &e : p->effects) eff << QStringLiteral("%1: %2 %3").arg(e.key, e.mode == "mul" ? "×" : "+", QString::number(e.full));
    row(tr("Model levers"), esc(eff.join("; ")));
    if (inst) {
        auto ctx = m_app->sim().context();
        row(tr("Current status"), QStringLiteral("Active since month %1 at %2 intensity; realised strength now %3").arg(inst->startMonth).arg(pct(inst->intensity, 0), pct(PolicyEngine::realisedStrength(ctx, *inst, *p), 0)));
    }
    h += "</table><p><small>Policies do not automatically succeed: realised strength = intensity × effectiveness × ramp-in, and is reduced when governance trust is low.</small></p>";
    m_details->setHtml(h);
    m_enact->setText(inst ? tr("Adjust") : tr("Enact"));
    m_repeal->setEnabled(inst != nullptr);
    if (inst) m_intensity->setValue(int(inst->intensity * 100));
}

} // namespace isim
