#include "AIProviderSettings.h"
#include "ai/AppSettings.h"
#include "ai/CloudProviders.h"
#include "ai/LocalLlamaProvider.h"
#include "ai/SecretStore.h"
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace isim {

LocalModelSettings::LocalModelSettings(AIManager *ai, QWidget *parent) : QWidget(parent), m_ai(ai)
{
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Choose a quantised <b>GGUF</b> model file already on this computer. Recommended: <b>%1</b> (≈1.1 GB). "
                                "The model runs offline through llama.cpp and needs no API key. The application never downloads a model by itself — "
                                "<a href='%2'>get it from Hugging Face</a> if you want it.").arg(LocalLlamaProvider::recommendedModel(), LocalLlamaProvider::recommendedModelUrl()));
    intro->setWordWrap(true);
    intro->setOpenExternalLinks(true);
    lay->addWidget(intro);
    auto *form = new QFormLayout;
    auto *mrow = new QHBoxLayout;
    m_model = new QLineEdit(AppSettings::localModelPath());
    m_model->setPlaceholderText(tr("Path to model .gguf"));
    auto *browse = new QPushButton(tr("Browse…"));
    mrow->addWidget(m_model, 1); mrow->addWidget(browse);
    form->addRow(tr("Model file:"), mrow);
    auto *srow = new QHBoxLayout;
    m_server = new QLineEdit(AppSettings::llamaServerPath());
    m_server->setPlaceholderText(tr("auto-detect: %1").arg(LocalLlamaProvider::findServerExecutable().isEmpty() ? tr("not found") : LocalLlamaProvider::findServerExecutable()));
    auto *browseS = new QPushButton(tr("Browse…"));
    srow->addWidget(m_server, 1); srow->addWidget(browseS);
    form->addRow(tr("llama-server:"), srow);
    m_ctx = new QSpinBox; m_ctx->setRange(2048, 32768); m_ctx->setSingleStep(1024); m_ctx->setValue(AppSettings::localContextSize());
    m_gpu = new QSpinBox; m_gpu->setRange(0, 999); m_gpu->setValue(AppSettings::localGpuLayers());
    m_gpu->setToolTip(tr("Layers offloaded to the GPU (needs a CUDA/Vulkan build of llama.cpp). 0 = CPU only. 99 = all layers; a 1.7B Q4_K_M model fits easily in 6 GB VRAM."));
    m_threads = new QSpinBox; m_threads->setRange(0, 64); m_threads->setValue(AppSettings::localThreads()); m_threads->setSpecialValueText(tr("auto"));
    form->addRow(tr("Context size (tokens):"), m_ctx);
    form->addRow(tr("GPU layers:"), m_gpu);
    form->addRow(tr("CPU threads:"), m_threads);
    lay->addLayout(form);
    m_info = new QLabel;
    m_info->setWordWrap(true);
    lay->addWidget(m_info);
    auto *trow = new QHBoxLayout;
    m_test = new QPushButton(tr("Test model (load + short prompt)"));
    trow->addWidget(m_test);
    trow->addStretch();
    lay->addLayout(trow);
    m_testResult = new QLabel;
    m_testResult->setWordWrap(true);
    lay->addWidget(m_testResult);
    lay->addStretch();
    connect(browse, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, tr("Select GGUF model"), QFileInfo(m_model->text()).absolutePath(), tr("GGUF models (*.gguf)"));
        if (!f.isEmpty()) { m_model->setText(f); inspect(); }
    });
    connect(browseS, &QPushButton::clicked, this, [this]() {
        const QString f = QFileDialog::getOpenFileName(this, tr("Select llama-server executable"));
        if (!f.isEmpty()) m_server->setText(f);
    });
    connect(m_model, &QLineEdit::editingFinished, this, &LocalModelSettings::inspect);
    connect(m_test, &QPushButton::clicked, this, [this]() {
        apply();
        auto *p = m_ai->provider("local");
        if (!p->isConfigured()) { m_testResult->setText(tr("<span style='color:#c0392b'>Select a valid model file first.</span>")); return; }
        m_testResult->setText(tr("Loading the model… (this can take a while on first load)"));
        p->stop();
        p->start();
        auto *watch = new QObject(this);
        auto runTest = [this, p, watch]() {
            connect(p, &AIProvider::testFinished, watch, [this, watch](bool ok, const QString &msg) {
                m_testResult->setText(ok ? tr("<span style='color:#2a9d4b'>✔ Model works.</span> %1").arg(msg.toHtmlEscaped()) : tr("<span style='color:#c0392b'>✖ %1</span>").arg(msg.toHtmlEscaped()));
                watch->deleteLater();
            });
            p->test();
        };
        if (p->status() == AIStatus::Error) { m_testResult->setText("<span style='color:#c0392b'>" + p->statusText().toHtmlEscaped() + "</span>"); watch->deleteLater(); return; }
        connect(p, &AIProvider::statusChanged, watch, [this, p, watch, runTest](AIStatus s, const QString &t) {
            if (s == AIStatus::Ready) { disconnect(p, &AIProvider::statusChanged, watch, nullptr); runTest(); }
            else if (s == AIStatus::Error) { m_testResult->setText("<span style='color:#c0392b'>" + t.toHtmlEscaped() + "</span>"); watch->deleteLater(); }
            else m_testResult->setText(t.toHtmlEscaped());
        });
    });
    inspect();
}

