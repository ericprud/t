/* Scanner for the cmds language.  Same structure as calc/scanner.ll;
 * see the comments there. */

%{
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>

#include "cmds/driver.hh"
#include "cmds_parser.hh"

#define YY_DECL parse::cmds_parser::symbol_type cmdslex(cmds::driver& drv)

#define YY_USER_ACTION loc.columns(yyleng);

namespace {

/* Strip the surrounding quotes and translate the supported escapes. */
std::string unescape(const char* text, int len) {
  std::string out;
  out.reserve(len);
  for (int i = 1; i < len - 1; ++i) {
    char c = text[i];
    if (c == '\\' && i + 1 < len - 1) {
      switch (text[++i]) {
        case 'n': c = '\n'; break;
        case 't': c = '\t'; break;
        default:  c = text[i]; break;  /* \" \\ and anything else: literal */
      }
    }
    out += c;
  }
  return out;
}

}  // namespace
%}

%option noyywrap nounput noinput batch debug
%option prefix="cmds"

int     [0-9]+
string  \"([^"\\\n]|\\.)*\"
blank   [ \t\r]

%%
%{
  parse::location& loc = drv.location();
  loc.step();
%}

{blank}+   loc.step();
\n+        loc.lines(yyleng); loc.step();
"#".*      loc.step();

"print"    return parse::cmds_parser::make_PRINT(loc);
"repeat"   return parse::cmds_parser::make_REPEAT(loc);
";"        return parse::cmds_parser::make_SEMI(loc);
","        return parse::cmds_parser::make_COMMA(loc);
"{"        return parse::cmds_parser::make_LBRACE(loc);
"}"        return parse::cmds_parser::make_RBRACE(loc);

{int}      {
  errno = 0;
  const long n = std::strtol(yytext, nullptr, 10);
  if (errno == ERANGE || n > INT_MAX)
    throw parse::cmds_parser::syntax_error(
        loc, "integer out of range: " + std::string(yytext));
  return parse::cmds_parser::make_INT(static_cast<int>(n), loc);
}

{string}   return parse::cmds_parser::make_STRING(unescape(yytext, yyleng), loc);

.          {
  throw parse::cmds_parser::syntax_error(
      loc, "invalid character: '" + std::string(yytext) + "'");
}

<<EOF>>    return parse::cmds_parser::make_YYEOF(loc);
%%

bool cmds::driver::scan_begin_file(const std::string& path) {
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

void cmds::driver::scan_begin_string(const std::string& text) {
  yy_flex_debug = trace_scanning();
  scan_buffer_ = yy_scan_bytes(text.data(), text.size());
}

void cmds::driver::scan_end() {
  if (scan_buffer_) {
    yy_delete_buffer(static_cast<YY_BUFFER_STATE>(scan_buffer_));
    scan_buffer_ = nullptr;
  }
  if (scan_file_) {
    std::fclose(scan_file_);
    scan_file_ = nullptr;
  }
}
