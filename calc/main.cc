// calc — parse each argument (or each stdin line) as an arithmetic
// expression, print the compiled tree and its value.
//
//   calc "1 + 2*(3+4)"      one-shot
//   echo "2*pi" | calc      stdin, one expression per line
//   calc -p "1+2"           with parser trace; -s traces the scanner
//
// Demonstrates the point of the compile tree: parse once, then call
// expression::operator()(environment) with whatever bindings you like.

#include <iostream>
#include <string>
#include <vector>

#include "calc/ast.hh"
#include "calc/driver.hh"

int main(int argc, char* argv[]) {
  calc::driver drv;
  std::vector<std::string> inputs;
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-p") drv.trace_parsing(true);
    else if (arg == "-s") drv.trace_scanning(true);
    else inputs.push_back(arg);
  }

  const calc::environment env = {
      {"pi", 3.141592653589793},
      {"e", 2.718281828459045},
  };

  int failures = 0;
  const auto evaluate = [&](const std::string& text) {
    const auto expr = drv.parse_string(text);
    if (!expr) {  // syntax errors were already reported by the driver
      ++failures;
      return;
    }
    try {
      const double value = (*expr)(env);  // evaluate before printing anything
      std::cout << *expr << " = " << value << '\n';
    } catch (const calc::eval_error& e) {
      std::cerr << e.where << ": runtime error: " << e.what() << '\n';
      ++failures;
    }
  };

  if (inputs.empty()) {
    std::string line;
    while (std::getline(std::cin, line))
      if (!line.empty()) evaluate(line);
  } else {
    for (const auto& input : inputs) evaluate(input);
  }
  return failures == 0 ? 0 : 1;
}
