#include "MainComponent.h"
#include "dsp/WavWriter.h"

#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace {
const auto ink = neon::ink, muted = neon::muted;
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
    parameters_ = kick.parameters;
    float peak = 0;
    for (auto sample : samples_) peak = std::max(peak, std::abs(sample));
    peakDb_ = peak > 0 ? 20 * std::log10(peak) : -120;
    repaint();
}

void WaveformComponent::paint(juce::Graphics& g) {
    neon::frame(g, getLocalBounds(), "PITCH / ENVELOPE + WAVEFORM");
    auto content = getLocalBounds().reduced(18).withTrimmedTop(30);
    auto info = content.removeFromRight(156);
    content.removeFromRight(16);
    auto legend = content.removeFromTop(22);
    g.setFont(neon::mono(11));
    g.setColour(neon::pink); g.fillRect(legend.getX(), legend.getY() + 6, 8, 8);
    g.drawText("WAVEFORM", legend.withTrimmedLeft(16).withWidth(100), juce::Justification::centredLeft);
    g.setColour(neon::cyan); g.fillRect(legend.getX() + 128, legend.getY() + 6, 8, 8);
    g.drawText("PITCH ENV", legend.withTrimmedLeft(144), juce::Justification::centredLeft);
    auto axis = content.removeFromBottom(24);
    auto area = content.reduced(1, 6);
    if (area.getWidth() <= 0 || samples_.empty()) return;
    g.setColour(neon::dark); g.fillRect(area);
    neon::grid(g, area, 24);
    const auto mid = static_cast<float>(area.getCentreY());
    g.setColour(neon::cyan.withAlpha(.30f));
    g.drawHorizontalLine(area.getCentreY(), static_cast<float>(area.getX()), static_cast<float>(area.getRight()));
    g.setFont(neon::mono(10));
    for (int i = 0; i <= 4; ++i) {
        auto tick = juce::Rectangle<int>(axis.getX() + i * (axis.getWidth() - 54) / 4, axis.getY(), 54, axis.getHeight());
        g.setColour(muted);
        g.drawText(juce::String(duration_ * 1000 * i / 4, 0) + " ms", tick, juce::Justification::centredLeft);
    }
    juce::Path wave, pitch;
    for (int x = 0; x < area.getWidth(); ++x) {
        const auto first = static_cast<std::size_t>(x) * samples_.size() / static_cast<std::size_t>(area.getWidth());
        const auto last = std::min(samples_.size(), std::max(first + 1,
            static_cast<std::size_t>(x + 1) * samples_.size() / static_cast<std::size_t>(area.getWidth())));
        const auto mm = std::minmax_element(samples_.begin() + static_cast<std::ptrdiff_t>(first),
                                           samples_.begin() + static_cast<std::ptrdiff_t>(last));
        const auto scale = static_cast<float>(area.getHeight()) * .47f;
        g.setColour(neon::pink.withAlpha(.65f));
        g.drawLine(static_cast<float>(area.getX() + x), mid - *mm.second * scale,
                   static_cast<float>(area.getX() + x), mid - *mm.first * scale, 1);
        const auto pointX = static_cast<float>(area.getX() + x);
        const auto waveY = mid - samples_[first] * scale;
        const auto t = duration_ * x / std::max(1, area.getWidth() - 1);
        const auto frequency = kravatek::pitchAt(parameters_, t);
        const auto relative = std::clamp((std::log(frequency) - std::log(20.0)) / std::log(220.0 / 20.0), 0.0, 1.0);
        const auto pitchY = static_cast<float>(area.getBottom()) - static_cast<float>(relative) * static_cast<float>(area.getHeight());
        if (x == 0) { wave.startNewSubPath(pointX, waveY); pitch.startNewSubPath(pointX, pitchY); }
        else { wave.lineTo(pointX, waveY); pitch.lineTo(pointX, pitchY); }
    }
    g.setColour(neon::pink.withAlpha(.20f)); g.strokePath(wave, juce::PathStrokeType(5));
    g.setColour(neon::pink); g.strokePath(wave, juce::PathStrokeType(1.3f));
    g.setColour(neon::cyan.withAlpha(.17f)); g.strokePath(pitch, juce::PathStrokeType(7));
    g.setColour(neon::cyan); g.strokePath(pitch, juce::PathStrokeType(2));
    g.setColour(neon::cyan.withAlpha(.25f));
    g.drawVerticalLine(info.getX() - 9, static_cast<float>(info.getY()), static_cast<float>(info.getBottom()));
    g.setFont(neon::mono(12, true)); g.setColour(neon::cyan);
    g.drawText("SYNTH DNA", info.removeFromTop(30), juce::Justification::centredLeft);
    const std::array<std::pair<juce::String, juce::String>, 5> fields {{
        {"LENGTH", juce::String(duration_ * 1000, 0) + " ms"},
        {"START", juce::String(parameters_.startHz, 1) + " Hz"},
        {"END", juce::String(parameters_.endHz, 1) + " Hz"},
        {"DROP", juce::String(parameters_.pitchDropSeconds * 1000, 1) + " ms"},
        {"PEAK", juce::String(peakDb_, 1) + " dBFS"}
    }};
    for (const auto& field : fields) {
        auto row = info.removeFromTop(std::min(32, info.getHeight() / 2));
        g.setFont(neon::mono(10)); g.setColour(muted);
        g.drawText(field.first, row.removeFromLeft(57), juce::Justification::centredLeft);
        g.setFont(neon::mono(11, true)); g.setColour(ink);
        g.drawText(field.second, row, juce::Justification::centredRight);
    }
}

