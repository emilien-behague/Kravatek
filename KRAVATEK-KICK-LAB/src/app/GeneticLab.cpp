#include "GeneticLab.h"
#include "dsp/WavWriter.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <set>
using namespace kravatek::genetic;
namespace {
void require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
std::filesystem::path nativePath(const juce::File &f) {
#if JUCE_WINDOWS
  return std::filesystem::path(f.getFullPathName().toWideCharPointer());
#else
  return std::filesystem::path(f.getFullPathName().toStdString());
#endif
}
void comboStyle(juce::ComboBox &box) {
  box.setColour(juce::ComboBox::backgroundColourId, krui::dark);
  box.setColour(juce::ComboBox::textColourId, krui::ink);
  box.setColour(juce::ComboBox::outlineColourId, krui::cyan.withAlpha(.6f));
}
} // namespace
GeneticLab::GeneticLab() {
  profiles_ = bundledProfiles();
  const auto external =
      juce::File::getSpecialLocation(juce::File::currentExecutableFile)
          .getSiblingFile("Profiles")
          .getChildFile("core-004.json");
  if (external.existsAsFile()) {
    try {
      if (external.getSize() > 1024 * 1024)
        throw std::invalid_argument("Catalogue trop grand");
      profiles_ = decodeCatalog(external.loadFileAsString().toStdString());
    } catch (const std::exception &e) {
      catalogWarning_ =
          "Catalogue externe refusé ; catalogue intégré actif : " +
          juce::String(e.what());
    }
  }
  for (auto *c : std::initializer_list<juce::Component *>{
           &genre_, &subgenre_, &character_, &architecture_, &note_, &details_,
           &genreLabel_, &subgenreLabel_, &characterLabel_, &architectureLabel_,
           &noteLabel_, &overlap_})
    addAndMakeVisible(c);
  for (auto *l : {&genreLabel_, &subgenreLabel_, &characterLabel_,
                  &architectureLabel_, &noteLabel_, &overlap_}) {
    l->setFont(krui::mono(10));
    l->setColour(juce::Label::textColourId, krui::ink);
  }
  genreLabel_.setText("GENRE", juce::dontSendNotification);
  subgenreLabel_.setText("SUBGENRE / PROFILE", juce::dontSendNotification);
  characterLabel_.setText("CHARACTER", juce::dontSendNotification);
  architectureLabel_.setText("ARCHITECTURE", juce::dontSendNotification);
  noteLabel_.setText("NOTE", juce::dontSendNotification);
  for (auto *box : {&genre_, &subgenre_, &character_, &architecture_, &note_}) {
    comboStyle(*box);
    box->setTooltip("Sélection = rendu et écoute d'un candidat. DNA LOCK "
                    "conserve les couches verrouillées ; l'historique garde "
                    "les sons précédents. GENERATE produit le lot COUNT.");
  }
  for (int i = 0; i < static_cast<int>(Character::count); ++i)
    character_.addItem(characterName(static_cast<Character>(i)), i + 1);
  architecture_.addItem("PROFILE / AUTO", 1);
  for (int i = 0; i < 6; ++i)
    architecture_.addItem(architectureName(static_cast<Architecture>(i)),
                          i + 2);
  note_.addItem("PROFILE", 1);
  static const char *notes[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  for (int n = 24; n <= 60; ++n)
    note_.addItem(juce::String(notes[n % 12]) + juce::String(n / 12 - 1) +
                      " / " + juce::String(n),
                  n + 2);
  genre_.onChange = [this] { chooseGenre(); };
  subgenre_.onChange = [this] {
    if (!busy_ && subgenre_.getSelectedId() > 0 &&
        session_.settings.universal) {
      session_.settings.universal->profile = browserProfiles_.at(
          static_cast<std::size_t>(subgenre_.getSelectedId() - 1));
      previewStyle();
    }
  };
  character_.onChange = [this] {
    if (!busy_ && session_.settings.universal) {
      session_.settings.universal->character =
          static_cast<Character>(character_.getSelectedId() - 1);
      previewStyle();
    }
  };
  architecture_.onChange = [this] {
    if (!busy_ && session_.settings.universal) {
      session_.settings.universal->overrideArchitecture =
          architecture_.getSelectedId() == 1
              ? Architecture::count
              : static_cast<Architecture>(architecture_.getSelectedId() - 2);
      previewStyle();
    }
  };
  note_.onChange = [this] {
    if (!busy_ && session_.settings.universal) {
      session_.settings.universal->note =
          note_.getSelectedId() == 1 ? 0 : note_.getSelectedId() - 2;
      previewStyle();
    }
  };
  details_.onClick = [this] { openDetails(); };

  for (auto *c : std::initializer_list<juce::Component *>{
           &brand_,        &graph_,      &cardViewport_, &mode_,
           &count_,        &bodyKind_,   &attackKind_,   &tailKind_,
           &scope_,        &rate_,       &aBox_,         &bBox_,
           &seed_,         &play_,       &export_,       &save_,
           &open_,         &keep_,       &generate_,     &regenerate_,
           &evolve_,       &cancel_,     &history_,      &setA_,
           &setB_,         &playA_,      &playB_,        &compare_,
           &audio_,        &loop_,       &refine_,       &match_,
           &sidecar_,      &novelty_,    &bpm_,          &selectedName_,
           &selectedInfo_, &status_,     &metrics_,      &compareMetrics_,
           &batchInfo_,    &modeLabel_,  &seedLabel_,    &countLabel_,
           &noveltyLabel_, &tempoLabel_, &rateLabel_,    &scopeLabel_})
    addAndMakeVisible(c);
  for (auto &c : controls_)
    addAndMakeVisible(c);
  for (auto *label :
       {&selectedName_, &selectedInfo_, &status_, &metrics_, &compareMetrics_,
        &batchInfo_, &modeLabel_, &seedLabel_, &countLabel_, &noveltyLabel_,
        &tempoLabel_, &rateLabel_, &scopeLabel_}) {
    label->setColour(juce::Label::textColourId, krui::ink);
    label->setFont(krui::mono(11));
  }
  selectedName_.setFont(krui::mono(16, true));
  selectedInfo_.setColour(juce::Label::textColourId, krui::muted);
  status_.setFont(krui::mono(10.5f));
  metrics_.setFont(krui::mono(10));
  compareMetrics_.setFont(krui::mono(10));
  for (auto *button : {&play_, &export_, &save_, &open_, &keep_, &generate_,
                       &regenerate_, &evolve_, &cancel_, &history_, &setA_,
                       &setB_, &playA_, &playB_, &compare_, &audio_, &loop_})
    button->setWantsKeyboardFocus(false);
  for (auto *box : {&mode_, &count_, &bodyKind_, &attackKind_, &tailKind_,
                    &scope_, &rate_, &aBox_, &bBox_})
    comboStyle(*box);
  for (int i = 0; i < 5; ++i)
    mode_.addItem(modeName(static_cast<Mode>(i)), i + 1);
  for (auto n : {4, 8, 12, 20})
    count_.addItem(juce::String(n), n);
  for (int i = 0; i < 6; ++i) {
    bodyKind_.addItem(bodyName(static_cast<BodyKind>(i)), i + 1);
    tailKind_.addItem(tailName(static_cast<TailKind>(i)), i + 1);
  }
  attackKind_.addItemList({"NOISE", "TONAL", "HARMONIC", "HYBRID"}, 1);
  scope_.addItemList({"MIX", "SUB", "ATTACK", "BODY", "TAIL"}, 1);
  rate_.addItem("44.1 kHz", 44100);
  rate_.addItem("48 kHz", 48000);
  modeLabel_.setText("MODE", juce::dontSendNotification);
  seedLabel_.setText("SEED", juce::dontSendNotification);
  countLabel_.setText("COUNT", juce::dontSendNotification);
  noveltyLabel_.setText("NOVELTY", juce::dontSendNotification);
  tempoLabel_.setText("TEMPO", juce::dontSendNotification);
  rateLabel_.setText("EXPORT", juce::dontSendNotification);
  scopeLabel_.setText("LISTEN", juce::dontSendNotification);
  seed_.setInputRestrictions(10, "0123456789");
  seed_.setFont(krui::mono(13));
  seed_.setColour(juce::TextEditor::backgroundColourId, krui::dark);
  seed_.setColour(juce::TextEditor::textColourId, krui::ink);
  seed_.setColour(juce::TextEditor::outlineColourId, krui::cyan.withAlpha(.7f));
  novelty_.setSliderStyle(juce::Slider::LinearHorizontal);
  novelty_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 62, 24);
  novelty_.setRange(0, 100, 1);
  novelty_.setTextValueSuffix(" %");
  novelty_.setTooltip(juce::String::fromUTF8(
      "Intensité d'exploration : CONTROLLED / EXPLORATION / EXPERIMENTAL. Pas "
      "un score d'originalité."));
  bpm_.setSliderStyle(juce::Slider::LinearHorizontal);
  bpm_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 24);
  bpm_.setRange(40, 400, 1);
  bpm_.setTextValueSuffix(" BPM");
  const std::array<const char *, 4> names{"LOCK SUB", "LOCK ATTACK",
                                          "LOCK BODY", "LOCK TAIL"};
  for (std::size_t i = 0; i < 4; ++i) {
    addAndMakeVisible(locks_[i]);
    locks_[i].setButtonText(names[i]);
    locks_[i].setColour(juce::ToggleButton::textColourId, krui::ink);
    locks_[i].setColour(juce::ToggleButton::tickColourId, krui::cyan);
    locks_[i].setTooltip(juce::String::fromUTF8(
        "EVOLVE et l'aperçu de style conservent les paramètres et le signal "
        "isolé de cette couche. GENERATE et l'édition manuelle restent "
        "libres."));
    locks_[i].onClick = [this] {
      session_.settings.locks = {
          locks_[0].getToggleState(), locks_[1].getToggleState(),
          locks_[2].getToggleState(), locks_[3].getToggleState()};
      dirty_ = true;
    };
  }
  for (auto *toggle : {&refine_, &match_, &sidecar_}) {
    toggle->setColour(juce::ToggleButton::textColourId, krui::ink);
    toggle->setColour(juce::ToggleButton::tickColourId, krui::cyan);
  }
  refine_.setTooltip(
      juce::String::fromUTF8("Petites variations, architectures conservées, "
                             "sans rejet des sons proches."));
  match_.setTooltip(juce::String::fromUTF8(
      "A/B : atténuer le plus fort selon le RMS sur 400 ms. Aucun fichier "
      "modifié ; ce n'est pas une mesure LUFS."));
  sidecar_.setTooltip(
      juce::String::fromUTF8("Exporter aussi un fichier .krava.json permettant "
                             "de reproduire ce kick."));
  scope_.setTooltip(
      juce::String::fromUTF8("Préécoute du mix ou d'une couche isolée. "
                             "L'export contient toujours le mix complet."));
  loop_.setClickingTogglesState(true);
  loop_.setToggleState(true, juce::dontSendNotification);
  scope_.setSelectedId(1, juce::dontSendNotification);
  const auto configure = [&](Control index, const char *title, double lo,
                             double hi, double step, const char *suffix,
                             const char *tooltip) {
    auto &c = controls_[index];
    c.configure(title, lo, hi, step, suffix, juce::String::fromUTF8(tooltip));
    c.slider.onValueChange = [this, index] { controlChanged(index); };
  };
  configure(pitchStart, "START", 30, 2000, 1, " Hz",
            "Départ du pitch du BODY.");
  configure(pitchEnd, "END", 25, 150, .1, " Hz",
            "Fin du pitch du BODY ; le SUB reste indépendant.");
  configure(pitchDrop, "DROP", 5, 300, .1, " ms",
            "Temps d'arrivée au pitch final.");
  configure(pitchCurve, "BEND", .4, 2.5, .01, "",
            "Courbure des deux segments de pitch. Le point intermédiaire se "
            "déplace sur le graphe.");
  configure(attackLevel, "LEVEL", 0, 100, .1, " %",
            "Niveau de la couche ATTACK.");
  configure(attackDecay, "DECAY", .5, 80, .1, " ms",
            "Décroissance du transient.");
  configure(attackHz, "TONE", 150, 6000, 1, " Hz",
            "Fréquence du transient tonal/harmonique.");
  configure(attackColour, "COLOUR", 0, 100, .1, " %",
            "Bande de bruit du transient bruité/hybride.");
  configure(bodyLength, "LENGTH", 80, 1500, 1, " ms", "Durée propre du BODY.");
  configure(bodyLevel, "LEVEL", 0, 100, .1, " %",
            "Niveau du BODY avant le mix.");
  configure(
      harmonics, "HARMONICS", 0, 100, .1, " %",
      "Partiels en ADDITIVE/FM, interaction en RING ; indisponible en SINE.");
  configure(fm, "FM / PM", 0, 4, .01, "",
            "Modulation en FM / PM et RING ; indisponible pour les "
            "architectures sans modulateur.");
  configure(tailLevel, "LEVEL", 0, 100, .1, " %", "Niveau propre de TAIL.");
  configure(tailLength, "LENGTH", 40, 1500, 1, " ms",
            "Durée indépendante de TAIL.");
  configure(tailDecay, "DECAY", 15, 1000, 1, " ms", "Décroissance de TAIL.");
  configure(tailMotion, "MOTION", 0, 100, .1, " %",
            "Mouvement de la tail ELASTIC / RESONANT / PHASE WARP ; "
            "indisponible en TONAL.");
  configure(drive, "DRIVE", 0, 30, .1, " dB",
            "Saturation du BODY suréchantillonnée.");
  configure(fold, "FOLD", 0, 100, .1, " %",
            "Morphing du BODY vers le sine-fold.");
  configure(cutoff, "CUTOFF", 80, 16000, 1, " Hz",
            "Passe-bas résonant du BODY.");
  configure(resonance, "RESONANCE", 0, 95, .1, " %",
            "Résonance stable du BODY. La TAIL a son propre filtre.");
  controls_[cutoff].slider.setSkewFactorFromMidPoint(1600);
  configure(subHz, "PITCH", 25, 100, .1, " Hz",
            "Départ du SUB ; cible et glide dans SYNTH / FX.");
  configure(subLevel, "LEVEL", 0, 100, .1, " %",
            "Niveau du SUB, indépendant des autres couches.");
  configure(subDecay, "DECAY", 15, 1000, 1, " ms",
            "Décroissance indépendante du SUB.");
  configure(subLength, "LENGTH", 40, 1500, 1, " ms", "Durée propre du SUB.");
  mode_.onChange = [this] {
    if (!busy_ && mode_.getSelectedId() > 0) {
      session_.settings.mode = static_cast<Mode>(mode_.getSelectedId() - 1);
      if (!session_.settings.universal)
        previewStyle();
    }
  };
  count_.onChange = [this] {
    session_.settings.count = count_.getSelectedId();
    dirty_ = true;
  };
  novelty_.onValueChange = [this] {
    session_.settings.novelty = novelty_.getValue() / 100;
    dirty_ = true;
    batchInfo_.setText(novelty_.getValue() <= 30   ? "CONTROLLED"
                       : novelty_.getValue() <= 70 ? "EXPLORATION"
                                                   : "EXPERIMENTAL",
                       juce::dontSendNotification);
  };
  refine_.onClick = [this] {
    session_.settings.refine = refine_.getToggleState();
    dirty_ = true;
  };
  match_.onClick = [this] {
    session_.matchLevel = match_.getToggleState();
    dirty_ = true;
    if (playing_)
      audition(listeningIndex_, listeningComparison_);
  };
  sidecar_.onClick = [this] {
    session_.sidecar = sidecar_.getToggleState();
    dirty_ = true;
  };
  bpm_.onValueChange = [this] {
    session_.bpm = bpm_.getValue();
    updateOverlap();
    dirty_ = true;
    if (playing_)
      audition();
  };
  loop_.onClick = [this] {
    session_.loop = loop_.getToggleState();
    updateOverlap();
    dirty_ = true;
    if (playing_)
      audition();
  };
  rate_.onChange = [this] {
    if (rate_.getSelectedId() > 0 && !busy_) {
      session_.settings.sampleRate = rate_.getSelectedId();
      cache_.clear();
      dirty_ = true;
      refreshLibrary(session_.visible);
    }
  };
  scope_.onChange = [this] {
    refreshCurrent();
    if (playing_)
      audition(listeningIndex_, listeningComparison_);
  };
  bodyKind_.onChange = [this] {
    if (bodyKind_.getSelectedId() > 0)
      modify([this](Genome &g) {
        g.body.kind = static_cast<BodyKind>(bodyKind_.getSelectedId() - 1);
        if (g.body.kind >= BodyKind::membrane) {
          upgrade(g);
          g.body.renderVersion = 4;
        }
      });
  };
  attackKind_.onChange = [this] {
    if (attackKind_.getSelectedId() > 0)
      modify([this](Genome &g) {
        g.attack.kind =
            static_cast<AttackKind>(attackKind_.getSelectedId() - 1);
      });
  };
  tailKind_.onChange = [this] {
    if (tailKind_.getSelectedId() > 0)
      modify([this](Genome &g) {
        g.tail.kind = static_cast<TailKind>(tailKind_.getSelectedId() - 1);
        if (g.tail.kind >= TailKind::gated) {
          upgrade(g);
          g.tail.renderVersion = 4;
        }
      });
  };
  generate_.onClick = [this] { startBatch(false); };
  regenerate_.onClick = [this] { startBatch(false, true); };
  evolve_.onClick = [this] { startBatch(true); };
  cancel_.onClick = [this] {
    previewAuditionPending_ = false;
    worker_.request_stop();
    status("Annulation demandée ; les candidats déjà calculés sont conservés.");
  };
  history_.onClick = [this] {
    commitEdit();
    std::vector<int> indices;
    for (std::size_t i = 0; i < session_.library.entries().size(); ++i)
      indices.push_back(static_cast<int>(i));
    refreshLibrary(indices);
  };
  play_.onClick = [this] { togglePlay(); };
  audio_.onClick = [this] {
    if (chooseAudio)
      chooseAudio();
  };
  save_.onClick = [this] { chooseFile(1); };
  open_.onClick = [this] { chooseFile(0); };
  keep_.onClick = [this] {
    commitEdit();
    status("Édition conservée comme enfant ; parent intact.");
  };
  export_.onClick = [this] { chooseFile(2); };
  setA_.onClick = [this] { setComparison(true); };
  setB_.onClick = [this] { setComparison(false); };
  aBox_.onChange = [this] {
    session_.compareA = aBox_.getSelectedId() - 1;
    dirty_ = true;
    updateComparisonMetrics();
  };
  bBox_.onChange = [this] {
    session_.compareB = bBox_.getSelectedId() - 1;
    dirty_ = true;
    updateComparisonMetrics();
  };
  playA_.onClick = [this] {
    activeA_ = true;
    audition(session_.compareA, true);
  };
  playB_.onClick = [this] {
    activeA_ = false;
    audition(session_.compareB, true);
  };
  compare_.onClick = [this] {
    activeA_ = !activeA_;
    audition(activeA_ ? session_.compareA : session_.compareB, true);
  };
  graph_.onEdit = [this](const BodyGenes &b) {
    modify([&](Genome &g) { g.body = b; });
  };
  cardViewport_.setViewedComponent(&cards_, false);
  cardViewport_.setScrollBarsShown(true, false);
  cards_.onSelect = [this](int i, bool play) { select(i, play); };
  session_.settings.universal = UniversalRequest{profiles_.front()};
  session_.selected = static_cast<int>(session_.library.append(
      generateUniversal(*session_.settings.universal, 1, .75)));
  session_.visible = {session_.selected};
  sync();
  refreshCurrent();
  rebuildCards();
  setBusy(false);
  status(catalogWarning_.isEmpty() ? "STYLE BROWSER / choisir un style le rend "
                                     "et l'écoute. GENERATE = lot COUNT."
                                   : catalogWarning_,
         catalogWarning_.isNotEmpty());
  setWantsKeyboardFocus(true);
}
GeneticLab::~GeneticLab() {
  stopTimer();
  if (worker_.joinable()) {
    worker_.request_stop();
    worker_.join();
  }
  cardViewport_.setViewedComponent(nullptr, false);
}
const Genome &GeneticLab::current() const {
  if (edit_)
    return *edit_;
  return session_.library.at(static_cast<std::size_t>(session_.selected));
}
Candidate &GeneticLab::cached(const Genome &g) {
  auto found = cache_.find(g.id);
  if (found != cache_.end() &&
      found->second.audio.sampleRate == session_.settings.sampleRate)
    return found->second;
  Candidate c;
  c.genome = g;
  c.audio = render(g, session_.settings.sampleRate, false);
  c.features =
      measure(c.audio.mix, c.audio.sampleRate, g.engine == universalVersion);
  if (cache_.size() > 80)
    cache_.erase(cache_.begin());
  return cache_.insert_or_assign(g.id, std::move(c)).first->second;
}
void GeneticLab::sync() {
  const auto &g = current();
  syncBrowser();
  controls_[bodyLength].slider.setRange(
      8, g.body.renderVersion == 4 ? 4000 : 1500, 1);
  controls_[tailLength].slider.setRange(
      8, g.tail.renderVersion == 4 ? 4000 : 1500, 1);
  controls_[subLength].slider.setRange(
      8, g.sub.renderVersion == 4 ? 4000 : 1500, 1);
  controls_[subHz].slider.setRange(25, g.sub.renderVersion == 4 ? 400 : 100,
                                   .1);
  controls_[pitchEnd].slider.setRange(25, g.body.renderVersion == 4 ? 400 : 150,
                                      .1);
  controls_[subDecay].slider.setRange(
      15, g.sub.renderVersion == 4 ? 3000 : 1000, 1);
  controls_[tailDecay].slider.setRange(
      15, g.tail.renderVersion == 4 ? 3000 : 1000, 1);
  const std::array<double, controlCount> values{
      g.body.startHz,      g.body.endHz,           g.body.settleTime * 1000,
      g.body.curve,        g.attack.level * 100,   g.attack.decay * 1000,
      g.attack.hz,         g.attack.colour * 100,  g.body.duration * 1000,
      g.body.level * 100,  g.body.harmonics * 100, g.body.fm,
      g.tail.level * 100,  g.tail.duration * 1000, g.tail.decay * 1000,
      g.tail.motion * 100, g.body.drive,           g.body.fold * 100,
      g.body.cutoff,       g.body.resonance * 100, g.sub.hz,
      g.sub.level * 100,   g.sub.decay * 1000,     g.sub.duration * 1000};
  for (std::size_t i = 0; i < values.size(); ++i)
    controls_[i].slider.setValue(values[i], juce::dontSendNotification);
  mode_.setSelectedId(static_cast<int>(session_.settings.mode) + 1,
                      juce::dontSendNotification);
  count_.setSelectedId(session_.settings.count == 1 ? 4
                                                    : session_.settings.count,
                       juce::dontSendNotification);
  seed_.setText(juce::String(static_cast<juce::int64>(session_.settings.seed)),
                false);
  novelty_.setValue(session_.settings.novelty * 100,
                    juce::dontSendNotification);
  bpm_.setValue(session_.bpm, juce::dontSendNotification);
  rate_.setSelectedId(static_cast<int>(session_.settings.sampleRate),
                      juce::dontSendNotification);
  loop_.setToggleState(session_.loop, juce::dontSendNotification);
  refine_.setToggleState(session_.settings.refine, juce::dontSendNotification);
  match_.setToggleState(session_.matchLevel, juce::dontSendNotification);
  sidecar_.setToggleState(session_.sidecar, juce::dontSendNotification);
  const auto &l = session_.settings.locks;
  const std::array<bool, 4> locked{l.sub, l.attack, l.body, l.tail};
  for (std::size_t i = 0; i < 4; ++i)
    locks_[i].setToggleState(locked[i], juce::dontSendNotification);
  bodyKind_.setSelectedId(static_cast<int>(g.body.kind) + 1,
                          juce::dontSendNotification);
  attackKind_.setSelectedId(static_cast<int>(g.attack.kind) + 1,
                            juce::dontSendNotification);
  tailKind_.setSelectedId(static_cast<int>(g.tail.kind) + 1,
                          juce::dontSendNotification);
  selectedName_.setText(juce::String(g.name) + (edit_ ? " *EDIT" : ""),
                        juce::dontSendNotification);
  selectedInfo_.setText(
      juce::String(g.engine) + " / " + juce::String(g.id.substr(0, 15)) +
          " / G" + juce::String(static_cast<int>(g.generation)) + " / " +
          bodyName(g.body.kind) + " + " + tailName(g.tail.kind),
      juce::dontSendNotification);
  aBox_.clear(juce::dontSendNotification);
  bBox_.clear(juce::dontSendNotification);
  for (std::size_t i = 0; i < session_.library.entries().size(); ++i) {
    const auto name = juce::String(static_cast<int>(i + 1)) + " / " +
                      juce::String(session_.library.at(i).name);
    aBox_.addItem(name, static_cast<int>(i + 1));
    bBox_.addItem(name, static_cast<int>(i + 1));
  }
  aBox_.setSelectedId(session_.compareA + 1, juce::dontSendNotification);
  bBox_.setSelectedId(session_.compareB + 1, juce::dontSendNotification);
  aBox_.setTextWhenNothingSelected("A / choose kick");
  bBox_.setTextWhenNothingSelected("B / choose kick");
  keep_.setEnabled(edit_.has_value() && !busy_);
  controls_[harmonics].setEnabled(!busy_ && g.body.kind != BodyKind::sine);
  controls_[fm].setEnabled(
      !busy_ && (g.body.kind == BodyKind::fm || g.body.kind == BodyKind::ring));
  controls_[tailMotion].setEnabled(!busy_ && g.tail.kind != TailKind::tonal &&
                                   g.tail.kind != TailKind::gated);
  controls_[attackHz].setEnabled(!busy_ && g.attack.kind != AttackKind::noise);
  controls_[attackColour].setEnabled(!busy_ &&
                                     (g.attack.kind == AttackKind::noise ||
                                      g.attack.kind == AttackKind::hybrid));
  updateComparisonMetrics();
  cards_.selected = session_.selected;
  updateOverlap();
  repaint();
}
void GeneticLab::rebuildCards() {
  cards_.cards.clear();
  for (auto index : session_.visible) {
    if (index < 0 ||
        index >= static_cast<int>(session_.library.entries().size()))
      continue;
    const auto &g = session_.library.at(static_cast<std::size_t>(index));
    auto &c = cached(g);
    CardView card;
    card.libraryIndex = index;
    card.genome = g;
    card.features = c.features;
    card.close = c.nearDuplicate;
    for (std::size_t j = 0; j < card.wave.size(); ++j) {
      const auto first = j * c.audio.mix.size() / card.wave.size(),
                 last = std::max(first + 1, (j + 1) * c.audio.mix.size() /
                                                card.wave.size());
      float v = 0;
      for (auto k = first; k < std::min(last, c.audio.mix.size()); ++k)
        if (std::abs(c.audio.mix[k]) > std::abs(v))
          v = c.audio.mix[k];
      card.wave[j] = v;
    }
    cards_.cards.push_back(std::move(card));
  }
  cards_.selected = session_.selected;
  cards_.layout(std::max(100, cardViewport_.getMaximumVisibleWidth()));
}
void GeneticLab::select(int index, bool play) {
  if (index < 0 || index >= static_cast<int>(session_.library.entries().size()))
    return;
  commitEdit();
  session_.selected = index;
  dirty_ = true;
  sync();
  refreshCurrent();
  cards_.repaint();
  if (play || playing_)
    audition();
}
void GeneticLab::modify(std::function<void(Genome &)> fn) {
  if (busy_)
    return;
  try {
    if (!edit_) {
      edit_ = current();
      const auto parent = edit_->id;
      edit_->parents = {parent};
      ++edit_->generation;
      edit_->name = "EDIT / " + edit_->name.substr(0, 110);
    }
    fn(*edit_);
    identify(*edit_);
    manualPending_ = true;
    dirty_ = true;
    sync();
    startTimer(35);
    status("Édition / KEEP EDIT ou SAVE conserve cet enfant. Le parent reste "
           "intact.");
  } catch (const std::exception &e) {
    edit_.reset();
    status(juce::String(e.what()), true);
    sync();
  }
}
void GeneticLab::controlChanged(Control c) {
  const auto v = controls_[c].slider.getValue();
  modify([=](Genome &g) {
    switch (c) {
    case pitchStart:
      g.body.startHz = v;
      break;
    case pitchEnd:
      g.body.endHz = v;
      break;
    case pitchDrop:
      g.body.settleTime = v / 1000;
      g.body.pivotTime = std::min(g.body.pivotTime, g.body.settleTime - .001);
      break;
    case pitchCurve:
      g.body.curve = v;
      break;
    case attackLevel:
      g.attack.level = v / 100;
      break;
    case attackDecay:
      g.attack.decay = v / 1000;
      break;
    case attackHz:
      g.attack.hz = v;
      break;
    case attackColour:
      g.attack.colour = v / 100;
      break;
    case bodyLength:
      g.body.duration = v / 1000;
      g.body.attack = std::min(g.body.attack, g.body.duration * .3);
      g.body.release = std::min(g.body.release, g.body.duration * .3);
      break;
    case bodyLevel:
      g.body.level = v / 100;
      break;
    case harmonics:
      g.body.harmonics = v / 100;
      break;
    case fm:
      g.body.fm = v;
      break;
    case tailLevel:
      g.tail.level = v / 100;
      break;
    case tailLength:
      g.tail.duration = v / 1000;
      g.tail.attack = std::min(g.tail.attack, g.tail.duration * .3);
      g.tail.release = std::min(g.tail.release, g.tail.duration * .3);
      break;
    case tailDecay:
      g.tail.decay = v / 1000;
      break;
    case tailMotion:
      g.tail.motion = v / 100;
      break;
    case drive:
      g.body.drive = v;
      break;
    case fold:
      g.body.fold = v / 100;
      break;
    case cutoff:
      g.body.cutoff = v;
      break;
    case resonance:
      g.body.resonance = v / 100;
      break;
    case subHz:
      if (g.sub.renderVersion == 4 && std::abs(g.sub.hz - g.sub.endHz) < .0001)
        g.sub.endHz = v;
      g.sub.hz = v;
      break;
    case subLevel:
      g.sub.level = v / 100;
      break;
    case subDecay:
      g.sub.decay = v / 1000;
      break;
    case subLength:
      g.sub.duration = v / 1000;
      g.sub.attack = std::min(g.sub.attack, g.sub.duration * .3);
      g.sub.release = std::min(g.sub.release, g.sub.duration * .3);
      break;
    default:
      break;
    }
  });
}
void GeneticLab::commitEdit() {
  if (!edit_)
    return;
  const auto index = static_cast<int>(session_.library.append(*edit_));
  session_.selected = index;
  if (std::find(session_.visible.begin(), session_.visible.end(), index) ==
      session_.visible.end())
    session_.visible.push_back(index);
  edit_.reset();
  manualPending_ = false;
  dirty_ = true;
  sync();
  rebuildCards();
}
void GeneticLab::refreshCurrent() {
  if (session_.selected < 0)
    return;
  try {
    auto &c = cached(current());
    const auto &f = c.features;
    auto wave = c.audio.mix;
    const auto scope = scope_.getSelectedId() - 1;
    if (scope > 0) {
      auto full = render(current(), session_.settings.sampleRate, true);
      wave = std::move(full.layers[static_cast<std::size_t>(scope - 1)]);
      for (auto &v : wave)
        v *= .35f;
    }
    graph_.setSound(current(), wave, session_.settings.sampleRate);
    updateOverlap();
    const auto estimate = f.fundamentalHz > 0
                              ? juce::String(f.fundamentalHz, 1) + " Hz / " +
                                    juce::String(static_cast<int>(std::lround(
                                        f.fundamentalConfidence * 100))) +
                                    "%"
                              : "? / low confidence";
    metrics_.setText(
        "F0 EST " + estimate + "\n" +
            juce::String(
                static_cast<int>(std::lround(f.effectiveSeconds * 1000))) +
            " ms eff. / PEAK " + juce::String(f.basic.peakDb, 1) +
            " dBFS\nRMS " + juce::String(f.basic.rmsDb, 1) + " / CREST " +
            juce::String(f.basic.peakDb - f.basic.rmsDb, 1) + " dB\nCENTROID " +
            juce::String(static_cast<int>(std::lround(f.centroidHz))) +
            " Hz / ATK " +
            juce::String(
                static_cast<int>(std::lround(f.transientFraction * 100))) +
            "%\nLOW/MID/HIGH " +
            juce::String(static_cast<int>(std::lround(f.bandEnergy[0] * 100))) +
            "/" +
            juce::String(static_cast<int>(std::lround(f.bandEnergy[1] * 100))) +
            "/" +
            juce::String(static_cast<int>(std::lround(f.bandEnergy[2] * 100))) +
            "%",
        juce::dontSendNotification);
    repaint();
  } catch (const std::exception &e) {
    status(e.what(), true);
  }
}
void GeneticLab::timerCallback() {
  if (manualPending_ && !busy_) {
    manualPending_ = false;
    refreshCurrent();
    if (playing_)
      audition();
  }
  if (busy_) {
    const auto n = progress_.load(), total = total_.load();
    batchInfo_.setText("RENDER " + juce::String(n) + " / " +
                           juce::String(total) + " / CANCEL possible",
                       juce::dontSendNotification);
  } else
    stopTimer();
}
void GeneticLab::readSettings() {
  const auto text = seed_.getText();
  if (text.isEmpty() || text.getLargeIntValue() > 4294967295LL)
    throw std::invalid_argument("SEED : 0 .. 4294967295");
  session_.settings.seed = static_cast<std::uint32_t>(text.getLargeIntValue());
  session_.settings.mode = static_cast<Mode>(mode_.getSelectedId() - 1);
  session_.settings.count = count_.getSelectedId();
  session_.settings.novelty = novelty_.getValue() / 100;
  session_.settings.refine = refine_.getToggleState();
  session_.settings.sampleRate = rate_.getSelectedId();
  session_.settings.parent.reset();
}
void GeneticLab::setBusy(bool value) {
  busy_ = value;
  for (auto *c : std::initializer_list<juce::Component *>{
           &generate_,   &regenerate_,   &evolve_, &history_, &save_,
           &open_,       &keep_,         &export_, &mode_,    &seed_,
           &count_,      &novelty_,      &refine_, &rate_,    &bodyKind_,
           &attackKind_, &tailKind_,     &graph_,  &genre_,   &subgenre_,
           &character_,  &architecture_, &note_,   &details_})
    c->setEnabled(!value);
  for (auto &c : controls_)
    c.setEnabled(!value);
  for (auto &l : locks_)
    l.setEnabled(!value);
  cancel_.setEnabled(value);
  if (!value)
    sync();
}
void GeneticLab::startBatch(bool evolving, bool regenerate, bool stylePreview) {
  if (busy_)
    return;
  try {
    readSettings();
    commitEdit();
    auto request = session_.settings;
    if (stylePreview) {
      request.count = 1;
      const auto &locks = request.locks;
      evolving = locks.sub || locks.attack || locks.body || locks.tail;
    }
    if (regenerate) {
      Random next(request.seed);
      request.seed = next.next();
      session_.settings.seed = request.seed;
      seed_.setText(juce::String(static_cast<juce::int64>(request.seed)),
                    false);
      request.count = 1;
    }
    if (evolving)
      request.parent = current();
    if (session_.library.entries().size() +
            static_cast<std::size_t>(request.count) >
        Library::maximum)
      throw std::invalid_argument(
          "Bibliothèque pleine : SAVE, puis ouvre une nouvelle session.");
    previewAuditionPending_ = stylePreview;
    setBusy(true);
    progress_.store(0);
    total_.store(request.count);
    startTimer(50);
    status(stylePreview ? "STYLE / rendu d'un aperçu ; historique et DNA "
                          "LOCK conservés."
           : evolving   ? "EVOLVE / rendu en arrière-plan ; parent conservé."
                        : "GENERATE / rendu en arrière-plan.");
    const auto job = stylePreview ? Job::style
                     : evolving   ? Job::evolve
                                  : Job::create;
    const juce::Component::SafePointer<GeneticLab> safe(this);
    worker_ = std::jthread([this, safe, request, job](std::stop_token token) {
      std::vector<Candidate> result;
      juce::String error;
      try {
        result = population(request, token,
                            [this](int n, int) { progress_.store(n); });
      } catch (const std::exception &e) {
        error = e.what();
      }
      juce::MessageManager::callAsync(
          [safe, result = std::move(result), job, error]() mutable {
            if (safe)
              safe->finishJob(std::move(result), job, -1, error);
          });
    });
  } catch (const std::exception &e) {
    previewAuditionPending_ = false;
    status(e.what(), true);
    if (smokeStage_)
      smokeDone(false, e.what());
  }
}
void GeneticLab::refreshLibrary(std::vector<int> indices) {
  if (busy_)
    return;
  if (indices.empty()) {
    rebuildCards();
    refreshCurrent();
    return;
  }
  setBusy(true);
  progress_ = 0;
  total_ = static_cast<int>(indices.size());
  startTimer(50);
  std::vector<Genome> records;
  for (auto i : indices)
    records.push_back(session_.library.at(static_cast<std::size_t>(i)));
  const auto selection = session_.selected;
  const auto rate = session_.settings.sampleRate;
  const juce::Component::SafePointer<GeneticLab> safe(this);
  worker_ = std::jthread([this, safe, records = std::move(records), selection,
                          rate](std::stop_token token) {
    std::vector<Candidate> result;
    juce::String error;
    try {
      for (const auto &g : records) {
        if (token.stop_requested())
          break;
        Candidate c;
        c.genome = g;
        c.audio = render(g, rate, false);
        c.features = measure(c.audio.mix, rate);
        result.push_back(std::move(c));
        progress_ = static_cast<int>(result.size());
      }
    } catch (const std::exception &e) {
      error = e.what();
    }
    juce::MessageManager::callAsync(
        [safe, result = std::move(result), selection, error]() mutable {
          if (safe)
            safe->finishJob(std::move(result), Job::refresh, selection, error);
        });
  });
}
void GeneticLab::finishJob(std::vector<Candidate> candidates, Job job,
                           int keepSelection, juce::String error) {
  try {
    if (error.isNotEmpty())
      throw std::runtime_error(error.toStdString());
    std::vector<int> visible;
    int close = 0;
    for (auto &c : candidates) {
      const auto index = static_cast<int>(session_.library.append(c.genome));
      visible.push_back(index);
      close += c.nearDuplicate;
      cache_.insert_or_assign(c.genome.id, std::move(c));
    }
    if (!visible.empty()) {
      session_.visible = visible;
      session_.selected = job == Job::refresh && keepSelection >= 0
                              ? keepSelection
                              : visible.front();
    }
    edit_.reset();
    dirty_ |= job != Job::refresh;
    setBusy(false);
    stopTimer();
    sync();
    rebuildCards();
    if (job == Job::refresh) {
      const auto at = std::find(session_.visible.begin(),
                                session_.visible.end(), session_.selected);
      if (at != session_.visible.end()) {
        const auto columns = std::max(1, cards_.getWidth() / 260);
        cardViewport_.setViewPosition(
            0, static_cast<int>(at - session_.visible.begin()) / columns * 92);
      }
    }
    if (job == Job::style)
      scope_.setSelectedId(1, juce::dontSendNotification);
    refreshCurrent();
    batchInfo_.setText(
        juce::String(static_cast<int>(visible.size())) + " candidats / " +
            juce::String(close) + " proches / " +
            juce::String(static_cast<int>(session_.library.entries().size())) +
            " en mémoire",
        juce::dontSendNotification);
    status(close > 0
               ? "Certaines variantes sont très proches. REFINE les autorise ; "
                 "déverrouille une couche pour explorer davantage."
               : "Double-clic sur une carte pour écouter. SAVE conserve la "
                 "session et ses parents.");
    const bool shouldAudition = previewAuditionPending_ || playing_;
    previewAuditionPending_ = false;
    if (!visible.empty() && job != Job::refresh && shouldAudition) {
      const auto previousAuditions = acceptedAuditions_;
      audition();
      if (job == Job::style && acceptedAuditions_ > previousAuditions) {
        const auto &l = session_.settings.locks;
        status(l.sub && l.attack && l.body && l.tail
                   ? "STYLE / 4 DNA LOCK : son conservé. Déverrouille une "
                     "couche pour changer le timbre."
                   : "STYLE / " +
                         (session_.settings.universal ? subgenre_.getText()
                                                      : mode_.getText()) +
                         " / aperçu MIX ; ancien son dans HISTORY.");
      }
    }
    if (smokeStage_)
      smokeAdvance();
  } catch (const std::exception &e) {
    previewAuditionPending_ = false;
    setBusy(false);
    stopTimer();
    status(e.what(), true);
    if (smokeStage_)
      smokeDone(false, e.what());
  }
}
std::vector<float> GeneticLab::listeningSamples(const Genome &g) {
  const auto rate = deviceRate ? deviceRate() : 48000;
  const auto key = g.id + "/" + std::to_string(static_cast<int>(rate));
  auto found = auditionCache_.find(key);
  if (found == auditionCache_.end()) {
    if (auditionCache_.size() > 8)
      auditionCache_.erase(auditionCache_.begin());
    found = auditionCache_.emplace(key, render(g, rate, true)).first;
  }
  const auto scope = scope_.getSelectedId() - 1;
  if (scope <= 0)
    return found->second.mix;
  auto audio = found->second.layers[static_cast<std::size_t>(scope - 1)];
  for (auto &v : audio)
    v *= .35f;
  return audio;
}
void GeneticLab::audition(int index, bool comparing) {
  try {
    const auto &g = index < 0
                        ? current()
                        : session_.library.at(static_cast<std::size_t>(index));
    auto samples = listeningSamples(g);
    lastAuditionGain_ = 1;
    const auto rate = deviceRate ? deviceRate() : 48000;
    if (comparing && session_.matchLevel && session_.compareA >= 0 &&
        session_.compareB >= 0) {
      const auto a = listeningSamples(session_.library.at(
                     static_cast<std::size_t>(session_.compareA))),
                 b = listeningSamples(session_.library.at(
                     static_cast<std::size_t>(session_.compareB)));
      const auto ar = kravatek::comparisonRms(a, rate),
                 br = kravatek::comparisonRms(b, rate),
                 own = kravatek::comparisonRms(samples, rate);
      if (ar > 1e-6 && br > 1e-6 && own > 0)
        lastAuditionGain_ = std::min(ar, br) / own;
    }
    for (auto &v : samples)
      v *= static_cast<float>(lastAuditionGain_);
    lastAudition_ = samples;
    playA_.setToggleState(comparing && activeA_, juce::dontSendNotification);
    playB_.setToggleState(comparing && !activeA_, juce::dontSendNotification);
    listeningIndex_ = index;
    listeningComparison_ = comparing;
    const bool accepted =
        queueAudio && queueAudio(samples, session_.loop, session_.bpm);
    if (accepted)
      ++acceptedAuditions_;
    playing_ = accepted && session_.loop;
    play_.setButtonText(playing_ ? "STOP" : "PLAY");
    if (!accepted)
      status("Lecture indisponible. AUDIO permet de choisir la sortie ; "
             "SAVE et EXPORT restent disponibles.",
             true);
    else
      status(
          comparing
              ? juce::String(activeA_ ? "A" : "B") + " / " +
                    (session_.matchLevel ? "RMS 400 ms matched / "
                                         : "REAL LEVEL / ") +
                    juce::String(
                        20 * std::log10(std::max(lastAuditionGain_, 1e-9)), 1) +
                    " dB preview gain / export intact"
              : "PLAY / " + scope_.getText() + " / SPACE = STOP en boucle");
  } catch (const std::exception &e) {
    status(e.what(), true);
  }
}
void GeneticLab::togglePlay() {
  if (playing_)
    stop();
  else
    audition();
}
void GeneticLab::stop() {
  previewAuditionPending_ = false;
  if (stopAudio)
    stopAudio();
  playing_ = false;
  play_.setButtonText("PLAY");
}
bool GeneticLab::keyPressed(const juce::KeyPress &k) {
  if (k.getKeyCode() == juce::KeyPress::spaceKey &&
      !seed_.hasKeyboardFocus(true)) {
    togglePlay();
    return true;
  }
  return false;
}
void GeneticLab::setComparison(bool a) {
  commitEdit();
  if (a)
    session_.compareA = session_.selected;
  else
    session_.compareB = session_.selected;
  dirty_ = true;
  sync();
  status(a ? "A sélectionné. Choisis une autre carte puis SET B."
           : "B sélectionné. A >, B > ou A/B permettent de comparer.");
}
void GeneticLab::updateComparisonMetrics() {
  playA_.setEnabled(session_.compareA >= 0);
  playB_.setEnabled(session_.compareB >= 0);
  compare_.setEnabled(session_.compareA >= 0 && session_.compareB >= 0);
  juce::String text;
  for (auto index : {session_.compareA, session_.compareB}) {
    if (index >= 0) {
      auto &c = cached(session_.library.at(static_cast<std::size_t>(index)));
      text += (text.isEmpty() ? "A " : "B ") +
              juce::String(c.features.basic.rmsDb, 1) + " RMS / " +
              juce::String(c.features.basic.peakDb, 1) + " PK dBFS\n";
    } else
      text += (text.isEmpty() ? "A / not selected\n" : "B / not selected\n");
  }
  compareMetrics_.setText(text, juce::dontSendNotification);
}
void GeneticLab::status(const juce::String &text, bool error) {
  status_.setColour(juce::Label::textColourId,
                    error ? juce::Colour(0xffffbd93) : krui::muted);
  status_.setText(text, juce::dontSendNotification);
  status_.setTooltip(text);
}
void GeneticLab::saveTo(const juce::File &file) {
  commitEdit();
  readSettings();
  const auto json = encodeSession(session_);
  juce::TemporaryFile temp(file);
  if (!temp.getFile().replaceWithText(juce::String::fromUTF8(json.c_str())) ||
      !temp.overwriteTargetFileWithTemporary())
    throw std::runtime_error("Impossible d'enregistrer la session");
  dirty_ = false;
  status("Session sauvegardée : " + file.getFullPathName());
}
void GeneticLab::loadFrom(const juce::File &file) {
  if (file.getSize() > 16 * 1024 * 1024)
    throw std::invalid_argument("Session supérieure à 16 MiB");
  const auto text = file.loadFileAsString();
  if (juce::JSON::parse(text)["schema"].toString() == "KRAVATEK_PRESET_002") {
    if (openLegacy) {
      openLegacy(file);
      return;
    }
    throw std::invalid_argument("Ouvrir ce fichier avec CLASSIC 0.2");
  }
  auto loaded = decodeSession(text.toStdString());
  if (loaded.library.entries().empty()) {
    loaded.selected = static_cast<int>(loaded.library.append(generateGenome(
        loaded.settings.mode, loaded.settings.seed, loaded.settings.novelty)));
    loaded.visible = {loaded.selected};
  }
  stop();
  session_ = std::move(loaded);
  edit_.reset();
  cache_.clear();
  auditionCache_.clear();
  dirty_ = false;
  sync();
  refreshLibrary(session_.visible);
  status("Session chargée : " + file.getFileName());
}
void GeneticLab::exportTo(const juce::File &file) {
  commitEdit();
  auto g = current();
  g.locks = session_.settings.locks;
  auto &c = cached(g);
  const auto json =
      encodeSound(g, session_.settings.sampleRate, session_.settings.universal);
  juce::TemporaryFile wav(file);
  kravatek::writePcm24(nativePath(wav.getFile()), c.audio.mix,
                       c.audio.sampleRate, json);
  const auto params = file.withFileExtension(".krava.json");
  juce::TemporaryFile sidecar(params);
  if (session_.sidecar &&
      !sidecar.getFile().replaceWithText(juce::String::fromUTF8(json.c_str())))
    throw std::runtime_error("Impossible d'écrire les paramètres associés");
  if (!wav.overwriteTargetFileWithTemporary())
    throw std::runtime_error("Impossible de remplacer le WAV");
  if (session_.sidecar && !sidecar.overwriteTargetFileWithTemporary())
    throw std::runtime_error(
        "WAV enregistré ; échec du fichier paramètres associé");
  status("Export : " + file.getFullPathName() +
         (session_.sidecar ? " + .krava.json" : ""));
}
void GeneticLab::chooseFile(int operation) {
  if (chooser_ || busy_)
    return;
  const bool opening = operation == 0;
  const auto extension = operation == 2 ? "wav" : "json";
  const auto fileName =
      operation == 2
          ? "Krava_" + juce::String(static_cast<juce::int64>(current().seed)) +
                ".wav"
          : "KRAVA-session.json";
  chooser_ = std::make_unique<juce::FileChooser>(
      opening          ? "OPEN SESSION / CLASSIC PRESET"
      : operation == 1 ? "SAVE SESSION + GENETIC HISTORY"
                       : "EXPORT MIX / WAV 24 BIT",
      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
          .getChildFile(fileName),
      "*." + juce::String(extension));
  const juce::Component::SafePointer<GeneticLab> safe(this);
  chooser_->launchAsync(
      (opening ? juce::FileBrowserComponent::openMode
               : juce::FileBrowserComponent::saveMode) |
          juce::FileBrowserComponent::canSelectFiles |
          (opening ? 0 : juce::FileBrowserComponent::warnAboutOverwriting),
      [safe, operation,
       extension = juce::String(extension)](const juce::FileChooser &chooser) {
        if (!safe)
          return;
        const auto file = chooser.getResult();
        safe->chooser_.reset();
        if (file == juce::File())
          return;
        try {
          if (!file.hasFileExtension(extension))
            throw std::invalid_argument("Choisis un fichier terminé par ." +
                                        extension.toStdString());
          if (operation == 0)
            safe->loadFrom(file);
          else if (operation == 1)
            safe->saveTo(file);
          else if (safe->session_.sidecar &&
                   file.withFileExtension(".krava.json").existsAsFile()) {
            juce::AlertWindow::showAsync(
                juce::MessageBoxOptions()
                    .withTitle("Remplacer les paramètres associés ?")
                    .withMessage(
                        file.withFileExtension(".krava.json").getFullPathName())
                    .withButton("Remplacer")
                    .withButton("Annuler"),
                [safe, file](int result) {
                  if (result == 1 && safe) {
                    try {
                      safe->exportTo(file);
                    } catch (const std::exception &e) {
                      safe->status(e.what(), true);
                    }
                  }
                });
          } else
            safe->exportTo(file);
        } catch (const std::exception &e) {
          safe->status(e.what(), true);
        }
      });
}
void GeneticLab::paint(juce::Graphics &g) {
  g.setGradientFill(juce::ColourGradient(
      juce::Colour(0xff1511ba), 0, 0, krui::blue,
      static_cast<float>(getWidth()), static_cast<float>(getHeight()), false));
  g.fillAll();
  krui::grid(g, getLocalBounds(), 24);
  krui::frame(g, browserBounds_, "STYLE BROWSER / GENERATION SETTINGS");
  krui::frame(g, presetBounds_, "PRESET / GENETIC IDENTITY");
  krui::frame(g, monitorBounds_, "MONITOR / A-B MIX");
  krui::frame(g, labBounds_, "GENETIC LAB / GENERATION LIBRARY");
  krui::frame(g, previewBounds_, "SEQUENCER / PREVIEW - EXPORT SETTINGS");
  const std::array<const char *, 6> titles{
      "OSC / PITCH",     "ATTACK",    "BODY / HARMONICS",
      "TAIL / MUTATION", "FX / BODY", "SUB / FOUNDATION"};
  for (std::size_t i = 0; i < 6; ++i)
    krui::frame(g, modules_[i], titles[i]);
  g.setFont(krui::mono(9.5f));
  g.setColour(krui::muted);
  g.drawText("BODY DNA / drag pitch points",
             modules_[0].reduced(9).withTrimmedTop(25).withHeight(21),
             juce::Justification::centredLeft);
  g.drawText("BODY DNA / 4x nonlinear DSP",
             modules_[4].reduced(9).withTrimmedTop(25).withHeight(21),
             juce::Justification::centredLeft);
  const auto plotted = cache_.find(current().id);
  if (session_.selected >= 0 && spectrumBounds_.getHeight() > 10 &&
      plotted != cache_.end()) {
    const auto &f = plotted->second.features;
    g.setColour(krui::dark);
    g.fillRect(spectrumBounds_);
    krui::grid(g, spectrumBounds_, 20);
    auto plot = spectrumBounds_.toFloat().reduced(6, 15);
    juce::Path p;
    for (std::size_t i = 0; i < 64; ++i) {
      const auto x = plot.getX() + static_cast<float>(i) * plot.getWidth() / 63,
                 y = plot.getBottom() -
                     static_cast<float>(std::clamp(
                         (f.basic.spectrumDb[i] + 90) / 90, 0.0, 1.0)) *
                         plot.getHeight();
      if (i == 0)
        p.startNewSubPath(x, y);
      else
        p.lineTo(x, y);
    }
    g.setColour(krui::pink);
    g.strokePath(p, juce::PathStrokeType(1));
    g.setColour(krui::muted);
    g.setFont(krui::mono(9));
    g.drawText("SPECTRUM / FIRST HANN WINDOW / dBFS",
               spectrumBounds_.withHeight(13).reduced(5, 0),
               juce::Justification::centredLeft);
    g.drawText("20 Hz                 18 kHz",
               spectrumBounds_.withY(spectrumBounds_.getBottom() - 13)
                   .withHeight(13)
                   .reduced(5, 0),
               juce::Justification::centredLeft);
  }
}
void GeneticLab::resized() {
  auto b = getLocalBounds().reduced(16);
  auto header = b.removeFromTop(100);
  brand_.setBounds(header.removeFromLeft(300));
  header.removeFromLeft(12);
  auto actions = header.removeFromRight(324);
  actions.removeFromRight(116);
  play_.setBounds(actions.removeFromLeft(96));
  actions.removeFromLeft(8);
  export_.setBounds(actions);
  header.removeFromRight(12);
  presetBounds_ = header;
  auto preset = header.reduced(12, 6).withTrimmedTop(26);
  selectedName_.setBounds(preset.removeFromTop(22));
  selectedInfo_.setBounds(preset.removeFromTop(14));
  auto row = preset.removeFromBottom(27);
  open_.setBounds(row.removeFromLeft(60));
  row.removeFromLeft(6);
  save_.setBounds(row.removeFromLeft(60));
  row.removeFromLeft(6);
  keep_.setBounds(row.removeFromLeft(90));

  b.removeFromTop(12);
  browserBounds_ = b.removeFromTop(82);
  b.removeFromTop(12);
  auto browser = browserBounds_.reduced(10).withTrimmedTop(21);
  const auto place = [&](juce::Label &label, juce::Component &control,
                         int width) {
    auto r = browser.removeFromLeft(width);
    label.setBounds(r.removeFromTop(15));
    control.setBounds(r.removeFromTop(28));
    browser.removeFromLeft(8);
  };
  place(genreLabel_, genre_, 166);
  place(subgenreLabel_, subgenre_, 225);
  mode_.setBounds(subgenre_.getBounds());
  place(characterLabel_, character_, 140);
  place(architectureLabel_, architecture_, 205);
  place(noteLabel_, note_, 130);
  details_.setBounds(
      browser.removeFromRight(112).withTrimmedTop(15).withHeight(28));
  status_.setBounds(b.removeFromBottom(20));
  b.removeFromBottom(3);
  previewBounds_ = b.removeFromBottom(64);
  b.removeFromBottom(12);
  labBounds_ = b.removeFromBottom(300);
  b.removeFromBottom(12);
  auto modules = b.removeFromBottom(258);
  b.removeFromBottom(12);
  const auto width = (modules.getWidth() - 50) / 6;
  for (std::size_t i = 0; i < 6; ++i) {
    modules_[i] = i < 5 ? modules.removeFromLeft(width) : modules;
    if (i < 5)
      modules.removeFromLeft(10);
    auto box = modules_[i].reduced(6).withTrimmedTop(26);
    auto top = box.removeFromTop(24);
    if (i == 1) {
      locks_[1].setBounds(top.removeFromLeft(top.getWidth() / 2));
      attackKind_.setBounds(top);
    }
    if (i == 2) {
      locks_[2].setBounds(top.removeFromLeft(top.getWidth() / 2));
      bodyKind_.setBounds(top);
    }
    if (i == 3) {
      locks_[3].setBounds(top.removeFromLeft(top.getWidth() / 2));
      tailKind_.setBounds(top);
    }
    if (i == 5)
      locks_[0].setBounds(top);
    const auto w = box.getWidth() / 2, h = box.getHeight() / 2;
    for (int j = 0; j < 4; ++j)
      controls_[i * 4 + static_cast<std::size_t>(j)].setBounds(
          box.getX() + (j % 2) * w, box.getY() + (j / 2) * h, w, h);
  }
  monitorBounds_ = b.removeFromRight(282);
  b.removeFromRight(12);
  graph_.setBounds(b);
  auto monitor = monitorBounds_.reduced(9).withTrimmedTop(25);
  auto ar = monitor.removeFromTop(26);
  playA_.setBounds(ar.removeFromRight(42));
  ar.removeFromRight(5);
  aBox_.setBounds(ar);
  monitor.removeFromTop(3);
  auto br = monitor.removeFromTop(26);
  playB_.setBounds(br.removeFromRight(42));
  br.removeFromRight(5);
  bBox_.setBounds(br);
  monitor.removeFromTop(3);
  auto ab = monitor.removeFromTop(25);
  setA_.setBounds(ab.removeFromLeft(73));
  ab.removeFromLeft(5);
  setB_.setBounds(ab.removeFromLeft(73));
  ab.removeFromLeft(5);
  compare_.setBounds(ab);
  compareMetrics_.setBounds(monitor.removeFromTop(29));
  metrics_.setBounds(monitor.removeFromTop(63));
  overlap_.setBounds(monitor.removeFromTop(30));
  monitor.removeFromTop(3);
  spectrumBounds_ = monitor;
  auto lab = labBounds_.reduced(10).withTrimmedTop(24);
  auto config = lab.removeFromTop(31);
  modeLabel_.setVisible(false);
  seedLabel_.setBounds(config.removeFromLeft(38));
  seed_.setBounds(config.removeFromLeft(120));
  config.removeFromLeft(12);
  countLabel_.setBounds(config.removeFromLeft(47));
  count_.setBounds(config.removeFromLeft(62));
  config.removeFromLeft(12);
  noveltyLabel_.setBounds(config.removeFromLeft(74));
  novelty_.setBounds(config.removeFromLeft(225));
  config.removeFromLeft(10);
  refine_.setBounds(config.removeFromLeft(110));
  lab.removeFromTop(4);
  auto buttons = lab.removeFromTop(31);
  for (auto *button :
       {&generate_, &regenerate_, &evolve_, &cancel_, &history_}) {
    const auto w = button == &regenerate_ ? 126 : button == &cancel_ ? 76 : 108;
    button->setBounds(buttons.removeFromLeft(w));
    buttons.removeFromLeft(7);
  }
  batchInfo_.setBounds(buttons);
  lab.removeFromTop(6);
  cardViewport_.setBounds(lab);
  cards_.layout(std::max(100, cardViewport_.getMaximumVisibleWidth()));
  auto preview = previewBounds_.reduced(10).withTrimmedTop(23);
  loop_.setBounds(preview.removeFromLeft(64));
  preview.removeFromLeft(7);
  tempoLabel_.setBounds(preview.removeFromLeft(48));
  bpm_.setBounds(preview.removeFromLeft(180));
  preview.removeFromLeft(10);
  scopeLabel_.setBounds(preview.removeFromLeft(52));
  scope_.setBounds(preview.removeFromLeft(100));
  preview.removeFromLeft(10);
  rateLabel_.setBounds(preview.removeFromLeft(53));
  rate_.setBounds(preview.removeFromLeft(90));
  preview.removeFromLeft(10);
  match_.setBounds(preview.removeFromLeft(125));
  sidecar_.setBounds(preview.removeFromLeft(130));
  audio_.setBounds(preview.removeFromLeft(75));
}
void GeneticLab::runSmokeTest(
    const juce::File &directory,
    std::function<void(bool, juce::String)> completion) {
  smokeDirectory_ = directory;
  smokeCompletion_ = std::move(completion);
  smokeStage_ = 1;
  // Configure the batch without auditioning intermediate browser choices.
  // Real dropdown actions are exercised separately by smokeStyleChange().
  session_.settings.universal.reset();
  session_.settings.mode = Mode::experimental;
  sync();
  novelty_.setValue(75, juce::sendNotificationSync);
  count_.setSelectedId(20, juce::sendNotificationSync);
  seed_.setText("778899", false);
  generate_.onClick();
}
void GeneticLab::smokeAdvance() {
  try {
    if (smokeStage_ == 1) {
      require(session_.visible.size() == 20,
              "GUI did not generate 20 candidates");
      std::set<std::string> ids;
      for (auto index : session_.visible)
        ids.insert(session_.library.at(static_cast<std::size_t>(index)).id);
      require(ids.size() == 20, "GUI population IDs are duplicated");
      std::set<std::string> audioHashes;
      for (auto index : session_.visible) {
        const auto &audio =
            cached(session_.library.at(static_cast<std::size_t>(index)))
                .audio.mix;
        audioHashes.insert(
            sha256(std::string(reinterpret_cast<const char *>(audio.data()),
                               audio.size() * sizeof(float))));
      }
      require(audioHashes.size() == 20,
              "GUI generation produced exact duplicate audio");
      select(session_.visible[7]);
      smokeParent_ = current();
      smokeParentIndex_ = session_.selected;
      locks_[0].setToggleState(true, juce::dontSendNotification);
      locks_[1].setToggleState(true, juce::dontSendNotification);
      session_.settings.locks = {true, true, false, false};
      count_.setSelectedId(8, juce::sendNotificationSync);
      seed_.setText("990011", false);
      smokeStage_ = 2;
      evolve_.onClick();
      return;
    }
    if (smokeStage_ == 2) {
      require(session_.visible.size() == 8, "GUI did not evolve 8 children");
      require(session_.library.at(
                  static_cast<std::size_t>(smokeParentIndex_)) == smokeParent_,
              "GUI overwrote parent");
      const auto parent = render(smokeParent_, session_.settings.sampleRate);
      for (auto index : session_.visible) {
        const auto &g = session_.library.at(static_cast<std::size_t>(index));
        const auto child = render(g, session_.settings.sampleRate);
        require(child.layers[0] == parent.layers[0] &&
                    child.layers[1] == parent.layers[1],
                "GUI DNA changed SUB/ATTACK audio");
        require(g.parents == std::vector<std::string>{smokeParent_.id},
                "GUI lost genealogy");
      }
      const auto before = cached(current()).audio.mix;
      controls_[drive].slider.setValue(current().body.drive < 15 ? 25 : 5,
                                       juce::sendNotificationSync);
      refreshCurrent();
      require(cached(current()).audio.mix != before,
              "GUI drive does not alter audio");
      keep_.onClick();
      const auto original = current();
      graph_.setAmplitudeMode(false);
      graph_.dragHandle(1, {10, -10});
      refreshCurrent();
      require(current().body.pivotHz != original.body.pivotHz,
              "Intermediate pitch editor is disconnected");
      graph_.setAmplitudeMode(true);
      graph_.dragHandle(1, {0, -12});
      refreshCurrent();
      require(current().body.ampCurve != original.body.ampCurve,
              "Amplitude editor is disconnected");
      keep_.onClick();
      session_.compareA = smokeParentIndex_;
      session_.compareB = session_.selected;
      match_.setToggleState(true, juce::dontSendNotification);
      session_.matchLevel = true;
      scope_.setSelectedId(1, juce::dontSendNotification);
      sync();
      playA_.onClick();
      const auto ar = kravatek::comparisonRms(
          lastAudition_, deviceRate ? deviceRate() : 48000);
      playB_.onClick();
      const auto br = kravatek::comparisonRms(
          lastAudition_, deviceRate ? deviceRate() : 48000);
      require(std::abs(ar - br) < 1e-6, "GUI A/B RMS matching failed");
      match_.setToggleState(false, juce::dontSendNotification);
      match_.onClick();
      playB_.onClick();
      require(lastAuditionGain_ == 1, "GUI real level mode still adjusts gain");
      scope_.setSelectedId(2, juce::sendNotificationSync);
      playA_.onClick();
      const auto subA = lastAudition_;
      playB_.onClick();
      require(lastAudition_ == subA, "GUI SUB solo is not preserved");
      scope_.setSelectedId(1, juce::sendNotificationSync);
      stop();
      saveTo(smokeDirectory_.getChildFile("genetic-session.json"));
      smokeSaved_ = encodeSession(session_);
      smokeStage_ = 3;
      loadFrom(smokeDirectory_.getChildFile("genetic-session.json"));
      return;
    }
    if (smokeStage_ == 3) {
      require(encodeSession(session_) == smokeSaved_,
              "GUI session reload changed state");
      const auto restored = decodeSession(smokeSaved_);
      require(cached(current()).audio.mix ==
                  render(restored.library.at(
                             static_cast<std::size_t>(restored.selected)),
                         session_.settings.sampleRate, false)
                      .mix,
              "GUI session reload changed audio");
      const auto previousLocks = session_.settings.locks;
      session_.settings.locks.body = !session_.settings.locks.body;
      auto exported = current();
      exported.locks = session_.settings.locks;
      exportTo(smokeDirectory_.getChildFile("gui-smoke-003.wav"));
      const auto side =
          decodeSession(smokeDirectory_.getChildFile("gui-smoke-003.krava.json")
                            .loadFileAsString()
                            .toStdString());
      require(side.library.at(0) == exported &&
                  side.settings.locks == session_.settings.locks,
              "GUI associated parameters differ from exported sound");
      session_.settings.locks = previousLocks;
      smokeProfile_ = 0;
      smokeStage_ = 4;
      smokeUniversal();
      return;
    }
    if (smokeStage_ == 4) {
      require(session_.visible.size() == 4 &&
                  current().engine == universalVersion,
              "Universal browser failed to generate a lot");
      require(current().profileId ==
                  profiles_.at(static_cast<std::size_t>(smokeProfile_)).id,
              "Browser profile ignored");
      if (smokeProfile_ == 6) {
        require(std::abs(current().sub.endHz - 55) < 1e-8,
                "Browser note tuning inactive");
        require(std::abs(controls_[subLength].slider.getValue() -
                         current().sub.duration * 1000) < .51,
                "808 duration display clamped");
        require(overlap_.getText().contains("TAIL > BEAT"),
                "Long tail warning missing");
      }
      if (++smokeProfile_ < static_cast<int>(profiles_.size())) {
        smokeUniversal();
        return;
      }
      const auto before = cached(current()).audio.mix;
      auto &popup = openDetails();
      require(popup.isVisible(), "Advanced popup did not open");
      popup.dismiss();
      UniversalControls panel(current(),
                              [this](auto fn) { modify(std::move(fn)); });
      panel.setValue(2, 18);
      refreshCurrent();
      require(cached(current()).audio.mix != before,
              "Advanced distortion control inactive");
      const auto oldSub = render(current()).layers[0];
      panel.setPage(2);
      panel.setValue(2, 123);
      refreshCurrent();
      require(render(current()).layers[0] != oldSub,
              "Advanced sub phase inactive");
      commitEdit();
      smokeParent_ = current();
      smokeParentIndex_ = session_.selected;
      session_.settings.locks = {true, true, false, false};
      count_.setSelectedId(8, juce::sendNotificationSync);
      seed_.setText("445566", false);
      smokeStage_ = 5;
      evolve_.onClick();
      return;
    }
    if (smokeStage_ == 5) {
      require(session_.visible.size() == 8, "Universal EVOLVE count wrong");
      require(session_.library.at(
                  static_cast<std::size_t>(smokeParentIndex_)) == smokeParent_,
              "Universal GUI overwrote parent");
      const auto before = render(smokeParent_);
      for (auto idx : session_.visible) {
        const auto child =
            render(session_.library.at(static_cast<std::size_t>(idx)));
        require(child.layers[0] == before.layers[0] &&
                    child.layers[1] == before.layers[1],
                "Universal GUI EVOLVE changed locked layers");
      }
      session_.compareA = smokeParentIndex_;
      session_.compareB = session_.selected;
      session_.matchLevel = true;
      sync();
      activeA_ = true;
      audition(session_.compareA, true);
      const auto a = lastAudition_;
      activeA_ = false;
      audition(session_.compareB, true);
      const auto rate = deviceRate ? deviceRate() : 48000;
      require(std::abs(kravatek::comparisonRms(a, rate) -
                       kravatek::comparisonRms(lastAudition_, rate)) < 1e-6,
              "Universal A/B mismatch");
      stop();
      saveTo(smokeDirectory_.getChildFile("universal-session.json"));
      smokeSaved_ = encodeSession(session_);
      smokeStage_ = 6;
      loadFrom(smokeDirectory_.getChildFile("universal-session.json"));
      return;
    }
    if (smokeStage_ == 6) {
      require(encodeSession(session_) == smokeSaved_,
              "Browser recipe/session changed on reload");
      exportTo(smokeDirectory_.getChildFile("gui-smoke-004.wav"));
      const auto side =
          decodeSession(smokeDirectory_.getChildFile("gui-smoke-004.krava.json")
                            .loadFileAsString()
                            .toStdString());
      require(render(side.library.at(0)).mix == render(current()).mix,
              "Universal GUI export metadata changed audio");
      smokeStyleStep_ = 0;
      smokeStage_ = 8;
      smokeStyleChange();
      return;
    }
    if (smokeStage_ == 8) {
      smokeStyleCheck();
      return;
    }
    if (smokeStage_ == 7) {
      require(session_.visible.size() == session_.library.entries().size(),
              "Universal HISTORY lost records");

      auto *outer = findParentComponentOfClass<juce::Viewport>();
      require(outer != nullptr, "Missing outer viewport");
      const auto previous = outer->getBounds();
      outer->setSize(1100, 708);
      outer->setViewPosition(0, getHeight());
      require(outer->getViewPositionY() > 0,
              "Genetic Lab not accessible on compact display");
      outer->setBounds(previous);
      outer->setViewPosition(0, 0);
      graph_.setAmplitudeMode(false);
      juce::DynamicObject::Ptr report = new juce::DynamicObject;
      report->setProperty("result", "passed");
      report->setProperty("theme", "krava-universal-004");
      report->setProperty("product", "KRAVA-KICK LAB");
      report->setProperty(
          "applicationVersion",
          juce::JUCEApplication::getInstance()->getApplicationVersion());
      report->setProperty("engine", universalVersion);
      report->setProperty("universalProfiles", smokeProfile_);
      report->setProperty("universalCandidates", smokeProfile_ * 4);
      report->setProperty("styleDropdownActions", 18);
      report->setProperty("styleSelectionAuditions", 17);
      report->setProperty("styleSelectionChangesAudio", true);
      report->setProperty("styleOverridesReset", true);
      report->setProperty("stylePreviewPreservesLocksAndHistory", true);
      report->setProperty("stylePreviewRespectsStopAndOneShot", true);
      report->setProperty("playingBufferFollowsSelectionAndGeneration", true);
      report->setProperty("long808AndTuning", true);
      report->setProperty("universalEvolvedChildren", 8);
      report->setProperty("universalLockedLayersExact", true);
      report->setProperty("persistentHistory", true);
      report->setProperty("advancedDistortionAndPhase", true);
      report->setProperty("legacy003Verified", true);
      report->setProperty("generatedCandidates", 20);
      report->setProperty("evolvedChildren", 8);
      report->setProperty("exactLockedSubAndAttack", true);
      report->setProperty("immutableParent", true);
      report->setProperty("backgroundGeneration", true);
      report->setProperty("sessionRoundTrip", true);
      report->setProperty("wavSidecar", true);
      report->setProperty("editableIntermediatePitch", true);
      report->setProperty("editableAmplitude", true);
      report->setProperty("matchedAB", true);
      report->setProperty("realLevelAB", true);
      report->setProperty("layerSolo", true);
      report->setProperty("compactViewport", true);
      report->setProperty("librarySize",
                          static_cast<int>(session_.library.entries().size()));
      report->setProperty("physicalListeningVerified", false);
      require(
          smokeDirectory_.getChildFile("gui-smoke.json")
              .replaceWithText(juce::JSON::toString(juce::var(report.get()))),
          "Cannot save GUI report");
      smokeDone(true);
    }
  } catch (const std::exception &e) {
    smokeDone(false, e.what());
  }
}
void GeneticLab::smokeDone(bool ok, juce::String error) {
  smokeStage_ = 0;
  stop();
  auto done = std::move(smokeCompletion_);
  if (done)
    done(ok, error);
}

