#include "calc/ast.hh"

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

class negate_node final : public node {
public:
  negate_node(node_ptr operand, parse::location l)
      : node(std::move(l)), operand_(std::move(operand)) {}

  double eval(const environment& env) const override {
    return -operand_->eval(env);
  }

  void print(std::ostream& os) const override {
    os << "(-" << *operand_ << ')';
  }

private:
  node_ptr operand_;
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

node_ptr negate(node_ptr operand, parse::location l) {
  return std::make_unique<negate_node>(std::move(operand), std::move(l));
}

}  // namespace calc
