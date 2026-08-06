// franken-exe — one binary, several parsers from one library.
//
// Arguments are find(1)-style groups, each handed to the named parser:
//
//   franken-exe calc\( 2 . 3 + 5 - 0 = \) cmds\( 'print "hi";' \)
//
// A group's words are joined with spaces to form the source text; a word of
// the form @path makes the group read that file instead.  Groups run in
// argument order; the first failure stops the run.
//
// Exit status: 0 all groups succeeded; -1 (i.e. 255) a calc "=" test
// failed, with both values on stderr; 1 parse or runtime error;
// 2 bad command line.

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "calc/ast.hh"
#include "calc/driver.hh"
#include "cmds/driver.hh"
#include "cmds/program.hh"

namespace {

int usage(const std::string& msg) {
  std::cerr << "franken-exe: " << msg
            << "\nusage: franken-exe [-p] [-s]"
               " { calc( <rpn words> ) | cmds( <script> ) }...\n"
               "       (quote the parens from your shell:  calc\\( ... \\))\n";
  return 2;
}

// Parse either the group's text or, for "@path", the named file.
template <typename Driver>
auto parse_group(Driver& drv, const std::string& text, const std::string& name)
    -> decltype(drv.parse_string(text)) {
  if (text.size() > 1 && text[0] == '@')
    return drv.parse_file(text.substr(1));
  return drv.parse_string(text, name);
}

int run_calc(calc::driver& drv, const std::string& text) {
  static const calc::environment env = {
      {"pi", 3.141592653589793},
      {"e", 2.718281828459045},
  };
  const auto prog = parse_group(drv, text, "<calc>");
  if (!prog) return 1;  // errors already reported by the driver
  try {
    if (const auto value = (*prog)(env))
      std::cout << *prog << " = " << *value << '\n';
    return 0;
  } catch (const calc::test_failure& e) {
    std::cerr << e.where << ": test failed: " << e.what() << '\n';
    return -1;
  } catch (const calc::eval_error& e) {
    std::cerr << e.where << ": runtime error: " << e.what() << '\n';
    return 1;
  }
}

int run_cmds(cmds::driver& drv, const std::string& text) {
  const auto prog = parse_group(drv, text, "<cmds>");
  if (!prog) return 1;
  (*prog)(std::cout);
  return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
  struct group {
    std::string lang, text;
  };
  std::vector<group> groups;
  bool trace_parsing = false, trace_scanning = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-p") { trace_parsing = true; continue; }
    if (arg == "-s") { trace_scanning = true; continue; }
    if (arg.size() > 1 && arg.back() == '(') {
      group g{arg.substr(0, arg.size() - 1), {}};
      if (g.lang != "calc" && g.lang != "cmds")
        return usage("unknown parser '" + g.lang + "' (have: calc, cmds)");
      bool closed = false;
      while (++i < argc) {
        const std::string word = argv[i];
        if (word == ")") { closed = true; break; }
        if (!g.text.empty()) g.text += ' ';
        g.text += word;
      }
      if (!closed) return usage("missing ')' after '" + g.lang + "('");
      groups.push_back(std::move(g));
      continue;
    }
    return usage("stray argument '" + arg + "'");
  }
  if (groups.empty()) return usage("nothing to do");

  calc::driver calc_drv;
  cmds::driver cmds_drv;
  calc_drv.trace_parsing(trace_parsing);
  calc_drv.trace_scanning(trace_scanning);
  cmds_drv.trace_parsing(trace_parsing);
  cmds_drv.trace_scanning(trace_scanning);

  for (const auto& g : groups) {
    const int rc = (g.lang == "calc") ? run_calc(calc_drv, g.text)
                                      : run_cmds(cmds_drv, g.text);
    if (rc != 0) return rc;
  }
  return 0;
}
