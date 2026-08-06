# Multiple flex/bison C++ parsers in one library, one executable

An idealized prototype of a library that ships several bison parsers
(the shape of something like SWObjects): everything the parsers can
share is factored out — the location/position classes, the parse-driver
scaffolding, the build machinery — and one binary front-ends all of
them.

```console
$ ./build/franken-exe calc\( 2 . 3 + 5 - 0 = \) cmds\( 'print "hi";' \)
hi
```

## The CLI

`franken-exe` takes find(1)-style groups; each group names the parser
that gets its contents (quote the parens from your shell):

```
franken-exe [-p] [-s] { calc( <rpn words> ) | cmds( <script> ) }...
```

The words inside a group are joined with spaces to form the source
text; a single word `@path` makes the group parse that file instead.
Groups run in argument order; the first failure stops the run. `-p` /
`-s` turn on the bison / flex traces.

Exit status: `0` success · `255` (i.e. `exit(-1)`) a calc `=` test
failed, with both values printed to stderr · `1` parse or runtime
error · `2` bad command line.

## The two languages

### `calc` — RPN, compiling to an evaluatable tree

A number or identifier becomes the *pending* operand; `.` pushes it
(think of the ENTER key on an RPN calculator); a binary operator takes
its right operand from the pending slot (or the stack) and its left
operand from the stack, and pushes the combination; `=` records a test.

| word | effect |
|---|---|
| `2`, `pi` | pending := operand (error if one is already pending — "missing `.`?") |
| `.` | push pending onto the stack |
| `+ - * /` | rhs := pending (else pop); push(pop ⊕ rhs) |
| `=` | rhs := pending (else pop); record test(pop == rhs) |

Crucially the stack is a **parse-time stack of expression trees, not
values**. Parsing produces a `calc::program`; only
`program::operator()(environment)` evaluates anything — it runs the
`=` tests in order (printing both sides and exiting nonzero on
mismatch, via a `test_failure` exception the main turns into
`return -1`) and then yields the final value, if the input left one:

```console
$ ./build/franken-exe calc\( 2 . 3 + \)
(2 + 3) = 5                                # the compile tree, then its value
$ ./build/franken-exe calc\( 2 . 3 + 5 - 0 = \) ; echo $?
0                                          # test held: silent success
$ ./build/franken-exe calc\( 2 . 3 + 99 = \) ; echo $?
<calc>:1.12: test failed: (2 + 3) = 5  !=  99 = 99
255
```

Every node stores the `@n` location the parser saw, so *runtime* errors
(undefined variable, division by zero, failed tests) still point back
into the source. Free variables (`pi`, `e` are pre-bound in the main)
are resolved per evaluation, not per parse.

Note what RPN buys at the grammar level: `calc/parser.yy` has no
precedence declarations, no `%prec`, no parentheses.

### `cmds` — deferred statements as closures

A trivial imperative language (`print`, `repeat`) whose actions build
**closures capturing the `$1..$n` semantic values from the reduce
stack**, deferring the work until `program::operator()`:

```yacc
stmt:
  "repeat" "integer" "{" script "}" {
      $$ = [count = $2, body = $4](std::ostream& os) {
        for (int i = 0; i < count; ++i) body(os);
      };
    }
```

## Two action styles, on purpose

The grammars deliberately demonstrate the two canonical ways to write
bison C++ semantic actions:

* **`cmds`: pure value-threading.** Every action computes a semantic
  value that flows up through `$$`/`$n`; the driver only receives the
  finished start-symbol value. State nests exactly like the parse tree.
* **`calc`: driver-held state.** RPN's pending/stack state spans rules
  and doesn't nest, so the actions thread nothing — each fires a side
  effect into a `calc::builder` owned by the driver
  (`drv.build().combine(...)`). This is the classic yacc shape, and the
  one large multi-parser codebases usually need. The builder reports
  bad input (e.g. stack underflow) through a diagnostic callback bound
  to the shared `error()`, and repairs with a placeholder so one
  mistake yields one message.

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
`set_result`). A `begin_parse()` hook lets a driver reset per-parse
state (calc resets its builder there). A concrete driver is then tiny:

```cpp
class driver : public common::driver_base<driver, parse::calc_parser, program> {
public:
  builder& build() { return builder_; }
  void begin_parse() { builder_.reset(); }
  bool scan_begin_file(const std::string&);   // defined in scanner.ll,
  void scan_begin_string(const std::string&); // where the flex buffer
  void scan_end();                            // primitives are visible
private:
  builder builder_;
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

### One library, one build recipe

Everything — both parsers, both scanners, the AST — links into a single
`libparselib.a`; consumers include `calc/driver.hh` / `cmds/driver.hh`
and the generated code stays an implementation detail. In CMake,
`add_bison_parser()` / `add_flex_scanner()` generate into a single flat
`build/gen/` directory so the shared `location.hh` resolves with a plain
include. A custom command (rather than CMake's `BISON_TARGET`) is used
so `location.hh` can be declared as an extra output of the calc
grammar's run; a `gen_parsers` custom target gives every consumer one
dependency that guarantees generation happens first.

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
| `%define api.value.automove` | every `$n` is an rvalue; move-only values flow through the parser without explicit `std::move` (corollary: use each `$n` at most once) |
| `%define parse.error detailed` + `parse.lac full` | "expected X before Y" diagnostics with exact lookahead correction |
| `%define parse.assert`, `parse.trace`, `%printer` | checked symbol lifetimes; `-p` / `-s` runtime traces print semantic values via the `%printer` rules |
| `%locations` | positions tracked by the scanner (`YY_USER_ACTION` + `loc.step()`), stored into the AST via `@n` |

Other details worth stealing:

* Named token aliases (`%token PLUS "+"`) so rules read `words "+"`.
* Scanners throw `parser::syntax_error(loc, msg)` for lexical errors
  (bad characters, out-of-range numbers); the parser catches it and
  reports through the normal `error()` path.
* `cmds` shows error recovery: `stmt: error ";"` resynchronizes at the
  next `;` so one bad statement doesn't hide later diagnostics (the
  driver still refuses to return a program if any error occurred).

## Layout

```
CMakeLists.txt          generation functions, parselib, franken-exe, ctest suite
main.cc                 franken-exe: group args -> the right parser
common/driver_base.hh   CRTP driver shared by both languages
calc/parser.yy          RPN grammar -> calc::program (compile trees + tests)
calc/scanner.ll         scanner + driver scan hooks
calc/ast.{hh,cc}        nodes, builder (parse-time tree stack), program
calc/driver.hh          driver = base + builder + scanner hooks
cmds/parser.yy          grammar -> cmds::program (deferred closures)
cmds/scanner.ll         scanner + driver scan hooks
cmds/program.hh         step = std::function, program::operator()
cmds/driver.hh          mirror of calc/driver.hh
examples/hello.cmds     sample script (try: franken-exe cmds\( @examples/hello.cmds \))
```

## Build & test

```console
$ cmake -S . -B build
$ cmake --build build
$ ctest --test-dir build
```

Requires bison ≥ 3.8, flex ≥ 2.6, a C++17 compiler.
