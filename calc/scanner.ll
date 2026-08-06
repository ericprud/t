/* Scanner for the calc language.
 *
 * Plain C scanner driven by a C++ parser, following the bison manual's
 * calc++ example.  `%option prefix` renames the flex globals (yyin -> calcin,
 * ...) so several scanners can be linked into one binary; inside this file
 * flex provides compatibility macros, so the code below still says yyin.
 */

%{
#include <cerrno>
#include <cstdlib>
#include <string>

#include "calc/driver.hh"
#include "calc_parser.hh"

/* The function flex generates.  The name matches the declaration in
 * calc/driver.hh; note that `%option prefix` also #defines yylex to calclex
 * inside this file, so writing yylex here would silently rename anyway. */
#define YY_DECL parse::calc_parser::symbol_type calclex(calc::driver& drv)

/* Runs on every match: extend the current location over the token. */
#define YY_USER_ACTION loc.columns(yyleng);
%}

%option noyywrap nounput noinput batch debug
%option prefix="calc"

id      [a-zA-Z_][a-zA-Z_0-9]*
number  ([0-9]+\.?[0-9]*|\.[0-9]+)([eE][-+]?[0-9]+)?
blank   [ \t\r]

/* Maximal munch caveat: "2." lexes as one number token, so the push
 * operator needs whitespace after a number: "2 ." — as in RPN input
 * anyway.  A lone "." never matches {number} and falls through to PUSH. */

%%
%{
  /* Runs each time yylex is entered. */
  parse::location& loc = drv.location();
  loc.step();
%}

{blank}+   loc.step();
\n+        loc.lines(yyleng); loc.step();

"+"        return parse::calc_parser::make_PLUS(loc);
"-"        return parse::calc_parser::make_MINUS(loc);
"*"        return parse::calc_parser::make_STAR(loc);
"/"        return parse::calc_parser::make_SLASH(loc);
"."        return parse::calc_parser::make_PUSH(loc);
"="        return parse::calc_parser::make_TEST(loc);

{number}   {
  errno = 0;
  const double value = std::strtod(yytext, nullptr);
  if (errno == ERANGE)
    throw parse::calc_parser::syntax_error(
        loc, "number out of range: " + std::string(yytext));
  return parse::calc_parser::make_NUMBER(value, loc);
}

{id}       return parse::calc_parser::make_IDENT(yytext, loc);

.          {
  throw parse::calc_parser::syntax_error(
      loc, "invalid character: '" + std::string(yytext) + "'");
}

<<EOF>>    return parse::calc_parser::make_YYEOF(loc);
%%

/* The driver's scanner hooks live here because only this file sees the flex
 * buffer primitives (yyin, yy_scan_bytes, ...). */

bool calc::driver::scan_begin_file(const std::string& path) {
  yy_flex_debug = trace_scanning();
  if (path == "-") {
    yyin = stdin;
  } else {
    yyin = std::fopen(path.c_str(), "r");
    if (!yyin) return false;
    scan_file_ = yyin;
  }
  yyrestart(yyin);
  return true;
}

void calc::driver::scan_begin_string(const std::string& text) {
  yy_flex_debug = trace_scanning();
  scan_buffer_ = yy_scan_bytes(text.data(), text.size());
}

void calc::driver::scan_end() {
  if (scan_buffer_) {
    yy_delete_buffer(static_cast<YY_BUFFER_STATE>(scan_buffer_));
    scan_buffer_ = nullptr;
  }
  if (scan_file_) {
    std::fclose(scan_file_);
    scan_file_ = nullptr;
  }
}