bool LocalModelSettings::hasModel() const { return !m_model->text().isEmpty() && GgufInfo::inspect(m_model->text()).valid; }

void LocalModelSettings::inspect()
{
    const QString path = m_model->text();
    if (path.isEmpty()) { m_info->setText(tr("<i>No model selected. You can skip this; the simulator works without AI.</i>")); return; }
    const auto g = GgufInfo::inspect(path);
    if (!g.valid) { m_info->setText(tr("<span style='color:#c0392b'>✖ %1</span>").arg(g.error.toHtmlEscaped())); return; }
    m_info->setText(tr("<span style='color:#2a9d4b'>✔ Valid GGUF v%1</span> — %2 MB, architecture: %3, name: %4, quantisation: %5, %6 tensors")
                        .arg(g.version).arg(g.fileSize / 1048576).arg(g.architecture.isEmpty() ? "?" : g.architecture, g.name.isEmpty() ? "?" : g.name,
                                                                       g.fileType.isEmpty() ? "?" : g.fileType).arg(g.tensorCount));
}

void LocalModelSettings::apply()
{
    AppSettings::setLocalModelPath(m_model->text().trimmed());
    AppSettings::setLlamaServerPath(m_server->text().trimmed());
    AppSettings::setLocalContextSize(m_ctx->value());
    AppSettings::setLocalGpuLayers(m_gpu->value());
    AppSettings::setLocalThreads(m_threads->value());
}

