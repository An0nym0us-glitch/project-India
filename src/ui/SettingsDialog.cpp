#include "SettingsDialog.h"
#include "AIProviderSettings.h"
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include "ai/AppSettings.h"
#include <QMessageBox>

namespace isim {

SettingsDialog::SettingsDialog(AppController *app, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Settings — AI Providers"));
    resize(760, 600);
    auto *lay = new QVBoxLayout(this);
    auto *tabs = new QTabWidget;
    m_default = new DefaultProviderSettings(app->ai());
    m_local = new LocalModelSettings(app->ai());
    m_openai = new CloudKeySettings(app->ai(), "openai");
    m_gemini = new CloudKeySettings(app->ai(), "gemini");
    tabs->addTab(m_default, tr("Default AI"));
    tabs->addTab(m_local, tr("Local model (llama.cpp)"));
    tabs->addTab(m_openai, tr("OpenAI"));
    tabs->addTab(m_gemini, tr("Gemini"));
    lay->addWidget(tabs);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto *rerun = bb->addButton(tr("Run setup wizard again on next start"), QDialogButtonBox::ResetRole);
    connect(rerun, &QPushButton::clicked, this, [this]() { AppSettings::setFirstRunComplete(false); QMessageBox::information(this, tr("Setup"), tr("The setup wizard will open the next time the application starts.")); });
    connect(bb, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(bb);
}

void SettingsDialog::accept()
{
    m_local->apply();
    m_openai->apply();
    m_gemini->apply();
    m_default->apply();
    QDialog::accept();
}

} // namespace isim
