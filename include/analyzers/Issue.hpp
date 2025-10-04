#pragma once
#include <string>
#include <vector>
#include <optional>

namespace aicr {

enum class Severity { Info, Warning, Error };

struct FixIt {
  std::string file;
  unsigned    offset = 0;     // byte offset in file
  unsigned    length = 0;     // bytes to replace
  std::string replacement;    // replacement text
  std::string note;           // human-friendly description
};

struct Issue {
  std::string id;             // eg. "LONG_FUNC", "MISSING_DOC"
  Severity    severity = Severity::Warning;
  std::string message;
  std::string file;
  unsigned    line = 0;
  unsigned    column = 0;
  std::vector<FixIt> fixes;   // zero or more automated fixes
};

} // namespace aicr
