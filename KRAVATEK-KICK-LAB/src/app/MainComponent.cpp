#include "MainComponent.h"
#include "dsp/WavWriter.h"
#include <cmath>
#include <filesystem>
#include <stdexcept>

namespace {
std::filesystem::path nativePath(const juce::File& file) {
#if JUCE_WINDOWS
    return std::filesystem::path(file.getFullPathName().toWideCharPointer());
#else
    return std::filesystem::path(file.getFullPathName().toStdString());
#endif
}
void require(bool condition, const char* text) {if (!condition) throw std::runtime_error(text);}
}

MainComponent::MainComponent() {
    setLookAndFeel(&lookAndFeel_); setWantsKeyboardFocus(true);
    for (auto* component : std::initializer_list<juce::Component*> {&brand_, &editor_, &factory_, &play_, &generate_,
            &evolve_, &export_, &audio_, &novelty_, &noveltyLabel_, &historyBox_, &subLock_, &open_, &save_, &restore_, &storeA_, &compare_, &loop_, &mutation_, &bpm_, &seed_,
            &presetLabel_, &mutationLabel_, &bpmLabel_, &seedLabel_, &status_, &compareLabel_, &format_, &help_, &previewHelp_})
        addAndMakeVisible(component);
    for (auto& control : controls_) addAndMakeVisible(control);
    for (auto* label : {&presetLabel_, &noveltyLabel_, &mutationLabel_, &bpmLabel_, &seedLabel_, &status_, &compareLabel_, &format_, &help_, &previewHelp_}) {
        label->setColour(juce::Label::textColourId, krui::ink);
        label->setFont(krui::mono(11));
    }
    presetLabel_.setText("SOUND FAMILY / EDITABLE PRESET", juce::dontSendNotification);
    noveltyLabel_.setText("NOVELTY", juce::dontSendNotification);
    mutationLabel_.setText("EVOLVE", juce::dontSendNotification);
    bpmLabel_.setText("TEMPO", juce::dontSendNotification);
    seedLabel_.setText("VARIATION SEED", juce::dontSendNotification);
    compareLabel_.setText("SESSION HISTORY", juce::dontSendNotification);
    help_.setText("PROMPT-TO-KICK : INDISPONIBLE", juce::dontSendNotification);
    help_.setColour(juce::Label::textColourId, krui::muted);
    previewHelp_.setText("SPACE / PLAY - STOP    |    SAVE / EXACT DSP STATE", juce::dontSendNotification);
    previewHelp_.setColour(juce::Label::textColourId, krui::pink);
    previewHelp_.setJustificationType(juce::Justification::centredRight);
    format_.setColour(juce::Label::textColourId, krui::muted);
    format_.setFont(krui::mono(11));
    status_.setFont(krui::mono(10.5f));
    for (int i = 0; i < static_cast<int>(kravatek::KickStyle::count); ++i)
        factory_.addItem(kravatek::styleName(static_cast<kravatek::KickStyle>(i)), i + 1);
    factory_.setColour(juce::ComboBox::backgroundColourId, krui::dark);
    factory_.setColour(juce::ComboBox::textColourId, krui::ink);
    factory_.setColour(juce::ComboBox::outlineColourId, krui::cyan.withAlpha(.5f));
    factory_.onChange = [this] {
        if (factory_.getSelectedId() < 1) return;
        remember(); comparisonGain_ = 1;
        state_.style = factory_.getSelectedId() - 1;
        state_.parameters = kravatek::factoryPreset(static_cast<kravatek::KickStyle>(state_.style));
        showingA_ = false; syncControls(); requestRender(); remember();
        setStatus(juce::String::fromUTF8("Base chargée. Écoute puis sculpte les enveloppes."));
    };
    const auto configure = [&](Control index, const char* caption, double low, double high, double step,
                               const char* suffix, const char* tooltip) {
        auto& control = controls_[static_cast<std::size_t>(index)];
        control.configure(caption, low, high, step, suffix, juce::String::fromUTF8(tooltip));
        control.slider.onValueChange = [this, index] {controlChanged(index);};
    };
    configure(start, "START", 120, 1600, 1, " Hz", "Fréquence initiale : monte-la pour une attaque plus tranchante.");
    configure(end, "END / KEY", 30, 100, .1, " Hz", "Fréquence finale du corps du kick.");
    configure(drop, "PITCH DROP", 5, 180, .1, " ms", "Temps de descente du pitch. Le point central du graphe le modifie aussi.");
    configure(bend, "PITCH BEND", .4, 2.4, .01, "", "Courbure du pitch : change la répartition du claquement et du corps.");
    configure(length, "LENGTH", 80, 1000, 1, " ms", "Durée totale du kick.");
    configure(attack, "ATTACK", .1, 6, .01, " ms", "Montée d'amplitude lissée ; une attaque courte est plus incisive.");
    configure(ampShape, "BODY SHAPE", .45, 2.3, .01, "", "Répartition de l'énergie : plus haut maintient davantage le corps.");
    configure(release, "RELEASE", 2, 35, .1, " ms", "Extinction finale. Adaptée à la longueur du kick.");
    configure(drive, "DRIVE", 0, 30, .1, " dB", "Saturation suréchantillonnée : harmoniques et densité.");
    configure(fm, "FM / PM", 0, 4, .01, "", "Modulation combinée de fréquence et de phase en attaque : du punch à la texture métallique.");
    configure(harmonics, "HARMONICS", 0, 100, .1, " %", "Partiels impairs synthétiques avant saturation.");
    configure(fmTime, "FM DECAY", 5, 500, .1, " ms", "Temps de décroissance de la modulation de phase du corps.");
    configure(tailLevel, "TAIL LEVEL", 0, 100, .1, " %", "Niveau de la couche de tail indépendante.");
    configure(tailTime, "TAIL DECAY", 20, 1000, 1, " ms", "Extinction exponentielle de la tail, indépendante du body.");
    configure(tailRatio, "TAIL RATIO", .5, 4, .01, " x", "Rapport de fréquence de la tail par rapport à la fondamentale cible.");
    configure(tailMotion, "MOTION", 0, 100, .1, " %", "Rebonds de pitch, interaction de phase et distorsion évolutive de la tail.");
    configure(cutoff, "CUTOFF", 80, 16000, 1, " Hz", "Filtre passe-bas résonant sur body et tail ; le sub reste indépendant.");
    controls_[cutoff].slider.setSkewFactorFromMidPoint(1600);
    configure(resonance, "RESONANCE", 0, 95, .1, " %", "Résonance du filtre à topologie préservée, amortissement borné.");
    configure(sweep, "SWEEP", -4, 4, .01, " oct", "Balayage de fréquence du filtre ; revient au cutoff en 80 ms.");
    configure(shape, "FOLD MORPH", 0, 100, .1, " %", "Morphing entre saturation tanh et repliement sinusoïdal suréchantillonné.");
    configure(subTime, "SUB DECAY", 20, 1000, 1, " ms", "Extinction du sub sinusoïdal à fréquence fixe, indépendante du body.");
    configure(sub, "CLEAN SUB", 0, 100, .1, " %", "Mélange de fondamentale propre pour conserver un grave lisible.");
    configure(click, "CLICK", 0, 100, .1, " %", "Attaque bruitée créée numériquement, reproductible avec sa graine.");
    configure(clickLength, "CLICK LENGTH", .5, 20, .1, " ms", "Durée de la texture bruitée de l'attaque.");
    const std::array<const char*, 6> lockNames {"LOCK PITCH", "LOCK AMP", "LOCK BODY", "LOCK TAIL", "LOCK FX", "LOCK ATTACK"};
    for (std::size_t i = 0; i < locks_.size(); ++i) {
        auto& lock = locks_[i]; addAndMakeVisible(lock); lock.setButtonText(lockNames[i]);
        lock.setColour(juce::ToggleButton::textColourId, krui::ink);
        lock.setColour(juce::ToggleButton::tickColourId, krui::cyan);
        lock.setTooltip(juce::String::fromUTF8("Conserver ce groupe de paramètres pendant EVOLVE."));
        lock.onClick = [this] {
            state_.locks = {locks_[0].getToggleState(), locks_[1].getToggleState(),
                locks_[2].getToggleState(), locks_[5].getToggleState(), locks_[3].getToggleState(),
                locks_[4].getToggleState(), subLock_.getToggleState()};
        };
    }
    subLock_.setColour(juce::ToggleButton::textColourId, krui::ink);
    subLock_.setColour(juce::ToggleButton::tickColourId, krui::cyan);
    subLock_.setTooltip(juce::String::fromUTF8("EVOLVE conserve fréquence, niveau, decay et durée du sub. Le mix global peut encore changer."));
    subLock_.onClick = [this] {state_.locks.sub = subLock_.getToggleState();};
    novelty_.setSliderStyle(juce::Slider::LinearHorizontal);
    novelty_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 24);
    novelty_.setRange(0, 100, 1); novelty_.setTextValueSuffix(" %");
    novelty_.setTooltip(juce::String::fromUTF8("Ouvre les plages de modulation et de repliement. Ce n'est pas un score d'originalité."));
    novelty_.onValueChange = [this] {state_.novelty = novelty_.getValue() / 100;};
    historyBox_.setColour(juce::ComboBox::backgroundColourId, krui::dark);
    historyBox_.setColour(juce::ComboBox::textColourId, krui::ink);
    historyBox_.onChange = [this] {
        const auto index = historyBox_.getSelectedId() - 1;
        if (index >= 0 && index < static_cast<int>(history_.size())) {
            state_ = history_[static_cast<std::size_t>(index)]; comparisonGain_ = 1; showingA_ = false;
            syncControls(); requestRender(); setStatus("HISTORY / restored exact saved state");
        }
    };
    mutation_.setSliderStyle(juce::Slider::LinearHorizontal);
    mutation_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 25);
    mutation_.setRange(0, 100, 1); mutation_.setTextValueSuffix(" %");
    mutation_.setTooltip(juce::String::fromUTF8("Amplitude de la mutation. À zéro, le son ne change pas."));
    mutation_.onValueChange = [this] {state_.mutation = mutation_.getValue() / 100;};
    bpm_.setSliderStyle(juce::Slider::LinearHorizontal);
    bpm_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 74, 25);
    bpm_.setRange(80, 220, 1); bpm_.setTextValueSuffix(" BPM");
    bpm_.onValueChange = [this] {state_.bpm = bpm_.getValue(); if (looping_) queuePreview();};
    loop_.setClickingTogglesState(true); loop_.setToggleState(true, juce::dontSendNotification);
    loop_.setTooltip(juce::String::fromUTF8("Écoute répétée au tempo ; désactive pour un seul coup."));
    loop_.onClick = [this] {
        if (looping_) {looping_ = loop_.getToggleState(); queuePreview(); play_.setButtonText(looping_ ? "STOP" : "PLAY");}
    };
    seed_.setInputRestrictions(10, "0123456789"); seed_.setFont(krui::mono(15));
    seed_.setColour(juce::TextEditor::backgroundColourId, krui::dark);
    seed_.setColour(juce::TextEditor::textColourId, krui::ink);
    seed_.setColour(juce::TextEditor::outlineColourId, krui::cyan.withAlpha(.5f));
    seed_.setColour(juce::TextEditor::focusedOutlineColourId, krui::pink);
    seed_.onReturnKey = [this] {reseed();};
    play_.onClick = [this] {playKick();}; generate_.onClick = [this] {generateKick();};
    evolve_.onClick = [this] {mutateKick();};
    export_.onClick = [this] {chooseFile(0);}; open_.onClick = [this] {chooseFile(1);};
    save_.onClick = [this] {chooseFile(2);}; restore_.onClick = [this] {reseed();};
    audio_.onClick = [this] {chooseAudio();};
    storeA_.onClick = [this] {storeComparison();}; compare_.onClick = [this] {toggleComparison();};
    compare_.setEnabled(false);
    editor_.onEdit = [this](const kravatek::KickParameters& p) {
        state_.parameters = p; comparisonGain_ = 1; showingA_ = false; syncControls(); requestRender();
    };
    syncControls(); renderNow(); remember();
    const auto error = devices_.initialise(0, 2, nullptr, true); devices_.addAudioCallback(this);
    if (error.isNotEmpty()) setStatus("Audio : " + error, true); else handleAsyncUpdate();
    genetic_=std::make_unique<GeneticLab>();addAndMakeVisible(*genetic_);addAndMakeVisible(modeSwitch_);
    genetic_->deviceRate=[this]{const auto r=deviceRate_.load();return r>=8000&&r<=192000?r:48000;};
    genetic_->queueAudio=[this](std::span<const float> samples,bool loop,double bpm){
        if(!audioActive_.load())return false;
        const auto period=loop?static_cast<std::size_t>(std::llround(deviceRate_.load()*60/bpm)):0;
        return playback_.queue(samples,period);
    };
    genetic_->stopAudio=[this]{playback_.stop();};genetic_->chooseAudio=[this]{chooseAudio();};
    genetic_->openLegacy=[this](const juce::File& f){showClassic(true);loadFrom(f);};
    modeSwitch_.setWantsKeyboardFocus(false);modeSwitch_.onClick=[this]{showClassic(!classicShowing_);};
    setSize(1440,1200);showClassic(false);
}