void GeneticLab::syncBrowser() {
  families_ = {"GENETIC 0.3"};
  for (const auto &p : profiles_)
    if (std::find(families_.begin(), families_.end(), p.family) ==
        families_.end())
      families_.push_back(p.family);
  if (session_.settings.universal &&
      std::find(families_.begin(), families_.end(),
                session_.settings.universal->profile.family) == families_.end())
    families_.push_back(session_.settings.universal->profile.family);
  genre_.clear(juce::dontSendNotification);
  for (std::size_t i = 0; i < families_.size(); ++i)
    genre_.addItem(families_[i], static_cast<int>(i + 1));
  const bool universal = session_.settings.universal.has_value();
  mode_.setVisible(!universal);
  subgenre_.setVisible(universal);
  for (auto *c : {&character_, &architecture_, &note_})
    c->setEnabled(universal && !busy_);
  if (!universal) {
    genre_.setSelectedId(1, juce::dontSendNotification);
    return;
  }
  const auto &r = *session_.settings.universal;
  genre_.setSelectedId(
      static_cast<int>(
          std::find(families_.begin(), families_.end(), r.profile.family) -
          families_.begin() + 1),
      juce::dontSendNotification);
  browserProfiles_.clear();
  subgenre_.clear(juce::dontSendNotification);
  int selected = 0;
  for (const auto &p : profiles_)
    if (p.family == r.profile.family) {
      browserProfiles_.push_back(p);
      const auto id = static_cast<int>(browserProfiles_.size());
      subgenre_.addItem(p.name, id);
      if (p.id == r.profile.id && p.digest == r.profile.digest)
        selected = id;
    }
  if (!selected) {
    browserProfiles_.push_back(r.profile);
    selected = static_cast<int>(browserProfiles_.size());
    subgenre_.addItem(r.profile.name + " / SAVED", selected);
  }
  subgenre_.setSelectedId(selected, juce::dontSendNotification);
  character_.setSelectedId(static_cast<int>(r.character) + 1,
                           juce::dontSendNotification);
  architecture_.setSelectedId(r.overrideArchitecture == Architecture::count
                                  ? 1
                                  : static_cast<int>(r.overrideArchitecture) +
                                        2,
                              juce::dontSendNotification);
  note_.setSelectedId(r.note == 0 ? 1 : r.note + 2, juce::dontSendNotification);
}
void GeneticLab::chooseGenre() {
  if (busy_)
    return;
  const auto at = genre_.getSelectedId() - 1;
  if (at < 0 || at >= static_cast<int>(families_.size()))
    return;
  if (at == 0)
    session_.settings.universal.reset();
  else
    for (const auto &p : profiles_)
      if (p.family == families_[static_cast<std::size_t>(at)]) {
        // A new family starts from its own recipe. Old forced architecture,
        // character or tuning must not silently override the chosen style.
        session_.settings.universal = UniversalRequest{p};
        break;
      }
  previewStyle();
}
void GeneticLab::previewStyle() {
  dirty_ = true;
  syncBrowser();
  startBatch(false, false, true);
}
juce::CallOutBox &GeneticLab::openDetails() {
  const juce::Component::SafePointer<GeneticLab> safe(this);
  auto panel = std::make_unique<UniversalControls>(current(), [safe](auto fn) {
    if (safe)
      safe->modify(std::move(fn));
  });
  return juce::CallOutBox::launchAsynchronously(
      std::move(panel), getLocalArea(&details_, details_.getLocalBounds()),
      this);
}
void GeneticLab::updateOverlap() {
  if (session_.selected < 0)
    return;
  const auto &g = current();
  const auto found = cache_.find(g.id);
  const auto seconds = found == cache_.end()
                           ? std::max({g.sub.duration, g.attack.duration,
                                       g.body.duration, g.tail.duration})
                           : found->second.features.effectiveSeconds;
  const auto beat = 60 / session_.bpm;
  const bool exceeds = seconds > beat;
  overlap_.setColour(juce::Label::textColourId,
                     exceeds ? krui::pink : krui::muted);
  overlap_.setFont(krui::mono(9));
  overlap_.setText(
      exceeds
          ? juce::String("TAIL > BEAT / ") +
                juce::String(static_cast<int>(std::lround(seconds * 1000))) +
                " > " +
                juce::String(static_cast<int>(std::lround(beat * 1000))) +
                " ms\n" +
                (session_.loop ? "LOOP trims tail; OFF = full one-shot"
                               : "ONE-SHOT / full tail")
          : (session_.settings.universal
                 ? "PROFILE BPM " +
                       juce::String(static_cast<int>(
                           session_.settings.universal->profile.bpmLow)) +
                       " - " +
                       juce::String(static_cast<int>(
                           session_.settings.universal->profile.bpmHigh)) +
                       " / guide"
                 : "GSE 0.3 / original generation"),
      juce::dontSendNotification);
}
void GeneticLab::smokeUniversal() {
  const auto &p = profiles_.at(static_cast<std::size_t>(smokeProfile_));
  const auto family = std::find(families_.begin(), families_.end(), p.family);
  require(family != families_.end(), "Profile family missing from browser");
  session_.settings.universal = UniversalRequest{p};
  session_.settings.universal->note = smokeProfile_ == 6 ? 33 : 0;
  syncBrowser();
  novelty_.setValue(75, juce::sendNotificationSync);
  count_.setSelectedId(4, juce::sendNotificationSync);
  seed_.setText(juce::String(44000 + smokeProfile_), false);
  generate_.onClick();
}

