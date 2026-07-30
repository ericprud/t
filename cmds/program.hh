#pragma once

#include <cstddef>
#include <functional>
#include <iostream>
#include <utility>
#include <vector>

namespace cmds {

// One executable statement, built by a semantic action in cmds/parser.yy.
// The action's lambda captures the $1..$n semantic values it needs from the
// reduce stack; running the program later replays them.
using step = std::function<void(std::ostream&)>;

// The parser's result: call it to execute the parsed script.
class program {
public:
  void push(step s) {
    if (s) steps_.push_back(std::move(s));  // error recovery yields empty steps
  }

  std::size_t size() const { return steps_.size(); }

  void operator()(std::ostream& os = std::cout) const {
    for (const auto& s : steps_) s(os);
  }

private:
  std::vector<step> steps_;
};

}  // namespace cmds
