// cmds — parse and run scripts in the cmds language.
//
//   cmds examples/hello.cmds        run script files ("-" for stdin)
//   cmds -e 'print "hi";'           run inline source
//   cmds -p script.cmds             with parser trace; -s traces the scanner
//
// Parsing yields a cmds::program; executing it is a separate step, which is
// the point: the closures built by the semantic actions carry the $1..$n
// values until operator() replays them.

#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "cmds/driver.hh"
#include "cmds/program.hh"

int main(int argc, char* argv[]) {
  cmds::driver drv;
  std::vector<std::string> files;
  std::vector<std::string> inline_sources;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-p") drv.trace_parsing(true);
    else if (arg == "-s") drv.trace_scanning(true);
    else if (arg == "-e" && i + 1 < argc) inline_sources.push_back(argv[++i]);
    else files.push_back(arg);
  }
  if (files.empty() && inline_sources.empty()) files.push_back("-");

  int failures = 0;
  const auto run = [&](std::optional<cmds::program> prog) {
    if (!prog) ++failures;   // errors were already reported by the driver
    else (*prog)(std::cout); // execute the parsed script
  };

  for (const auto& src : inline_sources) run(drv.parse_string(src, "<-e>"));
  for (const auto& file : files) run(drv.parse_file(file));
  return failures == 0 ? 0 : 1;
}
