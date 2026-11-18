#include "SetupWizard.h"
#include "AIProviderSettings.h"
#include "ai/AppSettings.h"
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWizardPage>

namespace isim {

static QWizardPage *makePage(const QString &title, const QString &sub, QWidget *content)
{
    auto *p = new QWizardPage;
    p->setTitle(title);
    p->setSubTitle(sub);
    auto *l = new QVBoxLayout(p);
    l->addWidget(content);
    return p;
}

SetupWizard::SetupWizard(AppController *app, QWidget *parent) : QWizard(parent), m_app(app)
{
    setWindowTitle(tr("Welcome — first-time setup"));
    setWizardStyle(QWizard::ModernStyle);
    setOption(QWizard::NoBackButtonOnStartPage);
    resize(780, 600);
    auto *welcome = new QLabel(tr(
        "<h2>India National Development &amp; Policy Simulator</h2>"
        "<p>A GIS-based planning laboratory: you act as the government of India and build the country's development infrastructure, "
        "institutions and policies on an initially <b>empty development layer</b> over real geography and real population data.</p>"
        "<ul><li>The C++ simulation engine computes all economic, demographic, engineering and environmental results.</li>"
        "<li>Every number is labelled as measured data, estimate, modelling assumption, user setting or simulated projection.</li>"
        "<li>An optional AI assistant explains results. It can run <b>fully offline</b> with a local model, or use OpenAI / Gemini if you add keys.</li></ul>"
        "<p>This setup takes a minute. Everything here can be changed later in <b>Settings</b>.</p>"));
    welcome->setWordWrap(true);
    addPage(makePage(tr("Welcome"), tr("What this application is"), welcome));
    m_local = new LocalModelSettings(app->ai());
    addPage(makePage(tr("Local AI model (optional)"), tr("Select a GGUF model file such as Qwen3-1.7B Q4_K_M. Nothing is downloaded automatically."), m_local));
    auto *cloud = new QWidget;
    auto *cl = new QVBoxLayout(cloud);
    m_openai = new CloudKeySettings(app->ai(), "openai");
    m_gemini = new CloudKeySettings(app->ai(), "gemini");
    cl->addWidget(m_openai);
    cl->addWidget(m_gemini);
    addPage(makePage(tr("Online AI providers (optional)"), tr("You can skip this and add keys later in Settings → AI Providers."), cloud));
    m_default = new DefaultProviderSettings(app->ai());
    addPage(makePage(tr("Default AI"), tr("Choose the assistant used for explanations."), m_default));
    auto *sc = new QWidget;
    auto *form = new QFormLayout(sc);
    m_name = new QLineEdit(tr("My India development plan"));
    m_year = new QSpinBox; m_year->setRange(2024, 2100); m_year->setValue(2025);
    m_fund = new QSpinBox; m_fund->setRange(0, 100000000); m_fund->setSingleStep(50000); m_fund->setValue(500000); m_fund->setSuffix(tr(" ₹ crore"));
    form->addRow(tr("Scenario name:"), m_name);
    form->addRow(tr("Start year:"), m_year);
    form->addRow(tr("Initial development fund:"), m_fund);
    auto *note = new QLabel(tr("<small>Population, age structure and labour force are initialised from World Bank data for the reference year and distributed across the 640 districts by Census 2011 shares. "
                               "Infrastructure starts empty.</small>"));
    note->setWordWrap(true);
    form->addRow(note);
    addPage(makePage(tr("Your first scenario"), tr("Starting conditions (user-configured)."), sc));
}

ScenarioSettings SetupWizard::scenarioSettings() const
{
    ScenarioSettings s;
    s.name = m_name->text();
    s.startYear = m_year->value();
    s.initialFundCr = m_fund->value();
    return s;
}

void SetupWizard::accept()
{
    m_local->apply();
    m_openai->apply();
    m_gemini->apply();
    m_default->apply();
    AppSettings::setFirstRunComplete(true);
    QWizard::accept();
}

} // namespace isim
