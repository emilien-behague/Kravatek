#include "MainComponent.h"
#include "dsp/WavWriter.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace {
const juce::Colour background {0xff101116}, panel {0xff191b23}, ink {0xffebecf3}, muted {0xff9b9fad}, accent {0xffb6f36a};
std::filesystem::path nativePath(const juce::File& file) {
#if JUCE_WINDOWS
    return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
    return std::filesystem::path(file.getFullPathName().toStdString());
#endif
}
}

void WaveformComponent::setKick(const kravatek::RenderedKick& kick) {
    samples_ = kick.samples;
    duration_ = static_cast<double>(samples_.size()) / kick.sampleRate;
    repaint();
}

void WaveformComponent::paint(juce::Graphics& g) {
    g.setColour(panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 8);
    auto area = getLocalBounds().reduced(18, 24);
    g.setColour(muted);
    g.setFont(12);
    g.drawText("WAVEFORM", 18, 6, 180, 18, juce::Justification::centredLeft);
    g.drawText(juce::String(duration_ * 1000, 0) + " ms / mono", 18, getHeight() - 23,
               getWidth() - 36, 18, juce::Justification::centredRight);
    if (area.getWidth() <= 0 || samples_.empty()) return;
    const auto mid = static_cast<float>(area.getCentreY());
    g.setColour(juce::Colour(0xff30323e));
    g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine(area.getX() + i * area.getWidth() / 8, static_cast<float>(area.getY()), static_cast<float>(area.getBottom()));
    g.setColour(accent);
    for (int x = 0; x < area.getWidth(); ++x) {
        const auto first = static_cast<std::size_t>(x) * samples_.size() / static_cast<std::size_t>(area.getWidth());
        const auto last = std::min(samples_.size(), std::max(first + 1,
            static_cast<std::size_t>(x + 1) * samples_.size() / static_cast<std::size_t>(area.getWidth())));
        const auto mm = std::minmax_element(samples_.begin() + static_cast<std::ptrdiff_t>(first),
                                           samples_.begin() + static_cast<std::ptrdiff_t>(last));
        const auto scale = static_cast<float>(area.getHeight()) * .47f;
        g.drawLine(static_cast<float>(area.getX() + x), mid - *mm.second * scale,
                   static_cast<float>(area.getX() + x), mid - *mm.first * scale, 1);
    }
}

