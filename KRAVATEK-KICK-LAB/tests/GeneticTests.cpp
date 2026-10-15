#include "genetic/Genome.h"
#include "genetic/LayerRenderer.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <stdexcept>
using namespace kravatek::genetic;
namespace {
void check(bool v, const char *text) {
  if (!v)
    throw std::runtime_error(text);
}
template <class F> void rejects(F f) {
  bool bad = false;
  try {
    f();
  } catch (const std::invalid_argument &) {
    bad = true;
  }
  check(bad, "Invalid genetic data accepted");
}
void safe(const Render &r) {
  check(!r.mix.empty(), "Empty genetic render");
  double p = 0, sum = 0;
  for (auto v : r.mix) {
    check(std::isfinite(v), "Nonfinite genetic sample");
    p = std::max(p, std::abs(static_cast<double>(v)));
    sum += v;
  }
  check(p <= .891251, "Uncontrolled peak");
  check(p > .001, "Silent genetic render");
  check(std::abs(sum / static_cast<double>(r.mix.size())) < 1e-7,
        "Genetic DC offset");
  check(r.mix.front() == 0 && r.mix.back() == 0, "Genetic endpoints click");
}
void identity() {
  check(sha256("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA256 empty vector");
  check(sha256("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA256 abc vector");
  auto a = foundation(Mode::deep);
  auto b = a;
  identify(b);
  check(a.id == b.id, "Unstable ID");
  b.body.drive += 1;
  identify(b);
  check(a.id != b.id, "Genes do not affect ID");
  b = a;
  b.engine = "future";
  rejects([&] { render(b); });
  b = a;
  b.body.resonance = 1;
  rejects([&] { render(b); });
  b = a;
  b.novelty = std::numeric_limits<double>::quiet_NaN();
  rejects([&] { render(b); });
}
void prngTest() {
  Random a(42), b(42), other(43);
  bool different = false;
  for (int i = 0; i < 100; ++i) {
    auto x = a.next();
    check(x == b.next(), "PRNG not reproducible");
    different |= x != other.next();
  }
  check(different, "PRNG seeds identical");
}
void layers() {
  for (int m = 0; m < 5; ++m)
    for (auto rate : {44100., 48000.}) {
      auto g = foundation(static_cast<Mode>(m));
      auto r = render(g, rate);
      safe(r);
      check(r.mix == render(g, rate).mix, "Layer rendering not deterministic");
      for (std::size_t i = 0; i < 4; ++i) {
        check(r.layers[i].front() == 0 && r.layers[i].back() == 0,
              "Layer edge");
        auto isolated = g;
        isolated.body.drive = 30;
        isolated.tail.motion = 1;
        if (i < 2)
          check(r.layers[i] == render(isolated, rate).layers[i],
                "Layer depends on other genes");
      }
    }
}
} // namespace
int main(int argc, char **argv) {
  try {
    const std::map<std::string, std::function<void()>> tests{
        {"genetic-identity", identity},
        {"genetic-prng", prngTest},
        {"genetic-layers", layers}};
    if (argc != 2 || !tests.contains(argv[1]))
      throw std::runtime_error("Choose genetic test");
    tests.at(argv[1])();
    std::cout << "PASS " << argv[1] << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << e.what() << '\n';
    return 1;
  }
}