void GeneticLab::smokeStyleChange() {
  session_.settings.locks = {};
  session_.loop = smokeStyleStep_ != 15;
  if (smokeStyleStep_ == 0) {
    // Simulate previously forced settings and a layer/A-B audition. A new
    // family must start from its own defaults and audition the full mix.
    session_.settings.universal->character = Character::warm;
    session_.settings.universal->overrideArchitecture = Architecture::tonal;
    session_.settings.universal->note = 48;
    activeA_ = true;
    audition(session_.compareA, true);
  }
  if (smokeStyleStep_ == 11) {
    // A session can contain an older snapshot beside the installed profile.
    // Choosing the installed entry must render it, just like a family change.
    session_.settings.universal = UniversalRequest{profiles_.front()};
    session_.settings.universal->profile.digest = "older-profile-snapshot";
  }
  if (smokeStyleStep_ == 12)
    session_.settings.locks = {true, true, false, false};
  if (smokeStyleStep_ == 13)
    session_.settings.locks = {true, true, true, true};
  sync();
  count_.setSelectedId(20, juce::sendNotificationSync);
  // Keep the same seed across the eight families: the style itself, rather
  // than a different random draw, must change the rendered and queued audio.
  seed_.setText(juce::String(78000 + std::max(0, smokeStyleStep_ - 7)), false);
  smokeParent_ = current();
  smokeParentIndex_ = session_.selected;
  smokeStyleLibrarySize_ = session_.library.entries().size();
  smokeStyleAuditions_ = acceptedAuditions_;
  const auto rate = deviceRate ? deviceRate() : 48000;
  smokeStyleAudio_ = render(current(), rate, true).mix;
  if (smokeStyleStep_ == 0)
    scope_.setSelectedId(2, juce::dontSendNotification);
  auto chooseFamily = [this](std::size_t profile) {
    const auto family = std::find(families_.begin(), families_.end(),
                                  profiles_.at(profile).family);
    require(family != families_.end(), "Style test family missing");
    genre_.setSelectedId(static_cast<int>(family - families_.begin() + 1),
                         juce::sendNotificationSync);
  };
  if (smokeStyleStep_ < 8)
    chooseFamily(static_cast<std::size_t>(smokeStyleStep_));
  else {
    switch (smokeStyleStep_) {
    case 8:
      character_.setSelectedId(static_cast<int>(Character::clean) + 1,
                               juce::sendNotificationSync);
      break;
    case 9:
      architecture_.setSelectedId(static_cast<int>(Architecture::acoustic) + 2,
                                  juce::sendNotificationSync);
      break;
    case 10:
      note_.setSelectedId(48 + 2, juce::sendNotificationSync);
      break;
    case 11:
      subgenre_.setSelectedId(1, juce::sendNotificationSync);
      break;
    case 12:
      chooseFamily(5); // House -> Hardcore, preserving SUB and ATTACK.
      break;
    case 13:
      chooseFamily(2); // All four locks: identical isolated audio is intended.
      break;
    case 14:
      chooseFamily(6);
      stop(); // STOP must suppress an audition still waiting for rendering.
      break;
    case 15:
      chooseFamily(7); // Audition also works when LOOP is off.
      break;
    case 16:
      genre_.setSelectedId(1, juce::sendNotificationSync);
      break;
    case 17:
      mode_.setSelectedId(static_cast<int>(Mode::deep) + 1,
                          juce::sendNotificationSync);
      break;
    case 18:
      count_.setSelectedId(4, juce::sendNotificationSync);
      generate_.onClick(); // Playing loop must follow a new generated batch.
      break;
    default:
      throw std::runtime_error("Unknown style test step");
    }
  }
  require(busy_, "Selecting a style did not start a render");
}

