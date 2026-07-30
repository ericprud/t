#pragma once

#include <map>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>

#include "location.hh"

namespace calc {

// Free variables are bound at evaluation time, not parse time:
// the same compiled expression can be evaluated under many environments.
using environment = std::map<std::string, double>;

// Runtime evaluation failure (unknown variable, division by zero).
// Carries the source location the parser recorded in the node, so runtime
// errors point back into the original input.
struct eval_error : std::runtime_error {
  eval_error(parse::location l, const std::string& msg)
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

// Factories used by the grammar's semantic actions.
node_ptr number(double value, parse::location l);
node_ptr variable(std::string name, parse::location l);
node_ptr binary(op o, node_ptr lhs, node_ptr rhs, parse::location l);
node_ptr negate(node_ptr operand, parse::location l);

// The parser's result: a compiled expression tree.  Calling it evaluates
// the tree; nothing was computed at parse time.
class expression {
public:
  expression() = default;
  explicit expression(node_ptr root) : root_(std::move(root)) {}

  explicit operator bool() const { return root_ != nullptr; }

  double operator()(const environment& env = {}) const {
    if (!root_) throw std::logic_error("evaluating an empty expression");
    return root_->eval(env);
  }

  friend std::ostream& operator<<(std::ostream& os, const expression& e) {
    if (e.root_) os << *e.root_; else os << "<empty>";
    return os;
  }

private:
  // shared_ptr (not unique_ptr) so expression is a copyable value type;
  // the tree is immutable after parsing, so sharing is safe.
  std::shared_ptr<const node> root_;
};

}  // namespace calc
