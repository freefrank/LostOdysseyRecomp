#!/bin/bash
# regenerates uam's bison/flex/mako outputs into generated/ (run from runtime/third_party/uam)
set -e
G=generated
mkdir -p $G/glsl/glcpp
bison -o $G/glsl/glsl_parser.cpp -p _mesa_glsl_ --defines=$G/glsl/glsl_parser.h mesa-imported/glsl/glsl_parser.yy
flex -o $G/glsl/glsl_lexer.cpp mesa-imported/glsl/glsl_lexer.ll
for m in enum constant strings; do
  case $m in enum) o=ir_expression_operation.h;; constant) o=ir_expression_operation_constant.h;; strings) o=ir_expression_operation_strings.h;; esac
  python3 mesa-imported/glsl/ir_expression_operation.py $m > $G/glsl/$o
done
bison -o $G/glsl/glcpp/glcpp-parse.c -p glcpp_parser_ --defines=$G/glsl/glcpp/glcpp-parse.h mesa-imported/glsl/glcpp/glcpp-parse.y
flex -o $G/glsl/glcpp/glcpp-lex.c mesa-imported/glsl/glcpp/glcpp-lex.l
bison --version | head -1; flex --version
