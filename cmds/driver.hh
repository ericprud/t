#pragma once

#include <string>

#include "cmds/program.hh"
#include "cmds_parser.hh"
#include "common/driver_base.hh"

namespace cmds {

// Mirror of calc::driver — see common/driver_base.hh for the shared logic.
// The scanner hooks are defined in cmds/scanner.ll.
class driver
    : public common::driver_base<driver, parse::cmds_parser, program> {
public:
  bool scan_begin_file(const std::string& path);
  void scan_begin_string(const std::string& text);
  void scan_end();
};

}  // namespace cmds

// Scanner entry point (defined by flex, YY_DECL in cmds/scanner.ll);
// distinct from calclex thanks to `%option prefix="cmds"`.
parse::cmds_parser::symbol_type cmdslex(cmds::driver& drv);
