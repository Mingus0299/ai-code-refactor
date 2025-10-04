#include "ai/AiEngine.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>

namespace aicr {

namespace {

static std::string toLower(std::string s) {
  for (auto& c : s) c = (char)std::tolower((unsigned char)c);
  return s;
}

class HeuristicAi final : public AiEngine {
public:
  // Human-friendly identifier suggestions (no numbers)
  std::optional<std::string>
  suggestIdentifier(const std::string& current,
                    const std::string& typeHint,
                    const std::string& usageHint) override
  {
    auto cur = toLower(current);
    auto isBad = [&]{
      return cur=="tmp" || cur=="data" || cur=="foo" || cur=="bar" || current.size()<=2;
    };
    if (!isBad()) return std::nullopt;

    const std::string t = toLower(typeHint);
    if (t.find("bool")   != std::string::npos) return std::string("flag");
    if (t.find("string") != std::string::npos) return std::string("text");
    if (t.find("vector") != std::string::npos) return std::string("values");
    if (t.find("map")    != std::string::npos) return std::string("lookup");
    if (t.find("set")    != std::string::npos) return std::string("items");
    if (t.find("size_t") != std::string::npos) return std::string("count");
    if (t.find("int")    != std::string::npos) return std::string("count");
    if (t.find("float")  != std::string::npos ||
        t.find("double") != std::string::npos) return std::string("value");
    if (t.find("char")   != std::string::npos) return std::string("ch");

    // fallback: combine usage/type, sanitize to a valid identifier
    std::string base = usageHint.empty() ? t : (toLower(usageHint) + "_" + t);
    if (base.empty()) base = "value";

    std::string out; out.reserve(base.size()+8);
    for (char c : base) {
      if (std::isalnum((unsigned char)c)) out.push_back((char)std::tolower((unsigned char)c));
      else if (!out.empty() && out.back()!='_') out.push_back('_');
    }
    if (out.empty() || std::isdigit((unsigned char)out[0])) out.insert(out.begin(), '_');
    out.erase(std::unique(out.begin(), out.end(),
               [](char a,char b){ return a=='_' && b=='_'; }), out.end());
    return out;
  }

  std::optional<std::string>
  docForSignature(const std::string& sig) override {
    std::string out;
    out += "/**\n";
    out += " * @brief TODO: describe " + sig + "\n";
    out += " * @details Auto-generated doc stub. Fill in behavior, edge cases, and invariants.\n";
    out += " */\n";
    return out;
  }
};

} // namespace

AiEngine* makeHeuristicAi() { return new HeuristicAi(); }

// If ONNX isn’t enabled, provide a fallback factory here
// When ONNX is enabled, OnnxAiEngine.cpp provides the real one
#ifndef ENABLE_ONNXRUNTIME
AiEngine* makeOnnxAi(const std::string&) { return makeHeuristicAi(); }
#endif

} // namespace aicr
