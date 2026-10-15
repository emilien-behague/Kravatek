#include "ProfileIO.h"
#include "BundledProfiles.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <juce_core/juce_core.h>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace kravatek::genetic {
namespace {
using juce::var;
std::string str(const var &v, const char *k) {
  if (!v[k].isString())
    throw std::invalid_argument(std::string("Expected profile text: ") + k);
  return v[k].toString().toStdString();
}
var real(double x) {
  std::ostringstream s;
  s.imbue(std::locale::classic());
  s << std::setprecision(17) << x;
  return juce::String::fromUTF8(s.str().c_str());
}
double number(const var &v) {
  if (!v.isString())
    throw std::invalid_argument(
        "Profile numbers must be exact decimal strings");
  auto s = v.toString().toStdString();
  double x = 0;
  auto p = std::from_chars(s.data(), s.data() + s.size(), x);
  if (p.ec != std::errc() || p.ptr != s.data() + s.size() || !std::isfinite(x))
    throw std::invalid_argument("Invalid profile decimal");
  return x;
}
var parse(std::string_view text) {
  if (text.size() > 1024 * 1024)
    throw std::invalid_argument("Profile JSON exceeds 1 MiB");
  auto v = juce::JSON::parse(
      juce::String::fromUTF8(text.data(), static_cast<int>(text.size())));
  if (!v.isObject())
    throw std::invalid_argument("Expected profile JSON object");
  return v;
}
template <class A> var weights(const A &a) {
  juce::Array<var> v;
  for (auto x : a)
    v.add(real(x));
  return v;
}
template <class A> void weights(A &a, const var &v) {
  auto *p = v.getArray();
  if (!p || p->size() != static_cast<int>(a.size()))
    throw std::invalid_argument("Invalid profile weights");
  for (std::size_t i = 0; i < a.size(); ++i)
    a[i] = number((*p)[static_cast<int>(i)]);
}
GenreProfile from(const var &v) {
  if (str(v, "schema") != "KRAVA_PROFILE_004")
    throw std::invalid_argument("Unknown profile schema");
  GenreProfile p;
  p.id = str(v, "id");
  p.name = str(v, "name");
  p.family = str(v, "family");
  auto revision = v["revision"];
  if (!revision.isInt() || static_cast<int>(revision) < 1)
    throw std::invalid_argument("Invalid profile revision");
  p.revision = static_cast<std::uint32_t>(static_cast<int>(revision));
  const auto *parents = v["parents"].getArray();
  if (!parents || parents->size() > 4)
    throw std::invalid_argument("Invalid profile parents");
  for (const auto &x : *parents) {
    if (!x.isString())
      throw std::invalid_argument("Invalid profile parent");
    p.parents.push_back(x.toString().toStdString());
  }
  p.bpmLow = number(v["bpmLow"]);
  p.bpmHigh = number(v["bpmHigh"]);
  p.fundamentalLow = number(v["fundamentalLow"]);
  p.fundamentalHigh = number(v["fundamentalHigh"]);
  p.subGlide = number(v["subGlide"]);
  p.tailInterval = number(v["tailInterval"]);
  weights(p.architectures, v["architectures"]);
  weights(p.bodyWeights, v["bodyWeights"]);
  weights(p.tailWeights, v["tailWeights"]);
  weights(p.attackWeights, v["attackWeights"]);
  weights(p.distortions, v["distortions"]);
  const auto *ranges = v["ranges"].getArray();
  if (!ranges || ranges->size() > 100)
    throw std::invalid_argument("Invalid profile ranges");
  for (const auto &x : *ranges) {
    ProfileRange r;
    r.parameter = str(x, "parameter");
    r.low = number(x["low"]);
    r.high = number(x["high"]);
    const auto d = str(x, "distribution");
    if (d == "uniform")
      r.distribution = Distribution::uniform;
    else if (d == "log")
      r.distribution = Distribution::logarithmic;
    else if (d == "triangular")
      r.distribution = Distribution::triangular;
    else
      throw std::invalid_argument("Unknown profile distribution");
    p.ranges.push_back(r);
  }
  identifyProfile(p);
  return p;
}
} // namespace
std::string encodeProfile(const GenreProfile &p) {
  validateProfile(p);
  juce::DynamicObject::Ptr v = new juce::DynamicObject;
  v->setProperty("schema", "KRAVA_PROFILE_004");
  v->setProperty("id", p.id.c_str());
  v->setProperty("name", p.name.c_str());
  v->setProperty("family", p.family.c_str());
  v->setProperty("revision", static_cast<int>(p.revision));
  juce::Array<var> parents;
  for (const auto &s : p.parents)
    parents.add(s.c_str());
  v->setProperty("parents", parents);
#define PUT(k) v->setProperty(#k, real(p.k));
  PUT(bpmLow)
  PUT(bpmHigh) PUT(fundamentalLow) PUT(fundamentalHigh) PUT(subGlide)
      PUT(tailInterval)
#undef PUT
          v->setProperty("architectures", weights(p.architectures));
  v->setProperty("bodyWeights", weights(p.bodyWeights));
  v->setProperty("tailWeights", weights(p.tailWeights));
  v->setProperty("attackWeights", weights(p.attackWeights));
  v->setProperty("distortions", weights(p.distortions));
  juce::Array<var> ranges;
  for (const auto &r : p.ranges) {
    juce::DynamicObject::Ptr a = new juce::DynamicObject;
    a->setProperty("parameter", r.parameter.c_str());
    a->setProperty("low", real(r.low));
    a->setProperty("high", real(r.high));
    a->setProperty("distribution", r.distribution == Distribution::uniform
                                       ? "uniform"
                                   : r.distribution == Distribution::logarithmic
                                       ? "log"
                                       : "triangular");
    ranges.add(var(a.get()));
  }
  v->setProperty("ranges", ranges);
  return juce::JSON::toString(var(v.get()), false).toStdString();
}
GenreProfile decodeProfile(std::string_view s) { return from(parse(s)); }
std::vector<GenreProfile> decodeCatalog(std::string_view s) {
  auto v = parse(s);
  if (str(v, "schema") != "KRAVA_PROFILES_004")
    throw std::invalid_argument("Unknown catalog version");
  const auto *a = v["profiles"].getArray();
  if (!a || a->isEmpty() || a->size() > 100)
    throw std::invalid_argument("Invalid profile catalog size");
  std::vector<GenreProfile> profiles;
  std::set<std::string> ids;
  for (const auto &x : *a) {
    auto p = from(x);
    if (!ids.insert(p.id).second)
      throw std::invalid_argument("Duplicate profile ID");
    profiles.push_back(std::move(p));
  }
  for (const auto &p : profiles)
    for (const auto &parent : p.parents)
      if (!ids.contains(parent) || parent == p.id)
        throw std::invalid_argument(
            "Unknown or self-referencing profile parent");
  return profiles;
}
std::vector<GenreProfile> bundledProfiles() {
  return decodeCatalog(kravaBundledProfiles);
}
} // namespace kravatek::genetic
