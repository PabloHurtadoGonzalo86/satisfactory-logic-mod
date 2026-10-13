#include "golden_io.hpp"

#include <algorithm>
#include <cstdio>
#include <map>

#include "harness.hpp"

namespace slgt {

// ---- formatting --------------------------------------------------------------------------------------------

namespace {

std::string nibble(unsigned v) { return cat(v); }

std::string pinText(Pin p) {
  return p.index == kOut ? cat(p.component, ".o") : cat(p.component, ".", static_cast<unsigned>(p.index));
}

std::string compText(const Spec& s, unsigned seed) {
  switch (s.kind) {
    case Kind::Const: return cat("const v=", s.p0);
    case Kind::Sensor: return "sensor";
    case Kind::Actuator: return cat("actuator seed=", seed);
    case Kind::Gate: {
      const char* op = s.p0 == 0 ? "and" : s.p0 == 1 ? "or" : "xor";
      return cat("gate op=", op, " inv=", s.p1, " k=", s.p2, " seed=", seed);
    }
    case Kind::Delay: return cat("delay d=", s.p0, " seed=", seed);
    case Kind::Compare: return cat("cmp m=", s.p0 == 0 ? "compare" : "subtract", " seed=", seed);
    case Kind::Latch: return cat("latch seed=", seed);
    case Kind::Toggle: return cat("toggle seed=", seed);
    case Kind::Clock: return cat("clock p=", s.p0, " h=", s.p1, " seed=", seed);
  }
  return "?";
}

std::string stateText(const BlobComp& c) {
  switch (c.spec.kind) {
    case Kind::Const: case Kind::Sensor: return "";
    case Kind::Actuator: case Kind::Gate: case Kind::Compare: case Kind::Latch: return nibble(c.state[0]);
    case Kind::Toggle: return nibble(c.state[0]) + "/" + cat(static_cast<unsigned>(c.state[1]));
    case Kind::Clock: return cat(static_cast<unsigned>(c.state[0] | (c.state[1] << 8)));
    case Kind::Delay: {
      std::string s = "[";
      for (std::size_t i = 0; i < c.state.size(); ++i) {
        if (i != 0) s += ",";
        s += nibble(c.state[i]);
      }
      return s + "]";
    }
  }
  return "";
}

}  // namespace

std::string formatLnet(const Model& m) {
  std::string s = "lnet 1\n";
  for (const CompDef& c : m.comps) s += cat("c ", c.id, " ", compText(c.spec, c.seed), "\n");
  for (const Wire& w : m.wires) s += cat("w ", pinText(w.a), " ", pinText(w.b), "\n");
  return s;
}

std::string formatLscn(const Scenario& sc) {
  std::string s = "lscn 1\n";
  s += cat("ticks ", sc.ticks, "\n");
  std::size_t i = 0;
  while (i < sc.inputs.size()) {
    std::size_t j = i;
    std::string line = cat("i ", sc.inputs[i].tau);
    while (j < sc.inputs.size() && sc.inputs[j].tau == sc.inputs[i].tau) {
      line += cat(" ", sc.inputs[j].id, "=", static_cast<unsigned>(sc.inputs[j].value));
      ++j;
    }
    s += line + "\n";
    i = j;
  }
  for (const TimedEdit& e : sc.edits) {
    const EditOp& op = e.op;
    switch (op.kind) {
      case EditKind::Add: s += cat("e ", e.tau, " add c ", op.id, " ", compText(op.spec, op.seed), "\n"); break;
      case EditKind::Remove: s += cat("e ", e.tau, " del c ", op.id, "\n"); break;
      case EditKind::Replace: s += cat("e ", e.tau, " rep c ", op.id, " ", compText(op.spec, op.seed), "\n"); break;
      case EditKind::Connect: s += cat("e ", e.tau, " add w ", pinText(op.a), " ", pinText(op.b), "\n"); break;
      case EditKind::Disconnect: s += cat("e ", e.tau, " del w ", pinText(op.a), " ", pinText(op.b), "\n"); break;
    }
  }
  return s;
}

std::string formatLtrace(const std::vector<TraceRow>& rows) {
  std::string s = "ltrace 1\n";
  for (const TraceRow& r : rows) {
    s += cat("row ", r.tau, " in");
    for (const auto& [id, v] : r.in) s += cat(" ", id, "=", nibble(v));
    s += " | st";
    for (const BlobComp& c : r.st) s += cat(" ", c.id, "=", stateText(c));
    s += " | cmd";
    for (const auto& [id, v] : r.cmd) s += cat(" ", id, "=", nibble(v));
    s += cat(" | h=", hex(r.hash), "\n");
  }
  return s;
}

ParsedRow toParsed(const TraceRow& r) {
  ParsedRow p;
  p.tau = r.tau;
  p.in = r.in;
  for (const BlobComp& c : r.st) p.st.emplace_back(c.id, stateText(c));
  p.cmd = r.cmd;
  p.hash = r.hash;
  return p;
}

std::size_t countRows(const std::string& ltrace) {
  std::size_t n = 0;
  std::size_t pos = 0;
  while (pos < ltrace.size()) {
    const std::size_t eol = ltrace.find('\n', pos);
    const std::size_t end = eol == std::string::npos ? ltrace.size() : eol;
    if (end - pos >= 4 && ltrace.compare(pos, 4, "row ") == 0) ++n;
    pos = end + 1;
  }
  return n;
}

// ---- parsing ------------------------------------------------------------------------------------------------

namespace {

struct Line {
  int number = 0;
  std::vector<std::string> tok;
};

std::vector<Line> splitLines(const std::string& text) {
  std::vector<Line> lines;
  std::size_t pos = 0;
  int number = 0;
  while (pos <= text.size()) {
    std::size_t eol = text.find('\n', pos);
    if (eol == std::string::npos) eol = text.size();
    ++number;
    std::string raw = text.substr(pos, eol - pos);
    pos = eol + 1;
    const std::size_t hash = raw.find('#');
    if (hash != std::string::npos) raw.resize(hash);
    Line ln;
    ln.number = number;
    std::string cur;
    for (char ch : raw) {
      if (ch == ' ' || ch == '\t' || ch == '\r') {
        if (!cur.empty()) ln.tok.push_back(std::move(cur));
        cur.clear();
      } else {
        cur.push_back(ch);
      }
    }
    if (!cur.empty()) ln.tok.push_back(std::move(cur));
    if (!ln.tok.empty()) lines.push_back(std::move(ln));
    if (eol == text.size()) break;
  }
  return lines;
}

bool parseDec(const std::string& t, std::uint64_t& out) {
  if (t.empty() || t.size() > 18) return false;
  std::uint64_t v = 0;
  for (char ch : t) {
    if (ch < '0' || ch > '9') return false;
    v = v * 10 + static_cast<std::uint64_t>(ch - '0');
  }
  out = v;
  return true;
}

// A signal-like number: one character is a hex digit (so F is 15), more are decimal.
bool parseNibbleLike(const std::string& t, std::uint64_t& out) {
  if (t.size() == 1) {
    const char ch = t[0];
    if (ch >= '0' && ch <= '9') { out = static_cast<std::uint64_t>(ch - '0'); return true; }
    if (ch >= 'A' && ch <= 'F') { out = static_cast<std::uint64_t>(ch - 'A' + 10); return true; }
    if (ch >= 'a' && ch <= 'f') { out = static_cast<std::uint64_t>(ch - 'a' + 10); return true; }
    return false;
  }
  return parseDec(t, out);
}

std::string lower(std::string s) {
  for (char& ch : s) {
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  }
  return s;
}

bool parsePin(const std::string& t, Pin& out) {
  const std::size_t dot = t.find('.');
  if (dot == std::string::npos) return false;
  std::uint64_t id = 0;
  if (!parseDec(t.substr(0, dot), id) || id > 0xFFFFFFFFULL) return false;
  const std::string rest = lower(t.substr(dot + 1));
  out.component = u32(id);
  if (rest == "o") {
    out.index = kOut;
    return true;
  }
  std::uint64_t n = 0;
  if (!parseDec(rest, n) || n > 254) return false;
  out.index = u8(n);
  return true;
}

bool kindFromToken(const std::string& tokRaw, Kind& k) {
  const std::string t = lower(tokRaw);
  static const std::pair<const char*, Kind> table[] = {
      {"const", Kind::Const}, {"sensor", Kind::Sensor}, {"actuator", Kind::Actuator}, {"gate", Kind::Gate},
      {"delay", Kind::Delay}, {"cmp", Kind::Compare}, {"compare", Kind::Compare}, {"latch", Kind::Latch},
      {"toggle", Kind::Toggle}, {"clock", Kind::Clock}};
  for (const auto& [name, kind] : table) {
    if (t == name) { k = kind; return true; }
  }
  return false;
}

// tokens[from..] are key=value pairs
bool parseComp(const std::vector<std::string>& tok, std::size_t from, Spec& spec, std::uint16_t& seed, std::string& err) {
  Kind kind;
  if (from >= tok.size() || !kindFromToken(tok[from], kind)) {
    err = "unknown component kind";
    return false;
  }
  std::map<std::string, std::string> kv;
  for (std::size_t i = from + 1; i < tok.size(); ++i) {
    const std::size_t eq = tok[i].find('=');
    if (eq == std::string::npos) {
      err = cat("expected key=value, got '", tok[i], "'");
      return false;
    }
    kv[lower(tok[i].substr(0, eq))] = tok[i].substr(eq + 1);
  }
  spec = Spec{};
  spec.kind = kind;
  auto take = [&](const char* key, bool nibbleLike, std::uint64_t& out) -> int {   // 1 found, 0 absent, -1 malformed
    const auto it = kv.find(key);
    if (it == kv.end()) return 0;
    const bool ok = nibbleLike ? parseNibbleLike(it->second, out) : parseDec(it->second, out);
    kv.erase(it);
    return ok ? 1 : -1;
  };
  std::uint64_t v = 0;
  int r = 0;
  bool haveSeed = false;
  std::uint64_t seedValue = 0;
  switch (kind) {
    case Kind::Const:
      if ((r = take("v", true, v)) < 0) { err = "bad v"; return false; }
      spec.p0 = u16(v);
      break;
    case Kind::Sensor: case Kind::Actuator: case Kind::Latch: case Kind::Toggle: break;
    case Kind::Gate: {
      const auto op = kv.find("op");
      if (op != kv.end()) {
        const std::string o = lower(op->second);
        if (o == "and" || o == "0") spec.p0 = 0;
        else if (o == "or" || o == "1") spec.p0 = 1;
        else if (o == "xor" || o == "2") spec.p0 = 2;
        else { err = "bad op"; return false; }
        kv.erase(op);
      }
      if ((r = take("inv", false, v)) < 0) { err = "bad inv"; return false; }
      spec.p1 = u16(v);
      v = 2;
      if ((r = take("k", false, v)) < 0) { err = "bad k"; return false; }
      spec.p2 = u16(r == 0 ? 2U : static_cast<unsigned>(v));
      break;
    }
    case Kind::Delay:
      v = 1;
      if ((r = take("d", false, v)) < 0) { err = "bad d"; return false; }
      spec.p0 = u16(v);
      break;
    case Kind::Compare: {
      const auto m = kv.find("m");
      if (m != kv.end()) {
        const std::string o = lower(m->second);
        if (o == "compare" || o == "cmp" || o == "0") spec.p0 = 0;
        else if (o == "subtract" || o == "sub" || o == "1") spec.p0 = 1;
        else { err = "bad m"; return false; }
        kv.erase(m);
      }
      break;
    }
    case Kind::Clock: {
      std::uint64_t p = 2;
      std::uint64_t h = 1;
      if (take("p", false, p) < 0) { err = "bad p"; return false; }
      if (take("h", false, h) < 0) { err = "bad h"; return false; }
      spec.p0 = u16(p);
      spec.p1 = u16(h);
      break;
    }
  }
  const bool nibbleSeed = kind != Kind::Clock;
  if ((r = take("seed", nibbleSeed, seedValue)) < 0) { err = "bad seed"; return false; }
  haveSeed = r == 1;
  if (!kv.empty()) {
    err = cat("unexpected key '", kv.begin()->first, "' for ", kindToken(kind));
    return false;
  }
  seed = haveSeed ? u16(seedValue) : restSeed(spec);
  return true;
}

}  // namespace

bool parseLtrace(const std::string& text, std::vector<ParsedRow>& rows, std::string& err) {
  const std::vector<Line> lines = splitLines(text);
  if (lines.empty() || lines[0].tok.size() != 2 || lines[0].tok[0] != "ltrace" || lines[0].tok[1] != "1") {
    err = "missing 'ltrace 1' header";
    return false;
  }
  rows.clear();
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const Line& ln = lines[i];
    const std::string where = cat("line ", ln.number, ": ");
    std::uint64_t tau = 0;
    if (ln.tok.size() < 3 || ln.tok[0] != "row" || !parseDec(ln.tok[1], tau)) {
      err = where + "not a row";
      return false;
    }
    ParsedRow row;
    row.tau = tau;
    int section = 0;   // 1 in, 2 st, 3 cmd
    bool haveHash = false;
    for (std::size_t k = 2; k < ln.tok.size(); ++k) {
      const std::string& t = ln.tok[k];
      if (t == "|") continue;
      if (t == "in") { section = 1; continue; }
      if (t == "st") { section = 2; continue; }
      if (t == "cmd") { section = 3; continue; }
      if (t.compare(0, 2, "h=") == 0) {
        std::uint64_t h = 0;
        if (t.size() != 18) { err = where + "the hash must have 16 hex digits"; return false; }
        for (std::size_t d = 2; d < t.size(); ++d) {
          const char ch = t[d];
          unsigned v;
          if (ch >= '0' && ch <= '9') v = static_cast<unsigned>(ch - '0');
          else if (ch >= 'A' && ch <= 'F') v = static_cast<unsigned>(ch - 'A' + 10);
          else if (ch >= 'a' && ch <= 'f') v = static_cast<unsigned>(ch - 'a' + 10);
          else { err = where + "bad hash digit"; return false; }
          h = (h << 4) | v;
        }
        row.hash = h;
        haveHash = true;
        continue;
      }
      const std::size_t eq = t.find('=');
      std::uint64_t id = 0;
      if (eq == std::string::npos || section == 0 || !parseDec(t.substr(0, eq), id)) {
        err = where + cat("unexpected token '", t, "'");
        return false;
      }
      const std::string value = t.substr(eq + 1);
      std::uint64_t v = 0;
      if (section == 2) {
        row.st.emplace_back(u32(id), value);
      } else if (!parseNibbleLike(value, v) || v > 255) {
        err = where + cat("bad value in '", t, "'");
        return false;
      } else if (section == 1) {
        row.in.emplace_back(u32(id), u8(v));
      } else {
        row.cmd.emplace_back(u32(id), u8(v));
      }
    }
    if (!haveHash) {
      err = where + "no h= field";
      return false;
    }
    rows.push_back(std::move(row));
  }
  return true;
}

