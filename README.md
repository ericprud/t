# Two flex/bison C++ parsers, one set of shared infrastructure

A small demonstration of how to keep several bison C++ parsers in one
project while factoring out everything they would otherwise duplicate:
the location/position classes, the parse-driver scaffolding, and the
build machinery.

## The two languages

**`calc`** — the tired old calculator, except that parsing produces a
*compile tree* instead of a value. The grammar's semantic actions only
assemble `calc::node` objects; the parser's result is a
`calc::expression` whose `operator()(environment)` evaluates the tree.
Parse once, evaluate under as many variable bindings as you like:

```console
$ ./build/calc "1 + 2*(3+4)" "2*pi"
(1 + (2 * (3 + 4))) = 15
(2 * pi) = 6.28319
$ ./build/calc "y + 1"
<string>:1.1: runtime error: undefined variable 'y'
```

Every AST node stores the `@$` location the parser saw, so *runtime*
errors (undefined variable, division by zero) still point back into the
source text.

**`cmds`** — a trivial imperative language (`print`, `repeat`) whose
grammar demonstrates the other style of semantic action. A classic yacc
action executes during the reduce:

```yacc
stmt: PRINT args ';'   { print_now($2); }   /* immediate */
```

Here instead each action builds a **closure capturing the `$1..$n`
semantic values from the reduce stack**, deferring the work:

```yacc
stmt:
  "repeat" "integer" "{" script "}" {
      $$ = [count = $2, body = $4](std::ostream& os) {
        for (int i = 0; i < count; ++i) body(os);
      };
    }
```

The start rule hands the accumulated `cmds::program` to the driver, and
`program::operator()` replays the steps:

```console
$ ./build/cmds -e 'repeat 2 { print "hi", 1; }'
hi 1
hi 1
$ ./build/cmds examples/hello.cmds
```

## What is factored out, and how

### One location type for all parsers (`gen/location.hh`)

Both grammars live in the same namespace (`%define api.namespace
{parse}`) and are distinguished by class name (`%define api.parser.class
{calc_parser}` / `{cmds_parser}`). That makes one location type usable
by both:

* `calc/parser.yy` **owns** the header: `%define api.location.file
  "location.hh"` makes bison write `parse::position` / `parse::location`
  into a standalone file next to its output.
* `cmds/parser.yy` **reuses** it: `%define api.location.type
  {parse::location}` suppresses bison's own location classes, and a
  `#include "location.hh"` in `%code requires` supplies the shared ones.

(Note: `%define api.location.file none` + `api.location.include` is *not*
the sharing mechanism — with `none`, bison 3.8 inlines a second copy of
the classes into the parser header, which would collide.)

### One parse driver (`common/driver_base.hh`)

`common::driver_base<Derived, Parser, Result>` is a CRTP base holding
everything language-independent: the current `parse::location`,
diagnostics and error counting, trace flags, `parse_file` / `parse_string`
entry points, and the parse-run scaffolding (construct parser, set debug
level, run, collect the result deposited by the start rule via
`set_result`). A concrete driver is then almost nothing:

```cpp
class driver : public common::driver_base<driver, parse::calc_parser, expression> {
public:
  bool scan_begin_file(const std::string&);   // defined in scanner.ll,
  void scan_begin_string(const std::string&); // where the flex buffer
  void scan_end();                            // primitives are visible
};
```

### Several scanners in one binary

The scanners are plain (non-`%option c++`) flex scanners driven by a C++
parser, following the bison manual's `calc++` example — with two
additions needed for coexistence:

* `%option prefix="calc"` / `"cmds"` renames each scanner's globals
  (`yyin` → `calcin`, ...) so both object files can link together.
  Inside each `.ll` file flex provides compatibility macros, so the code
  still says `yyin`.
* `YY_DECL` gives each lexer a distinct typed signature returning
  complete symbols:
  `parse::calc_parser::symbol_type calclex(calc::driver&)`.
  The parser is pointed at it with `#define yylex calclex` in `%code`.

The driver's `scan_begin_*` / `scan_end` member functions are *defined at
the bottom of the `.ll` file*, because only there are `yyrestart`,
`yy_scan_bytes`, and `YY_BUFFER_STATE` visible.

### One build recipe (`CMakeLists.txt`)

`add_bison_parser()` / `add_flex_scanner()` generate everything into a
single flat `build/gen/` directory so the shared `location.hh` resolves
with a plain include. A custom command (rather than CMake's
`BISON_TARGET`) is used so `location.hh` can be declared as an extra
output of the calc grammar's run; a `gen_parsers` custom target gives
every consumer one dependency that guarantees generation happens first.

## Modern bison practice on display

Both grammars use the same directive block, which is close to the
current (bison ≥ 3.8) recommended setup for C++:

| Directive | Effect |
|---|---|
| `%language "c++"` + `%header` | `lalr1.cc` skeleton, header emitted |
| `%define api.value.type variant` | real C++ types as semantic values — `std::unique_ptr`, `std::function`, `std::vector`, no `%union` |
| `%define api.token.constructor` | scanner returns *complete symbols* (`make_NUMBER(value, loc)`) — token kind, value, and location can never disagree |
| `%define api.token.raw` | token kinds are symbol numbers; no char-literal tokens |
| `%define api.token.prefix {TOK_}` | no name collisions with system macros |
| `%define api.value.automove` | every `$n` is an rvalue; move-only AST nodes flow through the parser without explicit `std::move` (corollary: use each `$n` at most once) |
| `%define parse.error detailed` + `parse.lac full` | "expected X before Y" diagnostics with exact lookahead correction |
| `%define parse.assert`, `parse.trace`, `%printer` | checked symbol lifetimes; `-p` / `-s` runtime traces print semantic values via the `%printer` rules |
| `%locations` | positions tracked by the scanner (`YY_USER_ACTION` + `loc.step()`), stored into the AST via `@$` |

Other details worth stealing:

* Named token aliases (`%token PLUS "+"`) so rules read `expr "+" expr`.
* Scanners throw `parser::syntax_error(loc, msg)` for lexical errors
  (bad characters, out-of-range numbers); the parser catches it and
  reports through the normal `error()` path.
* `cmds` shows error recovery: `stmt: error ";"` resynchronizes at the
  next `;` so one bad statement doesn't hide later diagnostics (the
  driver still refuses to return a program if any error occurred).
* Precedence declarations (`%left`, `%precedence NEG`) instead of
  grammar-encoded precedence levels.

## Layout

```
CMakeLists.txt          generation functions, targets, ctest suite
common/driver_base.hh   CRTP driver shared by both languages
calc/parser.yy          grammar -> calc::expression (compile tree)
calc/scanner.ll         scanner + driver scan hooks
calc/ast.{hh,cc}        node classes, expression::operator()
calc/driver.hh          driver = base + scanner hooks (~10 lines)
calc/main.cc            CLI / stdin REPL
cmds/parser.yy          grammar -> cmds::program (deferred closures)
cmds/scanner.ll         scanner + driver scan hooks
cmds/program.hh         step = std::function, program::operator()
cmds/driver.hh          mirror of calc/driver.hh
cmds/main.cc            CLI (-e inline, files, stdin)
examples/hello.cmds     sample script
```

## Build & test

```console
$ cmake -S . -B build
$ cmake --build build
$ ctest --test-dir build
```

Requires bison ≥ 3.8, flex ≥ 2.6, a C++17 compiler.