MainComponent::MainComponent() {
    lookAndFeel_.setColour(juce::ResizableWindow::backgroundColourId, background);
    lookAndFeel_.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff30343f));
    lookAndFeel_.setColour(juce::TextButton::textColourOffId, ink);
    lookAndFeel_.setColour(juce::Slider::thumbColourId, accent);
    lookAndFeel_.setColour(juce::Slider::trackColourId, accent);
    lookAndFeel_.setColour(juce::Slider::textBoxTextColourId, ink);
    lookAndFeel_.setColour(juce::Slider::textBoxBackgroundColourId, panel);
    lookAndFeel_.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setLookAndFeel(&lookAndFeel_);

    for (auto* component : std::initializer_list<juce::Component*> {&title_, &subtitle_, &pitchLabel_, &decayLabel_,
            &seedLabel_, &status_, &format_, &play_, &generate_, &export_, &audio_, &pitch_, &decay_, &seed_, &waveform_})
        addAndMakeVisible(component);
    for (auto* label : {&title_, &subtitle_, &pitchLabel_, &decayLabel_, &seedLabel_, &status_, &format_})
        label->setColour(juce::Label::textColourId, ink);
    title_.setText("KRAVATEK KICK LAB", juce::dontSendNotification);
    title_.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    subtitle_.setText("PROTOTYPE 001  /  KEEP THE DNA. MUTATE THE SOUND.", juce::dontSendNotification);
    subtitle_.setColour(juce::Label::textColourId, muted);
    subtitle_.setFont(juce::Font(juce::FontOptions(12.0f)));
    pitchLabel_.setText("PITCH END", juce::dontSendNotification);
    decayLabel_.setText("DECAY", juce::dontSendNotification);
    seedLabel_.setText("SEED", juce::dontSendNotification);
    format_.setText("SYNTHESIS ONLY  /  WAV PCM 24 BIT  /  48 kHz  /  PEAK -3 dBFS", juce::dontSendNotification);
    format_.setFont(juce::Font(juce::FontOptions(11.0f)));
    format_.setColour(juce::Label::textColourId, muted);
    status_.setFont(juce::Font(juce::FontOptions(12.0f)));
    play_.setColour(juce::TextButton::buttonColourId, accent);
    play_.setColour(juce::TextButton::textColourOffId, background);
    play_.setTooltip(juce::String::fromUTF8("Écouter le kick. Les clics répétés relancent avec une courte extinction."));
    generate_.setTooltip(juce::String::fromUTF8("Créer une variation synthétique avec une nouvelle graine."));
    export_.setTooltip(juce::String::fromUTF8("Exporter le kick affiché, avec sa graine et ses paramètres."));

    for (auto* slider : {&pitch_, &decay_}) {
        slider->setSliderStyle(juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 90, 28);
    }
    pitch_.setRange(30, 90, .1);
    pitch_.setTextValueSuffix(" Hz");
    decay_.setRange(100, 800, 1);
    decay_.setTextValueSuffix(" ms");
    seed_.setInputRestrictions(10, "0123456789");
    seed_.setColour(juce::TextEditor::backgroundColourId, panel);
    seed_.setColour(juce::TextEditor::textColourId, ink);
    seed_.setTooltip(juce::String::fromUTF8("Graine 0 à 4294967295. Entrée recrée la variation de base ; 1 = référence."));

    play_.onClick = [this] { playKick(); };
    generate_.onClick = [this] {
        auto newSeed = static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt());
        if (newSeed == parameters_.seed) ++newSeed;
        parameters_ = kravatek::variationFromSeed(newSeed);
        syncControls(); regenerate();
        setStatus(juce::String::fromUTF8("Nouvelle variation prête. PLAY pour écouter."));
    };
    export_.onClick = [this] { chooseExport(); };
    audio_.onClick = [this] { chooseAudio(); };
    pitch_.onValueChange = [this] { parameters_.endHz = pitch_.getValue(); regenerate(); };
    decay_.onValueChange = [this] { parameters_.durationSeconds = decay_.getValue() / 1000; regenerate(); };
    seed_.onReturnKey = [this] { applySeed(); };
    syncControls();
    regenerate();
    const auto error = deviceManager_.initialise(0, 2, nullptr, true);
    deviceManager_.addAudioCallback(this);
    if (error.isNotEmpty()) setStatus(juce::String::fromUTF8("Audio indisponible : ") + error, true);
    else handleAsyncUpdate();
    setSize(840, 560);
}

MainComponent::~MainComponent() {
    deviceManager_.removeAudioCallback(this);
    deviceManager_.closeAudioDevice();
    cancelPendingUpdate();
    chooser_.reset();
    setLookAndFeel(nullptr);
}

void MainComponent::syncControls() {
    pitch_.setValue(parameters_.endHz, juce::dontSendNotification);
    decay_.setValue(parameters_.durationSeconds * 1000, juce::dontSendNotification);
    seed_.setText(juce::String(static_cast<juce::int64>(parameters_.seed)), false);
}

void MainComponent::regenerate() {
    kick_ = kravatek::renderKick(parameters_, 48000);
    waveform_.setKick(kick_);
    renderedDeviceRate_ = deviceRate_.load();
    if (renderedDeviceRate_ >= 8000 && renderedDeviceRate_ <= 192000)
        deviceSamples_ = renderedDeviceRate_ == 48000 ? kick_.samples
            : kravatek::renderKick(parameters_, renderedDeviceRate_).samples;
}

void MainComponent::playKick() {
    if (!audioActive_.load()) {
        setStatus(juce::String::fromUTF8("Aucune sortie audio. Choisissez AUDIO ; l'export reste disponible."), true);
        return;
    }
    if (deviceRate_.load() != renderedDeviceRate_) regenerate();
    if (playback_.queue(deviceSamples_)) setStatus(juce::String::fromUTF8("Lecture — graine ")
        + juce::String(static_cast<juce::int64>(parameters_.seed)));
    else setStatus(juce::String::fromUTF8("Lecture déjà en attente. Réessayez dans un instant."));
}

