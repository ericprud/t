/* Calculator grammar.
 *
 * Parses one arithmetic expression into a compile tree (calc::expression).
 * Nothing is evaluated at parse time: the semantic actions only assemble AST
 * nodes, and the caller later invokes expression::operator()(environment) as
 * often as it likes, with whatever variable bindings it likes.
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
/* This grammar owns the shared location header; the other grammars say
 * `%define api.location.file none` and include this one. */
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
  PLUS   "+"
  MINUS  "-"
  STAR   "*"
  SLASH  "/"
  LPAREN "("
  RPAREN ")"
;

%nterm <calc::node_ptr> expr

/* Printers feed `bison --debug` traces (enable with driver.trace_parsing). */
%printer { yyo << $$; } <double> <std::string>;
%printer { if ($$) yyo << *$$; else yyo << "<null>"; } <calc::node_ptr>;

%left "+" "-";
%left "*" "/";
%precedence NEG;

%%
%start unit;

unit:
  expr  { drv.set_result(calc::expression($1)); }
;

expr:
  "number"           { $$ = calc::number($1, @$); }
| "identifier"       { $$ = calc::variable($1, @$); }
| expr "+" expr      { $$ = calc::binary(calc::op::add, $1, $3, @$); }
| expr "-" expr      { $$ = calc::binary(calc::op::sub, $1, $3, @$); }
| expr "*" expr      { $$ = calc::binary(calc::op::mul, $1, $3, @$); }
| expr "/" expr      { $$ = calc::binary(calc::op::div, $1, $3, @$); }
| "+" expr %prec NEG { $$ = $2; }
| "-" expr %prec NEG { $$ = calc::negate($2, @$); }
| "(" expr ")"       { $$ = $2; }
;
%%

void parse::calc_parser::error(const location_type& l, const std::string& m) {
  drv.error(l, m);
}
