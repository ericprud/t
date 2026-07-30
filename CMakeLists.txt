# Two flex/bison parsers in one project, sharing:
#   - one bison-generated location type   (gen/location.hh, namespace parse)
#   - one CRTP parse-driver base          (common/driver_base.hh)
#   - the generation machinery below      (add_bison_parser / add_flex_scanner)
cmake_minimum_required(VERSION 3.16)
project(multi_parser_demo CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

find_program(BISON_EXECUTABLE bison REQUIRED)
find_program(FLEX_EXECUTABLE flex REQUIRED)

# All generated sources land in one flat directory so that the parsers can
# include each other's shared artifacts (location.hh) with plain "..." paths.
set(GEN_DIR ${CMAKE_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN_DIR})

# add_bison_parser(<name> <grammar.yy> [EXTRA_OUTPUTS <files>...])
#
# Generates ${GEN_DIR}/<name>.cc and ${GEN_DIR}/<name>.hh (the grammar must say
# %header).  EXTRA_OUTPUTS declares additional files the run produces — the
# calc grammar uses it for the shared location.hh.  We use a plain
# add_custom_command instead of CMake's BISON_TARGET macro precisely because
# BISON_TARGET cannot express those extra outputs.
function(add_bison_parser name grammar)
  cmake_parse_arguments(ARG "" "" "EXTRA_OUTPUTS" ${ARGN})
  add_custom_command(
    OUTPUT ${GEN_DIR}/${name}.cc ${GEN_DIR}/${name}.hh ${ARG_EXTRA_OUTPUTS}
    COMMAND ${BISON_EXECUTABLE} -Wall -o ${GEN_DIR}/${name}.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/${grammar}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${grammar}
    COMMENT "bison ${grammar}"
    VERBATIM)
endfunction()

function(add_flex_scanner name lexer)
  add_custom_command(
    OUTPUT ${GEN_DIR}/${name}.cc
    COMMAND ${FLEX_EXECUTABLE} -o ${GEN_DIR}/${name}.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/${lexer}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${lexer}
    COMMENT "flex ${lexer}"
    VERBATIM)
endfunction()

# The calc grammar owns the shared location header (api.location.file);
# the cmds grammar reuses it (api.location.file none + api.location.include).
add_bison_parser(calc_parser calc/parser.yy EXTRA_OUTPUTS ${GEN_DIR}/location.hh)
add_flex_scanner(calc_scanner calc/scanner.ll)
add_bison_parser(cmds_parser cmds/parser.yy)
add_flex_scanner(cmds_scanner cmds/scanner.ll)

# Hand-written sources include generated headers; this target gives every
# consumer a single dependency that guarantees generation ran first.
add_custom_target(gen_parsers DEPENDS
  ${GEN_DIR}/calc_parser.cc ${GEN_DIR}/calc_scanner.cc
  ${GEN_DIR}/cmds_parser.cc ${GEN_DIR}/cmds_scanner.cc
  ${GEN_DIR}/location.hh)

# Shared include paths for everything in the project.
add_library(parse_common INTERFACE)
target_include_directories(parse_common INTERFACE ${CMAKE_SOURCE_DIR} ${GEN_DIR})

add_library(calc_lang STATIC
  calc/ast.cc
  ${GEN_DIR}/calc_parser.cc
  ${GEN_DIR}/calc_scanner.cc)
target_link_libraries(calc_lang PUBLIC parse_common)
add_dependencies(calc_lang gen_parsers)

add_executable(calc calc/main.cc)
target_link_libraries(calc PRIVATE calc_lang)
add_dependencies(calc gen_parsers)

add_library(cmds_lang STATIC
  ${GEN_DIR}/cmds_parser.cc
  ${GEN_DIR}/cmds_scanner.cc)
target_link_libraries(cmds_lang PUBLIC parse_common)
add_dependencies(cmds_lang gen_parsers)

add_executable(cmds cmds/main.cc)
target_link_libraries(cmds PRIVATE cmds_lang)
add_dependencies(cmds gen_parsers)

enable_testing()

add_test(NAME calc.arith  COMMAND calc "1 + 2*3 - 4/2")
set_tests_properties(calc.arith  PROPERTIES PASS_REGULAR_EXPRESSION "= 5")
add_test(NAME calc.unary  COMMAND calc "-(1+2)*3")
set_tests_properties(calc.unary  PROPERTIES PASS_REGULAR_EXPRESSION "= -9")
add_test(NAME calc.vars   COMMAND calc "2*pi")
set_tests_properties(calc.vars   PROPERTIES PASS_REGULAR_EXPRESSION "= 6.28319")
add_test(NAME calc.divzero COMMAND calc "1/0")
set_tests_properties(calc.divzero PROPERTIES WILL_FAIL TRUE)
add_test(NAME calc.syntax COMMAND calc "1 + * 2")
set_tests_properties(calc.syntax PROPERTIES WILL_FAIL TRUE)

add_test(NAME cmds.hello  COMMAND cmds ${CMAKE_SOURCE_DIR}/examples/hello.cmds)
set_tests_properties(cmds.hello  PROPERTIES PASS_REGULAR_EXPRESSION "tick 1 tock 2")
add_test(NAME cmds.inline COMMAND cmds -e "repeat 2 { print \"hi\"; }")
set_tests_properties(cmds.inline PROPERTIES PASS_REGULAR_EXPRESSION "hi\nhi")
add_test(NAME cmds.syntax COMMAND cmds -e "print ;")
set_tests_properties(cmds.syntax PROPERTIES WILL_FAIL TRUE)
