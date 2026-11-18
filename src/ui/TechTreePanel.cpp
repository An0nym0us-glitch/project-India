#include <algorithm>
#include "TechTreePanel.h"
#include "Format.h"
#include "advisor/Suitability.h"
#include "catalog/Catalog.h"
#include "engines/PopulationEngine.h"
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

TechTreePanel::TechTreePanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    auto *t = new QLabel(tr("<b>Technology maturity</b><br><small>Levels advance when research points, prerequisite domains and required operating facilities are all in place. Thresholds are modelling assumptions.</small>"));
    t->setWordWrap(true);
    lay->addWidget(t);
    auto *split = new QSplitter(Qt::Vertical);
    m_tree = new QTreeWidget;
    m_tree->setHeaderLabels({tr("Domain"), tr("Level"), tr("Points/yr"), tr("Progress")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->setRootIsDecorated(false);
    m_details = new QTextBrowser;
    split->addWidget(m_tree);
    split->addWidget(m_details);
    lay->addWidget(split, 1);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [this]() { refresh(); });
    connect(app, &AppController::worldUpdated, this, [this]() { if (isVisible() && m_app->sim().world().month % 3 == 0) refresh(); });
    connect(app, &AppController::worldReset, this, &TechTreePanel::refresh);
    refresh();
}

void TechTreePanel::refresh()
{
    const auto &cat = m_app->resources().catalog;
    const auto &w = m_app->sim().world();
    auto ctx = m_app->sim().context();
    const auto rates = TechnologyEngine::researchRates(ctx);
    const QString keep = m_tree->currentItem() ? m_tree->currentItem()->data(0, Qt::UserRole).toString() : QString();
    m_tree->blockSignals(true);
    m_tree->clear();
    QTreeWidgetItem *sel = nullptr;
    for (const auto &d : cat.techDomains()) {
        const int lvl = w.techLevel.value(d.id);
        auto *it = new QTreeWidgetItem(m_tree);
        it->setText(0, d.name);
        it->setText(1, QStringLiteral("%1 / %2").arg(lvl).arg(d.levels.size() - 1));
        it->setText(2, num(rates.value(d.id), 1));
        it->setData(0, Qt::UserRole, d.id);
        if (lvl + 1 < d.levels.size()) {
            const double need = d.levels[lvl + 1].pointsRequired;
            const double have = w.techPoints.value(d.id);
            auto *bar = new QProgressBar;
            bar->setRange(0, 100);
            bar->setValue(int(std::min(100.0, have / std::max(1.0, need) * 100)));
            bar->setFormat(QStringLiteral("%1 / %2").arg(num(have), num(need)));
            bar->setMaximumHeight(16);
            m_tree->setItemWidget(it, 3, bar);
        } else it->setText(3, tr("max"));
        if (d.id == keep) sel = it;
    }
    m_tree->blockSignals(false);
    if (sel) m_tree->setCurrentItem(sel);
    const QString id = sel ? keep : (m_tree->topLevelItemCount() ? m_tree->topLevelItem(0)->data(0, Qt::UserRole).toString() : QString());
    const auto *d = cat.techDomain(id);
    if (!d) return;
    const int lvl = w.techLevel.value(d->id);
    QString h = QStringLiteral("<h3>%1 — level %2</h3><p>%3</p><p><small>%4</small></p><table>").arg(esc(d->name)).arg(lvl).arg(esc(d->description), esc(d->initialLevelNote));
    for (int i = 0; i < d->levels.size(); ++i) {
        const auto &L = d->levels[i];
        const QString state = i <= lvl ? "<span style='color:#2a9d4b'>✔ achieved</span>" : i == lvl + 1 ? "<span style='color:#c98a00'>▶ next</span>" : "<span style='color:#888'>locked</span>";
        QStringList req;
        if (i > 0) req << QStringLiteral("%1 points").arg(num(L.pointsRequired));
        for (auto it = L.prerequisites.begin(); it != L.prerequisites.end(); ++it) { const auto *pd = cat.techDomain(it.key()); req << QStringLiteral("%1 ≥ %2").arg(pd ? pd->name : it.key()).arg(it.value()); }
        for (const auto &tg : L.requiredFacilityTags) req << QStringLiteral("operating '%1'").arg(tg);
        h += QStringLiteral("<tr><td valign='top'><b>L%1</b></td><td valign='top'>%2</td><td><b>%3</b><br>%4<br><small>Requires: %5%6</small></td></tr>")
                 .arg(i).arg(state, esc(L.name), esc(L.description), esc(req.join(", ")), L.unlocks.isEmpty() ? QString() : "<br>Unlocks: " + esc(L.unlocks));
    }
    h += "</table>";
    QStringList blockers;
    TechnologyEngine::canAdvance(ctx, d->id, &blockers);
    if (!blockers.isEmpty()) h += "<p><b>Blocking the next level:</b><br>" + esc(blockers.join("\n")).replace("\n", "<br>") + "</p>";
    QStringList best;
    for (const auto &fid : SuitabilityEngine::facilitiesForTechDomain(ctx, d->id, 4)) if (const auto *f = cat.facility(fid)) best << QStringLiteral("%1 (+%2 pts/yr, %3)").arg(f->name, num(f->researchPoints.value(d->id)), cr(f->capexCr));
    if (!best.isEmpty()) h += "<p><b>Most cost-effective research facilities:</b><br>" + esc(best.join("\n")).replace("\n", "<br>") + "</p>";
    m_details->setHtml(h);
}

} // namespace isim