bool parseLnet(const std::string& text, Model& out, std::string& err) {
  const std::vector<Line> lines = splitLines(text);
  if (lines.empty() || lines[0].tok.size() != 2 || lines[0].tok[0] != "lnet" || lines[0].tok[1] != "1") {
    err = "missing 'lnet 1' header";
    return false;
  }
  Model m;
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const Line& ln = lines[i];
    if (ln.tok[0] == "c") {
      std::uint64_t id = 0;
      CompDef c;
      if (ln.tok.size() < 3 || !parseDec(ln.tok[1], id) || id == 0 || id > 0xFFFFFFFEULL) {
        err = cat("line ", ln.number, ": bad component line");
        return false;
      }
      c.id = u32(id);
      std::string e;
      if (!parseComp(ln.tok, 2, c.spec, c.seed, e)) {
        err = cat("line ", ln.number, ": ", e);
        return false;
      }
      m.comps.push_back(c);
    } else if (ln.tok[0] == "w") {
      Pin a, b;
      if (ln.tok.size() != 3 || !parsePin(ln.tok[1], a) || !parsePin(ln.tok[2], b)) {
        err = cat("line ", ln.number, ": bad wire line");
        return false;
      }
      m.wires.push_back(makeWire(a, b));
    } else {
      err = cat("line ", ln.number, ": unknown line '", ln.tok[0], "'");
      return false;
    }
  }
  m.sortAndNormalize();
  out = std::move(m);
  return true;
}