MainComponent::~MainComponent() {
    genetic_.reset(); stopTimer(); devices_.removeAudioCallback(this); devices_.closeAudioDevice(); cancelPendingUpdate();
    chooser_.reset(); setLookAndFeel(nullptr);
}
void MainComponent::syncControls() {
    const auto& p = state_.parameters;
    const std::array<double, controlCount> values {p.startHz, p.endHz, p.pitchDropSeconds * 1000, p.pitchCurve,
        p.durationSeconds * 1000, p.attackSeconds * 1000, p.ampCurve, p.releaseSeconds * 1000,
        p.driveDb, p.fmAmount, p.fmSeconds * 1000, p.harmonics * 100,
        p.tailLevel * 100, p.tailSeconds * 1000, p.tailRatio, p.tailMotion * 100,
        p.filterHz, p.resonance * 100, p.filterSweep, p.shape * 100,
        p.clickLevel * 100, p.clickSeconds * 1000, p.subMix * 100, p.subSeconds * 1000};
    for (std::size_t i = 0; i < values.size(); ++i) controls_[i].slider.setValue(values[i], juce::dontSendNotification);
    factory_.setSelectedId(state_.style + 1, juce::dontSendNotification);
    const std::array<bool, 6> locks {state_.locks.pitch, state_.locks.amplitude, state_.locks.tone,
        state_.locks.tail, state_.locks.filter, state_.locks.transient};
    subLock_.setToggleState(state_.locks.sub, juce::dontSendNotification);
    for (std::size_t i = 0; i < locks.size(); ++i) locks_[i].setToggleState(locks[i], juce::dontSendNotification);
    seed_.setText(juce::String(static_cast<juce::int64>(p.seed)), false);
    mutation_.setValue(state_.mutation * 100, juce::dontSendNotification);
    novelty_.setValue(state_.novelty * 100, juce::dontSendNotification);
    bpm_.setValue(state_.bpm, juce::dontSendNotification);
    compare_.setButtonText(showingA_ ? "A ACTIVE" : "A / B");
    editor_.setParameters(p);
}
void MainComponent::controlChanged(Control control) {
    auto& p = state_.parameters; const auto v = controls_[static_cast<std::size_t>(control)].slider.getValue();
    switch (control) {
        case start: p.startHz = v; break; case end: p.endHz = v; break;
        case drop: p.pitchDropSeconds = v / 1000; break; case bend: p.pitchCurve = v; break;
        case length: p.durationSeconds = v / 1000; break; case attack: p.attackSeconds = v / 1000; break;
        case ampShape: p.ampCurve = v; break; case release: p.releaseSeconds = v / 1000; break;
        case drive: p.driveDb = v; break; case fm: p.fmAmount = v; break;
        case fmTime: p.fmSeconds = v / 1000; break;
        case tailLevel: p.tailLevel = v / 100; break; case tailTime: p.tailSeconds = v / 1000; break;
        case tailRatio: p.tailRatio = v; break; case tailMotion: p.tailMotion = v / 100; break;
        case cutoff: p.filterHz = v; break; case resonance: p.resonance = v / 100; break;
        case sweep: p.filterSweep = v; break; case shape: p.shape = v / 100; break;
        case subTime: p.subSeconds = v / 1000; break;
        case harmonics: p.harmonics = v / 100; break; case sub: p.subMix = v / 100; break;
        case click: p.clickLevel = v / 100; break; case clickLength: p.clickSeconds = v / 1000; break;
        default: break;
    }
    p.releaseSeconds = std::min(p.releaseSeconds, p.durationSeconds / 4);
    comparisonGain_ = 1; showingA_ = false; syncControls(); requestRender();
}
void MainComponent::requestRender() {
    pendingRender_ = true; editor_.setParameters(state_.parameters); startTimer(30);
}
void MainComponent::timerCallback() {renderNow();}
void MainComponent::renderNow() {
    stopTimer(); pendingRender_ = false;
    try {
        auto rendered = kravatek::renderKick(state_.parameters, 48000);
        const auto rate = deviceRate_.load();
        auto device = rate == 48000 || rate < 8000 || rate > 192000 ? rendered.samples
            : kravatek::renderKick(state_.parameters, rate).samples;
        kick_ = std::move(rendered); deviceSamples_ = std::move(device); renderedDeviceRate_ = rate;
        analysis_ = kravatek::analyse(kick_.samples, kick_.sampleRate);
        for (auto& sample : deviceSamples_) sample *= static_cast<float>(comparisonGain_);
        format_.setText(juce::String(analysis_.durationSeconds * 1000, 0) + " ms   F0 TARGET "
            + juce::String(kick_.parameters.endHz, 1) + " Hz\nPEAK " + juce::String(analysis_.peakDb, 1)
            + " / RMS " + juce::String(analysis_.rmsDb, 1) + " dBFS\nCREST "
            + juce::String(analysis_.peakDb - analysis_.rmsDb, 1) + " dB  /  48k / 24bit", juce::dontSendNotification);
        editor_.setKick(kick_); repaint();
        if (looping_) queuePreview();
    } catch (const std::exception& e) {setStatus("Rendu impossible : " + juce::String(e.what()), true);}
}
void MainComponent::queuePreview() {
    if (!audioActive_.load()) return;
    const auto period = loop_.getToggleState()
        ? static_cast<std::size_t>(std::llround(deviceRate_.load() * 60 / state_.bpm)) : 0;
    if (!playback_.queue(deviceSamples_, period)) setStatus(juce::String::fromUTF8("Lecture occupée ; réessaie dans un instant."));
}
void MainComponent::playKick() {
    if (looping_) {playback_.stop(); looping_ = false; play_.setButtonText("PLAY"); setStatus("STOP"); return;}
    if (!audioActive_.load()) {setStatus(juce::String::fromUTF8("Aucune sortie audio. Choisis AUDIO ; l'export reste disponible."), true); return;}
    if (pendingRender_ || renderedDeviceRate_ != deviceRate_.load()) renderNow();
    looping_ = loop_.getToggleState(); queuePreview(); play_.setButtonText(looping_ ? "STOP" : "PLAY");
    setStatus(looping_ ? "PREVIEW / " + juce::String(state_.bpm, 0) + " BPM / SPACE = STOP" : "ONE SHOT");
}
bool MainComponent::keyPressed(const juce::KeyPress& key) {
    if(!classicShowing_&&genetic_)return genetic_->keyPressed(key);
    if (key.getKeyCode() == juce::KeyPress::spaceKey && !seed_.hasKeyboardFocus(true)) {playKick(); return true;}
    return false;
}
void MainComponent::remember() {
    if (!history_.empty() && encodePreset(history_.back()) == encodePreset(state_)) return;
    if (history_.size() == 24) history_.erase(history_.begin());
    history_.push_back(state_); historyBox_.clear(juce::dontSendNotification);
    for (std::size_t i = 0; i < history_.size(); ++i)
        historyBox_.addItem("#" + juce::String(static_cast<int>(i + 1)) + " / "
            + juce::String(static_cast<juce::int64>(history_[i].parameters.seed)), static_cast<int>(i + 1));
    historyBox_.setSelectedId(static_cast<int>(history_.size()), juce::dontSendNotification);
}
void MainComponent::generateKick() {
    remember(); const auto seed = static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt());
    state_.parameters = kravatek::generate(static_cast<kravatek::KickStyle>(state_.style), seed, state_.novelty);
    comparisonGain_ = 1; showingA_ = false; syncControls(); requestRender(); remember();
    setStatus(juce::String::fromUTF8("GENERATE / nouvelle base. REGENERATE reproduit famille + seed + NOVELTY. DNA agit sur EVOLVE."));
}
void MainComponent::mutateKick() {
    remember(); auto seed = static_cast<std::uint32_t>(juce::Random::getSystemRandom().nextInt());
    if (seed == state_.parameters.seed) ++seed;
    state_.parameters = kravatek::evolve(state_.parameters, seed, state_.mutation, state_.novelty, state_.locks);
    comparisonGain_ = 1;
    showingA_ = false; syncControls(); requestRender();
    remember(); setStatus(juce::String::fromUTF8("EVOLVE / les paramètres DNA verrouillés sont conservés. SAVE garde le résultat complet."));
}
void MainComponent::reseed() {
    const auto text = seed_.getText();
    if (text.isEmpty() || text.getLargeIntValue() > 4294967295LL) {setStatus("Seed : 0 - 4294967295", true); return;}
    remember(); state_.parameters = kravatek::generate(static_cast<kravatek::KickStyle>(state_.style),
        static_cast<std::uint32_t>(text.getLargeIntValue()), state_.novelty);
    comparisonGain_ = 1;
    showingA_ = false; syncControls(); requestRender();
    remember(); setStatus(juce::String::fromUTF8("Base recréée depuis la graine. SAVE conserve un son modifié complet."));
}
void MainComponent::storeComparison() {
    comparisonGain_ = 1; requestRender(); comparisonA_ = state_; comparisonB_.reset(); showingA_ = false; compare_.setEnabled(true);
    setStatus(juce::String::fromUTF8("Version A mémorisée. Modifie ou mute le son, puis compare A/B."));
}
void MainComponent::toggleComparison() {
    if (!comparisonA_) return;
    if (!showingA_) {comparisonB_ = state_; state_ = *comparisonA_; showingA_ = true;}
    else {if (comparisonB_) state_ = *comparisonB_; showingA_ = false;}
    const auto rate = audioActive_.load() ? deviceRate_.load() : 48000;
    const auto a = kravatek::renderKick(comparisonA_->parameters, rate);
    const auto b = kravatek::renderKick(comparisonB_->parameters, rate);
    const auto aRms = kravatek::comparisonRms(a.samples, rate), bRms = kravatek::comparisonRms(b.samples, rate);
    const auto currentRms = showingA_ ? aRms : bRms;
    comparisonGain_ = currentRms > 0 ? std::min(aRms, bRms) / currentRms : 1;
    syncControls(); requestRender(); setStatus(juce::String(showingA_ ? "COMPARISON A" : "COMPARISON B")
        + " / RMS 400 ms matched / " + juce::String(20 * std::log10(std::max(1e-9, comparisonGain_)), 1)
        + " dB preview gain / export unchanged");
}
void MainComponent::exportTo(const juce::File& file) {
    if (pendingRender_) renderNow();
    kravatek::writeWav24(nativePath(file), kick_);
    setStatus(juce::String::fromUTF8("WAV exporté : ") + file.getFullPathName());
}
void MainComponent::saveTo(const juce::File& file) {
    if (!file.replaceWithText(encodePreset(state_))) throw std::runtime_error("Cannot write preset");
    setStatus(juce::String::fromUTF8("Son sauvegardé : ") + file.getFullPathName());
}
void MainComponent::loadFrom(const juce::File& file) {
    if (file.getSize() > 65536) throw std::invalid_argument("Preset is too large");
    auto loaded = decodePreset(file.loadFileAsString()); kravatek::validate(loaded.parameters, 48000);
    remember(); state_ = loaded; comparisonGain_ = 1; showingA_ = false; syncControls(); requestRender();
    remember(); setStatus(juce::String::fromUTF8("Son chargé : ") + file.getFileName());
}
void MainComponent::chooseFile(int operation) {
    if (chooser_) return;
    const bool opening = operation == 1;
    const auto extension = operation == 0 ? "wav" : "json";
    const auto name = "Kravatek_seed-" + juce::String(static_cast<juce::int64>(state_.parameters.seed)) + "." + extension;
    chooser_ = std::make_unique<juce::FileChooser>(opening ? "OPEN SOUND" : operation == 0 ? "EXPORT WAV 24 BIT" : "SAVE SOUND",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(name), "*." + juce::String(extension));
    const juce::Component::SafePointer<MainComponent> safe(this);
    chooser_->launchAsync((opening ? juce::FileBrowserComponent::openMode : juce::FileBrowserComponent::saveMode)
        | juce::FileBrowserComponent::canSelectFiles | (opening ? 0 : juce::FileBrowserComponent::warnAboutOverwriting),
        [safe, operation, extension = juce::String(extension)](const juce::FileChooser& chooser) {
            if (safe == nullptr) return;
            const auto file = chooser.getResult();
            if (file != juce::File()) {
                try {
                    if (!file.hasFileExtension(extension)) throw std::invalid_argument("Choose a file ending in ." + extension.toStdString());
                    if (operation == 0) safe->exportTo(file); else if (operation == 1) safe->loadFrom(file); else safe->saveTo(file);
                } catch (const std::exception& e) {safe->setStatus(juce::String::fromUTF8("Fichier : ") + juce::String(e.what()), true);}
            }
            juce::MessageManager::callAsync([safe] {if (safe != nullptr) safe->chooser_.reset();});
        });
}
void MainComponent::chooseAudio() {
    juce::PopupMenu menu;
    if (auto* type = devices_.getCurrentDeviceTypeObject()) {
        type->scanForDevices(); const auto names = type->getDeviceNames(false);
        const auto* current = devices_.getCurrentAudioDevice();
        for (int i = 0; i < names.size(); ++i) menu.addItem(i + 1, names[i], true, current != nullptr && current->getName() == names[i]);
    }
    menu.addSeparator(); menu.addItem(10000, juce::String::fromUTF8("Réessayer la sortie par défaut"));
    const juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&audio_), [safe](int id) {
        if (safe == nullptr || id == 0) return;
        juce::String error;
        if (id == 10000) {safe->devices_.closeAudioDevice(); error = safe->devices_.initialise(0, 2, nullptr, true);}
        else if (auto* type = safe->devices_.getCurrentDeviceTypeObject()) {
            const auto names = type->getDeviceNames(false); if (id > names.size()) return;
            auto setup = safe->devices_.getAudioDeviceSetup(); setup.outputDeviceName = names[id - 1];
            error = safe->devices_.setAudioDeviceSetup(setup, true);
        }
        if (error.isNotEmpty()) safe->setStatus(error, true); else safe->handleAsyncUpdate();
    });
}
void MainComponent::setStatus(const juce::String& text, bool error) {
    status_.setColour(juce::Label::textColourId, error ? juce::Colour(0xffffbd93) : krui::muted);
    status_.setText(text, juce::dontSendNotification); status_.setTooltip(text);
}
void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device) {
    playback_.reset(); const auto rate = device->getCurrentSampleRate(); deviceRate_.store(rate); playback_.setSampleRate(rate);
    audioActive_.store(rate >= 8000 && rate <= 192000); triggerAsyncUpdate();
}
void MainComponent::audioDeviceStopped() {audioActive_.store(false); playback_.reset(); triggerAsyncUpdate();}
void MainComponent::audioDeviceIOCallbackWithContext(const float* const*, int, float* const* outputs, int channels, int frames,
        const juce::AudioIODeviceCallbackContext&) {playback_.process(outputs, channels, frames); callbacks_.fetch_add(1, std::memory_order_relaxed);}
