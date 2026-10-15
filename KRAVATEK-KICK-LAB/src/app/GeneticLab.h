#pragma once
#include "GeneticGraph.h"
#include "GenomeCards.h"
#include "ParameterControl.h"
#include "genetic/SessionIO.h"
#include <map>
#include <optional>
#include <thread>
class GeneticLab final : public juce::Component, private juce::Timer {
public:
  GeneticLab();
  ~GeneticLab() override;
  std::function<double()> deviceRate;
  std::function<bool(std::span<const float>, bool, double)> queueAudio;
  std::function<void()> stopAudio, chooseAudio;
  std::function<void(const juce::File &)> openLegacy;
  void paint(juce::Graphics &) override;
  void resized() override;
  bool keyPressed(const juce::KeyPress &) override;
  void togglePlay();
  void stop();
  void runSmokeTest(const juce::File &,
                    std::function<void(bool, juce::String)>);

private:
  enum Control {
    pitchStart,
    pitchEnd,
    pitchDrop,
    pitchCurve,
    attackLevel,
    attackDecay,
    attackHz,
    attackColour,
    bodyLength,
    bodyLevel,
    harmonics,
    fm,
    tailLevel,
    tailLength,
    tailDecay,
    tailMotion,
    drive,
    fold,
    cutoff,
    resonance,
    subHz,
    subLevel,
    subDecay,
    subLength,
    controlCount
  };
  enum class Job { create, evolve, refresh };
  kravatek::genetic::Session session_;
  std::optional<kravatek::genetic::Genome> edit_;
  std::map<std::string, kravatek::genetic::Candidate> cache_;
  std::map<std::string, kravatek::genetic::Render> auditionCache_;
  std::jthread worker_;
  std::atomic<int> progress_{0}, total_{0};
  int listeningIndex_ = -1;
  bool listeningComparison_ = false;
  bool busy_ = false, manualPending_ = false, playing_ = false, dirty_ = false,
       activeA_ = false;
  double lastAuditionGain_ = 1;
  std::vector<float> lastAudition_;
  krui::Brand brand_;
  GeneticGraph graph_;
  GenomeCards cards_;
  juce::Viewport cardViewport_;
  std::array<ParameterControl, controlCount> controls_;
  juce::ComboBox mode_, count_, bodyKind_, attackKind_, tailKind_, scope_,
      rate_, aBox_, bBox_;
  juce::TextEditor seed_;
  juce::TextButton play_{"PLAY"}, export_{"EXPORT WAV"}, save_{"SAVE"},
      open_{"OPEN"}, keep_{"KEEP EDIT"};
  juce::TextButton generate_{"GENERATE"}, regenerate_{"REGENERATE"},
      evolve_{"EVOLVE"}, cancel_{"CANCEL"}, history_{"HISTORY"};
  juce::TextButton setA_{"SET A"}, setB_{"SET B"}, playA_{"A >"}, playB_{"B >"},
      compare_{"A / B"}, audio_{"AUDIO"}, loop_{"LOOP"};
  std::array<juce::ToggleButton, 4> locks_;
  juce::ToggleButton refine_{"REFINE"}, match_{"MATCH RMS"},
      sidecar_{"WAV + JSON"};
  juce::Slider novelty_, bpm_;
  juce::Label selectedName_, selectedInfo_, status_, metrics_, compareMetrics_,
      batchInfo_, modeLabel_, seedLabel_, countLabel_, noveltyLabel_,
      tempoLabel_, rateLabel_, scopeLabel_;
  std::array<juce::Rectangle<int>, 6> modules_;
  juce::Rectangle<int> presetBounds_, monitorBounds_, labBounds_,
      previewBounds_, spectrumBounds_;
  std::unique_ptr<juce::FileChooser> chooser_;
  int smokeStage_ = 0, smokeParentIndex_ = -1;
  kravatek::genetic::Genome smokeParent_;
  std::string smokeSaved_;
  juce::File smokeDirectory_;
  std::function<void(bool, juce::String)> smokeCompletion_;
  const kravatek::genetic::Genome &current() const;
  kravatek::genetic::Candidate &cached(const kravatek::genetic::Genome &);
  void sync();
  void rebuildCards();
  void select(int, bool audition = false);
  void modify(std::function<void(kravatek::genetic::Genome &)>);
  void controlChanged(Control);
  void commitEdit();
  void refreshCurrent();
  void timerCallback() override;
  void readSettings();
  void startBatch(bool evolving, bool regenerate = false);
  void refreshLibrary(std::vector<int>);
  void finishJob(std::vector<kravatek::genetic::Candidate>, Job,
                 int keepSelection, juce::String error);
  void setBusy(bool);
  void audition(int index = -1, bool comparing = false);
  std::vector<float> listeningSamples(const kravatek::genetic::Genome &);
  void setComparison(bool a);
  void updateComparisonMetrics();
  void chooseFile(int operation);
  void saveTo(const juce::File &);
  void loadFrom(const juce::File &);
  void exportTo(const juce::File &);
  void status(const juce::String &, bool error = false);
  void smokeAdvance();
  void smokeDone(bool, juce::String = {});
};
