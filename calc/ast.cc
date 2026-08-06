#include "calc/ast.hh"

#include <sstream>
#include <utility>

namespace calc {
namespace {

class number_node final : public node {
public:
  number_node(double v, parse::location l) : node(std::move(l)), value_(v) {}
  double eval(const environment&) const override { return value_; }
  void print(std::ostream& os) const override { os << value_; }

private:
  double value_;
};

class variable_node final : public node {
public:
  variable_node(std::string n, parse::location l)
      : node(std::move(l)), name_(std::move(n)) {}

  double eval(const environment& env) const override {
    auto it = env.find(name_);
    if (it == env.end())
      throw eval_error(where(), "undefined variable '" + name_ + "'");
    return it->second;
  }

  void print(std::ostream& os) const override { os << name_; }

private:
  std::string name_;
};

class binary_node final : public node {
public:
  binary_node(op o, node_ptr lhs, node_ptr rhs, parse::location l)
      : node(std::move(l)), op_(o), lhs_(std::move(lhs)), rhs_(std::move(rhs)) {}

  double eval(const environment& env) const override {
    const double l = lhs_->eval(env);
    const double r = rhs_->eval(env);
    switch (op_) {
      case op::add: return l + r;
      case op::sub: return l - r;
      case op::mul: return l * r;
      case op::div:
        if (r == 0) throw eval_error(where(), "division by zero");
        return l / r;
    }
    throw std::logic_error("unreachable: bad calc::op");
  }

  void print(std::ostream& os) const override {
    os << '(' << *lhs_ << ' ' << symbol() << ' ' << *rhs_ << ')';
  }

private:
  char symbol() const {
    switch (op_) {
      case op::add: return '+';
      case op::sub: return '-';
      case op::mul: return '*';
      case op::div: return '/';
    }
    return '?';
  }

  op op_;
  node_ptr lhs_, rhs_;
};

}  // namespace

std::ostream& operator<<(std::ostream& os, const node& n) {
  n.print(os);
  return os;
}

node_ptr number(double value, parse::location l) {
  return std::make_unique<number_node>(value, std::move(l));
}

node_ptr variable(std::string name, parse::location l) {
  return std::make_unique<variable_node>(std::move(name), std::move(l));
}

node_ptr binary(op o, node_ptr lhs, node_ptr rhs, parse::location l) {
  return std::make_unique<binary_node>(o, std::move(lhs), std::move(rhs),
                                       std::move(l));
}

// ---- program --------------------------------------------------------

std::optional<double> program::operator()(const environment& env) const {
  for (const auto& t : tests_) {
    const double lhs = t.lhs->eval(env);
    const double rhs = t.rhs->eval(env);
    if (lhs != rhs) {
      std::ostringstream msg;
      msg << *t.lhs << " = " << lhs << "  !=  " << *t.rhs << " = " << rhs;
      throw test_failure(t.where, msg.str());
    }
  }
  if (result_) return result_->eval(env);
  return std::nullopt;
}

// ---- builder --------------------------------------------------------

void builder::reset() {
  pending_.reset();
  stack_.clear();
  tests_.clear();
}

void builder::operand(node_ptr value) {
  if (pending_) {
    diag_(value->where(),
          "operand follows a pending value (missing '.'?)");
    stack_.push_back(std::move(pending_));  // repair: push it and go on
  }
  pending_ = std::move(value);
}

node_ptr builder::pop(const parse::location& where, const char* who) {
  if (stack_.empty()) {
    diag_(where, std::string("'") + who + "': stack underflow");
    return number(0, where);  // repair placeholder
  }
  node_ptr top = std::move(stack_.back());
  stack_.pop_back();
  return top;
}

node_ptr builder::take_rhs(const parse::location& where, const char* who) {
  if (pending_) return std::move(pending_);
  return pop(where, who);
}

void builder::combine(op o, const parse::location& where) {
  static const char* const names[] = {"+", "-", "*", "/"};
  const char* who = names[static_cast<int>(o)];
  node_ptr rhs = take_rhs(where, who);
  node_ptr lhs = pop(where, who);
  stack_.push_back(binary(o, std::move(lhs), std::move(rhs), where));
}

void builder::push(const parse::location& where) {
  if (!pending_) {
    diag_(where, "'.' with no value to push");
    return;
  }
  stack_.push_back(std::move(pending_));
}

void builder::test(const parse::location& where) {
  node_ptr rhs = take_rhs(where, "=");
  node_ptr lhs = pop(where, "=");
  tests_.push_back({std::move(lhs), std::move(rhs), where});
}

program builder::finish(const parse::location& end) {
  std::shared_ptr<const node> result;
  if (pending_) {
    result = std::move(pending_);
  } else if (!stack_.empty()) {
    result = std::move(stack_.back());
    stack_.pop_back();
  }
  if (!stack_.empty())
    diag_(end, std::to_string(stack_.size()) +
                   " unused value(s) left on the stack");
  program p(std::move(tests_), std::move(result));
  reset();
  return p;
}

}  // namespace calc