bool parseLscn(const std::string& text, Scenario& sc, std::string& err) {
  const std::vector<Line> lines = splitLines(text);
  if (lines.empty() || lines[0].tok.size() != 2 || lines[0].tok[0] != "lscn" || lines[0].tok[1] != "1") {
    err = "missing 'lscn 1' header";
    return false;
  }
  sc.ticks = 0;
  sc.inputs.clear();
  sc.edits.clear();
  for (std::size_t i = 1; i < lines.size(); ++i) {
    const Line& ln = lines[i];
    const std::string where = cat("line ", ln.number, ": ");
    if (ln.tok[0] == "ticks") {
      std::uint64_t n = 0;
      if (ln.tok.size() != 2 || !parseDec(ln.tok[1], n) || n > 100000000ULL) { err = where + "bad ticks"; return false; }
      sc.ticks = u32(n);
    } else if (ln.tok[0] == "i") {
      std::uint64_t tau = 0;
      if (ln.tok.size() < 2 || !parseDec(ln.tok[1], tau)) { err = where + "bad tick"; return false; }
      for (std::size_t k = 2; k < ln.tok.size(); ++k) {
        const std::size_t eq = ln.tok[k].find('=');
        std::uint64_t id = 0;
        std::uint64_t v = 0;
        if (eq == std::string::npos || !parseDec(ln.tok[k].substr(0, eq), id) ||
            !parseNibbleLike(ln.tok[k].substr(eq + 1), v) || v > 255) {
          err = where + cat("bad sensor order '", ln.tok[k], "'");
          return false;
        }
        sc.inputs.push_back(SensorOrder{tau, u32(id), u8(v)});
      }
    } else if (ln.tok[0] == "e") {
      std::uint64_t tau = 0;
      if (ln.tok.size() < 4 || !parseDec(ln.tok[1], tau)) { err = where + "bad edit line"; return false; }
      const std::string& verb = ln.tok[2];
      const std::string& what = ln.tok[3];
      EditOp op;
      std::string e;
      if (what == "c") {
        std::uint64_t id = 0;
        if (ln.tok.size() < 5 || !parseDec(ln.tok[4], id)) { err = where + "bad component id"; return false; }
        op.id = u32(id);
        if (verb == "del") {
          op.kind = EditKind::Remove;
        } else if (verb == "add" || verb == "rep") {
          op.kind = verb == "add" ? EditKind::Add : EditKind::Replace;
          if (!parseComp(ln.tok, 5, op.spec, op.seed, e)) { err = where + e; return false; }
        } else {
          err = where + "unknown edit verb";
          return false;
        }
      } else if (what == "w") {
        if (ln.tok.size() != 6 || !parsePin(ln.tok[4], op.a) || !parsePin(ln.tok[5], op.b)) { err = where + "bad wire edit"; return false; }
        if (verb == "add") op.kind = EditKind::Connect;
        else if (verb == "del") op.kind = EditKind::Disconnect;
        else { err = where + "unknown edit verb"; return false; }
      } else {
        err = where + "edit target must be c or w";
        return false;
      }
      sc.edits.push_back(TimedEdit{tau, op});
    } else {
      err = where + cat("unknown line '", ln.tok[0], "'");
      return false;
    }
  }
  return true;
}

bool readFile(const std::string& path, std::string& out) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f == nullptr) return false;
  std::string data;
  char buf[4096];
  std::size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
  std::fclose(f);
  out = std::move(data);
  return true;
}

bool writeFile(const std::string& path, const std::string& data) {
  std::FILE* f = std::fopen(path.c_str(), "wb");
  if (f == nullptr) return false;
  const std::size_t n = std::fwrite(data.data(), 1, data.size(), f);
  std::fclose(f);
  return n == data.size();
}

}  // namespace slgt
