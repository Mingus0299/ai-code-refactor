#include "refactor/RefactorEngine.hpp"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <filesystem>

namespace fs = std::filesystem;
namespace aicr {

static bool readFile(const std::string& path, std::string& out) {
  std::ifstream ifs(path, std::ios::binary);
  if (!ifs) return false;
  std::ostringstream ss; ss << ifs.rdbuf(); out = ss.str();
  return true;
}

static bool writeFile(const std::string& path, const std::string& data) {
  std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
  if (!ofs) return false;
  ofs << data;
  return true;
}

bool RefactorEngine::applyFixes(const std::vector<FixIt>& fixes, bool backup, std::string* error) {
  // group fixes by file
  std::unordered_map<std::string, std::vector<FixIt>> byFile;
  for (const auto& f : fixes) byFile[f.file].push_back(f);

  for (auto& [file, vec] : byFile) {
    std::string content;
    if (!readFile(file, content)) {
      if (error) *error = "Failed to read " + file;
      return false;
    }

    if (backup) {
      std::error_code ec;
      fs::copy_file(file, file + ".bak", fs::copy_options::overwrite_existing, ec);
    }

    // apply from highest offset → lowest to keep offsets valid
    std::sort(vec.begin(), vec.end(), [](const FixIt& a, const FixIt& b) {
      return a.offset > b.offset;
    });

    for (const auto& f : vec) {
      if (f.offset + f.length > content.size()) {
        if (error) *error = "Out-of-range fix in " + file;
        return false;
      }
      content.replace(f.offset, f.length, f.replacement);
    }

    if (!writeFile(file, content)) {
      if (error) *error = "Failed to write " + file;
      return false;
    }
  }
  return true;
}

} // namespace aicr