void MainComponent::handleAsyncUpdate() {
    renderNow();
    if (audioActive_.load()) setStatus(juce::String::fromUTF8("Prêt / sortie ") + juce::String(deviceRate_.load(), 0) + " Hz / SPACE = PLAY");
    else {looping_ = false; play_.setButtonText("PLAY"); setStatus(juce::String::fromUTF8("Audio indisponible. Synthèse, sauvegarde et export actifs."), true);}
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(krui::blue); if(!classicShowing_)return; krui::grid(g,getLocalBounds());
    krui::frame(g, presetBounds_, "01 / PRESET MEMORY"); krui::frame(g, sideBounds_, "MONITOR / LIBRARY");
    const std::array<const char*, 6> titles {"02 / OSC - PITCH", "03 / AMP", "04 / BODY", "05 / TAIL", "06 / FX", "07 / ATTACK - SUB"};
    for (std::size_t i = 0; i < moduleBounds_.size(); ++i) krui::frame(g, moduleBounds_[i], titles[i]);
    krui::frame(g, previewBounds_, "08 / EXPLORATION - PLAYBACK");
    if (spectrumBounds_.getWidth() > 0 && spectrumBounds_.getHeight() > 0) {
        g.setColour(krui::dark); g.fillRect(spectrumBounds_); krui::grid(g, spectrumBounds_, 22);
        const auto plot = spectrumBounds_.toFloat().reduced(6, 16);
        juce::Path spectrum;
        for (std::size_t i = 0; i < analysis_.spectrumDb.size(); ++i) {
            const auto x = plot.getX() + static_cast<float>(i) / 63 * plot.getWidth();
            const auto y = plot.getBottom() - static_cast<float>(std::clamp((analysis_.spectrumDb[i] + 90) / 90, 0.0, 1.0)) * plot.getHeight();
            if (i == 0) spectrum.startNewSubPath(x, y); else spectrum.lineTo(x, y);
        }
        g.setColour(krui::pink); g.strokePath(spectrum, juce::PathStrokeType(1.4f));
        g.setColour(krui::muted); g.setFont(krui::mono(9));
        g.drawText("SPECTRUM / HANN 8192 / dBFS", spectrumBounds_.withHeight(15).reduced(5, 0), juce::Justification::centredLeft);
        g.drawText("20 Hz                  18 kHz", spectrumBounds_.withY(spectrumBounds_.getBottom() - 15).withHeight(15).reduced(5, 0), juce::Justification::centredLeft);
    }
}
void MainComponent::resized() {
    if(genetic_){genetic_->setBounds(getLocalBounds());modeSwitch_.setBounds(getWidth()-124,22,106,29);modeSwitch_.toFront(false);}
    if(!classicShowing_)return;
    auto b = getLocalBounds().reduced(16);
    auto header = b.removeFromTop(104);
    brand_.setBounds(header.removeFromLeft(std::min(284, header.getWidth() / 4))); header.removeFromLeft(12);
    auto actions = header.removeFromRight(364).withTrimmedTop(3).withHeight(98);
    for (auto* button : {&play_, &generate_, &evolve_, &export_}) {button->setBounds(actions.removeFromLeft(86)); actions.removeFromLeft(6);}
    header.removeFromRight(12); presetBounds_ = header;
    auto preset = presetBounds_.reduced(12).withTrimmedTop(24); presetLabel_.setBounds(preset.removeFromTop(18));
    auto row = preset.removeFromTop(34); save_.setBounds(row.removeFromRight(55)); row.removeFromRight(6);
    open_.setBounds(row.removeFromRight(55)); row.removeFromRight(8); factory_.setBounds(row);
    b.removeFromTop(12); status_.setBounds(b.removeFromBottom(22));
    b.removeFromBottom(4); previewBounds_ = b.removeFromBottom(112); b.removeFromBottom(12);
    auto modules = b.removeFromBottom(std::clamp(getHeight() * 31 / 100, 248, 310)); b.removeFromBottom(12);
    const auto width = (modules.getWidth() - 50) / 6;
    for (std::size_t i = 0; i < moduleBounds_.size(); ++i) {
        moduleBounds_[i] = i < 5 ? modules.removeFromLeft(width) : modules; if (i < 5) modules.removeFromLeft(10);
        auto box = moduleBounds_[i].reduced(7).withTrimmedTop(26); locks_[i].setBounds(box.removeFromTop(24));
        const auto halfWidth = box.getWidth() / 2, halfHeight = box.getHeight() / 2;
        for (int j = 0; j < 4; ++j)
            controls_[i * 4 + static_cast<std::size_t>(j)].setBounds(box.getX() + (j % 2) * halfWidth,
                box.getY() + (j / 2) * halfHeight, halfWidth, halfHeight);
    }
    sideBounds_ = b.removeFromRight(236); b.removeFromRight(12); editor_.setBounds(b);
    auto side = sideBounds_.reduced(10).withTrimmedTop(25);
    compareLabel_.setBounds(side.removeFromTop(18)); historyBox_.setBounds(side.removeFromTop(26)); side.removeFromTop(4);
    auto ab = side.removeFromTop(30); storeA_.setBounds(ab.removeFromLeft(ab.getWidth() / 2)); ab.removeFromLeft(4); compare_.setBounds(ab);
    side.removeFromTop(6); format_.setBounds(side.removeFromTop(58)); side.removeFromTop(5);
    spectrumBounds_ = side; help_.setBounds(0, 0, 0, 0);
    auto preview = previewBounds_.reduced(12).withTrimmedTop(24); auto top = preview.removeFromTop(32);
    loop_.setBounds(top.removeFromLeft(70)); top.removeFromLeft(10); bpmLabel_.setBounds(top.removeFromLeft(47));
    bpm_.setBounds(top.removeFromLeft(210)); top.removeFromLeft(16); noveltyLabel_.setBounds(top.removeFromLeft(72));
    novelty_.setBounds(top.removeFromLeft(210)); top.removeFromLeft(18); mutationLabel_.setBounds(top.removeFromLeft(66));
    mutation_.setBounds(top.removeFromLeft(210)); subLock_.setBounds(top.reduced(10, 0));
    preview.removeFromTop(6); auto lower = preview.removeFromTop(30); seedLabel_.setBounds(lower.removeFromLeft(115));
    seed_.setBounds(lower.removeFromLeft(150)); lower.removeFromLeft(8); restore_.setBounds(lower.removeFromLeft(120));
    lower.removeFromLeft(12); audio_.setBounds(lower.removeFromLeft(80)); lower.removeFromLeft(12);
    help_.setBounds(lower.removeFromRight(290)); previewHelp_.setBounds(lower);
}

