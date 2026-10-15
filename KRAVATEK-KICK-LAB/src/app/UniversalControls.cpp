#include "UniversalControls.h"
#include <numbers>
using namespace kravatek::genetic;
UniversalControls::UniversalControls(
    const Genome &g, std::function<void(std::function<void(Genome &)>)> edit)
    : snapshot_(g), edit_(std::move(edit)) {
  if (snapshot_.sub.renderVersion == 3)
    snapshot_.sub.endHz = snapshot_.sub.hz;
  for (std::size_t i = 0; i < pages_.size(); ++i) {
    addAndMakeVisible(pages_[i]);
    pages_[i].setButtonText(i == 0   ? "BODY FX"
                            : i == 1 ? "TAIL FX"
                                     : "SYNTH / GLIDE");
    pages_[i].onClick = [this, i] { setPage(static_cast<int>(i)); };
  }
  addAndMakeVisible(kind_);
  for (int i = 0; i < 8; ++i)
    kind_.addItem(distortionName(static_cast<DistortionKind>(i)), i + 1);
  kind_.onChange = [this] {
    const auto value = static_cast<DistortionKind>(kind_.getSelectedId() - 1);
    const bool tail = page_ == 1;
    auto change = [tail, value](Genome &g) {
      upgrade(g);
      if (tail) {
        g.tail.renderVersion = 4;
        g.tail.fx.kind = value;
      } else {
        g.body.renderVersion = 4;
        g.body.fx.kind = value;
      }
    };
    change(snapshot_);
    edit_(change);
    configure();
  };
  for (std::size_t i = 0; i < knobs_.size(); ++i) {
    addAndMakeVisible(knobs_[i]);
    knobs_[i].slider.onValueChange = [this, i] {
      apply(static_cast<int>(i), knobs_[i].slider.getValue());
    };
  }
  configure();
  setSize(930, 318);
}
void UniversalControls::paint(juce::Graphics &g) {
  krui::frame(g, getLocalBounds(), "UNIVERSAL DSP / LAYER EDITOR");
  g.setColour(krui::muted);
  g.setFont(krui::mono(10));
  g.drawText("Edits upgrade this layer to UKE 004. Other layers and saved "
             "parents stay intact.",
             14, getHeight() - 19, getWidth() - 28, 16,
             juce::Justification::centredLeft);
}
void UniversalControls::resized() {
  auto r = getLocalBounds().reduced(12);
  r.removeFromTop(24);
  auto top = r.removeFromTop(29);
  for (auto &p : pages_) {
    p.setBounds(top.removeFromLeft(150));
    top.removeFromLeft(6);
  }
  kind_.setBounds(top.removeFromRight(230));
  r.removeFromTop(7);
  r.removeFromBottom(20);
  const auto w = r.getWidth() / 6, h = r.getHeight() / 2;
  for (int i = 0; i < 12; ++i)
    knobs_[static_cast<std::size_t>(i)].setBounds(r.getX() + (i % 6) * w,
                                                  r.getY() + (i / 6) * h, w, h);
}
void UniversalControls::setPage(int p) {
  page_ = p;
  configure();
}
void UniversalControls::configure() {
  for (int i = 0; i < 3; ++i)
    pages_[static_cast<std::size_t>(i)].setToggleState(
        page_ == i, juce::dontSendNotification);
  const auto &fx = page_ == 1 ? snapshot_.tail.fx : snapshot_.body.fx;
  kind_.setVisible(page_ < 2);
  kind_.setSelectedId(static_cast<int>(fx.kind) + 1,
                      juce::dontSendNotification);
  const auto setup = [&](int i, const char *n, double lo, double hi,
                         double step, const char *unit, double value,
                         bool enabled = true) {
    auto &k = knobs_[static_cast<std::size_t>(i)];
    k.setVisible(true);
    k.configure(n, lo, hi, step, unit, n);
    k.slider.setValue(value, juce::dontSendNotification);
    k.setEnabled(enabled);
  };
  if (page_ < 2) {
    setup(0, "PRE LP", 0, 18000, 1, " Hz", fx.preCut);
    setup(1, "POST LP", 0, 18000, 1, " Hz", fx.postCut);
    setup(2, "STAGE 2", 0, 24, .1, " dB", fx.stageDrive);
    setup(3, "MULTIBAND", 0, 100, .1, " %", fx.multiband * 100);
    setup(4, "LOW DRIVE", 0, 24, .1, " dB", fx.lowDrive, fx.multiband > 0);
    setup(5, "HIGH DRIVE", 0, 30, .1, " dB", fx.highDrive, fx.multiband > 0);
    setup(6, "BITS", 4, 16, 1, " bit", fx.bits,
          fx.kind == DistortionKind::digital);
    setup(7, "HOLD RATE", 500, 48000, 1, " Hz", fx.holdHz,
          fx.kind == DistortionKind::digital);
    if (fx.kind == DistortionKind::rectify)
      setup(8, "RECTIFY MIX", 0, 100, .1, " %", std::abs(fx.bias) * 125);
    else
      setup(8, "ASYMMETRY", -.8, .8, .01, "", fx.bias,
            fx.kind == DistortionKind::asymmetric);
    setup(9, "DRIVE ENV", 0, 100, .1, " %", fx.driveMotion * 100);
    setup(10, "ENV DECAY", 5, 2000, 1, " ms", fx.driveDecay * 1000,
          fx.driveMotion > 0);
    setup(11, "CROSSOVER", 60, 800, 1, " Hz", fx.crossover, fx.multiband > 0);
  } else {
    setup(0, "SUB TARGET", 25, 400, .1, " Hz", snapshot_.sub.endHz);
    setup(1, "SUB GLIDE", 1, 2000, 1, " ms", snapshot_.sub.glideTime * 1000);
    setup(2, "SUB PHASE", 0, 360, .1, " deg",
          snapshot_.sub.phase * 180 / std::numbers::pi);
    setup(3, "TAIL TARGET", 25, 800, .1, " Hz", snapshot_.tail.endHz);
    setup(4, "TAIL GLIDE", 1, 2000, 1, " ms", snapshot_.tail.glideTime * 1000);
    setup(5, "TAIL HARMONICS", 0, 100, .1, " %",
          snapshot_.tail.harmonics * 100);
    setup(6, "TAIL GATE", 5, 95, 1, " %", snapshot_.tail.gate * 100,
          snapshot_.tail.kind == TailKind::gated);
    setup(7, "MEMBRANE DAMP", 0, 100, .1, " %", snapshot_.body.damping * 100,
          snapshot_.body.kind == BodyKind::membrane);
    setup(8, "MODAL SPREAD", 0, 100, .1, " %",
          snapshot_.body.inharmonicity * 100,
          snapshot_.body.kind == BodyKind::membrane);
    setup(9, "MEMBRANE NOISE", 0, 100, .1, " %", snapshot_.body.noise * 100,
          snapshot_.body.kind == BodyKind::membrane);
    knobs_[10].setVisible(false);
    knobs_[11].setVisible(false);
  }
  repaint();
}
void UniversalControls::setValue(int i, double value) {
  knobs_.at(static_cast<std::size_t>(i))
      .slider.setValue(value, juce::sendNotificationSync);
}
void UniversalControls::apply(int i, double v) {
  const auto page = page_;
  auto change = [page, i, v](Genome &g) {
    upgrade(g);
    if (page < 2) {
      auto &f = page == 0 ? g.body.fx : g.tail.fx;
      if (page == 0)
        g.body.renderVersion = 4;
      else
        g.tail.renderVersion = 4;
      switch (i) {
      case 0:
        f.preCut = v;
        break;
      case 1:
        f.postCut = v;
        break;
      case 2:
        f.stageDrive = v;
        break;
      case 3:
        f.multiband = v / 100;
        break;
      case 4:
        f.lowDrive = v;
        break;
      case 5:
        f.highDrive = v;
        break;
      case 6:
        f.bits = v;
        break;
      case 7:
        f.holdHz = v;
        break;
      case 8:
        f.bias = f.kind == DistortionKind::rectify ? v * .008 : v;
        break;
      case 9:
        f.driveMotion = v / 100;
        break;
      case 10:
        f.driveDecay = v / 1000;
        break;
      case 11:
        f.crossover = v;
        break;
      default:
        break;
      }
    } else if (i < 3) {
      if (g.sub.renderVersion == 3)
        g.sub.endHz = g.sub.hz;
      g.sub.renderVersion = 4;
      if (i == 0)
        g.sub.endHz = v;
      if (i == 1)
        g.sub.glideTime = v / 1000;
      if (i == 2)
        g.sub.phase = v * std::numbers::pi / 180;
    } else if (i <= 6) {
      g.tail.renderVersion = 4;
      if (i == 3)
        g.tail.endHz = v;
      if (i == 4)
        g.tail.glideTime = v / 1000;
      if (i == 5)
        g.tail.harmonics = v / 100;
      if (i == 6)
        g.tail.gate = v / 100;
    } else {
      g.body.renderVersion = 4;
      if (i == 7)
        g.body.damping = v / 100;
      if (i == 8)
        g.body.inharmonicity = v / 100;
      if (i == 9)
        g.body.noise = v / 100;
    }
  };
  change(snapshot_);
  edit_(change);
  configure();
}
