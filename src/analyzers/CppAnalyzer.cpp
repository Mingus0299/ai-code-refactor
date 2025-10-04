#include "clang/Tooling/ArgumentsAdjusters.h"
#include "analyzers/Analyzer.hpp"
#include "ai/AiEngine.hpp"
#include "refactor/RefactorEngine.hpp"

#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "clang/Tooling/CompilationDatabase.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RawCommentList.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Lex/Lexer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <fstream>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;

namespace aicr {

static unsigned locSpan(const SourceManager& SM, SourceRange SR) {
  auto b = SM.getSpellingLineNumber(SR.getBegin());
  auto e = SM.getSpellingLineNumber(SR.getEnd());
  if (e < b) return 0;
  return (unsigned)(e - b + 1);
}

struct Context {
  const AnalyzeOptions* opts = nullptr;
  AiEngine* ai = nullptr;
  std::vector<Issue>* out = nullptr;
};

class LongFunctionCB : public MatchFinder::MatchCallback {
  Context& Ctx;
public:
  explicit LongFunctionCB(Context& c) : Ctx(c) {}
  void run(const MatchFinder::MatchResult& Result) override {
    const auto* FD = Result.Nodes.getNodeAs<FunctionDecl>("func");
    if (!FD || !FD->hasBody()) return;
    const auto& SM = *Result.SourceManager;
    unsigned lines = locSpan(SM, FD->getSourceRange());
    if ((int)lines >= Ctx.opts->longFunctionLineThreshold) {
      Issue is;
      is.id = "LONG_FUNC";
      is.severity = Severity::Warning;
      is.message = "Function '" + FD->getNameAsString() + "' is " + std::to_string(lines) +
                   " lines (threshold " + std::to_string(Ctx.opts->longFunctionLineThreshold) + ")";
      auto PL = SM.getPresumedLoc(FD->getBeginLoc());
      is.file = PL.getFilename() ? PL.getFilename() : "";
      is.line = PL.getLine(); is.column = PL.getColumn();
      Ctx.out->push_back(std::move(is));
    }
  }
};

class MissingDocCB : public MatchFinder::MatchCallback {
  Context& Ctx;
public:
  explicit MissingDocCB(Context& c) : Ctx(c) {}
  void run(const MatchFinder::MatchResult& Result) override {
    const auto* FD = Result.Nodes.getNodeAs<FunctionDecl>("func2");
    if (!FD || !FD->hasBody()) return;

    // Clang 18+ use ASTContext to check raw comments
    const RawComment* RC = Result.Context->getRawCommentForDeclNoCache(FD);
    if (RC) return; // already documented

    const auto& SM = *Result.SourceManager;
    auto PL = SM.getPresumedLoc(FD->getBeginLoc());

    Issue is;
    is.id = "MISSING_DOC";
    is.severity = Severity::Info;
    is.message = "Missing API docs for function '" + FD->getNameAsString() + "'";
    is.file = PL.getFilename() ? PL.getFilename() : "";
    is.line = PL.getLine(); is.column = PL.getColumn();

    if (Ctx.opts->suggestDocs && Ctx.ai) {
      std::string sig = FD->getReturnType().getAsString() + std::string(" ") + FD->getNameAsString() + "(";
      for (unsigned i = 0; i < FD->getNumParams(); ++i) {
        if (i) sig += ", ";
        sig += FD->getParamDecl(i)->getType().getAsString();
      }
      sig += ")";
      if (auto doc = Ctx.ai->docForSignature(sig)) {
        // Insert doc immediately before function
        auto Off = SM.getFileOffset(FD->getSourceRange().getBegin());
        FixIt fx;
        fx.file = is.file;
        fx.offset = Off;
        fx.length = 0;
        fx.replacement = *doc;
        fx.note = "Insert Doxygen stub";
        is.fixes.push_back(std::move(fx));
      }
    }
    Ctx.out->push_back(std::move(is));
  }
};

class WeakVarNameCB : public MatchFinder::MatchCallback {
  Context& Ctx;
public:
  explicit WeakVarNameCB(Context& c) : Ctx(c) {}
  void run(const MatchFinder::MatchResult& Result) override {
    const auto* VD = Result.Nodes.getNodeAs<VarDecl>("var");
    if (!VD || !VD->isLocalVarDeclOrParm()) return;
    const auto& SM = *Result.SourceManager;
    auto name = VD->getNameAsString();
    if (name == "i" || name == "j" || name == "k") return;

    std::string typeHint = VD->getType().getAsString();
    std::string usageHint = VD->isConstexpr() ? "const" : (VD->getType().isConstQualified() ? "const" : "");
    auto suggestion = Ctx.ai ? Ctx.ai->suggestIdentifier(name, typeHint, usageHint) : std::nullopt;
    if (!suggestion) return;

    Issue is;
    is.id = "WEAK_NAME";
    is.severity = Severity::Info;
    is.message = "Variable '" + name + "' could be clearer, e.g. '" + *suggestion + "'";
    auto PL = SM.getPresumedLoc(VD->getLocation());
    is.file = PL.getFilename() ? PL.getFilename() : "";
    is.line = PL.getLine(); is.column = PL.getColumn();

    // Rename at declaration token (safe MVP)
    SourceLocation NameLoc = VD->getLocation();
    auto TokRange = CharSourceRange::getTokenRange(NameLoc, NameLoc);
    auto BegOff = SM.getFileOffset(TokRange.getBegin());
    auto Len = Lexer::MeasureTokenLength(NameLoc, SM, Result.Context->getLangOpts());

    FixIt fx; fx.file = is.file; fx.offset = BegOff; fx.length = Len; fx.replacement = *suggestion;
    fx.note = "Rename at declaration (MVP)";
    is.fixes.push_back(std::move(fx));

    Ctx.out->push_back(std::move(is));
  }
};

class CppAnalyzerImpl : public Analyzer {
public:
  bool analyzePaths(const std::vector<std::string>& paths,
                    const AnalyzeOptions& opts,
                    AiEngine* ai,
                    std::vector<Issue>& out) override
  {
    if (paths.empty()) return false;

    std::string err;
    // Prefer source autodetect; fall back to current dir
    auto Compilations = CompilationDatabase::autoDetectFromSource(paths.front(), err);
    if (!Compilations) {
      auto Fallback = CompilationDatabase::autoDetectFromDirectory(".", err);
      if (Fallback) Compilations = std::move(Fallback);
    }
    if (!Compilations) {
      llvm::errs() << "Compilation DB not found (" << err << "). "
                   << "Generate compile_commands.json for the project.\n";
      return false;
    }

    // Extra compiler args
    std::vector<std::string> args = opts.extraArgs;
    if (opts.parseAllComments) args.push_back("-fparse-all-comments");

    ClangTool Tool(*Compilations, paths);

std::string resDir;
if (const char* rd = std::getenv("CLANG_RESOURCE_DIR")) {
  resDir = rd;
} else {

  resDir = "/opt/homebrew/opt/llvm@18/lib/clang/18";
}

using tooling::ArgumentInsertPosition;
using tooling::getInsertArgumentAdjuster;

Tool.appendArgumentsAdjuster(
  getInsertArgumentAdjuster({"-resource-dir", resDir}, ArgumentInsertPosition::BEGIN));

if (const char* sdk = std::getenv("SDKROOT")) {
  Tool.appendArgumentsAdjuster(
    getInsertArgumentAdjuster({"-isysroot", sdk}, ArgumentInsertPosition::BEGIN));
}


    Context Ctx;
    Ctx.opts = &opts; Ctx.ai = ai; Ctx.out = &out;

    MatchFinder Finder;

    LongFunctionCB longCB(Ctx);
    MissingDocCB  docCB(Ctx);
    WeakVarNameCB nameCB(Ctx);

    auto LongFuncMatcher =
      functionDecl(isDefinition(), hasBody(compoundStmt()), isExpansionInMainFile())
        .bind("func");

    auto MissingDocMatcher =
      functionDecl(isDefinition(), hasBody(compoundStmt()), isExpansionInMainFile())
        .bind("func2");

    auto WeakVarMatcher =
      varDecl(isExpansionInMainFile(), unless(parmVarDecl())).bind("var");

    Finder.addMatcher(LongFuncMatcher, &longCB);
    Finder.addMatcher(MissingDocMatcher, &docCB);
    Finder.addMatcher(WeakVarMatcher, &nameCB);

    auto res = Tool.run(newFrontendActionFactory(&Finder).get());
    if (res != 0) return false;

    // Apply fixes if requested
    if (opts.fix) {
      std::vector<FixIt> all;
      for (auto& i : out) for (auto& f : i.fixes) all.push_back(std::move(f));
      std::string e;
      if (!RefactorEngine::applyFixes(all, opts.backup, &e)) {
        llvm::errs() << "Apply failed: " << e << "\n";
        return false;
      }
    }
    return true;
  }
};

std::unique_ptr<Analyzer> makeCppAnalyzer() {
  return std::make_unique<CppAnalyzerImpl>();
}

} // namespace aicr