void MainComponent::applySeed() {
    const auto text = seed_.getText();
    if (text.isEmpty() || text.getLargeIntValue() > 4294967295LL) {
        setStatus(juce::String::fromUTF8("La graine doit être comprise entre 0 et 4294967295."), true);
        return;
    }
    parameters_ = kravatek::variationFromSeed(static_cast<std::uint32_t>(text.getLargeIntValue()));
    syncControls(); regenerate();
    setStatus(juce::String::fromUTF8("Graine restaurée. Les réglages manuels sont réinitialisés."));
}

void MainComponent::exportTo(const juce::File& file) {
    kravatek::writeWav24(nativePath(file), kick_);
    setStatus(juce::String::fromUTF8("WAV exporté : ") + file.getFullPathName());
}

void MainComponent::chooseExport() {
    if (chooser_) return;
    const auto name = "Kravatek_seed-" + juce::String(static_cast<juce::int64>(parameters_.seed)) + ".wav";
    chooser_ = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("Exporter le kick en WAV 24 bits"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(name), "*.wav");
    const juce::Component::SafePointer<MainComponent> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                          | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe](const juce::FileChooser& chooser) {
            if (safe == nullptr) return;
            const auto result = chooser.getResult();
            if (result != juce::File()) {
                // Keep the exact path confirmed by the native overwrite dialog.
                // An explicit non-WAV extension is rejected instead of redirecting a write.
                if (!result.hasFileExtension("wav")) {
                    safe->setStatus(juce::String::fromUTF8("Choisissez un nom se terminant par .wav."), true);
                } else {
                    try { safe->exportTo(result); }
                    catch (const std::exception& e) { safe->setStatus("Export impossible : " + juce::String(e.what()), true); }
                }
            }
            // Delete the chooser after its callback has returned.
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->chooser_.reset(); });
        });
}

void MainComponent::chooseAudio() {
    juce::PopupMenu menu;
    auto* type = deviceManager_.getCurrentDeviceTypeObject();
    if (type != nullptr) {
        type->scanForDevices();
        const auto names = type->getDeviceNames(false);
        const auto* current = deviceManager_.getCurrentAudioDevice();
        for (int i = 0; i < names.size(); ++i)
            menu.addItem(i + 1, names[i], true, current != nullptr && current->getName() == names[i]);
    }
    menu.addSeparator();
    menu.addItem(10000, juce::String::fromUTF8("Réessayer la sortie par défaut"));
    const juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&audio_), [safe](int id) {
        if (safe == nullptr || id == 0) return;
        juce::String error;
        if (id == 10000) {
            safe->deviceManager_.closeAudioDevice();
            error = safe->deviceManager_.initialise(0, 2, nullptr, true);
        } else if (auto* currentType = safe->deviceManager_.getCurrentDeviceTypeObject()) {
            const auto names = currentType->getDeviceNames(false);
            if (id > names.size()) return;
            auto setup = safe->deviceManager_.getAudioDeviceSetup();
            setup.outputDeviceName = names[id - 1];
            error = safe->deviceManager_.setAudioDeviceSetup(setup, true);
        }
        if (error.isNotEmpty()) safe->setStatus(error, true);
        else safe->handleAsyncUpdate();
    });
}

void MainComponent::setStatus(const juce::String& text, bool error) {
    status_.setColour(juce::Label::textColourId, error ? juce::Colour(0xffffb38a) : muted);
    status_.setText(text, juce::dontSendNotification);
    status_.setTooltip(text);
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    playback_.reset();
    const auto rate = device->getCurrentSampleRate();
    deviceRate_.store(rate);
    playback_.setSampleRate(rate);
    audioActive_.store(rate >= 8000 && rate <= 192000);
    triggerAsyncUpdate();
}

void MainComponent::audioDeviceStopped() {
    audioActive_.store(false);
    playback_.reset();
    triggerAsyncUpdate();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs,
        int channels, int frames, const juce::AudioIODeviceCallbackContext&) {
    playback_.process(outputs, channels, frames);
    callbackCount_.fetch_add(1, std::memory_order_relaxed);
}