MainComponent::MainComponent() {
    setLookAndFeel(&lookAndFeel_);

    for (auto* component : std::initializer_list<juce::Component*> {&brand_, &tagline_, &pitchLabel_, &decayLabel_,
            &seedLabel_, &status_, &format_, &tips_, &play_, &generate_, &export_, &audio_, &restore_, &pitch_, &decay_, &seed_, &waveform_})
        addAndMakeVisible(component);
    for (auto* label : {&tagline_, &pitchLabel_, &decayLabel_, &seedLabel_, &status_, &format_, &tips_}) {
        label->setColour(juce::Label::textColourId, ink);
        label->setFont(neon::mono(12));
    }
    tagline_.setText("KEEP THE DNA. MUTATE THE SOUND.", juce::dontSendNotification);
    tagline_.setFont(neon::mono(12, true));
    tagline_.setColour(juce::Label::textColourId, neon::pink);
    tagline_.setJustificationType(juce::Justification::centred);
    pitchLabel_.setText("PITCH END", juce::dontSendNotification);
    decayLabel_.setText("DECAY", juce::dontSendNotification);
    seedLabel_.setText("SEED / 0 - 4294967295", juce::dontSendNotification);
    format_.setText("EXPORT: MONO / 48 kHz / PCM 24 BIT", juce::dontSendNotification);
    format_.setFont(neon::mono(10.5f));
    format_.setColour(juce::Label::textColourId, muted);
    status_.setFont(neon::mono(10.5f));
    tips_.setFont(neon::mono(10.5f));
    tips_.setText(juce::String::fromUTF8("Graine + PITCH END + DECAY = votre son.\nEntrée ou RESTORE : variation de base."), juce::dontSendNotification);
    tips_.setColour(juce::Label::textColourId, muted);
    play_.setTooltip(juce::String::fromUTF8("Écouter le kick. Les clics répétés relancent avec une courte extinction."));
    generate_.setTooltip(juce::String::fromUTF8("Créer une variation synthétique avec une nouvelle graine."));
    export_.setTooltip(juce::String::fromUTF8("Exporter le kick affiché, avec sa graine et ses paramètres."));

    for (auto* slider : {&pitch_, &decay_}) {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 100, 24);
    }
    pitch_.setRange(30, 90, .1);
    pitch_.setTextValueSuffix(" Hz");
    decay_.setRange(100, 800, 1);
    decay_.setTextValueSuffix(" ms");
    seed_.setInputRestrictions(10, "0123456789");
    seed_.setColour(juce::TextEditor::backgroundColourId, neon::dark);
    seed_.setColour(juce::TextEditor::textColourId, ink);
    seed_.setColour(juce::TextEditor::outlineColourId, neon::cyan.withAlpha(.5f));
    seed_.setColour(juce::TextEditor::focusedOutlineColourId, neon::pink);
    seed_.setFont(neon::mono(16));
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
    restore_.onClick = [this] { applySeed(); };
    waveform_.setTooltip(juce::String::fromUTF8("Onde calculée en rose ; enveloppe de pitch en cyan. Réglez PITCH END et DECAY ci-dessous."));
    syncControls();
    regenerate();
    const auto error = deviceManager_.initialise(0, 2, nullptr, true);
    deviceManager_.addAudioCallback(this);
    if (error.isNotEmpty()) setStatus(juce::String::fromUTF8("Audio indisponible : ") + error, true);
    else handleAsyncUpdate();
    setSize(1120, 780);
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
    repaint();
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

