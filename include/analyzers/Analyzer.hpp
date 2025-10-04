#pragma once
#include "analyzers/Issue.hpp"
#include <string>
#include <vector>
#include <memory>

namespace aicr {

struct AnalyzeOptions {
  int longFunctionLineThreshold = 80;
  bool suggestDocs = true;
  bool suggestBetterVarNames = true;
  bool fix = false;           // apply edits
  bool backup = true;         // keep .bak copies before writing
  bool parseAllComments = true;
  std::vector<std::string> extraArgs; // extra compiler args for ClangTool
};

class AiEngine; // fwd-decl

class Analyzer {
public:
  virtual ~Analyzer() = default;
  virtual bool analyzePaths(const std::vector<std::string>& paths,
                            const AnalyzeOptions& opts,
                            AiEngine* ai,
                            std::vector<Issue>& out) = 0;
};

std::unique_ptr<Analyzer> makeCppAnalyzer();

} // namespace aicr