void MainComponent::handleAsyncUpdate() {
    regenerate();
    if (audioActive_.load())
        setStatus(juce::String::fromUTF8("Prêt — sortie ") + juce::String(deviceRate_.load(), 0)
                  + " Hz. Export fixe : 48 kHz / 24 bits.");
    else setStatus(juce::String::fromUTF8("Aucune sortie audio disponible. Synthèse et export WAV actifs."), true);
}

void MainComponent::paint(juce::Graphics& g) { g.fillAll(background); }

void MainComponent::resized() {
    auto bounds = getLocalBounds().reduced(28);
    title_.setBounds(bounds.removeFromTop(38));
    subtitle_.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(18);
    auto buttons = bounds.removeFromTop(42);
    play_.setBounds(buttons.removeFromLeft(130)); buttons.removeFromLeft(12);
    generate_.setBounds(buttons.removeFromLeft(146)); buttons.removeFromLeft(12);
    export_.setBounds(buttons.removeFromLeft(150));
    audio_.setBounds(buttons.removeFromRight(94));
    bounds.removeFromTop(18);
    auto controls = bounds.removeFromTop(62);
    auto first = controls.removeFromLeft((controls.getWidth() - 24) / 2);
    controls.removeFromLeft(24);
    pitchLabel_.setBounds(first.removeFromTop(22)); pitch_.setBounds(first);
    decayLabel_.setBounds(controls.removeFromTop(22)); decay_.setBounds(controls);
    bounds.removeFromTop(16);
    auto footer = bounds.removeFromBottom(28);
    status_.setBounds(footer);
    format_.setBounds(bounds.removeFromBottom(22));
    bounds.removeFromBottom(12);
    auto seed = bounds.removeFromBottom(30);
    seedLabel_.setBounds(seed.removeFromLeft(58));
    seed_.setBounds(seed.removeFromLeft(180));
    bounds.removeFromBottom(18);
    waveform_.setBounds(bounds);
}

void MainComponent::runSmokeTest(const juce::File& directory) {
    if (directory.createDirectory().failed()) throw std::runtime_error("Cannot create smoke-test directory");
    const auto initial = kick_.samples;
    generate_.onClick();
    if (kick_.samples == initial) throw std::runtime_error("GENERATE has no effect");
    seed_.setText("424242", false); seed_.onReturnKey();
    const auto restored = kravatek::renderKick(kravatek::variationFromSeed(424242));
    if (kick_.samples != restored.samples) throw std::runtime_error("Seed control did not restore kick");
    pitch_.setValue(57, juce::sendNotificationSync);
    decay_.setValue(400, juce::sendNotificationSync);
    if (kick_.parameters.endHz != 57 || kick_.samples.size() != 19200)
        throw std::runtime_error("Slider did not change synthesis");
    for (int i = 0; i < 20; ++i) play_.onClick();
    exportTo(directory.getChildFile("gui-smoke.wav"));
    auto image = createComponentSnapshot(getLocalBounds());
    auto imageFile = directory.getChildFile("prototype-001.png").createOutputStream();
    if (!imageFile || !juce::PNGImageFormat().writeImageToStream(image, *imageFile))
        throw std::runtime_error("Could not capture waveform UI");
    juce::DynamicObject::Ptr report = new juce::DynamicObject;
    report->setProperty("result", "passed");
    report->setProperty("generate", true);
    report->setProperty("seedRestore", true);
    report->setProperty("pitchEndHz", kick_.parameters.endHz);
    report->setProperty("decayMs", kick_.parameters.durationSeconds * 1000);
    report->setProperty("wavFrames", static_cast<juce::int64>(kick_.samples.size()));
    report->setProperty("audioDeviceActive", audioActive_.load());
    report->setProperty("audioCallbacks", static_cast<juce::int64>(callbackCount_.load()));
    report->setProperty("physicalListeningVerified", false);
    if (!directory.getChildFile("gui-smoke.json").replaceWithText(juce::JSON::toString(juce::var(report.get()))))
        throw std::runtime_error("Could not save smoke report");
}
