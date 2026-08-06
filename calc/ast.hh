#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "location.hh"

namespace calc {

// Free variables are bound at evaluation time, not parse time:
// the same compiled program can be evaluated under many environments.
using environment = std::map<std::string, double>;

// Runtime evaluation failure (unknown variable, division by zero).
// Carries the source location the parser recorded in the node, so runtime
// errors point back into the original input.
struct eval_error : std::runtime_error {
  eval_error(parse::location l, const std::string& msg)
      : std::runtime_error(msg), where(std::move(l)) {}
  parse::location where;
};

// An `=` test whose sides did not agree; what() holds both trees and both
// values, ready for stderr.
struct test_failure : std::runtime_error {
  test_failure(parse::location l, const std::string& msg)
      : std::runtime_error(msg), where(std::move(l)) {}
  parse::location where;
};

class node {
public:
  explicit node(parse::location l) : loc_(std::move(l)) {}
  virtual ~node() = default;
  virtual double eval(const environment& env) const = 0;
  virtual void print(std::ostream& os) const = 0;
  const parse::location& where() const { return loc_; }

private:
  parse::location loc_;
};

using node_ptr = std::unique_ptr<node>;

std::ostream& operator<<(std::ostream& os, const node& n);

enum class op { add, sub, mul, div };

// Factories used while building.
node_ptr number(double value, parse::location l);
node_ptr variable(std::string name, parse::location l);
node_ptr binary(op o, node_ptr lhs, node_ptr rhs, parse::location l);

// The parser's result: the `=` tests to run, in input order, plus the final
// expression tree if the input left one.  Everything is still a compile
// tree — nothing was evaluated at parse time.
class program {
public:
  struct test {
    std::shared_ptr<const node> lhs, rhs;
    parse::location where;
  };

  program() = default;
  program(std::vector<test> tests, std::shared_ptr<const node> result)
      : tests_(std::move(tests)), result_(std::move(result)) {}

  // Run each test (throwing test_failure on the first mismatch), then
  // return the final value, if any.  May also throw eval_error.
  std::optional<double> operator()(const environment& env = {}) const;

  std::size_t test_count() const { return tests_.size(); }
  bool has_result() const { return result_ != nullptr; }

  // Prints the final expression tree ("<no value>" if the input ended
  // with a test).
  friend std::ostream& operator<<(std::ostream& os, const program& p) {
    if (p.result_) os << *p.result_; else os << "<no value>";
    return os;
  }

private:
  std::vector<test> tests_;
  std::shared_ptr<const node> result_;
};

// How the builder reports bad input (bound to driver_base::error).
using diagnostic = std::function<void(const parse::location&, const std::string&)>;

// Parse-time state for the RPN grammar: a pending operand and a stack of
// expression *trees* (not values!).  The grammar's actions drive it:
//
//   number/ident   pending := operand        (error if one is pending)
//   .              push pending              (HP-calculator ENTER)
//   + - * /        rhs := pending, else pop; push(pop <op> rhs)
//   =              rhs := pending, else pop; record test(pop == rhs)
//
// Errors are reported through the diagnostic and repaired with a
// placeholder so one mistake yields one message, not a cascade.
class builder {
public:
  explicit builder(diagnostic diag) : diag_(std::move(diag)) {}

  void reset();
  void operand(node_ptr value);
  void combine(op o, const parse::location& where);
  void push(const parse::location& where);   // "."
  void test(const parse::location& where);   // "="

  // Called by the start rule: turn the leftovers into the program.
  program finish(const parse::location& end);

private:
  node_ptr pop(const parse::location& where, const char* who);
  node_ptr take_rhs(const parse::location& where, const char* who);

  diagnostic diag_;
  node_ptr pending_;
  std::vector<node_ptr> stack_;
  std::vector<program::test> tests_;
};

}  // namespace calc
