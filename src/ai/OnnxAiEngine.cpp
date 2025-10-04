#include "ai/AiEngine.hpp"
#ifdef ENABLE_ONNXRUNTIME
#include <onnxruntime_cxx_api.h>
#include <memory>
#include <stdexcept>

namespace aicr {

namespace {
class OnnxAi final : public AiEngine {
  Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "aicr"};
  Ort::Session session{nullptr};
  Ort::SessionOptions opts;

public:
  explicit OnnxAi(const std::string& modelPath) {
    opts.SetIntraOpNumThreads(1);
    session = Ort::Session(env, modelPath.c_str(), opts);
  }

  std::optional<std::string> suggestIdentifier(const std::string& current,
                                               const std::string& typeHint,
                                               const std::string& usageHint) override {
    // NOTE: placeholder wiring for a tiny model that maps (current,type,usage) → suggestion.
    // Tokenization/encoding is model-dependent; replace with your real pre/post-processing.
    // Delegated to a heuristic since haven't built an encoder yet
    (void)typeHint; (void)usageHint;
    if (current.size() <= 3) {
      return std::optional<std::string>{"improvedName"}; // replace with real inference result
    }
    return std::nullopt;
  }

  std::optional<std::string> docForSignature(const std::string& sig) override {
    // Stub: model would generate summary text; emulate it here
    return std::optional<std::string>{"/** @brief " + sig + " — auto-doc (replace with model output) */\n"};
  }
};
} // namespace

AiEngine* makeOnnxAi(const std::string& modelPath) { return new OnnxAi(modelPath); }

} // namespace aicr
#endif
