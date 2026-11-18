#include "AdvisorPanel.h"
#include "Format.h"
#include "ai/AIContextBuilder.h"
#include "catalog/Catalog.h"
#include "gis/GeoDatabase.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

AdvisorPanel::AdvisorPanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    auto *title = new QLabel(tr("<b>NATIONAL DEVELOPMENT ADVISOR</b><br><small>Recommendations computed from the current simulation state. You decide what to build.</small>"));
    title->setWordWrap(true);
    lay->addWidget(title);
    auto *top = new QHBoxLayout;
    m_filter = new QComboBox;
    m_filter->addItems({tr("Open recommendations"), tr("Saved"), tr("Accepted"), tr("Rejected"), tr("All")});
    m_reanalyse = new QPushButton(tr("Re-analyse"));
    top->addWidget(m_filter, 1);
    top->addWidget(m_reanalyse);
    lay->addLayout(top);
    auto *split = new QSplitter(Qt::Vertical);
    m_list = new QTreeWidget;
    m_list->setHeaderLabels({tr("Priority"), tr("Recommendation"), tr("Investment")});
    m_list->setRootIsDecorated(false);
    m_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_details = new QTextBrowser;
    split->addWidget(m_list);
    split->addWidget(m_details);
    split->setStretchFactor(1, 2);
    lay->addWidget(split, 1);
    auto *btns = new QHBoxLayout;
    m_accept = new QPushButton(tr("Accept && place"));
    m_save = new QPushButton(tr("Save"));
    m_reject = new QPushButton(tr("Reject"));
    m_ai = new QPushButton(tr("Explain (AI)"));
    btns->addWidget(m_accept); btns->addWidget(m_save); btns->addWidget(m_reject); btns->addWidget(m_ai);
    lay->addLayout(btns);

    connect(m_filter, &QComboBox::currentIndexChanged, this, &AdvisorPanel::refresh);
    connect(m_reanalyse, &QPushButton::clicked, this, [this]() { m_app->invalidateRecommendations(); refresh(); });
    connect(m_list, &QTreeWidget::currentItemChanged, this, &AdvisorPanel::showDetails);
    connect(m_accept, &QPushButton::clicked, this, [this]() {
        const auto *r = current();
        if (!r) return;
        m_app->setAdvisorDecision(r->id, "accepted");
        if (r->targetDistrict >= 0) m_app->focusDistrict(r->targetDistrict);
        if (!r->facilityId.isEmpty()) m_app->beginPlacement(r->facilityId);
        refresh();
    });
    connect(m_save, &QPushButton::clicked, this, [this]() { if (const auto *r = current()) { m_app->setAdvisorDecision(r->id, "saved"); refresh(); } });
    connect(m_reject, &QPushButton::clicked, this, [this]() { if (const auto *r = current()) { m_app->setAdvisorDecision(r->id, "rejected"); refresh(); } });
    connect(m_ai, &QPushButton::clicked, this, [this]() { if (const auto *r = current()) emit askAI("explain_recommendation", AIContextBuilder::recommendation(m_app->sim(), *r), r->title); });
    connect(app, &AppController::worldReset, this, &AdvisorPanel::refresh);
    // refresh yearly (recommendations are invalidated by the controller)
    connect(app, &AppController::worldUpdated, this, [this]() { if (m_app->sim().world().month % 12 == 0 && isVisible()) refresh(); });
}

const Recommendation *AdvisorPanel::current() const
{
    auto *it = m_list->currentItem();
    if (!it) return nullptr;
    const QString id = it->data(0, Qt::UserRole).toString();
    for (const auto &r : m_app->recommendations()) if (r.id == id) return &r;
    return nullptr;
}

void AdvisorPanel::refresh()
{
    const QString keep = m_list->currentItem() ? m_list->currentItem()->data(0, Qt::UserRole).toString() : QString();
    m_list->clear();
    const int f = m_filter->currentIndex();
    QTreeWidgetItem *sel = nullptr;
    for (const auto &r : m_app->recommendations()) {
        const QString d = r.decision;
        if (f == 0 && (d == "accepted" || d == "rejected")) continue;
        if (f == 1 && d != "saved") continue;
        if (f == 2 && d != "accepted") continue;
        if (f == 3 && d != "rejected") continue;
        auto *it = new QTreeWidgetItem(m_list);
        it->setText(0, QString::number(r.priority));
        it->setText(1, QStringLiteral("[%1] %2%3").arg(r.category, r.title, d.isEmpty() ? QString() : "  (" + d + ")"));
        it->setText(2, r.investmentCr > 0 ? cr(r.investmentCr) : tr("policy"));
        it->setData(0, Qt::UserRole, r.id);
        const QColor c = r.priority >= 80 ? QColor(192, 57, 43) : r.priority >= 60 ? QColor(201, 138, 0) : QColor(90, 90, 90);
        it->setForeground(0, c);
        if (r.id == keep) sel = it;
    }
    if (!sel && m_list->topLevelItemCount() > 0) sel = m_list->topLevelItem(0);
    if (sel) m_list->setCurrentItem(sel);
    else m_details->setHtml(tr("<p>No recommendations in this view.</p>"));
}

static QString ul(const QStringList &l)
{
    if (l.isEmpty()) return QStringLiteral("—");
    QString s = "<ul style='margin-top:0'>";
    for (const auto &x : l) s += "<li>" + x.toHtmlEscaped() + "</li>";
    return s + "</ul>";
}

void AdvisorPanel::showDetails()
{
    const auto *r = current();
    if (!r) return;
    const auto &cat = m_app->resources().catalog;
    QString h = QStringLiteral("<h3>%1</h3><div><b>%2</b> · priority %3/100 %4</div>").arg(esc(r->title), esc(r->category)).arg(r->priority).arg(badge(DataKind::Simulated));
    h += "<p><b>Why this matters.</b> " + esc(r->why) + "</p>";
    if (const auto *d = cat.facility(r->facilityId)) h += QStringLiteral("<p><b>Investment:</b> %1 × %2 — estimated %3</p>").arg(r->quantity).arg(esc(d->name), cr(r->investmentCr));
    if (const auto *p = m_app->resources().policies.find(r->policyId)) h += QStringLiteral("<p><b>Related policy:</b> %1 (see Policy workspace)</p>").arg(esc(p->name));
    h += "<b>Suitable locations</b>" + ul(r->locations);
    h += "<b>Required supporting infrastructure</b>" + ul(r->supportingInfrastructure);
    h += "<b>Expected benefits</b>" + ul(r->benefits);
    h += "<b>Expected drawbacks</b>" + ul(r->drawbacks);
    h += "<p><b>Opportunity cost.</b> " + esc(r->opportunityCost) + "</p>";
    h += "<p><b>Short-term effects.</b> " + esc(r->shortTerm) + "<br><b>Long-term effects.</b> " + esc(r->longTerm) + "</p>";
    h += "<b>Risks and uncertainties</b>" + ul(r->risks);
    h += "<b>Alternative investments</b>" + ul(r->alternatives);
    if (!r->evidence.isEmpty()) {
        h += "<b>Evidence</b><table>";
        for (const auto &e : r->evidence) h += QStringLiteral("<tr><td>%1</td><td><b>%2</b></td><td><small>%3</small></td></tr>").arg(esc(e.label), esc(e.value), esc(e.source));
        h += "</table>";
    }
    m_details->setHtml(h);
    m_accept->setEnabled(!r->facilityId.isEmpty());
}

} // namespace isim
