#include "analyzers/Analyzer.hpp"
#include "ai/AiEngine.hpp"

#include "llvm/Support/CommandLine.h"
#include <iostream>
#include <memory>
#include <filesystem> 

using namespace aicr;

static llvm::cl::OptionCategory ToolCat("aicr options");

static llvm::cl::list<std::string> Paths(
  "paths", llvm::cl::desc("Source files or directories to analyze (recursive)"),
  llvm::cl::OneOrMore, llvm::cl::cat(ToolCat));

static llvm::cl::opt<int> LongFnThresh(
  "long-fn", llvm::cl::desc("Long function threshold (lines)"),
  llvm::cl::init(80), llvm::cl::cat(ToolCat));

static llvm::cl::opt<bool> Fix(
  "fix", llvm::cl::desc("Apply available fixes"), llvm::cl::init(false),
  llvm::cl::cat(ToolCat));

static llvm::cl::opt<bool> NoBackup(
  "no-backup", llvm::cl::desc("Do not write .bak backups when applying fixes"),
  llvm::cl::init(false), llvm::cl::cat(ToolCat));

static llvm::cl::opt<bool> NoDocs(
  "no-docs", llvm::cl::desc("Disable doc stub suggestions"),
  llvm::cl::init(false), llvm::cl::cat(ToolCat));

static llvm::cl::opt<bool> NoNames(
  "no-names", llvm::cl::desc("Disable variable naming suggestions"),
  llvm::cl::init(false), llvm::cl::cat(ToolCat));

static llvm::cl::opt<std::string> OnnxModel(
  "onnx-model", llvm::cl::desc("Path to ONNX model (enables ONNX engine)"),
  llvm::cl::init(""), llvm::cl::cat(ToolCat));

int main(int argc, const char** argv) {
  llvm::cl::HideUnrelatedOptions(ToolCat);
  llvm::cl::ParseCommandLineOptions(argc, argv, "AI-powered code search & refactor (MVP)\n");

  AnalyzeOptions opts;
  opts.longFunctionLineThreshold = LongFnThresh;
  opts.suggestDocs = !NoDocs;
  opts.suggestBetterVarNames = !NoNames;
  opts.fix = Fix;
  opts.backup = !NoBackup;

  std::unique_ptr<AiEngine> ai;
#ifdef ENABLE_ONNXRUNTIME
  if (!OnnxModel.empty()) ai.reset(makeOnnxAi(OnnxModel));
#endif
  if (!ai) ai.reset(makeHeuristicAi());

  auto cpp = makeCppAnalyzer();

  std::vector<std::string> files;

  // Expand directories recursively
  auto addPath = [&](const std::string& p){
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::file_status st = fs::status(p, ec);
    if (ec) return;
    if (fs::is_directory(st)) {
      for (auto& e : fs::recursive_directory_iterator(p, fs::directory_options::follow_directory_symlink)) {
        const auto& path = e.path();
        auto ext = path.extension().string();
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".c" ||
            ext == ".hpp" || ext == ".hh" || ext == ".h")
          files.push_back(path.string());
      }
    } else {
      files.push_back(p);
    }
  };
  for (const auto& p : Paths) addPath(p);

  std::vector<Issue> issues;
  if (!cpp->analyzePaths(files, opts, ai.get(), issues)) {
    std::cerr << "Analysis failed.\n";
    return 1;
  }

  // Pretty-print results
  for (const auto& i : issues) {
    std::cout << i.file << ":" << i.line << ":" << i.column
              << " [" << i.id << "] " << i.message << "\n";
    for (const auto& f : i.fixes) {
      std::cout << "  fix: " << f.note << " (offset " << f.offset
                << ", len " << f.length << ")\n";
    }
  }

  if (opts.fix) std::cout << "\nApplied fixes where available.\n";
  return 0;
}
