#include "SessionIO.h"
#include "ProfileIO.h"
#include <charconv>
#include <cmath>
#include <iomanip>
#include <juce_core/juce_core.h>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace kravatek::genetic {
namespace {
using juce::DynamicObject;
using juce::var;
juce::String decimal(double value) {
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(17) << value;
  return juce::String::fromUTF8(out.str().c_str());
}
double real(const var &object, const char *key) {
  const auto value = object[juce::Identifier(key)];
  if (!value.isString())
    throw std::invalid_argument(std::string("Expected exact decimal string: ") +
                                key);
  const auto str = value.toString().toStdString();
  double result = 0;
  const auto parsed =
      std::from_chars(str.data(), str.data() + str.size(), result);
  if (parsed.ec != std::errc() || parsed.ptr != str.data() + str.size() ||
      !std::isfinite(result))
    throw std::invalid_argument(std::string("Invalid exact decimal: ") + key);
  return result;
}
DynamicObject::Ptr object() { return new DynamicObject; }
double number(const var &v, const char *key) {
  const auto value = v[juce::Identifier(key)];
  if (!(value.isDouble() || value.isInt() || value.isInt64()) ||
      !std::isfinite(static_cast<double>(value)))
    throw std::invalid_argument(std::string("Invalid numeric field: ") + key);
  return static_cast<double>(value);
}
std::uint32_t integer(const var &v, const char *key) {
  const auto n = number(v, key);
  if (n < 0 || n > 4294967295.0 || std::floor(n) != n)
    throw std::invalid_argument("Invalid integer");
  return static_cast<std::uint32_t>(n);
}
int index(const var &v, const char *key) {
  const auto n = number(v, key);
  if (n < -1 || n > 512 || std::floor(n) != n)
    throw std::invalid_argument("Invalid library index");
  return static_cast<int>(n);
}
bool flag(const var &v, const char *key) {
  const auto value = v[juce::Identifier(key)];
  if (!value.isBool())
    throw std::invalid_argument("Invalid boolean");
  return static_cast<bool>(value);
}
std::string text(const var &v, const char *key) {
  const auto value = v[juce::Identifier(key)];
  if (!value.isString())
    throw std::invalid_argument(std::string("Invalid text field: ") + key);
  return value.toString().toStdString();
}
var locksVar(Locks locks) {
  auto v = object();
  v->setProperty("sub", locks.sub);
  v->setProperty("attack", locks.attack);
  v->setProperty("body", locks.body);
  v->setProperty("tail", locks.tail);
  return var(v.get());
}
Locks locksFrom(const var &v) {
  return {flag(v, "sub"), flag(v, "attack"), flag(v, "body"), flag(v, "tail")};
}
#define SUB_FIELDS(X)                                                          \
  X(hz) X(duration) X(decay) X(attack) X(release) X(level) X(phase)
#define ATTACK_FIELDS(X)                                                       \
  X(hz) X(duration) X(decay) X(attack) X(release) X(level) X(colour) X(phase)
#define BODY_FIELDS(X)                                                         \
  X(startHz)                                                                   \
  X(pivotHz)                                                                   \
  X(endHz)                                                                     \
  X(pivotTime)                                                                 \
  X(settleTime)                                                                \
  X(curve)                                                                     \
  X(duration) X(attack) X(release) X(ampCurve) X(level) X(harmonics) X(fm)     \
      X(fmTime) X(drive) X(fold) X(cutoff) X(resonance) X(sweep) X(phase)
#define TAIL_FIELDS(X)                                                         \
  X(hz)                                                                        \
  X(endHz)                                                                     \
  X(duration)                                                                  \
  X(decay)                                                                     \
  X(attack)                                                                    \
  X(release)                                                                   \
  X(level) X(motion) X(speed) X(cutoff) X(resonance) X(sweep) X(drive)         \
      X(phase)
var fxVar(const FxGenes &f) {
  auto v = object();
  v->setProperty("kind", static_cast<int>(f.kind));
#define PUT(fld) v->setProperty(#fld, decimal(f.fld));
  PUT(stageDrive)
  PUT(preCut)
  PUT(postCut)
  PUT(bias) PUT(multiband) PUT(crossover) PUT(lowDrive) PUT(highDrive)
      PUT(driveDecay) PUT(driveMotion) PUT(bits) PUT(holdHz)
#undef PUT
          return var(v.get());
}
FxGenes fxFrom(const var &v) {
  FxGenes f;
  const auto k = integer(v, "kind");
  if (k >= static_cast<unsigned>(DistortionKind::count))
    throw std::invalid_argument("Invalid distortion");
  f.kind = static_cast<DistortionKind>(k);
#define GET(fld) f.fld = real(v, #fld);
  GET(stageDrive)
  GET(preCut)
  GET(postCut)
  GET(bias) GET(multiband) GET(crossover) GET(lowDrive) GET(highDrive)
      GET(driveDecay) GET(driveMotion) GET(bits) GET(holdHz)
#undef GET
          return f;
}
var genomeVar(const Genome &g) {
  auto root = object();
  root->setProperty("engine", g.engine.c_str());
  root->setProperty("id", g.id.c_str());
  root->setProperty("name", g.name.c_str());
  root->setProperty("seed", static_cast<juce::int64>(g.seed));
  root->setProperty("generation", static_cast<juce::int64>(g.generation));
  root->setProperty("mode", modeName(g.mode));
  root->setProperty("novelty", decimal(g.novelty));
  root->setProperty("locks", locksVar(g.locks));
  juce::Array<var> parents;
  for (const auto &p : g.parents)
    parents.add(p.c_str());
  root->setProperty("parents", parents);
  auto sub = object(), attack = object(), body = object(), tail = object();
#define PUT_SUB(f) sub->setProperty(#f, decimal(g.sub.f));
#define PUT_ATTACK(f) attack->setProperty(#f, decimal(g.attack.f));
#define PUT_BODY(f) body->setProperty(#f, decimal(g.body.f));
#define PUT_TAIL(f) tail->setProperty(#f, decimal(g.tail.f));
  SUB_FIELDS(PUT_SUB)
  ATTACK_FIELDS(PUT_ATTACK)
  BODY_FIELDS(PUT_BODY)
  TAIL_FIELDS(PUT_TAIL)
#undef PUT_SUB
#undef PUT_ATTACK
#undef PUT_BODY
#undef PUT_TAIL
  attack->setProperty("kind", static_cast<int>(g.attack.kind));
  attack->setProperty("seed", static_cast<juce::int64>(g.attack.seed));
  body->setProperty("kind", static_cast<int>(g.body.kind));
  body->setProperty("seed", static_cast<juce::int64>(g.body.seed));
  tail->setProperty("kind", static_cast<int>(g.tail.kind));
  tail->setProperty("seed", static_cast<juce::int64>(g.tail.seed));
  if (g.engine == universalVersion) {
    root->setProperty("architecture", static_cast<int>(g.architecture));
    root->setProperty("character", static_cast<int>(g.character));
    root->setProperty("profileId", g.profileId.c_str());
    root->setProperty("profileDigest", g.profileDigest.c_str());
    root->setProperty("profileRevision",
                      static_cast<juce::int64>(g.profileRevision));
    sub->setProperty("renderer", g.sub.renderVersion);
    sub->setProperty("endHz", decimal(g.sub.endHz));
    sub->setProperty("glideTime", decimal(g.sub.glideTime));
    attack->setProperty("renderer", g.attack.renderVersion);
    body->setProperty("renderer", g.body.renderVersion);
    body->setProperty("damping", decimal(g.body.damping));
    body->setProperty("inharmonicity", decimal(g.body.inharmonicity));
    body->setProperty("noise", decimal(g.body.noise));
    body->setProperty("fx", fxVar(g.body.fx));
    tail->setProperty("renderer", g.tail.renderVersion);
    tail->setProperty("glideTime", decimal(g.tail.glideTime));
    tail->setProperty("harmonics", decimal(g.tail.harmonics));
    tail->setProperty("gate", decimal(g.tail.gate));
    tail->setProperty("fx", fxVar(g.tail.fx));
  }
  root->setProperty("sub", var(sub.get()));
  root->setProperty("attack", var(attack.get()));
  root->setProperty("body", var(body.get()));
  root->setProperty("tail", var(tail.get()));
  return var(root.get());
}
Mode modeFrom(const var &v) {
  const auto name = text(v, "mode");
  for (int i = 0; i < 5; ++i)
    if (name == modeName(static_cast<Mode>(i)))
      return static_cast<Mode>(i);
  throw std::invalid_argument("Invalid saved mode");
}
Genome genomeFrom(const var &root) {
  Genome g;
  g.engine = text(root, "engine");
  g.id = text(root, "id");
  g.name = text(root, "name");
  g.seed = integer(root, "seed");
  g.generation = integer(root, "generation");
  g.mode = modeFrom(root);
  g.novelty = real(root, "novelty");
  g.locks = locksFrom(root["locks"]);
  const auto *parents = root["parents"].getArray();
  if (!parents || parents->size() > 2)
    throw std::invalid_argument("Invalid parents");
  for (const auto &p : *parents) {
    if (!p.isString())
      throw std::invalid_argument("Invalid parent");
    g.parents.push_back(p.toString().toStdString());
  }
  const auto sub = root["sub"], attack = root["attack"], body = root["body"],
             tail = root["tail"];
#define GET_SUB(f) g.sub.f = real(sub, #f);
#define GET_ATTACK(f) g.attack.f = real(attack, #f);
#define GET_BODY(f) g.body.f = real(body, #f);
#define GET_TAIL(f) g.tail.f = real(tail, #f);
  SUB_FIELDS(GET_SUB)
  ATTACK_FIELDS(GET_ATTACK)
  BODY_FIELDS(GET_BODY)
  TAIL_FIELDS(GET_TAIL)
#undef GET_SUB
#undef GET_ATTACK
#undef GET_BODY
#undef GET_TAIL
  const auto ak = integer(attack, "kind"), bk = integer(body, "kind"),
             tk = integer(tail, "kind");
  if (ak >= 4 || bk >= (g.engine == universalVersion ? 6u : 4u) ||
      tk >= (g.engine == universalVersion ? 6u : 4u))
    throw std::invalid_argument("Invalid layer architecture");
  g.attack.kind = static_cast<AttackKind>(ak);
  g.attack.seed = integer(attack, "seed");
  g.body.kind = static_cast<BodyKind>(bk);
  g.body.seed = integer(body, "seed");
  g.tail.kind = static_cast<TailKind>(tk);
  g.tail.seed = integer(tail, "seed");
  if (g.engine == universalVersion) {
    const auto ar = integer(root, "architecture"),
               ch = integer(root, "character");
    if (ar >= static_cast<unsigned>(Architecture::count) ||
        ch >= static_cast<unsigned>(Character::count))
      throw std::invalid_argument("Invalid universal type");
    g.architecture = static_cast<Architecture>(ar);
    g.character = static_cast<Character>(ch);
    g.profileId = text(root, "profileId");
    g.profileDigest = text(root, "profileDigest");
    g.profileRevision = integer(root, "profileRevision");
    g.sub.renderVersion = static_cast<int>(integer(sub, "renderer"));
    g.attack.renderVersion = static_cast<int>(integer(attack, "renderer"));
    g.body.renderVersion = static_cast<int>(integer(body, "renderer"));
    g.tail.renderVersion = static_cast<int>(integer(tail, "renderer"));
    g.sub.endHz = real(sub, "endHz");
    g.sub.glideTime = real(sub, "glideTime");
    g.body.damping = real(body, "damping");
    g.body.inharmonicity = real(body, "inharmonicity");
    g.body.noise = real(body, "noise");
    g.body.fx = fxFrom(body["fx"]);
    g.tail.glideTime = real(tail, "glideTime");
    g.tail.harmonics = real(tail, "harmonics");
    g.tail.gate = real(tail, "gate");
    g.tail.fx = fxFrom(tail["fx"]);
  }
  validate(g);
  auto checked = g;
  identify(checked);
  if (checked.id != g.id)
    throw std::invalid_argument("Saved genome identity mismatch");
  return g;
}
#undef SUB_FIELDS
#undef ATTACK_FIELDS
#undef BODY_FIELDS
#undef TAIL_FIELDS
} // namespace
std::string encodeSession(const Session &s) {
  validateSession(s);
  auto root = object();
  const bool universal = s.settings.universal.has_value() ||
      std::any_of(s.library.entries().begin(), s.library.entries().end(),
                  [](const Genome &g) { return g.engine == universalVersion; });
  root->setProperty("schema",
                    universal ? "KRAVA_SESSION_004" : "KRAVA_SESSION_003");
  root->setProperty("engine", universal ? universalVersion : engineVersion);
  juce::Array<var> library;
  for (const auto &g : s.library.entries())
    library.add(genomeVar(g));
  root->setProperty("library", library);
  root->setProperty("selected", s.selected);
  root->setProperty("compareA", s.compareA);
  root->setProperty("compareB", s.compareB);
  juce::Array<var> visible;
  for (auto i : s.visible)
    visible.add(i);
  root->setProperty("visible", visible);
  auto settings = object();
  settings->setProperty("mode", modeName(s.settings.mode));
  settings->setProperty("seed", static_cast<juce::int64>(s.settings.seed));
  settings->setProperty("novelty", decimal(s.settings.novelty));
  settings->setProperty("sampleRate", decimal(s.settings.sampleRate));
  settings->setProperty("count", s.settings.count);
  settings->setProperty("refine", s.settings.refine);
  settings->setProperty("locks", locksVar(s.settings.locks));
  if (s.settings.universal) {
    const auto &r = *s.settings.universal;
    auto u = object();
    u->setProperty("profile",
                   juce::String::fromUTF8(encodeProfile(r.profile).c_str()));
    u->setProperty("architecture", static_cast<int>(r.overrideArchitecture));
    u->setProperty("character", static_cast<int>(r.character));
    u->setProperty("note", r.note);
    settings->setProperty("universal", var(u.get()));
  }
  root->setProperty("settings", var(settings.get()));
  root->setProperty("bpm", decimal(s.bpm));
  root->setProperty("loop", s.loop);
  root->setProperty("matchLevel", s.matchLevel);
  root->setProperty("sidecar", s.sidecar);
  return juce::JSON::toString(var(root.get()), false, 25).toStdString();
}
Session decodeSession(std::string_view json) {
  if (json.size() > 16 * 1024 * 1024)
    throw std::invalid_argument("Session exceeds 16 MiB");
  const auto root = juce::JSON::parse(
      juce::String::fromUTF8(json.data(), static_cast<int>(json.size())));
  if (!root.isObject() || !((text(root, "schema") == "KRAVA_SESSION_003" &&
                             text(root, "engine") == engineVersion) ||
                            (text(root, "schema") == "KRAVA_SESSION_004" &&
                             text(root, "engine") == universalVersion)))
    throw std::invalid_argument("Not a supported KRAVA session");
  Session s;
  const auto *entries = root["library"].getArray();
  if (!entries || entries->size() > static_cast<int>(Library::maximum))
    throw std::invalid_argument("Invalid library size");
  for (const auto &item : *entries) {
    const auto g = genomeFrom(item);
    const auto size = s.library.entries().size();
    s.library.append(g);
    if (s.library.entries().size() == size)
      throw std::invalid_argument("Duplicate saved genome ID");
  }
  s.selected = index(root, "selected");
  s.compareA = index(root, "compareA");
  s.compareB = index(root, "compareB");
  const auto *visible = root["visible"].getArray();
  if (!visible || visible->size() > 512)
    throw std::invalid_argument("Invalid visible generation");
  for (const auto &v : *visible) {
    if (!v.isInt() && !v.isInt64())
      throw std::invalid_argument("Invalid visible index");
    const auto value = static_cast<juce::int64>(v);
    if (value < 0 || value >= 512)
      throw std::invalid_argument("Invalid visible index range");
    s.visible.push_back(static_cast<int>(value));
  }
  const auto settings = root["settings"];
  s.settings.mode = modeFrom(settings);
  s.settings.seed = integer(settings, "seed");
  s.settings.novelty = real(settings, "novelty");
  s.settings.sampleRate = real(settings, "sampleRate");
  s.settings.count = index(settings, "count");
  s.settings.refine = flag(settings, "refine");
  s.settings.locks = locksFrom(settings["locks"]);
  if (settings.hasProperty("universal")) {
    const auto u = settings["universal"];
    UniversalRequest r;
    r.profile = decodeProfile(text(u, "profile"));
    auto a = integer(u, "architecture"), c = integer(u, "character");
    if (a > static_cast<unsigned>(Architecture::count) ||
        c >= static_cast<unsigned>(Character::count))
      throw std::invalid_argument("Invalid universal request enum");
    r.overrideArchitecture = static_cast<Architecture>(a);
    r.character = static_cast<Character>(c);
    r.note = static_cast<int>(integer(u, "note"));
    s.settings.universal = r;
  }
  s.bpm = real(root, "bpm");
  s.loop = flag(root, "loop");
  s.matchLevel = flag(root, "matchLevel");
  s.sidecar = flag(root, "sidecar");
  validateSession(s);
  return s;
}
std::string encodeSound(const Genome &g, double rate) {
  Session s;
  s.selected = static_cast<int>(s.library.append(g));
  s.visible = {s.selected};
  s.settings.mode = g.mode;
  s.settings.seed = g.seed;
  s.settings.novelty = g.novelty;
  s.settings.sampleRate = rate;
  s.settings.locks = g.locks;
  return encodeSession(s);
}
} // namespace kravatek::genetic
