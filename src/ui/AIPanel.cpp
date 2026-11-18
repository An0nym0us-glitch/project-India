#include "AIPanel.h"
#include "Format.h"
#include "ai/AIContextBuilder.h"
#include "ai/AppSettings.h"
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace isim {
using namespace fmt;

AIPanel::AIPanel(AppController *app, QWidget *parent) : QWidget(parent), m_app(app)
{
    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(4, 4, 4, 4);
    auto *top = new QHBoxLayout;
    m_provider = new QComboBox;
    m_provider->addItem(tr("No AI (simulation only)"), "none");
    for (auto *p : app->ai()->providers()) m_provider->addItem(p->displayName(), p->id());
    m_provider->setCurrentIndex(std::max(0, m_provider->findData(app->ai()->activeId())));
    m_startStop = new QPushButton(tr("Load model"));
    auto *settings = new QPushButton(tr("Settings…"));
    top->addWidget(m_provider, 1); top->addWidget(m_startStop); top->addWidget(settings);
    lay->addLayout(top);
    m_status = new QLabel;
    m_status->setWordWrap(true);
    lay->addWidget(m_status);
    auto *quick = new QHBoxLayout;
    auto *report = new QPushButton(tr("Interpret national report"));
    auto *alts = new QPushButton(tr("Suggest alternative strategies"));
    quick->addWidget(report); quick->addWidget(alts);
    lay->addLayout(quick);
    m_out = new QTextBrowser;
    lay->addWidget(m_out, 1);
    auto *qrow = new QHBoxLayout;
    m_question = new QLineEdit;
    m_question->setPlaceholderText(tr("Ask about the current simulation…"));
    auto *askBtn = new QPushButton(tr("Ask"));
    qrow->addWidget(m_question, 1); qrow->addWidget(askBtn);
    lay->addLayout(qrow);
    auto *note = new QLabel(tr("<small>The AI only explains results computed by the simulation engine. Numbers it mentions that are not in the provided data are flagged as <b>unverified</b>.</small>"));
    note->setWordWrap(true);
    lay->addWidget(note);

    connect(m_provider, &QComboBox::currentIndexChanged, this, [this]() {
        const QString id = m_provider->currentData().toString();
        auto *p = m_app->ai()->provider(id);
        if (p && !p->isLocal() && !AppSettings::cloudConsentGiven()) {
            if (QMessageBox::question(this, tr("Cloud AI provider"), tr("Using %1 sends structured simulation data (no personal data) to an external service. Continue?").arg(p->displayName())) != QMessageBox::Yes) {
                m_provider->setCurrentIndex(m_provider->findData(m_app->ai()->activeId()));
                return;
            }
            AppSettings::setCloudConsentGiven(true);
        }
        m_app->ai()->setActive(id);
        updateStatus();
    });
    connect(m_startStop, &QPushButton::clicked, this, [this]() {
        auto *p = m_app->ai()->active();
        if (!p) return;
        if (p->status() == AIStatus::Ready || p->status() == AIStatus::Loading) p->stop(); else p->start();
    });
    connect(settings, &QPushButton::clicked, this, &AIPanel::openSettings);
    connect(report, &QPushButton::clicked, this, [this]() { ask("interpret_report", AIContextBuilder::nationalSnapshot(m_app->sim()), tr("National report")); });
    connect(alts, &QPushButton::clicked, this, [this]() { ask("alternatives", AIContextBuilder::nationalSnapshot(m_app->sim()), tr("Alternative strategies")); });
    auto doAsk = [this]() {
        if (m_question->text().trimmed().isEmpty()) return;
        ask("question", AIContextBuilder::nationalSnapshot(m_app->sim()), m_question->text().trimmed());
        m_question->clear();
    };
    connect(askBtn, &QPushButton::clicked, this, doAsk);
    connect(m_question, &QLineEdit::returnPressed, this, doAsk);
    connect(app->ai(), &AIManager::providerStatusChanged, this, &AIPanel::updateStatus);
    connect(app->ai(), &AIManager::activeChanged, this, &AIPanel::updateStatus);
    connect(app->ai(), &AIManager::answered, this, &AIPanel::showAnswer);
    updateStatus();
}

void AIPanel::updateStatus()
{
    auto *p = m_app->ai()->active();
    if (!p) {
        m_status->setText(tr("AI is off. The simulation works fully without it."));
        m_startStop->setVisible(false);
        return;
    }
    m_startStop->setVisible(p->isLocal());
    m_startStop->setText(p->status() == AIStatus::Ready || p->status() == AIStatus::Loading ? tr("Unload model") : tr("Load model"));
    const QString col = p->status() == AIStatus::Ready ? "#2a9d4b" : p->status() == AIStatus::Error ? "#c0392b" : p->status() == AIStatus::Busy || p->status() == AIStatus::Loading ? "#c98a00" : "#888";
    m_status->setText(QStringLiteral("<b>%1</b>: <span style='color:%2'>%3</span> — %4").arg(esc(p->displayName()), col, aiStatusName(p->status()), esc(p->statusText())));
}

void AIPanel::ask(const QString &task, const QJsonObject &context, const QString &title)
{
    auto *p = m_app->ai()->active();
    if (!p) { QMessageBox::information(this, tr("AI is off"), tr("Select an AI provider above (local model, OpenAI or Gemini). The simulation itself does not need AI.")); return; }
    if (p->isLocal() && p->status() != AIStatus::Ready) {
        if (p->isConfigured() && p->status() != AIStatus::Loading) p->start();
        QMessageBox::information(this, tr("Local model"), p->isConfigured() ? tr("The local model is loading; ask again when its status is Ready.") : tr("No local model file is selected. Choose one in Settings → AI Providers."));
        return;
    }
    const QString id = m_app->ai()->ask(task, context, task == "question" ? title : QString());
    m_titles.insert(id, title);
    m_html = QStringLiteral("<p style='color:#888'>Asking %1 about <b>%2</b>…</p>").arg(esc(p->displayName()), esc(title)) + m_html;
    m_out->setHtml(m_html);
}

static QString ul(const QStringList &l)
{
    QString s = "<ul style='margin-top:0'>";
    for (const auto &x : l) s += "<li>" + x.toHtmlEscaped() + "</li>";
    return s + "</ul>";
}

void AIPanel::showAnswer(const AIAnswer &a)
{
    QString h = QStringLiteral("<hr><h3>%1</h3><div style='color:#888'><small>%2 · %3 s</small></div>").arg(esc(m_titles.value(a.requestId, a.task)), esc(a.providerName)).arg(a.seconds, 0, 'f', 1);
    if (!a.error.isEmpty()) h += QStringLiteral("<p style='color:#c0392b'><b>Request failed:</b> %1</p>").arg(esc(a.error));
    else {
        const auto &r = a.response;
        if (!r.structured) h += QStringLiteral("<p style='color:#c0392b'><b>The response did not pass validation</b> (%1). Raw text shown below for transparency; treat it as unverified.</p><pre style='white-space:pre-wrap'>%2</pre>").arg(esc(r.problems.join("; ")), esc(r.rawText.left(3000)));
        else {
            QString summary = esc(r.summary);
            for (const auto &n : r.unverifiedNumbers) summary.replace(n, QStringLiteral("<span style='background:#ffd7d7' title='Not in simulation data'>%1 [unverified]</span>").arg(n));
            h += "<p>" + summary + "</p>";
            if (!r.keyPoints.isEmpty()) h += "<b>Key points</b>" + ul(r.keyPoints);
            if (!r.risks.isEmpty()) h += "<b>Risks</b>" + ul(r.risks);
            if (!r.alternatives.isEmpty()) h += "<b>Alternatives</b>" + ul(r.alternatives);
            if (!r.missingAssumptions.isEmpty()) h += "<b>Missing information / assumptions</b>" + ul(r.missingAssumptions);
            if (!r.unverifiedNumbers.isEmpty())
                h += QStringLiteral("<p style='color:#c0392b'><small>⚠ The model mentioned numbers that are not in the simulation data: %1. They should not be treated as facts.</small></p>").arg(esc(r.unverifiedNumbers.join(", ")));
            else h += "<p style='color:#2a9d4b'><small>✔ Every number in this answer appears in the simulation data provided to the model.</small></p>";
        }
    }
    // replace the pending line for this request
    const int idx = m_html.indexOf("<p style='color:#888'>Asking");
    if (idx >= 0) { const int end = m_html.indexOf("</p>", idx); m_html.remove(idx, end - idx + 4); }
    m_html = h + m_html;
    m_out->setHtml(m_html);
}

} // namespace isim
