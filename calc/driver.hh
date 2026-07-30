#pragma once

#include <string>

#include "calc/ast.hh"
#include "calc_parser.hh"
#include "common/driver_base.hh"

namespace calc {

// Everything interesting lives in the shared base; this class only names the
// parser/result pair and carries the scanner hooks, whose definitions sit in
// calc/scanner.ll (they need the flex buffer primitives).
class driver
    : public common::driver_base<driver, parse::calc_parser, expression> {
public:
  bool scan_begin_file(const std::string& path);
  void scan_begin_string(const std::string& text);
  void scan_end();
};

}  // namespace calc

// The scanner entry point the parser calls: flex generates its definition
// (see YY_DECL in calc/scanner.ll).  Each scanner gets a distinct name via
// `%option prefix`, which is what lets several scanners link into one
// binary; the parser is pointed at it with `#define yylex calclex`.
parse::calc_parser::symbol_type calclex(calc::driver& drv);
