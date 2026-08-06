#pragma once

#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "location.hh"  // bison-generated once, shared by every parser

namespace common {

// CRTP base shared by all parse drivers in the project.
//
// Each language supplies three things:
//   Parser  — its bison-generated parser class (parse::calc_parser, ...)
//   Result  — the value its start rule deposits via set_result()
//   Derived — itself, providing the scanner hooks
//               bool scan_begin_file(const std::string& path);
//               void scan_begin_string(const std::string& text);
//               void scan_end();
//             These are member functions *defined in the language's .ll file*,
//             because only there are the flex buffer primitives visible.
//
// The base owns everything the languages would otherwise duplicate: the
// current location, diagnostics, trace flags, and the parse-run scaffolding.
template <typename Derived, typename Parser, typename Result>
class driver_base {
public:
  using result_type = Result;

  // Parse a file ("-" means stdin).  Returns nullopt on any error.
  std::optional<Result> parse_file(const std::string& path) {
    file_ = (path == "-") ? "<stdin>" : path;
    if (!self().scan_begin_file(path)) {
      std::cerr << file_ << ": cannot open\n";
      return std::nullopt;
    }
    return run();
  }

  std::optional<Result> parse_string(const std::string& text,
                                     const std::string& name = "<string>") {
    file_ = name;
    self().scan_begin_string(text);
    return run();
  }

  // ---- interface used by the scanner and by grammar actions ----
  parse::location& location() { return loc_; }

  void set_result(Result r) { result_ = std::move(r); }

  void error(const parse::location& l, const std::string& msg) {
    std::cerr << l << ": error: " << msg << '\n';
    ++num_errors_;
  }

  int num_errors() const { return num_errors_; }

  bool trace_scanning() const { return trace_scanning_; }
  void trace_scanning(bool on) { trace_scanning_ = on; }
  bool trace_parsing() const { return trace_parsing_; }
  void trace_parsing(bool on) { trace_parsing_ = on; }

  // CRTP hook: run() calls self().begin_parse() before each parse.
  // Shadow it in a derived driver to reset per-parse state (see
  // calc::driver, which resets its builder here).
  void begin_parse() {}

protected:
  driver_base() = default;
  ~driver_base() = default;

  // Scanner bookkeeping used by the scan_* hooks in the .ll files.
  // YY_BUFFER_STATE is scanner-local, hence the opaque pointer.
  std::FILE* scan_file_ = nullptr;   // owned FILE* to close, if any
  void* scan_buffer_ = nullptr;      // string-scan buffer to free, if any

private:
  Derived& self() { return static_cast<Derived&>(*this); }

  std::optional<Result> run() {
    num_errors_ = 0;
    result_.reset();
    loc_.initialize(&file_);
    self().begin_parse();
    Parser parser(self());
    parser.set_debug_level(trace_parsing_);
    const int status = parser.parse();
    self().scan_end();
    if (status != 0 || num_errors_ != 0) return std::nullopt;
    return std::move(result_);
  }

  std::string file_;              // location() points at this; keep it stable
  parse::location loc_;
  std::optional<Result> result_;
  int num_errors_ = 0;
  bool trace_scanning_ = false;
  bool trace_parsing_ = false;
};

}  // namespace common