void MainComponent::paint(juce::Graphics& g) {
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff1314c0), 0, 0, neon::blue,
        static_cast<float>(getWidth()), static_cast<float>(getHeight()), false));
    g.fillAll();
    neon::grid(g, getLocalBounds());
    g.setColour(neon::cyan.withAlpha(.6f)); neon::star(g, {10, 196}, 28);
    g.setColour(neon::pink.withAlpha(.6f)); neon::star(g, {static_cast<float>(getWidth() - 9), 444}, 24);
    if (play_.getX() - brand_.getRight() > 70) {
        g.setColour(neon::cyan);
        neon::star(g, {static_cast<float>(brand_.getRight() + (play_.getX() - brand_.getRight()) / 2), 78}, 23);
    }
    neon::frame(g, pitchBounds_, "01 / OSCILLATOR");
    neon::frame(g, decayBounds_, "02 / AMP ENVELOPE");
    neon::frame(g, seedBounds_, "03 / SEED DNA");
    const auto details = [&](juce::Rectangle<int> bounds, const juce::String& line1,
                             const juce::String& line2, const juce::String& line3) {
        auto text = bounds.reduced(12).withTrimmedLeft(132).withTrimmedTop(77);
        g.setFont(neon::mono(11)); g.setColour(muted);
        for (const auto& line : {line1, line2, line3})
            g.drawText(line, text.removeFromTop(24), juce::Justification::centredLeft);
    };
    details(pitchBounds_, "SINE / PHASE CONT.", "START " + juce::String(parameters_.startHz, 1) + " Hz",
        "DROP " + juce::String(parameters_.pitchDropSeconds * 1000, 1) + " ms");
    details(decayBounds_, "EXPONENTIAL AMP", "ATTACK " + juce::String(parameters_.attackSeconds * 1000, 1) + " ms",
        "RELEASE " + juce::String(parameters_.releaseSeconds * 1000, 1) + " ms");
}

void MainComponent::resized() {
    auto bounds = getLocalBounds().reduced(22);
    auto header = bounds.removeFromTop(116);
    brand_.setBounds(header.removeFromLeft(std::min(430, header.getWidth() - 458)));
    auto buttons = header.removeFromRight(434).withTrimmedTop(14).withHeight(88);
    for (auto* button : {&play_, &generate_, &export_, &audio_}) {
        button->setBounds(buttons.removeFromLeft(98));
        buttons.removeFromLeft(14);
    }
    bounds.removeFromTop(18);
    status_.setBounds(bounds.removeFromBottom(24));
    tagline_.setBounds(bounds.removeFromBottom(22));
    bounds.removeFromBottom(14);
    auto modules = bounds.removeFromBottom(188);
    const auto width = (modules.getWidth() - 28) / 3;
    pitchBounds_ = modules.removeFromLeft(width); modules.removeFromLeft(14);
    decayBounds_ = modules.removeFromLeft(width); modules.removeFromLeft(14);
    seedBounds_ = modules;
    pitch_.setBounds(pitchBounds_.getX() + 10, pitchBounds_.getY() + 40, 132, 135);
    decay_.setBounds(decayBounds_.getX() + 10, decayBounds_.getY() + 40, 132, 135);
    pitchLabel_.setBounds(pitchBounds_.getX() + 146, pitchBounds_.getY() + 48, width - 158, 26);
    decayLabel_.setBounds(decayBounds_.getX() + 146, decayBounds_.getY() + 48, width - 158, 26);
    auto seed = seedBounds_.reduced(16).withTrimmedTop(29);
    seedLabel_.setBounds(seed.removeFromTop(24));
    seed.removeFromTop(4);
    auto input = seed.removeFromTop(33);
    restore_.setBounds(input.removeFromRight(98)); input.removeFromRight(8);
    seed_.setBounds(input);
    seed.removeFromTop(8);
    tips_.setBounds(seed.removeFromTop(39));
    format_.setBounds(seed.removeFromTop(22));
    bounds.removeFromBottom(18);
    waveform_.setBounds(bounds);
}

void MainComponent::runSmokeTest(const juce::File& directory) {
    if (directory.createDirectory().failed()) throw std::runtime_error("Cannot create smoke-test directory");
    const auto initial = kick_.samples;
    generate_.onClick();
    if (kick_.samples == initial) throw std::runtime_error("GENERATE has no effect");
    seed_.setText("1", false); restore_.onClick();
    if (kick_.samples != kravatek::renderKick({}).samples)
        throw std::runtime_error("RESTORE button did not restore the reference");
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
    if (!imageFile || !imageFile->setPosition(0) || imageFile->truncate().failed()
        || !juce::PNGImageFormat().writeImageToStream(image, *imageFile))
        throw std::runtime_error("Could not capture waveform UI");
    juce::DynamicObject::Ptr report = new juce::DynamicObject;
    report->setProperty("result", "passed");
    report->setProperty("theme", "neon-retro");
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
