#pragma once
#include <optional>
#include <string>

namespace aicr {

// Very small interface: ask for a better identifier name or doc stub
class AiEngine {
public:
  virtual ~AiEngine() = default;

  // Suggest a better identifier name given current name + brief context
  virtual std::optional<std::string>
  suggestIdentifier(const std::string& current,
                    const std::string& typeHint,
                    const std::string& usageHint) = 0;

  // Produce a docstring snippet (eg. Doxygen) for a function signature
  virtual std::optional<std::string>
  docForSignature(const std::string& signature) = 0;
};

// Factory for a heuristic no-ML engine — always available
AiEngine* makeHeuristicAi();

// Factory for an ONNX backed engine only if compiled in
AiEngine* makeOnnxAi(const std::string& modelPath);

} // namespace aicr
