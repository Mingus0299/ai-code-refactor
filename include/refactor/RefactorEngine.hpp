#pragma once
#include "analyzers/Issue.hpp"
#include <string>
#include <vector>

namespace aicr {

// Simple file-level apply that creates .bak backups if requested.
// Offsets/lengths are byte-based in original file content.
class RefactorEngine {
public:
  static bool applyFixes(const std::vector<FixIt>& fixes, bool backup, std::string* error);
};

} // namespace aicr