void GeneticLab::smokeStyleCheck() {
  const bool batch = smokeStyleStep_ == 18;
  const auto expected = batch ? 4u : 1u;
  require(session_.visible.size() == expected,
          "Style preview generated COUNT candidates instead of one");
  require(session_.library.entries().size() ==
              smokeStyleLibrarySize_ + expected,
          "Style preview lost history or created extra candidates");
  require(session_.library.at(static_cast<std::size_t>(smokeParentIndex_)) ==
              smokeParent_,
          "Changing style overwrote the previous sound");
  require(count_.getSelectedId() == (batch ? 4 : 20) &&
              session_.settings.count == (batch ? 4 : 20),
          "Style preview changed the user's batch count");
  require(scope_.getSelectedId() == 1, "Style preview left layer solo enabled");
  const auto audio = listeningSamples(current());
  if (smokeStyleStep_ != 13)
    require(audio != smokeStyleAudio_, "Style selection kept the old audio");
  else {
    const auto before = render(smokeParent_, 48000, true);
    require(render(current(), 48000, true).layers == before.layers,
            "All-locked style preview changed an isolated layer");
    require(status_.getText().contains("4 DNA LOCK"),
            "All-locked preview does not explain why the sound is unchanged");
  }
  if (smokeStyleStep_ == 12) {
    const auto before = render(smokeParent_, 48000, true);
    const auto after = render(current(), 48000, true);
    require(after.layers[0] == before.layers[0] &&
                after.layers[1] == before.layers[1],
            "Changing style ignored DNA SUB/ATTACK locks");
    require(current().parents == std::vector<std::string>{smokeParent_.id},
            "Locked style preview lost its parent");
  }
  if (smokeStyleStep_ == 14)
    require(!playing_ && acceptedAuditions_ == smokeStyleAuditions_,
            "Pending style preview restarted after STOP");
  else {
    require(acceptedAuditions_ == smokeStyleAuditions_ + 1,
            "Style action did not queue exactly one actual audition");
    require(lastAudition_ == audio && listeningIndex_ == -1 &&
                !listeningComparison_ && lastAuditionGain_ == 1,
            "Playback buffer does not match the selected style's real mix");
    require(playing_ == session_.loop,
            "Style audition did not respect LOOP / one-shot");
  }
  if (smokeStyleStep_ < 8) {
    require(current().profileId ==
                profiles_.at(static_cast<std::size_t>(smokeStyleStep_)).id,
            "Genre selection rendered the wrong profile");
    const auto &request = *session_.settings.universal;
    require(request.character == Character::neutral &&
                request.overrideArchitecture == Architecture::count &&
                request.note == 0,
            "Old browser overrides masked the new family's recipe");
  }
  if (smokeStyleStep_ == 10)
    require(std::abs(current().sub.endHz - 130.8127826503) < 1e-6,
            "Selecting NOTE did not retune the rendered kick");
  if (smokeStyleStep_ >= 16)
    require(current().engine == engineVersion,
            "Legacy mode selection no longer renders GSE 0.3 audio");
  if (++smokeStyleStep_ <= 18) {
    smokeStyleChange();
    return;
  }
  // Single-clicking a library card during a loop must update the audio too.
  cards_.onSelect(smokeParentIndex_, false);
  require(playing_ && acceptedAuditions_ == smokeStyleAuditions_ + 2 &&
              lastAudition_ == listeningSamples(current()),
          "Playing loop did not follow a single-click library selection");
  stop();
  smokeStage_ = 7;
  history_.onClick();
}