void MainComponent::runSmokeTest(const juce::File& directory) {
    showClassic(true);
    require(directory.createDirectory().wasOk(), "Cannot create smoke directory");
    auto* viewport = findParentComponentOfClass<juce::Viewport>();
    require(viewport != nullptr, "Instrument viewport is missing");
    const auto previousBounds = viewport->getBounds();
    viewport->setSize(1100, 708);
    require(getHeight() >= 860 && viewport->getVerticalScrollBar().isVisible(), "Compact view hides controls");
    viewport->setViewPosition(0, getHeight());
    require(viewport->getViewPositionY() > 0, "Compact view cannot scroll to exploration controls");
    viewport->setBounds(previousBounds); viewport->setViewPosition(0, 0);
    factory_.setSelectedId(static_cast<int>(kravatek::KickStyle::hardTechno) + 1, juce::sendNotificationSync);
    renderNow(); const auto original = kick_.samples;
    controls_[drive].slider.setValue(25, juce::sendNotificationSync); renderNow();
    require(kick_.samples != original, "DRIVE has no effect");
    storeA_.onClick(); const auto a = kick_.samples;
    controls_[end].slider.setValue(55, juce::sendNotificationSync); renderNow(); const auto b = kick_.samples;
    compare_.onClick(); renderNow(); require(kick_.samples == a, "A comparison failed");
    const auto aPreviewRms = kravatek::comparisonRms(deviceSamples_, deviceRate_.load());
    compare_.onClick(); renderNow(); require(kick_.samples == b, "B comparison failed");
    const auto bPreviewRms = kravatek::comparisonRms(deviceSamples_, deviceRate_.load());
    require(std::abs(aPreviewRms - bPreviewRms) < 1e-6, "A/B preview is not level matched");
    editor_.setAmplitudeMode(false); const auto oldPitch = state_.parameters.endHz;
    editor_.dragHandle(2, {0, -13}); renderNow(); require(state_.parameters.endHz != oldPitch, "Pitch editor is not connected");
    editor_.setAmplitudeMode(true); const auto oldAmp = state_.parameters.ampCurve;
    editor_.dragHandle(1, {0, -15}); renderNow(); require(state_.parameters.ampCurve != oldAmp, "Amplitude editor is not connected");
    const auto locked = state_.parameters;
    locks_[0].setToggleState(true, juce::sendNotificationSync); locks_[1].setToggleState(true, juce::sendNotificationSync);
    state_.locks = {true, true, false, false}; evolve_.onClick(); renderNow();
    require(state_.parameters.startHz == locked.startHz && state_.parameters.endHz == locked.endHz
        && state_.parameters.ampCurve == locked.ampCurve && state_.parameters.durationSeconds == locked.durationSeconds,
        "EVOLVE changed locked DNA");
    const auto sound = kick_.samples; const auto preset = directory.getChildFile("test-sound.json"); saveTo(preset);
    controls_[fm].slider.setValue(.07, juce::sendNotificationSync); renderNow();
    loadFrom(preset); renderNow(); require(kick_.samples == sound, "Preset save/load changed audio");
    bool rejected = false; try {decodePreset("{\"schema\":\"KRAVATEK_PRESET_002\",\"parameters\":{}}");}
    catch (const std::exception&) {rejected = true;} require(rejected, "Invalid preset accepted");
    seed_.setText("424242", false); restore_.onClick(); renderNow();
    require(kick_.samples == kravatek::renderKick(kravatek::generate(static_cast<kravatek::KickStyle>(state_.style), 424242, state_.novelty)).samples,
        "Seed restore is not deterministic");
    generate_.onClick(); renderNow(); const auto generated = kick_.samples;
    historyBox_.setSelectedId(1, juce::sendNotificationSync); renderNow();
    require(kick_.samples == kravatek::renderKick(history_.front().parameters).samples, "History restore failed");
    require(kick_.samples != generated, "GENERATE or HISTORY has no effect");
    editor_.setAmplitudeMode(false);
    loop_.setToggleState(true, juce::dontSendNotification); play_.onClick();
    if (audioActive_.load()) {require(looping_, "Tempo preview did not start"); play_.onClick(); require(!looping_, "STOP did not stop preview");}
    exportTo(directory.getChildFile("gui-smoke.wav"));
    auto image = createComponentSnapshot(getLocalBounds());
    auto imageFile = directory.getChildFile("prototype-002.png").createOutputStream();
    require(imageFile && imageFile->setPosition(0) && imageFile->truncate().wasOk()
        && juce::PNGImageFormat().writeImageToStream(image, *imageFile), "Screenshot failed");
    juce::DynamicObject::Ptr report = new juce::DynamicObject;
    report->setProperty("result", "passed"); report->setProperty("theme", "kravatek-instrument-002");
    report->setProperty("factoryPresets", static_cast<int>(kravatek::KickStyle::count));
    report->setProperty("parameters", static_cast<int>(controlCount)); report->setProperty("editablePitch", true);
    report->setProperty("editableAmplitude", true); report->setProperty("dnaLocks", true);
    report->setProperty("comparisonAB", true); report->setProperty("comparisonRmsMatched", true); report->setProperty("presetRoundTrip", true);
    report->setProperty("historyRestore", true); report->setProperty("novelty", true);
    report->setProperty("compactViewport", true); report->setProperty("measuredSpectrum", true); report->setProperty("peakDb", analysis_.peakDb);
    report->setProperty("rmsDb", analysis_.rmsDb); report->setProperty("seedRestore", true); report->setProperty("tempoPreview", audioActive_.load());
    report->setProperty("audioDeviceActive", audioActive_.load());
    report->setProperty("audioCallbacks", static_cast<juce::int64>(callbacks_.load()));
    report->setProperty("physicalListeningVerified", false);
    require(directory.getChildFile("gui-smoke.json").replaceWithText(juce::JSON::toString(juce::var(report.get()))), "Report failed");
}

