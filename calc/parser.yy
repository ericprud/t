/* RPN calculator grammar.
 *
 *   2 . 3 + 5 - 0 =
 *
 * A number or identifier becomes the pending operand; "." pushes it onto the
 * stack (think of the ENTER key on an RPN calculator); a binary operator
 * takes its right operand from the pending slot (or the stack) and its left
 * operand from the stack, and pushes the combination; "=" records a test.
 *
 * The stack is a *parse-time* stack of expression trees, not values: parsing
 * produces a calc::program, and only program::operator()(environment)
 * evaluates anything (running the "=" tests and yielding the final value).
 *
 * Contrast the action style with cmds/parser.yy.  There, every action
 * computes a semantic value that flows up through $$/$n — the pure
 * value-threading style.  Here the actions thread nothing: each token's
 * action fires a side effect into the driver-held calc::builder, which owns
 * the pending/stack state that spans rules.  This is the classic yacc shape
 * for grammars whose natural state doesn't nest like the parse tree does.
 *
 * Note also what RPN buys at the grammar level: no precedence declarations,
 * no %prec, no parentheses — the input order *is* the evaluation order.
 */

%require "3.8"
%language "c++"
%header

/* Every parser in the project lives in namespace `parse` so they can all
 * share the single bison-generated location type (api.location.file below).
 * The per-parser class name is what keeps them apart. */
%define api.namespace {parse}
%define api.parser.class {calc_parser}

%define api.token.raw           /* the scanner only uses make_* symbols */
%define api.token.constructor   /* typed, location-carrying complete symbols */
%define api.token.prefix {TOK_}
%define api.value.type variant
%define api.value.automove      /* every $n is an rvalue: use each once */

%define parse.assert
%define parse.trace
%define parse.error detailed
%define parse.lac full

%locations
/* This grammar owns the shared location header; the other grammars reuse
 * it via api.location.type. */
%define api.location.file "location.hh"

/* Passed to yylex and to the parser; also visible in actions as `drv`. */
%param { calc::driver& drv }

%code requires {
  #include "calc/ast.hh"
  namespace calc { class driver; }
}

%code {
  #include "calc/driver.hh"
  /* The parser calls yylex(drv); route that to this language's scanner
   * (renamed by `%option prefix="calc"` in calc/scanner.ll). */
  #define yylex calclex
}

%token <double>      NUMBER "number"
%token <std::string> IDENT  "identifier"
%token
  PLUS  "+"
  MINUS "-"
  STAR  "*"
  SLASH "/"
  PUSH  "."
  TEST  "="
;

/* Printers feed `bison --debug` traces (enable with driver.trace_parsing). */
%printer { yyo << $$; } <double>;
%printer { yyo << '"' << $$ << '"'; } <std::string>;

%%
%start unit;

unit:
  words  { drv.set_result(drv.build().finish(@$)); }
;

words:
  %empty
| words word
;

word:
  "number"      { drv.build().operand(calc::number($1, @1)); }
| "identifier"  { drv.build().operand(calc::variable($1, @1)); }
| "+"           { drv.build().combine(calc::op::add, @1); }
| "-"           { drv.build().combine(calc::op::sub, @1); }
| "*"           { drv.build().combine(calc::op::mul, @1); }
| "/"           { drv.build().combine(calc::op::div, @1); }
| "."           { drv.build().push(@1); }
| "="           { drv.build().test(@1); }
;
%%

void parse::calc_parser::error(const location_type& l, const std::string& m) {
  drv.error(l, m);
}