CloudKeySettings::CloudKeySettings(AIManager *ai, const QString &id, QWidget *parent) : QWidget(parent), m_ai(ai), m_id(id)
{
    const bool openai = id == "openai";
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(openai ? tr("Optional. Enter an <b>OpenAI API key</b> to use OpenAI models for explanations. You can add it later in Settings → AI Providers.")
                                    : tr("Optional. Enter a <b>Google Gemini API key</b> (Google AI Studio) to use Gemini for explanations. You can add it later in Settings → AI Providers."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto *form = new QFormLayout;
    m_key = new QLineEdit;
    m_key->setEchoMode(QLineEdit::Password);
    m_key->setPlaceholderText(SecretStore::read(openai ? OpenAIProvider::secretKey() : GeminiProvider::secretKey()).isEmpty() ? tr("paste API key") : tr("•••••• stored — leave empty to keep"));
    form->addRow(tr("API key:"), m_key);
    m_model = new QLineEdit(openai ? AppSettings::openAIModel() : AppSettings::geminiModel());
    form->addRow(tr("Model:"), m_model);
    if (openai) {
        m_endpoint = new QLineEdit(AppSettings::openAIEndpoint());
        m_endpoint->setToolTip(tr("Any OpenAI-compatible endpoint can be used."));
        form->addRow(tr("Endpoint:"), m_endpoint);
    }
    lay->addLayout(form);
    m_state = new QLabel(tr("Key storage: %1%2").arg(SecretStore::backendName(), SecretStore::isSecureBackend() ? QString() : tr(" — on this platform keys are not encrypted")));
    m_state->setWordWrap(true);
    lay->addWidget(m_state);
    auto *row = new QHBoxLayout;
    auto *test = new QPushButton(tr("Test"));
    auto *remove = new QPushButton(tr("Remove stored key"));
    row->addWidget(test); row->addWidget(remove); row->addStretch();
    lay->addLayout(row);
    m_testResult = new QLabel;
    m_testResult->setWordWrap(true);
    lay->addWidget(m_testResult);
    auto *privacy = new QLabel(tr("<small>Privacy: when a cloud provider is selected, the structured simulation data needed for the question is sent to that provider. No personal data is included.</small>"));
    privacy->setWordWrap(true);
    lay->addWidget(privacy);
    lay->addStretch();
    connect(test, &QPushButton::clicked, this, [this]() {
        apply();
        auto *p = m_ai->provider(m_id);
        p->start();
        if (!p->isConfigured()) { m_testResult->setText(tr("<span style='color:#c0392b'>No key stored.</span>")); return; }
        m_testResult->setText(tr("Contacting the service…"));
        auto *watch = new QObject(this);
        connect(p, &AIProvider::testFinished, watch, [this, watch](bool ok, const QString &msg) {
            m_testResult->setText(ok ? tr("<span style='color:#2a9d4b'>✔ Connected.</span> %1").arg(msg.toHtmlEscaped()) : tr("<span style='color:#c0392b'>✖ %1</span>").arg(msg.toHtmlEscaped()));
            watch->deleteLater();
        });
        p->test();
    });
    connect(remove, &QPushButton::clicked, this, [this]() {
        SecretStore::remove(m_id == "openai" ? OpenAIProvider::secretKey() : GeminiProvider::secretKey());
        m_ai->provider(m_id)->start();
        m_key->setPlaceholderText(tr("paste API key"));
        m_testResult->setText(tr("Key removed."));
    });
}

void CloudKeySettings::apply()
{
    const bool openai = m_id == "openai";
    if (!m_key->text().trimmed().isEmpty()) {
        QString err;
        if (!SecretStore::write(openai ? OpenAIProvider::secretKey() : GeminiProvider::secretKey(), m_key->text().trimmed(), &err))
            QMessageBox::warning(this, tr("Could not store key"), err);
        m_key->clear();
        m_key->setPlaceholderText(tr("•••••• stored — leave empty to keep"));
    }
    if (openai) { AppSettings::setOpenAIModel(m_model->text().trimmed()); if (m_endpoint) AppSettings::setOpenAIEndpoint(m_endpoint->text().trimmed()); }
    else AppSettings::setGeminiModel(m_model->text().trimmed());
    m_ai->provider(m_id)->start();
}

DefaultProviderSettings::DefaultProviderSettings(AIManager *ai, QWidget *parent) : QWidget(parent), m_ai(ai)
{
    auto *lay = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("Which AI should the simulator use for explanations? You can change this at any time from the AI panel. "
                                "The AI never calculates the simulation's numbers; it only explains them."));
    intro->setWordWrap(true);
    lay->addWidget(intro);
    auto add = [&](const QString &id, const QString &label) {
        auto *b = new QRadioButton(label);
        b->setChecked(AppSettings::defaultProvider() == id);
        lay->addWidget(b);
        m_buttons.append({id, b});
    };
    add("local", tr("Local model (offline, private) — recommended"));
    add("openai", tr("OpenAI (cloud; needs API key)"));
    add("gemini", tr("Google Gemini (cloud; needs API key)"));
    add("none", tr("No AI for now"));
    bool any = false; for (auto &b : m_buttons) any |= b.second->isChecked();
    if (!any) m_buttons.last().second->setChecked(true);
    lay->addStretch();
}

void DefaultProviderSettings::apply()
{
    for (auto &b : m_buttons) if (b.second->isChecked()) { m_ai->setActive(b.first); if (b.first == "openai" || b.first == "gemini") AppSettings::setCloudConsentGiven(true); }
}

} // namespace isim