void MainComponent::showClassic(bool classic) {
    playback_.stop();looping_=false;play_.setButtonText("PLAY");if(genetic_)genetic_->stop();classicShowing_=classic;
    for(int i=0;i<getNumChildComponents();++i){auto* c=getChildComponent(i);if(c!=genetic_.get()&&c!=&modeSwitch_)c->setVisible(classic);}
    if(genetic_)genetic_->setVisible(!classic);modeSwitch_.setVisible(true);modeSwitch_.setButtonText(classic?"GENETIC LAB":"CLASSIC 0.2");resized();repaint();
}
void MainComponent::runGeneticSmokeTest(const juce::File& directory,std::function<void(bool,juce::String)> done) {
    showClassic(false);genetic_->runSmokeTest(directory,[this,directory,done=std::move(done)](bool ok,juce::String error){
        if(ok){try{auto report=juce::JSON::parse(directory.getChildFile("gui-smoke.json").loadFileAsString());auto* object=report.getDynamicObject();if(!object)throw std::runtime_error("Missing genetic GUI report");object->setProperty("audioDeviceActive",audioActive_.load());object->setProperty("audioCallbacks",static_cast<juce::int64>(callbacks_.load()));object->setProperty("classic002Verified",true);
            if(!directory.getChildFile("gui-smoke.json").replaceWithText(juce::JSON::toString(report)))throw std::runtime_error("Cannot save complete GUI report");
            auto image=createComponentSnapshot(getLocalBounds());auto stream=directory.getChildFile("instrument-004.png").createOutputStream();if(!stream||!stream->setPosition(0)||stream->truncate().failed()||!juce::PNGImageFormat().writeImageToStream(image,*stream))throw std::runtime_error("Cannot save genetic screenshot");
        }catch(const std::exception& e){ok=false;error=e.what();}}
        done(ok,error);
    });
}
