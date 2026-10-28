/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         _mesa_glsl_parse
#define yylex           _mesa_glsl_lex
#define yyerror         _mesa_glsl_error
#define yydebug         _mesa_glsl_debug
#define yynerrs         _mesa_glsl_nerrs

/* First part of user prologue.  */
#line 1 "mesa-imported/glsl/glsl_parser.yy"

/*
 * Copyright © 2008, 2009 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _MSC_VER
#include <strings.h>
#endif
#include <assert.h>

#include "glsl/ast.h" // fincs-edit
#include "glsl/glsl_parser_extras.h" // fincs-edit
#include "compiler/glsl_types.h"
#include "main/context.h"

#ifdef _MSC_VER
#pragma warning( disable : 4065 ) // switch statement contains 'default' but no 'case' labels
#endif

#undef yyerror

static void yyerror(YYLTYPE *loc, _mesa_glsl_parse_state *st, const char *msg)
{
   _mesa_glsl_error(loc, st, "%s", msg);
}

static int
_mesa_glsl_lex(YYSTYPE *val, YYLTYPE *loc, _mesa_glsl_parse_state *state)
{
   return _mesa_glsl_lexer_lex(val, loc, state->scanner);
}

static bool match_layout_qualifier(const char *s1, const char *s2,
                                   _mesa_glsl_parse_state *state)
{
   /* From the GLSL 1.50 spec, section 4.3.8 (Layout Qualifiers):
    *
    *     "The tokens in any layout-qualifier-id-list ... are not case
    *     sensitive, unless explicitly noted otherwise."
    *
    * The text "unless explicitly noted otherwise" appears to be
    * vacuous--no desktop GLSL spec (up through GLSL 4.40) notes
    * otherwise.
    *
    * However, the GLSL ES 3.00 spec says, in section 4.3.8 (Layout
    * Qualifiers):
    *
    *     "As for other identifiers, they are case sensitive."
    *
    * So we need to do a case-sensitive or a case-insensitive match,
    * depending on whether we are compiling for GLSL ES.
    */
   if (state->es_shader)
      return strcmp(s1, s2);
   else
      return strcasecmp(s1, s2);
}

#line 156 "generated/glsl/glsl_parser.cpp"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "glsl_parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_ATTRIBUTE = 3,                  /* ATTRIBUTE  */
  YYSYMBOL_CONST_TOK = 4,                  /* CONST_TOK  */
  YYSYMBOL_BASIC_TYPE_TOK = 5,             /* BASIC_TYPE_TOK  */
  YYSYMBOL_BREAK = 6,                      /* BREAK  */
  YYSYMBOL_BUFFER = 7,                     /* BUFFER  */
  YYSYMBOL_CONTINUE = 8,                   /* CONTINUE  */
  YYSYMBOL_DO = 9,                         /* DO  */
  YYSYMBOL_ELSE = 10,                      /* ELSE  */
  YYSYMBOL_FOR = 11,                       /* FOR  */
  YYSYMBOL_IF = 12,                        /* IF  */
  YYSYMBOL_DISCARD = 13,                   /* DISCARD  */
  YYSYMBOL_RETURN = 14,                    /* RETURN  */
  YYSYMBOL_SWITCH = 15,                    /* SWITCH  */
  YYSYMBOL_CASE = 16,                      /* CASE  */
  YYSYMBOL_DEFAULT = 17,                   /* DEFAULT  */
  YYSYMBOL_CENTROID = 18,                  /* CENTROID  */
  YYSYMBOL_IN_TOK = 19,                    /* IN_TOK  */
  YYSYMBOL_OUT_TOK = 20,                   /* OUT_TOK  */
  YYSYMBOL_INOUT_TOK = 21,                 /* INOUT_TOK  */
  YYSYMBOL_UNIFORM = 22,                   /* UNIFORM  */
  YYSYMBOL_VARYING = 23,                   /* VARYING  */
  YYSYMBOL_SAMPLE = 24,                    /* SAMPLE  */
  YYSYMBOL_NOPERSPECTIVE = 25,             /* NOPERSPECTIVE  */
  YYSYMBOL_FLAT = 26,                      /* FLAT  */
  YYSYMBOL_SMOOTH = 27,                    /* SMOOTH  */
  YYSYMBOL_IMAGE1DSHADOW = 28,             /* IMAGE1DSHADOW  */
  YYSYMBOL_IMAGE2DSHADOW = 29,             /* IMAGE2DSHADOW  */
  YYSYMBOL_IMAGE1DARRAYSHADOW = 30,        /* IMAGE1DARRAYSHADOW  */
  YYSYMBOL_IMAGE2DARRAYSHADOW = 31,        /* IMAGE2DARRAYSHADOW  */
  YYSYMBOL_COHERENT = 32,                  /* COHERENT  */
  YYSYMBOL_VOLATILE = 33,                  /* VOLATILE  */
  YYSYMBOL_RESTRICT = 34,                  /* RESTRICT  */
  YYSYMBOL_READONLY = 35,                  /* READONLY  */
  YYSYMBOL_WRITEONLY = 36,                 /* WRITEONLY  */
  YYSYMBOL_SHARED = 37,                    /* SHARED  */
  YYSYMBOL_STRUCT = 38,                    /* STRUCT  */
  YYSYMBOL_VOID_TOK = 39,                  /* VOID_TOK  */
  YYSYMBOL_WHILE = 40,                     /* WHILE  */
  YYSYMBOL_IDENTIFIER = 41,                /* IDENTIFIER  */
  YYSYMBOL_TYPE_IDENTIFIER = 42,           /* TYPE_IDENTIFIER  */
  YYSYMBOL_NEW_IDENTIFIER = 43,            /* NEW_IDENTIFIER  */
  YYSYMBOL_FLOATCONSTANT = 44,             /* FLOATCONSTANT  */
  YYSYMBOL_DOUBLECONSTANT = 45,            /* DOUBLECONSTANT  */
  YYSYMBOL_INTCONSTANT = 46,               /* INTCONSTANT  */
  YYSYMBOL_UINTCONSTANT = 47,              /* UINTCONSTANT  */
  YYSYMBOL_BOOLCONSTANT = 48,              /* BOOLCONSTANT  */
  YYSYMBOL_INT64CONSTANT = 49,             /* INT64CONSTANT  */
  YYSYMBOL_UINT64CONSTANT = 50,            /* UINT64CONSTANT  */
  YYSYMBOL_FIELD_SELECTION = 51,           /* FIELD_SELECTION  */
  YYSYMBOL_LEFT_OP = 52,                   /* LEFT_OP  */
  YYSYMBOL_RIGHT_OP = 53,                  /* RIGHT_OP  */
  YYSYMBOL_INC_OP = 54,                    /* INC_OP  */
  YYSYMBOL_DEC_OP = 55,                    /* DEC_OP  */
  YYSYMBOL_LE_OP = 56,                     /* LE_OP  */
  YYSYMBOL_GE_OP = 57,                     /* GE_OP  */
  YYSYMBOL_EQ_OP = 58,                     /* EQ_OP  */
  YYSYMBOL_NE_OP = 59,                     /* NE_OP  */
  YYSYMBOL_AND_OP = 60,                    /* AND_OP  */
  YYSYMBOL_OR_OP = 61,                     /* OR_OP  */
  YYSYMBOL_XOR_OP = 62,                    /* XOR_OP  */
  YYSYMBOL_MUL_ASSIGN = 63,                /* MUL_ASSIGN  */
  YYSYMBOL_DIV_ASSIGN = 64,                /* DIV_ASSIGN  */
  YYSYMBOL_ADD_ASSIGN = 65,                /* ADD_ASSIGN  */
  YYSYMBOL_MOD_ASSIGN = 66,                /* MOD_ASSIGN  */
  YYSYMBOL_LEFT_ASSIGN = 67,               /* LEFT_ASSIGN  */
  YYSYMBOL_RIGHT_ASSIGN = 68,              /* RIGHT_ASSIGN  */
  YYSYMBOL_AND_ASSIGN = 69,                /* AND_ASSIGN  */
  YYSYMBOL_XOR_ASSIGN = 70,                /* XOR_ASSIGN  */
  YYSYMBOL_OR_ASSIGN = 71,                 /* OR_ASSIGN  */
  YYSYMBOL_SUB_ASSIGN = 72,                /* SUB_ASSIGN  */
  YYSYMBOL_INVARIANT = 73,                 /* INVARIANT  */
  YYSYMBOL_PRECISE = 74,                   /* PRECISE  */
  YYSYMBOL_LOWP = 75,                      /* LOWP  */
  YYSYMBOL_MEDIUMP = 76,                   /* MEDIUMP  */
  YYSYMBOL_HIGHP = 77,                     /* HIGHP  */
  YYSYMBOL_SUPERP = 78,                    /* SUPERP  */
  YYSYMBOL_PRECISION = 79,                 /* PRECISION  */
  YYSYMBOL_VERSION_TOK = 80,               /* VERSION_TOK  */
  YYSYMBOL_EXTENSION = 81,                 /* EXTENSION  */
  YYSYMBOL_LINE = 82,                      /* LINE  */
  YYSYMBOL_COLON = 83,                     /* COLON  */
  YYSYMBOL_EOL = 84,                       /* EOL  */
  YYSYMBOL_INTERFACE = 85,                 /* INTERFACE  */
  YYSYMBOL_OUTPUT = 86,                    /* OUTPUT  */
  YYSYMBOL_PRAGMA_DEBUG_ON = 87,           /* PRAGMA_DEBUG_ON  */
  YYSYMBOL_PRAGMA_DEBUG_OFF = 88,          /* PRAGMA_DEBUG_OFF  */
  YYSYMBOL_PRAGMA_OPTIMIZE_ON = 89,        /* PRAGMA_OPTIMIZE_ON  */
  YYSYMBOL_PRAGMA_OPTIMIZE_OFF = 90,       /* PRAGMA_OPTIMIZE_OFF  */
  YYSYMBOL_PRAGMA_WARNING_ON = 91,         /* PRAGMA_WARNING_ON  */
  YYSYMBOL_PRAGMA_WARNING_OFF = 92,        /* PRAGMA_WARNING_OFF  */
  YYSYMBOL_PRAGMA_INVARIANT_ALL = 93,      /* PRAGMA_INVARIANT_ALL  */
  YYSYMBOL_LAYOUT_TOK = 94,                /* LAYOUT_TOK  */
  YYSYMBOL_DOT_TOK = 95,                   /* DOT_TOK  */
  YYSYMBOL_ASM = 96,                       /* ASM  */
  YYSYMBOL_CLASS = 97,                     /* CLASS  */
  YYSYMBOL_UNION = 98,                     /* UNION  */
  YYSYMBOL_ENUM = 99,                      /* ENUM  */
  YYSYMBOL_TYPEDEF = 100,                  /* TYPEDEF  */
  YYSYMBOL_TEMPLATE = 101,                 /* TEMPLATE  */
  YYSYMBOL_THIS = 102,                     /* THIS  */
  YYSYMBOL_PACKED_TOK = 103,               /* PACKED_TOK  */
  YYSYMBOL_GOTO = 104,                     /* GOTO  */
  YYSYMBOL_INLINE_TOK = 105,               /* INLINE_TOK  */
  YYSYMBOL_NOINLINE = 106,                 /* NOINLINE  */
  YYSYMBOL_PUBLIC_TOK = 107,               /* PUBLIC_TOK  */
  YYSYMBOL_STATIC = 108,                   /* STATIC  */
  YYSYMBOL_EXTERN = 109,                   /* EXTERN  */
  YYSYMBOL_EXTERNAL = 110,                 /* EXTERNAL  */
  YYSYMBOL_LONG_TOK = 111,                 /* LONG_TOK  */
  YYSYMBOL_SHORT_TOK = 112,                /* SHORT_TOK  */
  YYSYMBOL_HALF = 113,                     /* HALF  */
  YYSYMBOL_FIXED_TOK = 114,                /* FIXED_TOK  */
  YYSYMBOL_UNSIGNED = 115,                 /* UNSIGNED  */
  YYSYMBOL_INPUT_TOK = 116,                /* INPUT_TOK  */
  YYSYMBOL_HVEC2 = 117,                    /* HVEC2  */
  YYSYMBOL_HVEC3 = 118,                    /* HVEC3  */
  YYSYMBOL_HVEC4 = 119,                    /* HVEC4  */
  YYSYMBOL_FVEC2 = 120,                    /* FVEC2  */
  YYSYMBOL_FVEC3 = 121,                    /* FVEC3  */
  YYSYMBOL_FVEC4 = 122,                    /* FVEC4  */
  YYSYMBOL_SAMPLER3DRECT = 123,            /* SAMPLER3DRECT  */
  YYSYMBOL_SIZEOF = 124,                   /* SIZEOF  */
  YYSYMBOL_CAST = 125,                     /* CAST  */
  YYSYMBOL_NAMESPACE = 126,                /* NAMESPACE  */
  YYSYMBOL_USING = 127,                    /* USING  */
  YYSYMBOL_RESOURCE = 128,                 /* RESOURCE  */
  YYSYMBOL_PATCH = 129,                    /* PATCH  */
  YYSYMBOL_SUBROUTINE = 130,               /* SUBROUTINE  */
  YYSYMBOL_ERROR_TOK = 131,                /* ERROR_TOK  */
  YYSYMBOL_COMMON = 132,                   /* COMMON  */
  YYSYMBOL_PARTITION = 133,                /* PARTITION  */
  YYSYMBOL_ACTIVE = 134,                   /* ACTIVE  */
  YYSYMBOL_FILTER = 135,                   /* FILTER  */
  YYSYMBOL_ROW_MAJOR = 136,                /* ROW_MAJOR  */
  YYSYMBOL_THEN = 137,                     /* THEN  */
  YYSYMBOL_138_ = 138,                     /* '('  */
  YYSYMBOL_139_ = 139,                     /* ')'  */
  YYSYMBOL_140_ = 140,                     /* '['  */
  YYSYMBOL_141_ = 141,                     /* ']'  */
  YYSYMBOL_142_ = 142,                     /* ','  */
  YYSYMBOL_143_ = 143,                     /* '+'  */
  YYSYMBOL_144_ = 144,                     /* '-'  */
  YYSYMBOL_145_ = 145,                     /* '!'  */
  YYSYMBOL_146_ = 146,                     /* '~'  */
  YYSYMBOL_147_ = 147,                     /* '*'  */
  YYSYMBOL_148_ = 148,                     /* '/'  */
  YYSYMBOL_149_ = 149,                     /* '%'  */
  YYSYMBOL_150_ = 150,                     /* '<'  */
  YYSYMBOL_151_ = 151,                     /* '>'  */
  YYSYMBOL_152_ = 152,                     /* '&'  */
  YYSYMBOL_153_ = 153,                     /* '^'  */
  YYSYMBOL_154_ = 154,                     /* '|'  */
  YYSYMBOL_155_ = 155,                     /* '?'  */
  YYSYMBOL_156_ = 156,                     /* ':'  */
  YYSYMBOL_157_ = 157,                     /* '='  */
  YYSYMBOL_158_ = 158,                     /* ';'  */
  YYSYMBOL_159_ = 159,                     /* '{'  */
  YYSYMBOL_160_ = 160,                     /* '}'  */
  YYSYMBOL_YYACCEPT = 161,                 /* $accept  */
  YYSYMBOL_translation_unit = 162,         /* translation_unit  */
  YYSYMBOL_163_1 = 163,                    /* $@1  */
  YYSYMBOL_version_statement = 164,        /* version_statement  */
  YYSYMBOL_pragma_statement = 165,         /* pragma_statement  */
  YYSYMBOL_extension_statement_list = 166, /* extension_statement_list  */
  YYSYMBOL_any_identifier = 167,           /* any_identifier  */
  YYSYMBOL_extension_statement = 168,      /* extension_statement  */
  YYSYMBOL_external_declaration_list = 169, /* external_declaration_list  */
  YYSYMBOL_variable_identifier = 170,      /* variable_identifier  */
  YYSYMBOL_primary_expression = 171,       /* primary_expression  */
  YYSYMBOL_postfix_expression = 172,       /* postfix_expression  */
  YYSYMBOL_integer_expression = 173,       /* integer_expression  */
  YYSYMBOL_function_call = 174,            /* function_call  */
  YYSYMBOL_function_call_or_method = 175,  /* function_call_or_method  */
  YYSYMBOL_function_call_generic = 176,    /* function_call_generic  */
  YYSYMBOL_function_call_header_no_parameters = 177, /* function_call_header_no_parameters  */
  YYSYMBOL_function_call_header_with_parameters = 178, /* function_call_header_with_parameters  */
  YYSYMBOL_function_call_header = 179,     /* function_call_header  */
  YYSYMBOL_function_identifier = 180,      /* function_identifier  */
  YYSYMBOL_unary_expression = 181,         /* unary_expression  */
  YYSYMBOL_unary_operator = 182,           /* unary_operator  */
  YYSYMBOL_multiplicative_expression = 183, /* multiplicative_expression  */
  YYSYMBOL_additive_expression = 184,      /* additive_expression  */
  YYSYMBOL_shift_expression = 185,         /* shift_expression  */
  YYSYMBOL_relational_expression = 186,    /* relational_expression  */
  YYSYMBOL_equality_expression = 187,      /* equality_expression  */
  YYSYMBOL_and_expression = 188,           /* and_expression  */
  YYSYMBOL_exclusive_or_expression = 189,  /* exclusive_or_expression  */
  YYSYMBOL_inclusive_or_expression = 190,  /* inclusive_or_expression  */
  YYSYMBOL_logical_and_expression = 191,   /* logical_and_expression  */
  YYSYMBOL_logical_xor_expression = 192,   /* logical_xor_expression  */
  YYSYMBOL_logical_or_expression = 193,    /* logical_or_expression  */
  YYSYMBOL_conditional_expression = 194,   /* conditional_expression  */
  YYSYMBOL_assignment_expression = 195,    /* assignment_expression  */
  YYSYMBOL_assignment_operator = 196,      /* assignment_operator  */
  YYSYMBOL_expression = 197,               /* expression  */
  YYSYMBOL_constant_expression = 198,      /* constant_expression  */
  YYSYMBOL_declaration = 199,              /* declaration  */
  YYSYMBOL_function_prototype = 200,       /* function_prototype  */
  YYSYMBOL_function_declarator = 201,      /* function_declarator  */
  YYSYMBOL_function_header_with_parameters = 202, /* function_header_with_parameters  */
  YYSYMBOL_function_header = 203,          /* function_header  */
  YYSYMBOL_parameter_declarator = 204,     /* parameter_declarator  */
  YYSYMBOL_parameter_declaration = 205,    /* parameter_declaration  */
  YYSYMBOL_parameter_qualifier = 206,      /* parameter_qualifier  */
  YYSYMBOL_parameter_direction_qualifier = 207, /* parameter_direction_qualifier  */
  YYSYMBOL_parameter_type_specifier = 208, /* parameter_type_specifier  */
  YYSYMBOL_init_declarator_list = 209,     /* init_declarator_list  */
  YYSYMBOL_single_declaration = 210,       /* single_declaration  */
  YYSYMBOL_fully_specified_type = 211,     /* fully_specified_type  */
  YYSYMBOL_layout_qualifier = 212,         /* layout_qualifier  */
  YYSYMBOL_layout_qualifier_id_list = 213, /* layout_qualifier_id_list  */
  YYSYMBOL_layout_qualifier_id = 214,      /* layout_qualifier_id  */
  YYSYMBOL_interface_block_layout_qualifier = 215, /* interface_block_layout_qualifier  */
  YYSYMBOL_subroutine_qualifier = 216,     /* subroutine_qualifier  */
  YYSYMBOL_subroutine_type_list = 217,     /* subroutine_type_list  */
  YYSYMBOL_interpolation_qualifier = 218,  /* interpolation_qualifier  */
  YYSYMBOL_type_qualifier = 219,           /* type_qualifier  */
  YYSYMBOL_auxiliary_storage_qualifier = 220, /* auxiliary_storage_qualifier  */
  YYSYMBOL_storage_qualifier = 221,        /* storage_qualifier  */
  YYSYMBOL_memory_qualifier = 222,         /* memory_qualifier  */
  YYSYMBOL_array_specifier = 223,          /* array_specifier  */
  YYSYMBOL_type_specifier = 224,           /* type_specifier  */
  YYSYMBOL_type_specifier_nonarray = 225,  /* type_specifier_nonarray  */
  YYSYMBOL_basic_type_specifier_nonarray = 226, /* basic_type_specifier_nonarray  */
  YYSYMBOL_precision_qualifier = 227,      /* precision_qualifier  */
  YYSYMBOL_struct_specifier = 228,         /* struct_specifier  */
  YYSYMBOL_struct_declaration_list = 229,  /* struct_declaration_list  */
  YYSYMBOL_struct_declaration = 230,       /* struct_declaration  */
  YYSYMBOL_struct_declarator_list = 231,   /* struct_declarator_list  */
  YYSYMBOL_struct_declarator = 232,        /* struct_declarator  */
  YYSYMBOL_initializer = 233,              /* initializer  */
  YYSYMBOL_initializer_list = 234,         /* initializer_list  */
  YYSYMBOL_declaration_statement = 235,    /* declaration_statement  */
  YYSYMBOL_statement = 236,                /* statement  */
  YYSYMBOL_simple_statement = 237,         /* simple_statement  */
  YYSYMBOL_compound_statement = 238,       /* compound_statement  */
  YYSYMBOL_239_2 = 239,                    /* $@2  */
  YYSYMBOL_statement_no_new_scope = 240,   /* statement_no_new_scope  */
  YYSYMBOL_compound_statement_no_new_scope = 241, /* compound_statement_no_new_scope  */
  YYSYMBOL_statement_list = 242,           /* statement_list  */
  YYSYMBOL_expression_statement = 243,     /* expression_statement  */
  YYSYMBOL_selection_statement = 244,      /* selection_statement  */
  YYSYMBOL_selection_rest_statement = 245, /* selection_rest_statement  */
  YYSYMBOL_condition = 246,                /* condition  */
  YYSYMBOL_switch_statement = 247,         /* switch_statement  */
  YYSYMBOL_switch_body = 248,              /* switch_body  */
  YYSYMBOL_case_label = 249,               /* case_label  */
  YYSYMBOL_case_label_list = 250,          /* case_label_list  */
  YYSYMBOL_case_statement = 251,           /* case_statement  */
  YYSYMBOL_case_statement_list = 252,      /* case_statement_list  */
  YYSYMBOL_iteration_statement = 253,      /* iteration_statement  */
  YYSYMBOL_for_init_statement = 254,       /* for_init_statement  */
  YYSYMBOL_conditionopt = 255,             /* conditionopt  */
  YYSYMBOL_for_rest_statement = 256,       /* for_rest_statement  */
  YYSYMBOL_jump_statement = 257,           /* jump_statement  */
  YYSYMBOL_external_declaration = 258,     /* external_declaration  */
  YYSYMBOL_function_definition = 259,      /* function_definition  */
  YYSYMBOL_interface_block = 260,          /* interface_block  */
  YYSYMBOL_basic_interface_block = 261,    /* basic_interface_block  */
  YYSYMBOL_interface_qualifier = 262,      /* interface_qualifier  */
  YYSYMBOL_instance_name_opt = 263,        /* instance_name_opt  */
  YYSYMBOL_member_list = 264,              /* member_list  */
  YYSYMBOL_member_declaration = 265,       /* member_declaration  */
  YYSYMBOL_layout_uniform_defaults = 266,  /* layout_uniform_defaults  */
  YYSYMBOL_layout_buffer_defaults = 267,   /* layout_buffer_defaults  */
  YYSYMBOL_layout_in_defaults = 268,       /* layout_in_defaults  */
  YYSYMBOL_layout_out_defaults = 269,      /* layout_out_defaults  */
  YYSYMBOL_layout_defaults = 270           /* layout_defaults  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* 1 */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL \
             && defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  5
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   2180

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  161
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  110
/* YYNRULES -- Number of rules.  */
#define YYNRULES  308
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  469

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   392


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   145,     2,     2,     2,   149,   152,     2,
     138,   139,   147,   143,   142,   144,     2,   148,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,   156,   158,
     150,   157,   151,   155,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,   140,     2,   141,   153,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,   159,   154,   160,   146,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    69,    70,    71,    72,    73,    74,
      75,    76,    77,    78,    79,    80,    81,    82,    83,    84,
      85,    86,    87,    88,    89,    90,    91,    92,    93,    94,
      95,    96,    97,    98,    99,   100,   101,   102,   103,   104,
     105,   106,   107,   108,   109,   110,   111,   112,   113,   114,
     115,   116,   117,   118,   119,   120,   121,   122,   123,   124,
     125,   126,   127,   128,   129,   130,   131,   132,   133,   134,
     135,   136,   137
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   291,   291,   290,   314,   316,   323,   333,   334,   335,
     336,   337,   361,   366,   373,   375,   379,   380,   381,   385,
     394,   402,   410,   421,   422,   426,   433,   440,   447,   454,
     461,   468,   475,   482,   489,   490,   496,   500,   507,   513,
     522,   526,   530,   534,   535,   539,   540,   544,   550,   562,
     566,   572,   586,   587,   593,   599,   609,   610,   611,   612,
     616,   617,   623,   629,   638,   639,   645,   654,   655,   661,
     670,   671,   677,   683,   689,   698,   699,   705,   714,   715,
     724,   725,   734,   735,   744,   745,   754,   755,   764,   765,
     774,   775,   784,   785,   794,   795,   796,   797,   798,   799,
     800,   801,   802,   803,   804,   808,   812,   828,   832,   837,
     841,   846,   863,   867,   868,   872,   877,   885,   903,   914,
     931,   946,   954,   971,   974,   982,   990,  1002,  1014,  1021,
    1026,  1031,  1040,  1044,  1045,  1055,  1065,  1075,  1089,  1096,
    1107,  1118,  1129,  1140,  1152,  1167,  1174,  1192,  1199,  1200,
    1210,  1683,  1848,  1874,  1879,  1884,  1892,  1897,  1906,  1915,
    1927,  1932,  1937,  1946,  1951,  1956,  1957,  1958,  1959,  1960,
    1961,  1962,  1980,  1988,  2013,  2037,  2051,  2056,  2072,  2092,
    2104,  2112,  2117,  2122,  2129,  2134,  2139,  2144,  2149,  2174,
    2186,  2191,  2196,  2204,  2209,  2214,  2220,  2225,  2233,  2241,
    2247,  2257,  2268,  2269,  2277,  2283,  2289,  2298,  2299,  2303,
    2308,  2313,  2321,  2328,  2345,  2350,  2358,  2396,  2401,  2409,
    2415,  2424,  2425,  2429,  2436,  2443,  2450,  2456,  2457,  2461,
    2462,  2463,  2464,  2465,  2466,  2470,  2477,  2476,  2490,  2491,
    2495,  2501,  2510,  2520,  2532,  2538,  2547,  2556,  2561,  2569,
    2573,  2591,  2599,  2604,  2612,  2617,  2625,  2633,  2641,  2649,
    2657,  2665,  2673,  2680,  2687,  2697,  2698,  2702,  2704,  2710,
    2715,  2724,  2730,  2736,  2742,  2748,  2757,  2758,  2759,  2760,
    2761,  2765,  2779,  2783,  2796,  2814,  2833,  2838,  2843,  2848,
    2853,  2868,  2871,  2876,  2884,  2889,  2897,  2921,  2928,  2932,
    2939,  2943,  2953,  2962,  2972,  2981,  2993,  3015,  3025
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "ATTRIBUTE",
  "CONST_TOK", "BASIC_TYPE_TOK", "BREAK", "BUFFER", "CONTINUE", "DO",
  "ELSE", "FOR", "IF", "DISCARD", "RETURN", "SWITCH", "CASE", "DEFAULT",
  "CENTROID", "IN_TOK", "OUT_TOK", "INOUT_TOK", "UNIFORM", "VARYING",
  "SAMPLE", "NOPERSPECTIVE", "FLAT", "SMOOTH", "IMAGE1DSHADOW",
  "IMAGE2DSHADOW", "IMAGE1DARRAYSHADOW", "IMAGE2DARRAYSHADOW", "COHERENT",
  "VOLATILE", "RESTRICT", "READONLY", "WRITEONLY", "SHARED", "STRUCT",
  "VOID_TOK", "WHILE", "IDENTIFIER", "TYPE_IDENTIFIER", "NEW_IDENTIFIER",
  "FLOATCONSTANT", "DOUBLECONSTANT", "INTCONSTANT", "UINTCONSTANT",
  "BOOLCONSTANT", "INT64CONSTANT", "UINT64CONSTANT", "FIELD_SELECTION",
  "LEFT_OP", "RIGHT_OP", "INC_OP", "DEC_OP", "LE_OP", "GE_OP", "EQ_OP",
  "NE_OP", "AND_OP", "OR_OP", "XOR_OP", "MUL_ASSIGN", "DIV_ASSIGN",
  "ADD_ASSIGN", "MOD_ASSIGN", "LEFT_ASSIGN", "RIGHT_ASSIGN", "AND_ASSIGN",
  "XOR_ASSIGN", "OR_ASSIGN", "SUB_ASSIGN", "INVARIANT", "PRECISE", "LOWP",
  "MEDIUMP", "HIGHP", "SUPERP", "PRECISION", "VERSION_TOK", "EXTENSION",
  "LINE", "COLON", "EOL", "INTERFACE", "OUTPUT", "PRAGMA_DEBUG_ON",
  "PRAGMA_DEBUG_OFF", "PRAGMA_OPTIMIZE_ON", "PRAGMA_OPTIMIZE_OFF",
  "PRAGMA_WARNING_ON", "PRAGMA_WARNING_OFF", "PRAGMA_INVARIANT_ALL",
  "LAYOUT_TOK", "DOT_TOK", "ASM", "CLASS", "UNION", "ENUM", "TYPEDEF",
  "TEMPLATE", "THIS", "PACKED_TOK", "GOTO", "INLINE_TOK", "NOINLINE",
  "PUBLIC_TOK", "STATIC", "EXTERN", "EXTERNAL", "LONG_TOK", "SHORT_TOK",
  "HALF", "FIXED_TOK", "UNSIGNED", "INPUT_TOK", "HVEC2", "HVEC3", "HVEC4",
  "FVEC2", "FVEC3", "FVEC4", "SAMPLER3DRECT", "SIZEOF", "CAST",
  "NAMESPACE", "USING", "RESOURCE", "PATCH", "SUBROUTINE", "ERROR_TOK",
  "COMMON", "PARTITION", "ACTIVE", "FILTER", "ROW_MAJOR", "THEN", "'('",
  "')'", "'['", "']'", "','", "'+'", "'-'", "'!'", "'~'", "'*'", "'/'",
  "'%'", "'<'", "'>'", "'&'", "'^'", "'|'", "'?'", "':'", "'='", "';'",
  "'{'", "'}'", "$accept", "translation_unit", "$@1", "version_statement",
  "pragma_statement", "extension_statement_list", "any_identifier",
  "extension_statement", "external_declaration_list",
  "variable_identifier", "primary_expression", "postfix_expression",
  "integer_expression", "function_call", "function_call_or_method",
  "function_call_generic", "function_call_header_no_parameters",
  "function_call_header_with_parameters", "function_call_header",
  "function_identifier", "unary_expression", "unary_operator",
  "multiplicative_expression", "additive_expression", "shift_expression",
  "relational_expression", "equality_expression", "and_expression",
  "exclusive_or_expression", "inclusive_or_expression",
  "logical_and_expression", "logical_xor_expression",
  "logical_or_expression", "conditional_expression",
  "assignment_expression", "assignment_operator", "expression",
  "constant_expression", "declaration", "function_prototype",
  "function_declarator", "function_header_with_parameters",
  "function_header", "parameter_declarator", "parameter_declaration",
  "parameter_qualifier", "parameter_direction_qualifier",
  "parameter_type_specifier", "init_declarator_list", "single_declaration",
  "fully_specified_type", "layout_qualifier", "layout_qualifier_id_list",
  "layout_qualifier_id", "interface_block_layout_qualifier",
  "subroutine_qualifier", "subroutine_type_list",
  "interpolation_qualifier", "type_qualifier",
  "auxiliary_storage_qualifier", "storage_qualifier", "memory_qualifier",
  "array_specifier", "type_specifier", "type_specifier_nonarray",
  "basic_type_specifier_nonarray", "precision_qualifier",
  "struct_specifier", "struct_declaration_list", "struct_declaration",
  "struct_declarator_list", "struct_declarator", "initializer",
  "initializer_list", "declaration_statement", "statement",
  "simple_statement", "compound_statement", "$@2",
  "statement_no_new_scope", "compound_statement_no_new_scope",
  "statement_list", "expression_statement", "selection_statement",
  "selection_rest_statement", "condition", "switch_statement",
  "switch_body", "case_label", "case_label_list", "case_statement",
  "case_statement_list", "iteration_statement", "for_init_statement",
  "conditionopt", "for_rest_statement", "jump_statement",
  "external_declaration", "function_definition", "interface_block",
  "basic_interface_block", "interface_qualifier", "instance_name_opt",
  "member_list", "member_declaration", "layout_uniform_defaults",
  "layout_buffer_defaults", "layout_in_defaults", "layout_out_defaults",
  "layout_defaults", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-335)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-290)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     -32,    40,   112,  -335,   -14,  -335,    46,  -335,  -335,  -335,
    -335,    69,    47,  1467,  -335,  -335,    68,  -335,  -335,  -335,
     133,  -335,   158,   164,  -335,   171,  -335,  -335,  -335,  -335,
    -335,  -335,  -335,  -335,  -335,  -335,  -335,   -20,  -335,  -335,
    1859,  1859,  -335,  -335,  -335,   183,   135,   178,   190,   192,
     197,   200,   207,   117,  -335,   155,  -335,  -335,  1368,  -335,
    -116,   156,   160,   176,  -126,  -335,   246,  1922,  1987,  1987,
     123,  2050,  1987,  2050,  -335,   157,  -335,  1987,  -335,  -335,
    -335,  -335,  -335,   261,  -335,  -335,  -335,  -335,  -335,    47,
    1782,   148,  -335,  -335,  -335,  -335,  -335,  -335,  1987,  1987,
    -335,  1987,  -335,  1987,  1987,  -335,  -335,   123,  -335,  -335,
    -335,  -335,  -335,  -335,  -335,    67,    47,  -335,  -335,  -335,
     485,  -335,  -335,   315,   315,  -335,  -335,  -335,   315,  -335,
       2,   315,   315,   315,    47,  -335,   170,   172,  -102,   173,
     -38,   -26,   -23,   -18,  -335,  -335,  -335,  -335,  -335,  -335,
    -335,  -335,  -335,  -335,  -335,  -335,  2050,  -335,  -335,   635,
     174,  -335,   161,   232,    47,   797,  -335,  1782,   175,  -335,
    -335,  -335,   180,    43,  -335,  -335,  -335,    63,   184,   188,
    1054,   191,   193,   195,  1524,   203,   214,  -335,  -335,  -335,
    -335,  -335,  -335,  -335,  1596,  1596,  1596,  -335,  -335,  -335,
    -335,  -335,   168,  -335,  -335,  -335,   104,  -335,  -335,  -335,
     199,    76,  1707,   218,   594,  1596,   151,   102,   196,    22,
     211,   206,   177,   205,   300,   299,   -53,  -335,  -335,   -67,
    -335,   209,   225,  -335,  -335,  -335,  -335,   562,  -335,  -335,
    -335,  -335,  -335,  -335,  -335,  -335,  -335,  -335,   123,    47,
    -335,  -335,  -335,  -101,   890,   -88,  -335,  -335,  -335,  -335,
    -335,  -335,  -335,  -335,   223,  -335,  1132,  1782,  -335,   157,
     -58,  -335,  -335,  -335,   878,  -335,  1596,  -335,    67,  -335,
      47,  -335,  -335,   328,  1288,  1596,  -335,  -335,    33,  1596,
    1653,  -335,  -335,    87,  -335,  1054,  -335,  -335,   318,  1596,
    -335,  -335,  1596,   231,  -335,  -335,  -335,  -335,  -335,  -335,
    -335,  -335,  -335,  -335,  -335,  -335,  -335,  1596,  -335,  1596,
    1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,
    1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,  1596,
    -335,  -335,  -335,    47,   157,   890,     3,   890,  -335,  -335,
     890,  -335,  -335,   230,    47,   212,  1782,   174,    47,  -335,
    -335,  -335,  -335,  -335,   235,  -335,  -335,  1653,    92,  -335,
      94,   233,    47,   238,  -335,   720,  -335,   237,   233,  -335,
    -335,  -335,  -335,  -335,   151,   151,   102,   102,   196,   196,
     196,   196,    22,    22,   211,   206,   177,   205,   300,   299,
    -106,  -335,  -335,   174,  -335,   890,  -335,  -127,  -335,  -335,
      35,   331,  -335,  -335,  1596,  -335,   221,   241,  1054,   222,
     226,  1211,  -335,  -335,  1596,  -335,  1473,  -335,  -335,   157,
     224,   101,  1596,  1211,   374,  -335,   -15,  -335,   890,  -335,
    -335,  -335,  -335,  -335,  -335,   174,  -335,   227,   233,  -335,
    1054,  1596,   239,  -335,  -335,   977,  1054,    -4,  -335,  -335,
    -335,    44,  -335,  -335,  -335,  -335,  -335,  1054,  -335
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int16 yydefact[] =
{
       4,     0,     0,    14,     0,     1,     2,    16,    17,    18,
       5,     0,     0,     0,    15,     6,     0,   185,   184,   208,
     191,   181,   187,   188,   189,   190,   186,   182,   162,   161,
     160,   193,   194,   195,   196,   197,   192,     0,   207,   206,
     163,   164,   211,   210,   209,     0,     0,     0,     0,     0,
       0,     0,     0,     0,   183,   156,   280,   278,     3,   277,
       0,     0,   114,   123,     0,   133,   138,   168,   170,   167,
       0,   165,   166,   169,   145,   202,   204,   171,   205,    20,
     276,   111,   282,     0,   305,   306,   307,   308,   279,     0,
       0,     0,   191,   187,   188,   190,    23,    24,   163,   164,
     143,   168,   173,   165,   169,   144,   172,     0,     7,     8,
       9,    10,    12,    13,    11,     0,     0,    22,    21,   108,
       0,   281,   112,   123,   123,   129,   130,   131,   123,   115,
       0,   123,   123,   123,     0,   109,    16,    18,   139,     0,
     191,   187,   188,   190,   175,   283,   297,   299,   301,   303,
     176,   174,   146,   177,   290,   178,   168,   180,   284,     0,
     203,   179,     0,     0,     0,     0,   214,     0,     0,   155,
     154,   153,   150,     0,   148,   152,   158,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,    30,    31,    26,
      27,    32,    28,    29,     0,     0,     0,    56,    57,    58,
      59,   244,   236,   240,    25,    34,    52,    36,    41,    42,
       0,     0,    46,     0,    60,     0,    64,    67,    70,    75,
      78,    80,    82,    84,    86,    88,    90,    92,   105,     0,
     226,     0,   145,   229,   242,   228,   227,     0,   230,   231,
     232,   233,   234,   116,   124,   125,   121,   122,     0,   132,
     126,   128,   127,   134,     0,   140,   117,   300,   302,   304,
     298,   198,    60,   107,     0,    50,     0,     0,    19,   219,
       0,   217,   213,   215,     0,   110,     0,   147,     0,   157,
       0,   272,   271,     0,     0,     0,   275,   273,     0,     0,
       0,    53,    54,     0,   235,     0,    38,    39,     0,     0,
      44,    43,     0,   207,    47,    49,    95,    96,    98,    97,
     100,   101,   102,   103,   104,    99,    94,     0,    55,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     245,   241,   243,     0,   118,     0,   135,     0,   221,   142,
       0,   199,   200,     0,     0,     0,   294,   220,     0,   216,
     212,   151,   149,   159,     0,   266,   265,   268,     0,   274,
       0,   249,     0,     0,    33,     0,    37,     0,    40,    48,
      93,    61,    62,    63,    65,    66,    68,    69,    73,    74,
      71,    72,    76,    77,    79,    81,    83,    85,    87,    89,
       0,   106,   119,   120,   137,     0,   224,     0,   141,   201,
       0,   291,   295,   218,     0,   267,     0,     0,     0,     0,
       0,     0,   237,    35,     0,   136,     0,   222,   296,   292,
       0,     0,   269,     0,   248,   246,     0,   251,     0,   239,
     262,   238,    91,   223,   225,   293,   285,     0,   270,   264,
       0,     0,     0,   252,   256,     0,   260,     0,   250,   263,
     247,     0,   255,   258,   257,   259,   253,   261,   254
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -335,  -335,  -335,  -335,  -335,  -335,    14,   329,  -335,     6,
    -335,  -335,  -335,  -335,  -335,  -335,  -335,  -335,  -335,  -335,
     150,  -335,   -17,   -12,   -62,    -7,    56,    60,    62,    59,
      61,    64,  -335,  -140,  -201,  -335,  -111,   -82,    18,    24,
    -335,  -335,  -335,  -335,   275,    89,  -335,  -335,  -335,  -335,
     -84,     1,  -335,   121,  -335,  -335,  -335,  -335,    -6,    65,
    -335,    -9,  -128,   -13,  -335,  -335,   194,  -335,   236,  -156,
      48,    42,  -158,  -335,   120,  -177,  -334,  -335,  -335,   -27,
     347,   115,   127,  -335,  -335,    50,  -335,  -335,   -42,  -335,
     -39,  -335,  -335,  -335,  -335,  -335,  -335,   356,  -335,   -43,
    -335,   344,  -335,    71,  -335,   358,   361,   362,   363,  -335
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     2,    13,     3,    57,     6,   269,    14,    58,   204,
     205,   206,   377,   207,   208,   209,   210,   211,   212,   213,
     214,   215,   216,   217,   218,   219,   220,   221,   222,   223,
     224,   225,   226,   227,   228,   317,   229,   264,   230,   231,
      61,    62,    63,   246,   129,   130,   131,   247,    64,    65,
      66,   101,   173,   174,   175,    68,   177,    69,    70,    71,
      72,   104,   160,   265,    75,    76,    77,    78,   165,   166,
     270,   271,   349,   407,   233,   234,   235,   236,   295,   440,
     441,   237,   238,   239,   435,   373,   240,   437,   454,   455,
     456,   457,   241,   367,   416,   417,   242,    79,    80,    81,
      82,    83,   430,   355,   356,    84,    85,    86,    87,    88
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      74,   451,   452,   283,    73,  -289,   164,    19,   337,   273,
     255,   304,   451,   452,    67,   426,   134,  -286,    11,   263,
    -287,     7,     8,     9,   145,  -288,    16,     7,     8,     9,
     158,    59,   135,   427,   102,   106,   339,    60,   159,   159,
      37,    38,   119,   120,    39,    74,   100,   105,     1,    73,
     424,    91,   266,   348,   132,   254,   345,   152,    73,    67,
     342,   144,   150,   151,    73,   153,   155,   157,    67,   350,
      10,   161,   139,   288,   156,   339,    59,    74,   326,   327,
     138,   164,    60,   164,   358,   293,     4,   439,     7,     8,
       9,   340,   102,   106,   168,   144,    53,   153,   157,   439,
     359,   379,   338,   163,   169,   103,   103,   232,     7,     8,
       9,    73,     5,   145,   132,   132,   380,   249,   273,   132,
     257,   156,   132,   132,   132,   346,   263,    12,    19,   172,
     176,   248,   258,   103,   103,   259,   263,   103,   401,    90,
     260,   357,   103,   266,   348,   453,   348,    73,   253,   348,
     144,    89,    74,    15,    74,   103,   466,   156,   296,   297,
     405,    37,    38,   103,   103,    39,   103,   232,   103,   103,
     170,    73,   328,   329,   368,   339,  -289,   358,   370,   371,
     124,   156,   277,   354,   353,   278,   339,   404,   378,   406,
     164,   369,   408,   428,   361,   125,   126,   127,   342,   298,
     468,  -286,   279,   171,   348,   280,   372,  -287,    31,    32,
      33,    34,    35,   244,  -288,   301,   403,   245,   302,   108,
     250,   251,   252,   442,   232,   348,   374,   400,    73,   339,
     103,   418,   103,   419,   339,   343,   339,   348,   156,   107,
     447,   434,   -51,   339,   299,   322,   323,   425,   324,   325,
     128,    42,    43,    44,    74,   115,   371,   133,    42,    43,
      44,    74,   109,   344,   388,   389,   390,   391,   444,   330,
     331,   232,   354,   460,   110,    73,   111,   232,   463,   465,
     458,   112,   232,   372,   113,   156,    73,   136,     8,   137,
     465,   114,   172,   116,   363,   122,   156,   159,   319,   320,
     321,   445,   123,   431,   162,   384,   385,   167,   -23,   262,
     -24,   256,   386,   387,   266,  -113,   268,   133,   133,   124,
     267,   448,   133,   392,   393,   133,   133,   133,   294,   284,
     333,   285,   103,   275,   125,   126,   127,   276,   300,   103,
     461,   289,   281,    74,   291,   292,   282,    31,    32,    33,
      34,    35,   290,   286,   232,   103,   305,   402,   332,   334,
     335,   336,   232,   -50,   351,   318,    73,   119,   364,   376,
     -45,   409,   411,   414,   429,   339,   156,   421,   423,   432,
     433,   436,   446,   438,   450,   459,   420,   117,   394,   128,
      42,    43,    44,   395,   397,   462,   396,   398,   243,   362,
     413,   399,   410,   274,   365,   232,   449,   121,   232,    73,
     375,   366,    73,   464,   118,   154,   262,   415,   467,   156,
     232,   103,   156,     0,    73,   146,   262,   412,   147,   148,
     149,     0,   103,     0,   156,     0,     0,   232,     0,     0,
       0,    73,   232,   232,     0,     0,    73,    73,     0,     0,
       0,   156,     0,     0,   232,     0,   156,   156,    73,     0,
       0,     0,     0,     0,     0,     0,     0,     0,   156,   381,
     382,   383,   262,   262,   262,   262,   262,   262,   262,   262,
     262,   262,   262,   262,   262,   262,   262,   262,    17,    18,
      19,   178,    20,   179,   180,     0,   181,   182,   183,   184,
     185,     0,     0,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,     0,     0,     0,     0,    31,    32,    33,
      34,    35,    36,    37,    38,   186,    96,    39,    97,   187,
     188,   189,   190,   191,   192,   193,     0,     0,     0,   194,
     195,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    40,    41,
      42,    43,    44,     0,    45,    17,    18,    19,   178,    20,
     179,   180,     0,   181,   182,   183,   184,   185,     0,    53,
      21,    22,    23,    24,    25,    26,    27,    28,    29,    30,
       0,     0,     0,     0,    31,    32,    33,    34,    35,    36,
      37,    38,   186,    96,    39,    97,   187,   188,   189,   190,
     191,   192,   193,     0,    54,    55,   194,   195,     0,     0,
       0,     0,     0,   196,     0,     0,     0,     0,   197,   198,
     199,   200,     0,     0,     0,    40,    41,    42,    43,    44,
      19,    45,     0,   201,   202,   203,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,   306,   307,   308,
     309,   310,   311,   312,   313,   314,   315,     0,     0,     0,
       0,     0,     0,    37,    38,     0,    96,    39,    97,   187,
     188,   189,   190,   191,   192,   193,     0,     0,     0,   194,
     195,    54,    55,     0,     0,     0,     0,     0,     0,     0,
     196,     0,     0,     0,     0,   197,   198,   199,   200,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     201,   202,   341,    17,    18,    19,   178,    20,   179,   180,
       0,   181,   182,   183,   184,   185,     0,     0,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,     0,     0,
       0,   316,    31,    32,    33,    34,    35,    36,    37,    38,
     186,    96,    39,    97,   187,   188,   189,   190,   191,   192,
     193,     0,     0,   196,   194,   195,   261,     0,   197,   198,
     199,   200,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,    40,    41,    42,    43,    44,     0,    45,
      17,    18,    19,     0,    92,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,    21,    93,    94,    24,    95,
      26,    27,    28,    29,    30,     0,     0,     0,     0,    31,
      32,    33,    34,    35,    36,    37,    38,     0,     0,    39,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55,     0,     0,     0,     0,     0,     0,     0,   196,     0,
       0,     0,     0,   197,   198,   199,   200,     0,     0,     0,
      98,    99,    42,    43,    44,     0,     0,     0,   201,   202,
     422,    17,    18,    19,     0,    92,     0,     0,     0,     0,
       0,    53,     0,     0,     0,    19,    21,    93,    94,    24,
      95,    26,    27,    28,    29,    30,     0,     0,     0,     0,
      31,    32,    33,    34,    35,    36,    37,    38,     0,     0,
      39,     0,     0,     0,     0,     0,    54,    55,    37,    38,
       0,    96,    39,    97,   187,   188,   189,   190,   191,   192,
     193,     0,     0,     0,   194,   195,     0,     0,     0,     0,
       0,    98,    99,    42,    43,    44,     0,   272,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    53,     0,     0,     0,     0,     0,     0,     0,
      17,    18,    19,   178,    20,   179,   180,     0,   181,   182,
     183,   184,   185,   451,   452,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,     0,     0,    54,    55,    31,
      32,    33,    34,    35,    36,    37,    38,   186,    96,    39,
      97,   187,   188,   189,   190,   191,   192,   193,   196,     0,
       0,   194,   195,   197,   198,   199,   200,     0,   360,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,   347,
      40,    41,    42,    43,    44,     0,    45,    17,    18,    19,
     178,    20,   179,   180,     0,   181,   182,   183,   184,   185,
       0,    53,    21,    22,    23,    24,    25,    26,    27,    28,
      29,    30,     0,     0,     0,     0,    31,    32,    33,    34,
      35,    36,    37,    38,   186,    96,    39,    97,   187,   188,
     189,   190,   191,   192,   193,     0,    54,    55,   194,   195,
       0,     0,     0,     0,     0,   196,     0,     0,     0,     0,
     197,   198,   199,   200,     0,     0,     0,    40,    41,    42,
      43,    44,     0,    45,     0,   201,   202,    19,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      37,    38,     0,    96,    39,    97,   187,   188,   189,   190,
     191,   192,   193,    54,    55,     0,   194,   195,     0,     0,
       0,     0,   196,     0,     0,     0,     0,   197,   198,   199,
     200,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,   201,   202,    17,    18,    19,   178,    20,   179,
     180,     0,   181,   182,   183,   184,   185,     0,     0,    21,
      22,    23,    24,    25,    26,    27,    28,    29,    30,     0,
       0,     0,     0,    31,    32,    33,    34,    35,    36,    37,
      38,   186,    96,    39,    97,   187,   188,   189,   190,   191,
     192,   193,     0,     0,     0,   194,   195,     0,     0,     0,
     196,     0,     0,   352,     0,   197,   198,   199,   200,     0,
       0,     0,     0,     0,    40,    41,    42,    43,    44,     0,
      45,    17,    18,    19,     0,    20,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    53,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,     0,     0,     0,     0,
      31,    32,    33,    34,    35,    36,    37,    38,     0,    96,
      39,    97,   187,   188,   189,   190,   191,   192,   193,     0,
      54,    55,   194,   195,     0,     0,     0,     0,     0,   196,
       0,     0,     0,     0,   197,   198,   199,   200,     0,     0,
       0,    40,    41,    42,    43,    44,     0,    45,     0,   201,
     120,    17,    18,    19,     0,    20,     0,     0,     0,     0,
       0,     0,    53,     0,     0,     0,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,     0,     0,     0,     0,
      31,    32,    33,    34,    35,    36,    37,    38,     0,     0,
      39,     0,     0,     0,     0,     0,     0,    54,    55,     0,
       0,     0,     0,     0,     0,     0,   196,     0,     0,     0,
       0,   197,   198,   199,   200,     0,     0,     0,     0,     0,
       0,    40,    41,    42,    43,    44,   201,    45,     0,    12,
       0,     0,     0,     0,     0,    46,    47,    48,    49,    50,
      51,    52,    53,     0,     0,     0,     0,     0,     0,     0,
      17,    18,    19,     0,    20,     0,     0,     0,    19,     0,
       0,     0,     0,     0,     0,    21,    22,    23,    24,    25,
      26,    27,    28,    29,    30,     0,     0,    54,    55,    31,
      32,    33,    34,    35,    36,    37,    38,     0,     0,    39,
       0,    37,    38,     0,    96,    39,    97,   187,   188,   189,
     190,   191,   192,   193,     0,     0,    56,   194,   195,    19,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      40,    41,    42,    43,    44,     0,    45,     0,     0,     0,
       0,     0,     0,     0,    46,    47,    48,    49,    50,    51,
      52,    53,    37,    38,     0,    96,    39,    97,   187,   188,
     189,   190,   191,   192,   193,     0,     0,     0,   194,   195,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,     0,     0,
       0,    19,     0,     0,     0,     0,     0,     0,     0,     0,
       0,   196,     0,     0,     0,     0,   197,   198,   199,   200,
       0,     0,     0,     0,     0,    56,     0,     0,     0,     0,
       0,     0,   347,   443,    37,    38,     0,    96,    39,    97,
     187,   188,   189,   190,   191,   192,   193,     0,     0,     0,
     194,   195,     0,     0,     0,     0,    17,    18,    19,     0,
      92,     0,   196,     0,     0,     0,     0,   197,   198,   199,
     200,    21,    93,    94,    24,    95,    26,    27,    28,    29,
      30,     0,   287,     0,     0,    31,    32,    33,    34,    35,
      36,    37,    38,     0,    96,    39,    97,   187,   188,   189,
     190,   191,   192,   193,     0,     0,     0,   194,   195,     0,
       0,     0,    19,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    98,    99,    42,    43,
      44,     0,     0,     0,   196,     0,     0,     0,     0,   197,
     198,   199,   200,     0,     0,    37,   303,    53,    96,    39,
      97,   187,   188,   189,   190,   191,   192,   193,     0,     0,
       0,   194,   195,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,    54,    55,     0,    17,    18,    19,     0,    92,
       0,   196,     0,     0,     0,     0,   197,   198,   199,   200,
      21,    93,    94,    24,    95,    26,    27,    28,    29,    30,
       0,     0,     0,     0,    31,    32,    33,    34,    35,    36,
      37,    38,     0,     0,    39,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,   196,     0,     0,     0,     0,
     197,   198,   199,   200,     0,    98,    99,    42,    43,    44,
       0,     0,    17,    18,     0,     0,    92,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    53,    21,    93,    94,
      24,    95,    26,    27,    28,    29,    30,     0,     0,     0,
       0,    31,    32,    33,    34,    35,    36,     0,     0,     0,
      96,     0,    97,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    17,    18,     0,     0,   140,
       0,     0,    98,    99,    42,    43,    44,     0,     0,     0,
      21,   141,   142,    24,   143,    26,    27,    28,    29,    30,
       0,     0,     0,    53,    31,    32,    33,    34,    35,    36,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    54,    55,
      17,    18,     0,     0,    92,    98,    99,    42,    43,    44,
       0,     0,     0,     0,     0,    21,    93,    94,    24,    95,
      26,    27,    28,    29,    30,     0,    53,     0,     0,    31,
      32,    33,    34,    35,    36,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    54,    55,    17,    18,     0,     0,    20,     0,     0,
      98,    99,    42,    43,    44,     0,     0,     0,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,     0,     0,
       0,    53,    31,    32,    33,    34,    35,    36,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    54,    55,     0,     0,
       0,     0,     0,    98,    99,    42,    43,    44,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    53,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    54,
      55
};

static const yytype_int16 yycheck[] =
{
      13,    16,    17,   180,    13,    43,    90,     5,    61,   165,
     138,   212,    16,    17,    13,   142,   142,    43,     4,   159,
      43,    41,    42,    43,    67,    43,    12,    41,    42,    43,
      73,    13,   158,   160,    40,    41,   142,    13,   140,   140,
      38,    39,   158,   159,    42,    58,    40,    41,    80,    58,
     156,    37,   140,   254,    63,   157,   157,    70,    67,    58,
     237,    67,    68,    69,    73,    71,    72,    73,    67,   157,
      84,    77,    66,   184,    73,   142,    58,    90,    56,    57,
      66,   165,    58,   167,   142,   196,    46,   421,    41,    42,
      43,   158,    98,    99,   107,   101,    94,   103,   104,   433,
     158,   302,   155,    89,    37,    40,    41,   120,    41,    42,
      43,   120,     0,   156,   123,   124,   317,   130,   274,   128,
     158,   120,   131,   132,   133,   253,   266,    81,     5,   115,
     116,   130,   158,    68,    69,   158,   276,    72,   339,   159,
     158,   269,    77,   140,   345,   160,   347,   156,   134,   350,
     156,    83,   165,    84,   167,    90,   160,   156,    54,    55,
     157,    38,    39,    98,    99,    42,   101,   180,   103,   104,
     103,   180,   150,   151,   285,   142,    43,   142,   289,   290,
       4,   180,   139,   267,   266,   142,   142,   345,   299,   347,
     274,   158,   350,   158,   276,    19,    20,    21,   375,    95,
     156,    43,   139,   136,   405,   142,   290,    43,    32,    33,
      34,    35,    36,   124,    43,   139,   344,   128,   142,    84,
     131,   132,   133,   424,   237,   426,   139,   338,   237,   142,
     165,   139,   167,   139,   142,   248,   142,   438,   237,    45,
     139,   418,   138,   142,   140,   143,   144,   405,    52,    53,
      74,    75,    76,    77,   267,   138,   367,    63,    75,    76,
      77,   274,    84,   249,   326,   327,   328,   329,   426,    58,
      59,   284,   356,   450,    84,   284,    84,   290,   455,   456,
     438,    84,   295,   367,    84,   284,   295,    41,    42,    43,
     467,    84,   278,   138,   280,   139,   295,   140,   147,   148,
     149,   429,   142,   414,    43,   322,   323,   159,   138,   159,
     138,   138,   324,   325,   140,   139,    84,   123,   124,     4,
     159,   432,   128,   330,   331,   131,   132,   133,   160,   138,
     153,   138,   267,   158,    19,    20,    21,   157,   139,   274,
     451,   138,   158,   356,   194,   195,   158,    32,    33,    34,
      35,    36,   138,   158,   367,   290,   138,   343,   152,   154,
      60,    62,   375,   138,   141,   215,   375,   158,    40,    51,
     139,   141,   160,   138,    43,   142,   375,   139,   141,   158,
     139,   159,   158,   157,    10,   158,   372,    58,   332,    74,
      75,    76,    77,   333,   335,   156,   334,   336,   123,   278,
     358,   337,   354,   167,   284,   418,   433,    60,   421,   418,
     295,   284,   421,   455,    58,    71,   266,   367,   457,   418,
     433,   356,   421,    -1,   433,    67,   276,   356,    67,    67,
      67,    -1,   367,    -1,   433,    -1,    -1,   450,    -1,    -1,
      -1,   450,   455,   456,    -1,    -1,   455,   456,    -1,    -1,
      -1,   450,    -1,    -1,   467,    -1,   455,   456,   467,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   467,   319,
     320,   321,   322,   323,   324,   325,   326,   327,   328,   329,
     330,   331,   332,   333,   334,   335,   336,   337,     3,     4,
       5,     6,     7,     8,     9,    -1,    11,    12,    13,    14,
      15,    -1,    -1,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    -1,    -1,    -1,    -1,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    -1,    -1,    -1,    54,
      55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    73,    74,
      75,    76,    77,    -1,    79,     3,     4,     5,     6,     7,
       8,     9,    -1,    11,    12,    13,    14,    15,    -1,    94,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    39,    40,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,    -1,   129,   130,    54,    55,    -1,    -1,
      -1,    -1,    -1,   138,    -1,    -1,    -1,    -1,   143,   144,
     145,   146,    -1,    -1,    -1,    73,    74,    75,    76,    77,
       5,    79,    -1,   158,   159,   160,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    94,    63,    64,    65,
      66,    67,    68,    69,    70,    71,    72,    -1,    -1,    -1,
      -1,    -1,    -1,    38,    39,    -1,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    -1,    -1,    -1,    54,
      55,   129,   130,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     138,    -1,    -1,    -1,    -1,   143,   144,   145,   146,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
     158,   159,   160,     3,     4,     5,     6,     7,     8,     9,
      -1,    11,    12,    13,    14,    15,    -1,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    -1,    -1,
      -1,   157,    32,    33,    34,    35,    36,    37,    38,    39,
      40,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,   138,    54,    55,   141,    -1,   143,   144,
     145,   146,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    73,    74,    75,    76,    77,    -1,    79,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    94,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,    32,
      33,    34,    35,    36,    37,    38,    39,    -1,    -1,    42,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   129,
     130,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   138,    -1,
      -1,    -1,    -1,   143,   144,   145,   146,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    -1,    -1,    -1,   158,   159,
     160,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    -1,
      -1,    94,    -1,    -1,    -1,     5,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      42,    -1,    -1,    -1,    -1,    -1,   129,   130,    38,    39,
      -1,    41,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    -1,    54,    55,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    -1,   160,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,     6,     7,     8,     9,    -1,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,   129,   130,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,   138,    -1,
      -1,    54,    55,   143,   144,   145,   146,    -1,   160,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   159,
      73,    74,    75,    76,    77,    -1,    79,     3,     4,     5,
       6,     7,     8,     9,    -1,    11,    12,    13,    14,    15,
      -1,    94,    18,    19,    20,    21,    22,    23,    24,    25,
      26,    27,    -1,    -1,    -1,    -1,    32,    33,    34,    35,
      36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    -1,   129,   130,    54,    55,
      -1,    -1,    -1,    -1,    -1,   138,    -1,    -1,    -1,    -1,
     143,   144,   145,   146,    -1,    -1,    -1,    73,    74,    75,
      76,    77,    -1,    79,    -1,   158,   159,     5,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    94,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      38,    39,    -1,    41,    42,    43,    44,    45,    46,    47,
      48,    49,    50,   129,   130,    -1,    54,    55,    -1,    -1,
      -1,    -1,   138,    -1,    -1,    -1,    -1,   143,   144,   145,
     146,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   158,   159,     3,     4,     5,     6,     7,     8,
       9,    -1,    11,    12,    13,    14,    15,    -1,    -1,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    -1,
      -1,    -1,    -1,    32,    33,    34,    35,    36,    37,    38,
      39,    40,    41,    42,    43,    44,    45,    46,    47,    48,
      49,    50,    -1,    -1,    -1,    54,    55,    -1,    -1,    -1,
     138,    -1,    -1,   141,    -1,   143,   144,   145,   146,    -1,
      -1,    -1,    -1,    -1,    73,    74,    75,    76,    77,    -1,
      79,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    94,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    41,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    -1,
     129,   130,    54,    55,    -1,    -1,    -1,    -1,    -1,   138,
      -1,    -1,    -1,    -1,   143,   144,   145,   146,    -1,    -1,
      -1,    73,    74,    75,    76,    77,    -1,    79,    -1,   158,
     159,     3,     4,     5,    -1,     7,    -1,    -1,    -1,    -1,
      -1,    -1,    94,    -1,    -1,    -1,    18,    19,    20,    21,
      22,    23,    24,    25,    26,    27,    -1,    -1,    -1,    -1,
      32,    33,    34,    35,    36,    37,    38,    39,    -1,    -1,
      42,    -1,    -1,    -1,    -1,    -1,    -1,   129,   130,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   138,    -1,    -1,    -1,
      -1,   143,   144,   145,   146,    -1,    -1,    -1,    -1,    -1,
      -1,    73,    74,    75,    76,    77,   158,    79,    -1,    81,
      -1,    -1,    -1,    -1,    -1,    87,    88,    89,    90,    91,
      92,    93,    94,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
       3,     4,     5,    -1,     7,    -1,    -1,    -1,     5,    -1,
      -1,    -1,    -1,    -1,    -1,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    -1,   129,   130,    32,
      33,    34,    35,    36,    37,    38,    39,    -1,    -1,    42,
      -1,    38,    39,    -1,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    -1,    -1,   158,    54,    55,     5,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      73,    74,    75,    76,    77,    -1,    79,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    87,    88,    89,    90,    91,    92,
      93,    94,    38,    39,    -1,    41,    42,    43,    44,    45,
      46,    47,    48,    49,    50,    -1,    -1,    -1,    54,    55,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   129,   130,    -1,    -1,
      -1,     5,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   138,    -1,    -1,    -1,    -1,   143,   144,   145,   146,
      -1,    -1,    -1,    -1,    -1,   158,    -1,    -1,    -1,    -1,
      -1,    -1,   159,   160,    38,    39,    -1,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    -1,    -1,    -1,
      54,    55,    -1,    -1,    -1,    -1,     3,     4,     5,    -1,
       7,    -1,   138,    -1,    -1,    -1,    -1,   143,   144,   145,
     146,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    -1,   158,    -1,    -1,    32,    33,    34,    35,    36,
      37,    38,    39,    -1,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    -1,    -1,    -1,    54,    55,    -1,
      -1,    -1,     5,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    73,    74,    75,    76,
      77,    -1,    -1,    -1,   138,    -1,    -1,    -1,    -1,   143,
     144,   145,   146,    -1,    -1,    38,    39,    94,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    -1,    -1,
      -1,    54,    55,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,   129,   130,    -1,     3,     4,     5,    -1,     7,
      -1,   138,    -1,    -1,    -1,    -1,   143,   144,   145,   146,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    -1,    32,    33,    34,    35,    36,    37,
      38,    39,    -1,    -1,    42,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,   138,    -1,    -1,    -1,    -1,
     143,   144,   145,   146,    -1,    73,    74,    75,    76,    77,
      -1,    -1,     3,     4,    -1,    -1,     7,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    94,    18,    19,    20,
      21,    22,    23,    24,    25,    26,    27,    -1,    -1,    -1,
      -1,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      41,    -1,    43,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   129,   130,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,     3,     4,    -1,    -1,     7,
      -1,    -1,    73,    74,    75,    76,    77,    -1,    -1,    -1,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      -1,    -1,    -1,    94,    32,    33,    34,    35,    36,    37,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   129,   130,
       3,     4,    -1,    -1,     7,    73,    74,    75,    76,    77,
      -1,    -1,    -1,    -1,    -1,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    -1,    94,    -1,    -1,    32,
      33,    34,    35,    36,    37,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,   129,   130,     3,     4,    -1,    -1,     7,    -1,    -1,
      73,    74,    75,    76,    77,    -1,    -1,    -1,    18,    19,
      20,    21,    22,    23,    24,    25,    26,    27,    -1,    -1,
      -1,    94,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   129,   130,    -1,    -1,
      -1,    -1,    -1,    73,    74,    75,    76,    77,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    94,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,   129,
     130
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int16 yystos[] =
{
       0,    80,   162,   164,    46,     0,   166,    41,    42,    43,
      84,   167,    81,   163,   168,    84,   167,     3,     4,     5,
       7,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    32,    33,    34,    35,    36,    37,    38,    39,    42,
      73,    74,    75,    76,    77,    79,    87,    88,    89,    90,
      91,    92,    93,    94,   129,   130,   158,   165,   169,   199,
     200,   201,   202,   203,   209,   210,   211,   212,   216,   218,
     219,   220,   221,   222,   224,   225,   226,   227,   228,   258,
     259,   260,   261,   262,   266,   267,   268,   269,   270,    83,
     159,   167,     7,    19,    20,    22,    41,    43,    73,    74,
     170,   212,   219,   220,   222,   170,   219,   227,    84,    84,
      84,    84,    84,    84,    84,   138,   138,   168,   258,   158,
     159,   241,   139,   142,     4,    19,    20,    21,    74,   205,
     206,   207,   222,   227,   142,   158,    41,    43,   167,   170,
       7,    19,    20,    22,   219,   260,   266,   267,   268,   269,
     219,   219,   224,   219,   262,   219,   212,   219,   260,   140,
     223,   219,    43,   167,   211,   229,   230,   159,   224,    37,
     103,   136,   167,   213,   214,   215,   167,   217,     6,     8,
       9,    11,    12,    13,    14,    15,    40,    44,    45,    46,
      47,    48,    49,    50,    54,    55,   138,   143,   144,   145,
     146,   158,   159,   160,   170,   171,   172,   174,   175,   176,
     177,   178,   179,   180,   181,   182,   183,   184,   185,   186,
     187,   188,   189,   190,   191,   192,   193,   194,   195,   197,
     199,   200,   224,   235,   236,   237,   238,   242,   243,   244,
     247,   253,   257,   205,   206,   206,   204,   208,   212,   224,
     206,   206,   206,   167,   157,   223,   138,   158,   158,   158,
     158,   141,   181,   194,   198,   224,   140,   159,    84,   167,
     231,   232,   160,   230,   229,   158,   157,   139,   142,   139,
     142,   158,   158,   236,   138,   138,   158,   158,   197,   138,
     138,   181,   181,   197,   160,   239,    54,    55,    95,   140,
     139,   139,   142,    39,   195,   138,    63,    64,    65,    66,
      67,    68,    69,    70,    71,    72,   157,   196,   181,   147,
     148,   149,   143,   144,    52,    53,    56,    57,   150,   151,
      58,    59,   152,   153,   154,    60,    62,    61,   155,   142,
     158,   160,   236,   224,   167,   157,   223,   159,   195,   233,
     157,   141,   141,   198,   211,   264,   265,   223,   142,   158,
     160,   198,   214,   167,    40,   235,   243,   254,   197,   158,
     197,   197,   211,   246,   139,   242,    51,   173,   197,   195,
     195,   181,   181,   181,   183,   183,   184,   184,   185,   185,
     185,   185,   186,   186,   187,   188,   189,   190,   191,   192,
     197,   195,   167,   223,   233,   157,   233,   234,   233,   141,
     231,   160,   264,   232,   138,   246,   255,   256,   139,   139,
     167,   139,   160,   141,   156,   233,   142,   160,   158,    43,
     263,   197,   158,   139,   236,   245,   159,   248,   157,   237,
     240,   241,   195,   160,   233,   223,   158,   139,   197,   240,
      10,    16,    17,   160,   249,   250,   251,   252,   233,   158,
     236,   197,   156,   236,   249,   236,   160,   251,   156
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int16 yyr1[] =
{
       0,   161,   163,   162,   164,   164,   164,   165,   165,   165,
     165,   165,   165,   165,   166,   166,   167,   167,   167,   168,
     169,   169,   169,   170,   170,   171,   171,   171,   171,   171,
     171,   171,   171,   171,   172,   172,   172,   172,   172,   172,
     173,   174,   175,   176,   176,   177,   177,   178,   178,   179,
     180,   180,   181,   181,   181,   181,   182,   182,   182,   182,
     183,   183,   183,   183,   184,   184,   184,   185,   185,   185,
     186,   186,   186,   186,   186,   187,   187,   187,   188,   188,
     189,   189,   190,   190,   191,   191,   192,   192,   193,   193,
     194,   194,   195,   195,   196,   196,   196,   196,   196,   196,
     196,   196,   196,   196,   196,   197,   197,   198,   199,   199,
     199,   199,   200,   201,   201,   202,   202,   203,   204,   204,
     204,   205,   205,   206,   206,   206,   206,   206,   206,   207,
     207,   207,   208,   209,   209,   209,   209,   209,   210,   210,
     210,   210,   210,   210,   210,   211,   211,   212,   213,   213,
     214,   214,   214,   215,   215,   215,   216,   216,   217,   217,
     218,   218,   218,   219,   219,   219,   219,   219,   219,   219,
     219,   219,   219,   219,   219,   219,   219,   219,   219,   219,
     219,   220,   220,   220,   221,   221,   221,   221,   221,   221,
     221,   221,   221,   222,   222,   222,   222,   222,   223,   223,
     223,   223,   224,   224,   225,   225,   225,   226,   226,   227,
     227,   227,   228,   228,   229,   229,   230,   231,   231,   232,
     232,   233,   233,   233,   234,   234,   235,   236,   236,   237,
     237,   237,   237,   237,   237,   238,   239,   238,   240,   240,
     241,   241,   242,   242,   243,   243,   244,   245,   245,   246,
     246,   247,   248,   248,   249,   249,   250,   250,   251,   251,
     252,   252,   253,   253,   253,   254,   254,   255,   255,   256,
     256,   257,   257,   257,   257,   257,   258,   258,   258,   258,
     258,   259,   260,   260,   260,   261,   262,   262,   262,   262,
     262,   263,   263,   263,   264,   264,   265,   266,   266,   267,
     267,   268,   268,   269,   269,   270,   270,   270,   270
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     4,     0,     3,     4,     2,     2,     2,
       2,     2,     2,     2,     0,     2,     1,     1,     1,     5,
       1,     2,     2,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     3,     1,     4,     1,     3,     2,     2,
       1,     1,     1,     2,     2,     2,     1,     2,     3,     2,
       1,     1,     1,     2,     2,     2,     1,     1,     1,     1,
       1,     3,     3,     3,     1,     3,     3,     1,     3,     3,
       1,     3,     3,     3,     3,     1,     3,     3,     1,     3,
       1,     3,     1,     3,     1,     3,     1,     3,     1,     3,
       1,     5,     1,     3,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     3,     1,     2,     2,
       4,     1,     2,     1,     1,     2,     3,     3,     2,     3,
       3,     2,     2,     0,     2,     2,     2,     2,     2,     1,
       1,     1,     1,     1,     3,     4,     6,     5,     1,     2,
       3,     5,     4,     2,     2,     1,     2,     4,     1,     3,
       1,     3,     1,     1,     1,     1,     1,     4,     1,     3,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     2,     3,
       3,     4,     1,     2,     1,     1,     1,     1,     1,     1,
       1,     1,     5,     4,     1,     2,     3,     1,     3,     1,
       2,     1,     3,     4,     1,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     0,     4,     1,     1,
       2,     3,     1,     2,     1,     2,     5,     3,     1,     1,
       4,     5,     2,     3,     3,     2,     1,     2,     2,     2,
       1,     2,     5,     7,     6,     1,     1,     1,     0,     2,
       3,     2,     2,     2,     3,     2,     1,     1,     1,     1,
       1,     2,     1,     2,     2,     7,     1,     1,     1,     1,
       2,     0,     1,     2,     1,     2,     3,     2,     3,     2,
       3,     2,     3,     2,     3,     1,     1,     1,     1
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, state, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
}

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, state); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (state);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, struct _mesa_glsl_parse_state *state)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, state);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, struct _mesa_glsl_parse_state *state)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), state);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, state); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


/* Context of a parse error.  */
typedef struct
{
  yy_state_t *yyssp;
  yysymbol_kind_t yytoken;
  YYLTYPE *yylloc;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int
yypcontext_expected_tokens (const yypcontext_t *yyctx,
                            yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default (yyn))
    {
      /* Start YYX at -YYN if negative to avoid negative indexes in
         YYCHECK.  In other words, skip the first -YYN actions for
         this state because they are default actions.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;
      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yyx;
      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
        if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror
            && !yytable_value_is_error (yytable[yyx + yyn]))
          {
            if (!yyarg)
              ++yycount;
            else if (yycount == yyargn)
              return 0;
            else
              yyarg[yycount++] = YY_CAST (yysymbol_kind_t, yyx);
          }
    }
  if (yyarg && yycount == 0 && 0 < yyargn)
    yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}




#ifndef yystrlen
# if defined __GLIBC__ && defined _STRING_H
#  define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
# else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
# endif
#endif

#ifndef yystpcpy
# if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#  define yystpcpy stpcpy
# else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
# endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;
      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
#endif


static int
yy_syntax_error_arguments (const yypcontext_t *yyctx,
                           yysymbol_kind_t yyarg[], int yyargn)
{
  /* Actual size of YYARG. */
  int yycount = 0;
  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY)
    {
      int yyn;
      if (yyarg)
        yyarg[yycount] = yyctx->yytoken;
      ++yycount;
      yyn = yypcontext_expected_tokens (yyctx,
                                        yyarg ? yyarg + 1 : yyarg, yyargn - 1);
      if (yyn == YYENOMEM)
        return YYENOMEM;
      else
        yycount += yyn;
    }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                const yypcontext_t *yyctx)
{
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments (yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM)
    return YYENOMEM;

  switch (yycount)
    {
#define YYCASE_(N, S)                       \
      case N:                               \
        yyformat = S;                       \
        break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
    }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen (yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi)
      {
        YYPTRDIFF_T yysize1
          = yysize + yytnamerr (YY_NULLPTR, yytname[yyarg[yyi]]);
        if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
          yysize = yysize1;
        else
          return YYENOMEM;
      }
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return -1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yytname[yyarg[yyi++]]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, struct _mesa_glsl_parse_state *state)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (state);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}






/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct _mesa_glsl_parse_state *state)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

    /* Number of syntax errors so far.  */
    int yynerrs = 0;

    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls = yylsa;
    YYLTYPE *yylsp = yyls;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */


/* User initialization code.  */
#line 87 "mesa-imported/glsl/glsl_parser.yy"
{
   yylloc.first_line = 1;
   yylloc.first_column = 1;
   yylloc.last_line = 1;
   yylloc.last_column = 1;
   yylloc.source = 0;
}

#line 2360 "generated/glsl/glsl_parser.cpp"

  yylsp[0] = yylloc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex (&yylval, &yylloc, state);
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* $@1: %empty  */
#line 291 "mesa-imported/glsl/glsl_parser.yy"
   {
      _mesa_glsl_initialize_types(state);
   }
#line 2575 "generated/glsl/glsl_parser.cpp"
    break;

  case 3: /* translation_unit: version_statement extension_statement_list $@1 external_declaration_list  */
#line 295 "mesa-imported/glsl/glsl_parser.yy"
   {
      delete state->symbols;
      state->symbols = new(ralloc_parent(state)) glsl_symbol_table;
      if (state->es_shader) {
         if (state->stage == MESA_SHADER_FRAGMENT) {
            state->symbols->add_default_precision_qualifier("int", ast_precision_medium);
         } else {
            state->symbols->add_default_precision_qualifier("float", ast_precision_high);
            state->symbols->add_default_precision_qualifier("int", ast_precision_high);
         }
         state->symbols->add_default_precision_qualifier("sampler2D", ast_precision_low);
         state->symbols->add_default_precision_qualifier("samplerExternalOES", ast_precision_low);
         state->symbols->add_default_precision_qualifier("samplerCube", ast_precision_low);
         state->symbols->add_default_precision_qualifier("atomic_uint", ast_precision_high);
      }
      _mesa_glsl_initialize_types(state);
   }
#line 2597 "generated/glsl/glsl_parser.cpp"
    break;

  case 5: /* version_statement: VERSION_TOK INTCONSTANT EOL  */
#line 317 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->process_version_directive(&(yylsp[-1]), (yyvsp[-1].n), NULL);
      if (state->error) {
         YYERROR;
      }
   }
#line 2608 "generated/glsl/glsl_parser.cpp"
    break;

  case 6: /* version_statement: VERSION_TOK INTCONSTANT any_identifier EOL  */
#line 324 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->process_version_directive(&(yylsp[-2]), (yyvsp[-2].n), (yyvsp[-1].identifier));
      if (state->error) {
         YYERROR;
      }
   }
#line 2619 "generated/glsl/glsl_parser.cpp"
    break;

  case 7: /* pragma_statement: PRAGMA_DEBUG_ON EOL  */
#line 333 "mesa-imported/glsl/glsl_parser.yy"
                       { (yyval.node) = NULL; }
#line 2625 "generated/glsl/glsl_parser.cpp"
    break;

  case 8: /* pragma_statement: PRAGMA_DEBUG_OFF EOL  */
#line 334 "mesa-imported/glsl/glsl_parser.yy"
                          { (yyval.node) = NULL; }
#line 2631 "generated/glsl/glsl_parser.cpp"
    break;

  case 9: /* pragma_statement: PRAGMA_OPTIMIZE_ON EOL  */
#line 335 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = NULL; }
#line 2637 "generated/glsl/glsl_parser.cpp"
    break;

  case 10: /* pragma_statement: PRAGMA_OPTIMIZE_OFF EOL  */
#line 336 "mesa-imported/glsl/glsl_parser.yy"
                             { (yyval.node) = NULL; }
#line 2643 "generated/glsl/glsl_parser.cpp"
    break;

  case 11: /* pragma_statement: PRAGMA_INVARIANT_ALL EOL  */
#line 338 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* Pragma invariant(all) cannot be used in a fragment shader.
       *
       * Page 27 of the GLSL 1.20 spec, Page 53 of the GLSL ES 3.00 spec:
       *
       *     "It is an error to use this pragma in a fragment shader."
       */
      if (state->is_version(120, 300) &&
          state->stage == MESA_SHADER_FRAGMENT) {
         _mesa_glsl_error(& (yylsp[-1]), state,
                          "pragma `invariant(all)' cannot be used "
                          "in a fragment shader.");
      } else if (!state->is_version(120, 100)) {
         _mesa_glsl_warning(& (yylsp[-1]), state,
                            "pragma `invariant(all)' not supported in %s "
                            "(GLSL ES 1.00 or GLSL 1.20 required)",
                            state->get_version_string());
      } else {
         state->all_invariant = true;
      }

      (yyval.node) = NULL;
   }
#line 2671 "generated/glsl/glsl_parser.cpp"
    break;

  case 12: /* pragma_statement: PRAGMA_WARNING_ON EOL  */
#line 362 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *mem_ctx = state->linalloc;
      (yyval.node) = new(mem_ctx) ast_warnings_toggle(true);
   }
#line 2680 "generated/glsl/glsl_parser.cpp"
    break;

  case 13: /* pragma_statement: PRAGMA_WARNING_OFF EOL  */
#line 367 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *mem_ctx = state->linalloc;
      (yyval.node) = new(mem_ctx) ast_warnings_toggle(false);
   }
#line 2689 "generated/glsl/glsl_parser.cpp"
    break;

  case 19: /* extension_statement: EXTENSION any_identifier COLON any_identifier EOL  */
#line 386 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (!_mesa_glsl_process_extension((yyvsp[-3].identifier), & (yylsp[-3]), (yyvsp[-1].identifier), & (yylsp[-1]), state)) {
         YYERROR;
      }
   }
#line 2699 "generated/glsl/glsl_parser.cpp"
    break;

  case 20: /* external_declaration_list: external_declaration  */
#line 395 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* FINISHME: The NULL test is required because pragmas are set to
       * FINISHME: NULL. (See production rule for external_declaration.)
       */
      if ((yyvsp[0].node) != NULL)
         state->translation_unit.push_tail(& (yyvsp[0].node)->link);
   }
#line 2711 "generated/glsl/glsl_parser.cpp"
    break;

  case 21: /* external_declaration_list: external_declaration_list external_declaration  */
#line 403 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* FINISHME: The NULL test is required because pragmas are set to
       * FINISHME: NULL. (See production rule for external_declaration.)
       */
      if ((yyvsp[0].node) != NULL)
         state->translation_unit.push_tail(& (yyvsp[0].node)->link);
   }
#line 2723 "generated/glsl/glsl_parser.cpp"
    break;

  case 22: /* external_declaration_list: external_declaration_list extension_statement  */
#line 410 "mesa-imported/glsl/glsl_parser.yy"
                                                   {
      if (!state->allow_extension_directive_midshader) {
         _mesa_glsl_error(& (yylsp[0]), state,
                          "#extension directive is not allowed "
                          "in the middle of a shader");
         YYERROR;
      }
   }
#line 2736 "generated/glsl/glsl_parser.cpp"
    break;

  case 25: /* primary_expression: variable_identifier  */
#line 427 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_identifier, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.identifier = (yyvsp[0].identifier);
   }
#line 2747 "generated/glsl/glsl_parser.cpp"
    break;

  case 26: /* primary_expression: INTCONSTANT  */
#line 434 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_int_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.int_constant = (yyvsp[0].n);
   }
#line 2758 "generated/glsl/glsl_parser.cpp"
    break;

  case 27: /* primary_expression: UINTCONSTANT  */
#line 441 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_uint_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.uint_constant = (yyvsp[0].n);
   }
#line 2769 "generated/glsl/glsl_parser.cpp"
    break;

  case 28: /* primary_expression: INT64CONSTANT  */
#line 448 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_int64_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.int64_constant = (yyvsp[0].n64);
   }
#line 2780 "generated/glsl/glsl_parser.cpp"
    break;

  case 29: /* primary_expression: UINT64CONSTANT  */
#line 455 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_uint64_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.uint64_constant = (yyvsp[0].n64);
   }
#line 2791 "generated/glsl/glsl_parser.cpp"
    break;

  case 30: /* primary_expression: FLOATCONSTANT  */
#line 462 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_float_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.float_constant = (yyvsp[0].real);
   }
#line 2802 "generated/glsl/glsl_parser.cpp"
    break;

  case 31: /* primary_expression: DOUBLECONSTANT  */
#line 469 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_double_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.double_constant = (yyvsp[0].dreal);
   }
#line 2813 "generated/glsl/glsl_parser.cpp"
    break;

  case 32: /* primary_expression: BOOLCONSTANT  */
#line 476 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_bool_constant, NULL, NULL, NULL);
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->primary_expression.bool_constant = (yyvsp[0].n);
   }
#line 2824 "generated/glsl/glsl_parser.cpp"
    break;

  case 33: /* primary_expression: '(' expression ')'  */
#line 483 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[-1].expression);
   }
#line 2832 "generated/glsl/glsl_parser.cpp"
    break;

  case 35: /* postfix_expression: postfix_expression '[' integer_expression ']'  */
#line 491 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_array_index, (yyvsp[-3].expression), (yyvsp[-1].expression), NULL);
      (yyval.expression)->set_location_range((yylsp[-3]), (yylsp[0]));
   }
#line 2842 "generated/glsl/glsl_parser.cpp"
    break;

  case 36: /* postfix_expression: function_call  */
#line 497 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[0].expression);
   }
#line 2850 "generated/glsl/glsl_parser.cpp"
    break;

  case 37: /* postfix_expression: postfix_expression DOT_TOK FIELD_SELECTION  */
#line 501 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_field_selection, (yyvsp[-2].expression), NULL, NULL);
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
      (yyval.expression)->primary_expression.identifier = (yyvsp[0].identifier);
   }
#line 2861 "generated/glsl/glsl_parser.cpp"
    break;

  case 38: /* postfix_expression: postfix_expression INC_OP  */
#line 508 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_post_inc, (yyvsp[-1].expression), NULL, NULL);
      (yyval.expression)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 2871 "generated/glsl/glsl_parser.cpp"
    break;

  case 39: /* postfix_expression: postfix_expression DEC_OP  */
#line 514 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_post_dec, (yyvsp[-1].expression), NULL, NULL);
      (yyval.expression)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 2881 "generated/glsl/glsl_parser.cpp"
    break;

  case 47: /* function_call_header_with_parameters: function_call_header assignment_expression  */
#line 545 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[-1].expression);
      (yyval.expression)->set_location((yylsp[-1]));
      (yyval.expression)->expressions.push_tail(& (yyvsp[0].expression)->link);
   }
#line 2891 "generated/glsl/glsl_parser.cpp"
    break;

  case 48: /* function_call_header_with_parameters: function_call_header_with_parameters ',' assignment_expression  */
#line 551 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[-2].expression);
      (yyval.expression)->set_location((yylsp[-2]));
      (yyval.expression)->expressions.push_tail(& (yyvsp[0].expression)->link);
   }
#line 2901 "generated/glsl/glsl_parser.cpp"
    break;

  case 50: /* function_identifier: type_specifier  */
#line 567 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_function_expression((yyvsp[0].type_specifier));
      (yyval.expression)->set_location((yylsp[0]));
      }
#line 2911 "generated/glsl/glsl_parser.cpp"
    break;

  case 51: /* function_identifier: postfix_expression  */
#line 573 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_function_expression((yyvsp[0].expression));
      (yyval.expression)->set_location((yylsp[0]));
      }
#line 2921 "generated/glsl/glsl_parser.cpp"
    break;

  case 53: /* unary_expression: INC_OP unary_expression  */
#line 588 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_pre_inc, (yyvsp[0].expression), NULL, NULL);
      (yyval.expression)->set_location((yylsp[-1]));
   }
#line 2931 "generated/glsl/glsl_parser.cpp"
    break;

  case 54: /* unary_expression: DEC_OP unary_expression  */
#line 594 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_pre_dec, (yyvsp[0].expression), NULL, NULL);
      (yyval.expression)->set_location((yylsp[-1]));
   }
#line 2941 "generated/glsl/glsl_parser.cpp"
    break;

  case 55: /* unary_expression: unary_operator unary_expression  */
#line 600 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression((yyvsp[-1].n), (yyvsp[0].expression), NULL, NULL);
      (yyval.expression)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 2951 "generated/glsl/glsl_parser.cpp"
    break;

  case 56: /* unary_operator: '+'  */
#line 609 "mesa-imported/glsl/glsl_parser.yy"
         { (yyval.n) = ast_plus; }
#line 2957 "generated/glsl/glsl_parser.cpp"
    break;

  case 57: /* unary_operator: '-'  */
#line 610 "mesa-imported/glsl/glsl_parser.yy"
         { (yyval.n) = ast_neg; }
#line 2963 "generated/glsl/glsl_parser.cpp"
    break;

  case 58: /* unary_operator: '!'  */
#line 611 "mesa-imported/glsl/glsl_parser.yy"
         { (yyval.n) = ast_logic_not; }
#line 2969 "generated/glsl/glsl_parser.cpp"
    break;

  case 59: /* unary_operator: '~'  */
#line 612 "mesa-imported/glsl/glsl_parser.yy"
         { (yyval.n) = ast_bit_not; }
#line 2975 "generated/glsl/glsl_parser.cpp"
    break;

  case 61: /* multiplicative_expression: multiplicative_expression '*' unary_expression  */
#line 618 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_mul, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 2985 "generated/glsl/glsl_parser.cpp"
    break;

  case 62: /* multiplicative_expression: multiplicative_expression '/' unary_expression  */
#line 624 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_div, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 2995 "generated/glsl/glsl_parser.cpp"
    break;

  case 63: /* multiplicative_expression: multiplicative_expression '%' unary_expression  */
#line 630 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_mod, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3005 "generated/glsl/glsl_parser.cpp"
    break;

  case 65: /* additive_expression: additive_expression '+' multiplicative_expression  */
#line 640 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_add, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3015 "generated/glsl/glsl_parser.cpp"
    break;

  case 66: /* additive_expression: additive_expression '-' multiplicative_expression  */
#line 646 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_sub, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3025 "generated/glsl/glsl_parser.cpp"
    break;

  case 68: /* shift_expression: shift_expression LEFT_OP additive_expression  */
#line 656 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_lshift, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3035 "generated/glsl/glsl_parser.cpp"
    break;

  case 69: /* shift_expression: shift_expression RIGHT_OP additive_expression  */
#line 662 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_rshift, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3045 "generated/glsl/glsl_parser.cpp"
    break;

  case 71: /* relational_expression: relational_expression '<' shift_expression  */
#line 672 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_less, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3055 "generated/glsl/glsl_parser.cpp"
    break;

  case 72: /* relational_expression: relational_expression '>' shift_expression  */
#line 678 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_greater, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3065 "generated/glsl/glsl_parser.cpp"
    break;

  case 73: /* relational_expression: relational_expression LE_OP shift_expression  */
#line 684 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_lequal, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3075 "generated/glsl/glsl_parser.cpp"
    break;

  case 74: /* relational_expression: relational_expression GE_OP shift_expression  */
#line 690 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_gequal, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3085 "generated/glsl/glsl_parser.cpp"
    break;

  case 76: /* equality_expression: equality_expression EQ_OP relational_expression  */
#line 700 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_equal, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3095 "generated/glsl/glsl_parser.cpp"
    break;

  case 77: /* equality_expression: equality_expression NE_OP relational_expression  */
#line 706 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_nequal, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3105 "generated/glsl/glsl_parser.cpp"
    break;

  case 79: /* and_expression: and_expression '&' equality_expression  */
#line 716 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_and, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3115 "generated/glsl/glsl_parser.cpp"
    break;

  case 81: /* exclusive_or_expression: exclusive_or_expression '^' and_expression  */
#line 726 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_xor, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3125 "generated/glsl/glsl_parser.cpp"
    break;

  case 83: /* inclusive_or_expression: inclusive_or_expression '|' exclusive_or_expression  */
#line 736 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_bit_or, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3135 "generated/glsl/glsl_parser.cpp"
    break;

  case 85: /* logical_and_expression: logical_and_expression AND_OP inclusive_or_expression  */
#line 746 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_and, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3145 "generated/glsl/glsl_parser.cpp"
    break;

  case 87: /* logical_xor_expression: logical_xor_expression XOR_OP logical_and_expression  */
#line 756 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_xor, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3155 "generated/glsl/glsl_parser.cpp"
    break;

  case 89: /* logical_or_expression: logical_or_expression OR_OP logical_xor_expression  */
#line 766 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression_bin(ast_logic_or, (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3165 "generated/glsl/glsl_parser.cpp"
    break;

  case 91: /* conditional_expression: logical_or_expression '?' expression ':' assignment_expression  */
#line 776 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression(ast_conditional, (yyvsp[-4].expression), (yyvsp[-2].expression), (yyvsp[0].expression));
      (yyval.expression)->set_location_range((yylsp[-4]), (yylsp[0]));
   }
#line 3175 "generated/glsl/glsl_parser.cpp"
    break;

  case 93: /* assignment_expression: unary_expression assignment_operator assignment_expression  */
#line 786 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_expression((yyvsp[-1].n), (yyvsp[-2].expression), (yyvsp[0].expression), NULL);
      (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 3185 "generated/glsl/glsl_parser.cpp"
    break;

  case 94: /* assignment_operator: '='  */
#line 794 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_assign; }
#line 3191 "generated/glsl/glsl_parser.cpp"
    break;

  case 95: /* assignment_operator: MUL_ASSIGN  */
#line 795 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_mul_assign; }
#line 3197 "generated/glsl/glsl_parser.cpp"
    break;

  case 96: /* assignment_operator: DIV_ASSIGN  */
#line 796 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_div_assign; }
#line 3203 "generated/glsl/glsl_parser.cpp"
    break;

  case 97: /* assignment_operator: MOD_ASSIGN  */
#line 797 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_mod_assign; }
#line 3209 "generated/glsl/glsl_parser.cpp"
    break;

  case 98: /* assignment_operator: ADD_ASSIGN  */
#line 798 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_add_assign; }
#line 3215 "generated/glsl/glsl_parser.cpp"
    break;

  case 99: /* assignment_operator: SUB_ASSIGN  */
#line 799 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_sub_assign; }
#line 3221 "generated/glsl/glsl_parser.cpp"
    break;

  case 100: /* assignment_operator: LEFT_ASSIGN  */
#line 800 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_ls_assign; }
#line 3227 "generated/glsl/glsl_parser.cpp"
    break;

  case 101: /* assignment_operator: RIGHT_ASSIGN  */
#line 801 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_rs_assign; }
#line 3233 "generated/glsl/glsl_parser.cpp"
    break;

  case 102: /* assignment_operator: AND_ASSIGN  */
#line 802 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_and_assign; }
#line 3239 "generated/glsl/glsl_parser.cpp"
    break;

  case 103: /* assignment_operator: XOR_ASSIGN  */
#line 803 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_xor_assign; }
#line 3245 "generated/glsl/glsl_parser.cpp"
    break;

  case 104: /* assignment_operator: OR_ASSIGN  */
#line 804 "mesa-imported/glsl/glsl_parser.yy"
                      { (yyval.n) = ast_or_assign; }
#line 3251 "generated/glsl/glsl_parser.cpp"
    break;

  case 105: /* expression: assignment_expression  */
#line 809 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[0].expression);
   }
#line 3259 "generated/glsl/glsl_parser.cpp"
    break;

  case 106: /* expression: expression ',' assignment_expression  */
#line 813 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      if ((yyvsp[-2].expression)->oper != ast_sequence) {
         (yyval.expression) = new(ctx) ast_expression(ast_sequence, NULL, NULL, NULL);
         (yyval.expression)->set_location_range((yylsp[-2]), (yylsp[0]));
         (yyval.expression)->expressions.push_tail(& (yyvsp[-2].expression)->link);
      } else {
         (yyval.expression) = (yyvsp[-2].expression);
      }

      (yyval.expression)->expressions.push_tail(& (yyvsp[0].expression)->link);
   }
#line 3276 "generated/glsl/glsl_parser.cpp"
    break;

  case 108: /* declaration: function_prototype ';'  */
#line 833 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->symbols->pop_scope();
      (yyval.node) = (yyvsp[-1].function);
   }
#line 3285 "generated/glsl/glsl_parser.cpp"
    break;

  case 109: /* declaration: init_declarator_list ';'  */
#line 838 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = (yyvsp[-1].declarator_list);
   }
#line 3293 "generated/glsl/glsl_parser.cpp"
    break;

  case 110: /* declaration: PRECISION precision_qualifier type_specifier ';'  */
#line 842 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyvsp[-1].type_specifier)->default_precision = (yyvsp[-2].n);
      (yyval.node) = (yyvsp[-1].type_specifier);
   }
#line 3302 "generated/glsl/glsl_parser.cpp"
    break;

  case 111: /* declaration: interface_block  */
#line 847 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_interface_block *block = (ast_interface_block *) (yyvsp[0].node);
      if (block->layout.has_layout() || block->layout.has_memory()) {
         if (!block->default_layout.merge_qualifier(& (yylsp[0]), state, block->layout, false)) {
            YYERROR;
         }
      }
      block->layout = block->default_layout;
      if (!block->layout.push_to_global(& (yylsp[0]), state)) {
         YYERROR;
      }
      (yyval.node) = (yyvsp[0].node);
   }
#line 3320 "generated/glsl/glsl_parser.cpp"
    break;

  case 115: /* function_header_with_parameters: function_header parameter_declaration  */
#line 873 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.function) = (yyvsp[-1].function);
      (yyval.function)->parameters.push_tail(& (yyvsp[0].parameter_declarator)->link);
   }
#line 3329 "generated/glsl/glsl_parser.cpp"
    break;

  case 116: /* function_header_with_parameters: function_header_with_parameters ',' parameter_declaration  */
#line 878 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.function) = (yyvsp[-2].function);
      (yyval.function)->parameters.push_tail(& (yyvsp[0].parameter_declarator)->link);
   }
#line 3338 "generated/glsl/glsl_parser.cpp"
    break;

  case 117: /* function_header: fully_specified_type variable_identifier '('  */
#line 886 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.function) = new(ctx) ast_function();
      (yyval.function)->set_location((yylsp[-1]));
      (yyval.function)->return_type = (yyvsp[-2].fully_specified_type);
      (yyval.function)->identifier = (yyvsp[-1].identifier);

      if ((yyvsp[-2].fully_specified_type)->qualifier.is_subroutine_decl()) {
         /* add type for IDENTIFIER search */
         state->symbols->add_type((yyvsp[-1].identifier), glsl_type::get_subroutine_instance((yyvsp[-1].identifier)));
      } else
         state->symbols->add_function(new(state) ir_function((yyvsp[-1].identifier)));
      state->symbols->push_scope();
   }
#line 3357 "generated/glsl/glsl_parser.cpp"
    break;

  case 118: /* parameter_declarator: type_specifier any_identifier  */
#line 904 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
      (yyval.parameter_declarator)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
      (yyval.parameter_declarator)->type->set_location((yylsp[-1]));
      (yyval.parameter_declarator)->type->specifier = (yyvsp[-1].type_specifier);
      (yyval.parameter_declarator)->identifier = (yyvsp[0].identifier);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[0].identifier), ir_var_auto));
   }
#line 3372 "generated/glsl/glsl_parser.cpp"
    break;

  case 119: /* parameter_declarator: layout_qualifier type_specifier any_identifier  */
#line 915 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (state->allow_layout_qualifier_on_function_parameter) {
         void *ctx = state->linalloc;
         (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
         (yyval.parameter_declarator)->set_location_range((yylsp[-1]), (yylsp[0]));
         (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
         (yyval.parameter_declarator)->type->set_location((yylsp[-1]));
         (yyval.parameter_declarator)->type->specifier = (yyvsp[-1].type_specifier);
         (yyval.parameter_declarator)->identifier = (yyvsp[0].identifier);
         state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[0].identifier), ir_var_auto));
      } else {
         _mesa_glsl_error(&(yylsp[-2]), state,
                          "is is not allowed on function parameter");
         YYERROR;
      }
   }
#line 3393 "generated/glsl/glsl_parser.cpp"
    break;

  case 120: /* parameter_declarator: type_specifier any_identifier array_specifier  */
#line 932 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
      (yyval.parameter_declarator)->set_location_range((yylsp[-2]), (yylsp[0]));
      (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
      (yyval.parameter_declarator)->type->set_location((yylsp[-2]));
      (yyval.parameter_declarator)->type->specifier = (yyvsp[-2].type_specifier);
      (yyval.parameter_declarator)->identifier = (yyvsp[-1].identifier);
      (yyval.parameter_declarator)->array_specifier = (yyvsp[0].array_specifier);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-1].identifier), ir_var_auto));
   }
#line 3409 "generated/glsl/glsl_parser.cpp"
    break;

  case 121: /* parameter_declaration: parameter_qualifier parameter_declarator  */
#line 947 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.parameter_declarator) = (yyvsp[0].parameter_declarator);
      (yyval.parameter_declarator)->type->qualifier = (yyvsp[-1].type_qualifier);
      if (!(yyval.parameter_declarator)->type->qualifier.push_to_global(& (yylsp[-1]), state)) {
         YYERROR;
      }
   }
#line 3421 "generated/glsl/glsl_parser.cpp"
    break;

  case 122: /* parameter_declaration: parameter_qualifier parameter_type_specifier  */
#line 955 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.parameter_declarator) = new(ctx) ast_parameter_declarator();
      (yyval.parameter_declarator)->set_location((yylsp[0]));
      (yyval.parameter_declarator)->type = new(ctx) ast_fully_specified_type();
      (yyval.parameter_declarator)->type->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.parameter_declarator)->type->qualifier = (yyvsp[-1].type_qualifier);
      if (!(yyval.parameter_declarator)->type->qualifier.push_to_global(& (yylsp[-1]), state)) {
         YYERROR;
      }
      (yyval.parameter_declarator)->type->specifier = (yyvsp[0].type_specifier);
   }
#line 3438 "generated/glsl/glsl_parser.cpp"
    break;

  case 123: /* parameter_qualifier: %empty  */
#line 971 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
   }
#line 3446 "generated/glsl/glsl_parser.cpp"
    break;

  case 124: /* parameter_qualifier: CONST_TOK parameter_qualifier  */
#line 975 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).flags.q.constant)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate const qualifier");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).flags.q.constant = 1;
   }
#line 3458 "generated/glsl/glsl_parser.cpp"
    break;

  case 125: /* parameter_qualifier: PRECISE parameter_qualifier  */
#line 983 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).flags.q.precise)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate precise qualifier");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).flags.q.precise = 1;
   }
#line 3470 "generated/glsl/glsl_parser.cpp"
    break;

  case 126: /* parameter_qualifier: parameter_direction_qualifier parameter_qualifier  */
#line 991 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (((yyvsp[-1].type_qualifier).flags.q.in || (yyvsp[-1].type_qualifier).flags.q.out) && ((yyvsp[0].type_qualifier).flags.q.in || (yyvsp[0].type_qualifier).flags.q.out))
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate in/out/inout qualifier");

      if (!state->has_420pack_or_es31() && (yyvsp[0].type_qualifier).flags.q.constant)
         _mesa_glsl_error(&(yylsp[-1]), state, "in/out/inout must come after const "
                                      "or precise");

      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 3486 "generated/glsl/glsl_parser.cpp"
    break;

  case 127: /* parameter_qualifier: precision_qualifier parameter_qualifier  */
#line 1003 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).precision != ast_precision_none)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate precision qualifier");

      if (!state->has_420pack_or_es31() &&
          (yyvsp[0].type_qualifier).flags.i != 0)
         _mesa_glsl_error(&(yylsp[-1]), state, "precision qualifiers must come last");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).precision = (yyvsp[-1].n);
   }
#line 3502 "generated/glsl/glsl_parser.cpp"
    break;

  case 128: /* parameter_qualifier: memory_qualifier parameter_qualifier  */
#line 1015 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 3511 "generated/glsl/glsl_parser.cpp"
    break;

  case 129: /* parameter_direction_qualifier: IN_TOK  */
#line 1022 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.in = 1;
   }
#line 3520 "generated/glsl/glsl_parser.cpp"
    break;

  case 130: /* parameter_direction_qualifier: OUT_TOK  */
#line 1027 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.out = 1;
   }
#line 3529 "generated/glsl/glsl_parser.cpp"
    break;

  case 131: /* parameter_direction_qualifier: INOUT_TOK  */
#line 1032 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.in = 1;
      (yyval.type_qualifier).flags.q.out = 1;
   }
#line 3539 "generated/glsl/glsl_parser.cpp"
    break;

  case 134: /* init_declarator_list: init_declarator_list ',' any_identifier  */
#line 1046 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[0].identifier), NULL, NULL);
      decl->set_location((yylsp[0]));

      (yyval.declarator_list) = (yyvsp[-2].declarator_list);
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[0].identifier), ir_var_auto));
   }
#line 3553 "generated/glsl/glsl_parser.cpp"
    break;

  case 135: /* init_declarator_list: init_declarator_list ',' any_identifier array_specifier  */
#line 1056 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-1].identifier), (yyvsp[0].array_specifier), NULL);
      decl->set_location_range((yylsp[-1]), (yylsp[0]));

      (yyval.declarator_list) = (yyvsp[-3].declarator_list);
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-1].identifier), ir_var_auto));
   }
#line 3567 "generated/glsl/glsl_parser.cpp"
    break;

  case 136: /* init_declarator_list: init_declarator_list ',' any_identifier array_specifier '=' initializer  */
#line 1066 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-3].identifier), (yyvsp[-2].array_specifier), (yyvsp[0].expression));
      decl->set_location_range((yylsp[-3]), (yylsp[-2]));

      (yyval.declarator_list) = (yyvsp[-5].declarator_list);
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-3].identifier), ir_var_auto));
   }
#line 3581 "generated/glsl/glsl_parser.cpp"
    break;

  case 137: /* init_declarator_list: init_declarator_list ',' any_identifier '=' initializer  */
#line 1076 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-2].identifier), NULL, (yyvsp[0].expression));
      decl->set_location((yylsp[-2]));

      (yyval.declarator_list) = (yyvsp[-4].declarator_list);
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-2].identifier), ir_var_auto));
   }
#line 3595 "generated/glsl/glsl_parser.cpp"
    break;

  case 138: /* single_declaration: fully_specified_type  */
#line 1090 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      /* Empty declaration list is valid. */
      (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[0].fully_specified_type));
      (yyval.declarator_list)->set_location((yylsp[0]));
   }
#line 3606 "generated/glsl/glsl_parser.cpp"
    break;

  case 139: /* single_declaration: fully_specified_type any_identifier  */
#line 1097 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[0].identifier), NULL, NULL);
      decl->set_location((yylsp[0]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[-1].fully_specified_type));
      (yyval.declarator_list)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[0].identifier), ir_var_auto));
   }
#line 3621 "generated/glsl/glsl_parser.cpp"
    break;

  case 140: /* single_declaration: fully_specified_type any_identifier array_specifier  */
#line 1108 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-1].identifier), (yyvsp[0].array_specifier), NULL);
      decl->set_location_range((yylsp[-1]), (yylsp[0]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[-2].fully_specified_type));
      (yyval.declarator_list)->set_location_range((yylsp[-2]), (yylsp[0]));
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-1].identifier), ir_var_auto));
   }
#line 3636 "generated/glsl/glsl_parser.cpp"
    break;

  case 141: /* single_declaration: fully_specified_type any_identifier array_specifier '=' initializer  */
#line 1119 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-3].identifier), (yyvsp[-2].array_specifier), (yyvsp[0].expression));
      decl->set_location_range((yylsp[-3]), (yylsp[-2]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[-4].fully_specified_type));
      (yyval.declarator_list)->set_location_range((yylsp[-4]), (yylsp[-2]));
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-3].identifier), ir_var_auto));
   }
#line 3651 "generated/glsl/glsl_parser.cpp"
    break;

  case 142: /* single_declaration: fully_specified_type any_identifier '=' initializer  */
#line 1130 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-2].identifier), NULL, (yyvsp[0].expression));
      decl->set_location((yylsp[-2]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list((yyvsp[-3].fully_specified_type));
      (yyval.declarator_list)->set_location_range((yylsp[-3]), (yylsp[-2]));
      (yyval.declarator_list)->declarations.push_tail(&decl->link);
      state->symbols->add_variable(new(state) ir_variable(NULL, (yyvsp[-2].identifier), ir_var_auto));
   }
#line 3666 "generated/glsl/glsl_parser.cpp"
    break;

  case 143: /* single_declaration: INVARIANT variable_identifier  */
#line 1141 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[0].identifier), NULL, NULL);
      decl->set_location((yylsp[0]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list(NULL);
      (yyval.declarator_list)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.declarator_list)->invariant = true;

      (yyval.declarator_list)->declarations.push_tail(&decl->link);
   }
#line 3682 "generated/glsl/glsl_parser.cpp"
    break;

  case 144: /* single_declaration: PRECISE variable_identifier  */
#line 1153 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[0].identifier), NULL, NULL);
      decl->set_location((yylsp[0]));

      (yyval.declarator_list) = new(ctx) ast_declarator_list(NULL);
      (yyval.declarator_list)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.declarator_list)->precise = true;

      (yyval.declarator_list)->declarations.push_tail(&decl->link);
   }
#line 3698 "generated/glsl/glsl_parser.cpp"
    break;

  case 145: /* fully_specified_type: type_specifier  */
#line 1168 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
      (yyval.fully_specified_type)->set_location((yylsp[0]));
      (yyval.fully_specified_type)->specifier = (yyvsp[0].type_specifier);
   }
#line 3709 "generated/glsl/glsl_parser.cpp"
    break;

  case 146: /* fully_specified_type: type_qualifier type_specifier  */
#line 1175 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.fully_specified_type) = new(ctx) ast_fully_specified_type();
      (yyval.fully_specified_type)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.fully_specified_type)->qualifier = (yyvsp[-1].type_qualifier);
      if (!(yyval.fully_specified_type)->qualifier.push_to_global(& (yylsp[-1]), state)) {
         YYERROR;
      }
      (yyval.fully_specified_type)->specifier = (yyvsp[0].type_specifier);
      if ((yyval.fully_specified_type)->specifier->structure != NULL &&
          (yyval.fully_specified_type)->specifier->structure->is_declaration) {
            (yyval.fully_specified_type)->specifier->structure->layout = &(yyval.fully_specified_type)->qualifier;
      }
   }
#line 3728 "generated/glsl/glsl_parser.cpp"
    break;

  case 147: /* layout_qualifier: LAYOUT_TOK '(' layout_qualifier_id_list ')'  */
#line 1193 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
   }
#line 3736 "generated/glsl/glsl_parser.cpp"
    break;

  case 149: /* layout_qualifier_id_list: layout_qualifier_id_list ',' layout_qualifier_id  */
#line 1201 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-2].type_qualifier);
      if (!(yyval.type_qualifier).merge_qualifier(& (yylsp[0]), state, (yyvsp[0].type_qualifier), true)) {
         YYERROR;
      }
   }
#line 3747 "generated/glsl/glsl_parser.cpp"
    break;

  case 150: /* layout_qualifier_id: any_identifier  */
#line 1211 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));

      /* Layout qualifiers for ARB_fragment_coord_conventions. */
      if (!(yyval.type_qualifier).flags.i && (state->ARB_fragment_coord_conventions_enable ||
                          state->is_version(150, 0))) {
         if (match_layout_qualifier((yyvsp[0].identifier), "origin_upper_left", state) == 0) {
            (yyval.type_qualifier).flags.q.origin_upper_left = 1;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "pixel_center_integer",
                                           state) == 0) {
            (yyval.type_qualifier).flags.q.pixel_center_integer = 1;
         }

         if ((yyval.type_qualifier).flags.i && state->ARB_fragment_coord_conventions_warn) {
            _mesa_glsl_warning(& (yylsp[0]), state,
                               "GL_ARB_fragment_coord_conventions layout "
                               "identifier `%s' used", (yyvsp[0].identifier));
         }
      }

      /* Layout qualifiers for AMD/ARB_conservative_depth. */
      if (!(yyval.type_qualifier).flags.i &&
          (state->AMD_conservative_depth_enable ||
           state->ARB_conservative_depth_enable ||
           state->is_version(420, 0))) {
         if (match_layout_qualifier((yyvsp[0].identifier), "depth_any", state) == 0) {
            (yyval.type_qualifier).flags.q.depth_type = 1;
            (yyval.type_qualifier).depth_type = ast_depth_any;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "depth_greater", state) == 0) {
            (yyval.type_qualifier).flags.q.depth_type = 1;
            (yyval.type_qualifier).depth_type = ast_depth_greater;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "depth_less", state) == 0) {
            (yyval.type_qualifier).flags.q.depth_type = 1;
            (yyval.type_qualifier).depth_type = ast_depth_less;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "depth_unchanged",
                                           state) == 0) {
            (yyval.type_qualifier).flags.q.depth_type = 1;
            (yyval.type_qualifier).depth_type = ast_depth_unchanged;
         }

         if ((yyval.type_qualifier).flags.i && state->AMD_conservative_depth_warn) {
            _mesa_glsl_warning(& (yylsp[0]), state,
                               "GL_AMD_conservative_depth "
                               "layout qualifier `%s' is used", (yyvsp[0].identifier));
         }
         if ((yyval.type_qualifier).flags.i && state->ARB_conservative_depth_warn) {
            _mesa_glsl_warning(& (yylsp[0]), state,
                               "GL_ARB_conservative_depth "
                               "layout qualifier `%s' is used", (yyvsp[0].identifier));
         }
      }

      /* See also interface_block_layout_qualifier. */
      if (!(yyval.type_qualifier).flags.i && state->has_uniform_buffer_objects()) {
         if (match_layout_qualifier((yyvsp[0].identifier), "std140", state) == 0) {
            (yyval.type_qualifier).flags.q.std140 = 1;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "shared", state) == 0) {
            (yyval.type_qualifier).flags.q.shared = 1;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "std430", state) == 0) {
            (yyval.type_qualifier).flags.q.std430 = 1;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "column_major", state) == 0) {
            (yyval.type_qualifier).flags.q.column_major = 1;
         /* "row_major" is a reserved word in GLSL 1.30+. Its token is parsed
          * below in the interface_block_layout_qualifier rule.
          *
          * It is not a reserved word in GLSL ES 3.00, so it's handled here as
          * an identifier.
          *
          * Also, this takes care of alternate capitalizations of
          * "row_major" (which is necessary because layout qualifiers
          * are case-insensitive in desktop GLSL).
          */
         } else if (match_layout_qualifier((yyvsp[0].identifier), "row_major", state) == 0) {
            (yyval.type_qualifier).flags.q.row_major = 1;
         /* "packed" is a reserved word in GLSL, and its token is
          * parsed below in the interface_block_layout_qualifier rule.
          * However, we must take care of alternate capitalizations of
          * "packed", because layout qualifiers are case-insensitive
          * in desktop GLSL.
          */
         } else if (match_layout_qualifier((yyvsp[0].identifier), "packed", state) == 0) {
           (yyval.type_qualifier).flags.q.packed = 1;
         }

         if ((yyval.type_qualifier).flags.i && state->ARB_uniform_buffer_object_warn) {
            _mesa_glsl_warning(& (yylsp[0]), state,
                               "#version 140 / GL_ARB_uniform_buffer_object "
                               "layout qualifier `%s' is used", (yyvsp[0].identifier));
         }
      }

      /* Layout qualifiers for GLSL 1.50 geometry shaders. */
      if (!(yyval.type_qualifier).flags.i) {
         static const struct {
            const char *s;
            GLenum e;
         } map[] = {
                 { "points", GL_POINTS },
                 { "lines", GL_LINES },
                 { "lines_adjacency", GL_LINES_ADJACENCY },
                 { "line_strip", GL_LINE_STRIP },
                 { "triangles", GL_TRIANGLES },
                 { "triangles_adjacency", GL_TRIANGLES_ADJACENCY },
                 { "triangle_strip", GL_TRIANGLE_STRIP },
         };
         for (unsigned i = 0; i < ARRAY_SIZE(map); i++) {
            if (match_layout_qualifier((yyvsp[0].identifier), map[i].s, state) == 0) {
               (yyval.type_qualifier).flags.q.prim_type = 1;
               (yyval.type_qualifier).prim_type = map[i].e;
               break;
            }
         }

         if ((yyval.type_qualifier).flags.i && !state->has_geometry_shader() &&
             !state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state, "#version 150 layout "
                             "qualifier `%s' used", (yyvsp[0].identifier));
         }
      }

      /* Layout qualifiers for ARB_shader_image_load_store. */
      if (state->has_shader_image_load_store()) {
         if (!(yyval.type_qualifier).flags.i) {
            static const struct {
               const char *name;
               GLenum format;
               glsl_base_type base_type;
               /** Minimum desktop GLSL version required for the image
                * format.  Use 130 if already present in the original
                * ARB extension.
                */
               unsigned required_glsl;
               /** Minimum GLSL ES version required for the image format. */
               unsigned required_essl;
               /* NV_image_formats */
               bool nv_image_formats;
            } map[] = {
               { "rgba32f", GL_RGBA32F, GLSL_TYPE_FLOAT, 130, 310, false },
               { "rgba16f", GL_RGBA16F, GLSL_TYPE_FLOAT, 130, 310, false },
               { "rg32f", GL_RG32F, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rg16f", GL_RG16F, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r11f_g11f_b10f", GL_R11F_G11F_B10F, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r32f", GL_R32F, GLSL_TYPE_FLOAT, 130, 310, false },
               { "r16f", GL_R16F, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rgba32ui", GL_RGBA32UI, GLSL_TYPE_UINT, 130, 310, false },
               { "rgba16ui", GL_RGBA16UI, GLSL_TYPE_UINT, 130, 310, false },
               { "rgb10_a2ui", GL_RGB10_A2UI, GLSL_TYPE_UINT, 130, 0, true },
               { "rgba8ui", GL_RGBA8UI, GLSL_TYPE_UINT, 130, 310, false },
               { "rg32ui", GL_RG32UI, GLSL_TYPE_UINT, 130, 0, true },
               { "rg16ui", GL_RG16UI, GLSL_TYPE_UINT, 130, 0, true },
               { "rg8ui", GL_RG8UI, GLSL_TYPE_UINT, 130, 0, true },
               { "r32ui", GL_R32UI, GLSL_TYPE_UINT, 130, 310, false },
               { "r16ui", GL_R16UI, GLSL_TYPE_UINT, 130, 0, true },
               { "r8ui", GL_R8UI, GLSL_TYPE_UINT, 130, 0, true },
               { "rgba32i", GL_RGBA32I, GLSL_TYPE_INT, 130, 310, false },
               { "rgba16i", GL_RGBA16I, GLSL_TYPE_INT, 130, 310, false },
               { "rgba8i", GL_RGBA8I, GLSL_TYPE_INT, 130, 310, false },
               { "rg32i", GL_RG32I, GLSL_TYPE_INT, 130, 0, true },
               { "rg16i", GL_RG16I, GLSL_TYPE_INT, 130, 0, true },
               { "rg8i", GL_RG8I, GLSL_TYPE_INT, 130, 0, true },
               { "r32i", GL_R32I, GLSL_TYPE_INT, 130, 310, false },
               { "r16i", GL_R16I, GLSL_TYPE_INT, 130, 0, true },
               { "r8i", GL_R8I, GLSL_TYPE_INT, 130, 0, true },
               { "rgba16", GL_RGBA16, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rgb10_a2", GL_RGB10_A2, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rgba8", GL_RGBA8, GLSL_TYPE_FLOAT, 130, 310, false },
               { "rg16", GL_RG16, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rg8", GL_RG8, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r16", GL_R16, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r8", GL_R8, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rgba16_snorm", GL_RGBA16_SNORM, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rgba8_snorm", GL_RGBA8_SNORM, GLSL_TYPE_FLOAT, 130, 310, false },
               { "rg16_snorm", GL_RG16_SNORM, GLSL_TYPE_FLOAT, 130, 0, true },
               { "rg8_snorm", GL_RG8_SNORM, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r16_snorm", GL_R16_SNORM, GLSL_TYPE_FLOAT, 130, 0, true },
               { "r8_snorm", GL_R8_SNORM, GLSL_TYPE_FLOAT, 130, 0, true }
            };

            for (unsigned i = 0; i < ARRAY_SIZE(map); i++) {
               if ((state->is_version(map[i].required_glsl,
                                      map[i].required_essl) ||
                    (state->NV_image_formats_enable &&
                     map[i].nv_image_formats)) &&
                   match_layout_qualifier((yyvsp[0].identifier), map[i].name, state) == 0) {
                  (yyval.type_qualifier).flags.q.explicit_image_format = 1;
                  (yyval.type_qualifier).image_format = map[i].format;
                  (yyval.type_qualifier).image_base_type = map[i].base_type;
                  break;
               }
            }
         }
      }

      if (!(yyval.type_qualifier).flags.i) {
         if (match_layout_qualifier((yyvsp[0].identifier), "early_fragment_tests", state) == 0) {
            /* From section 4.4.1.3 of the GLSL 4.50 specification
             * (Fragment Shader Inputs):
             *
             *  "Fragment shaders also allow the following layout
             *   qualifier on in only (not with variable declarations)
             *     layout-qualifier-id
             *        early_fragment_tests
             *   [...]"
             */
            if (state->stage != MESA_SHADER_FRAGMENT) {
               _mesa_glsl_error(& (yylsp[0]), state,
                                "early_fragment_tests layout qualifier only "
                                "valid in fragment shaders");
            }

            (yyval.type_qualifier).flags.q.early_fragment_tests = 1;
         }

         if (match_layout_qualifier((yyvsp[0].identifier), "inner_coverage", state) == 0) {
            if (state->stage != MESA_SHADER_FRAGMENT) {
               _mesa_glsl_error(& (yylsp[0]), state,
                                "inner_coverage layout qualifier only "
                                "valid in fragment shaders");
            }

	    if (state->INTEL_conservative_rasterization_enable) {
	       (yyval.type_qualifier).flags.q.inner_coverage = 1;
	    } else {
	       _mesa_glsl_error(& (yylsp[0]), state,
                                "inner_coverage layout qualifier present, "
                                "but the INTEL_conservative_rasterization extension "
                                "is not enabled.");
            }
         }

         if (match_layout_qualifier((yyvsp[0].identifier), "post_depth_coverage", state) == 0) {
            if (state->stage != MESA_SHADER_FRAGMENT) {
               _mesa_glsl_error(& (yylsp[0]), state,
                                "post_depth_coverage layout qualifier only "
                                "valid in fragment shaders");
            }

            if (state->ARB_post_depth_coverage_enable ||
		state->INTEL_conservative_rasterization_enable) {
               (yyval.type_qualifier).flags.q.post_depth_coverage = 1;
            } else {
               _mesa_glsl_error(& (yylsp[0]), state,
                                "post_depth_coverage layout qualifier present, "
                                "but the GL_ARB_post_depth_coverage extension "
                                "is not enabled.");
            }
         }

         if ((yyval.type_qualifier).flags.q.post_depth_coverage && (yyval.type_qualifier).flags.q.inner_coverage) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "post_depth_coverage & inner_coverage layout qualifiers "
                             "are mutually exclusive");
         }
      }

      const bool pixel_interlock_ordered = match_layout_qualifier((yyvsp[0].identifier),
         "pixel_interlock_ordered", state) == 0;
      const bool pixel_interlock_unordered = match_layout_qualifier((yyvsp[0].identifier),
         "pixel_interlock_unordered", state) == 0;
      const bool sample_interlock_ordered = match_layout_qualifier((yyvsp[0].identifier),
         "sample_interlock_ordered", state) == 0;
      const bool sample_interlock_unordered = match_layout_qualifier((yyvsp[0].identifier),
         "sample_interlock_unordered", state) == 0;

      if (pixel_interlock_ordered + pixel_interlock_unordered +
          sample_interlock_ordered + sample_interlock_unordered > 0 &&
          state->stage != MESA_SHADER_FRAGMENT) {
         _mesa_glsl_error(& (yylsp[0]), state, "interlock layout qualifiers: "
                          "pixel_interlock_ordered, pixel_interlock_unordered, "
                          "sample_interlock_ordered and sample_interlock_unordered, "
                          "only valid in fragment shader input layout declaration.");
      } else if (pixel_interlock_ordered + pixel_interlock_unordered +
                 sample_interlock_ordered + sample_interlock_unordered > 0 &&
                 !state->ARB_fragment_shader_interlock_enable &&
                 !state->NV_fragment_shader_interlock_enable) {
         _mesa_glsl_error(& (yylsp[0]), state,
                          "interlock layout qualifier present, but the "
                          "GL_ARB_fragment_shader_interlock or "
                          "GL_NV_fragment_shader_interlock extension is not "
                          "enabled.");
      } else {
         (yyval.type_qualifier).flags.q.pixel_interlock_ordered = pixel_interlock_ordered;
         (yyval.type_qualifier).flags.q.pixel_interlock_unordered = pixel_interlock_unordered;
         (yyval.type_qualifier).flags.q.sample_interlock_ordered = sample_interlock_ordered;
         (yyval.type_qualifier).flags.q.sample_interlock_unordered = sample_interlock_unordered;
      }

      /* Layout qualifiers for tessellation evaluation shaders. */
      if (!(yyval.type_qualifier).flags.i) {
         static const struct {
            const char *s;
            GLenum e;
         } map[] = {
                 /* triangles already parsed by gs-specific code */
                 { "quads", GL_QUADS },
                 { "isolines", GL_ISOLINES },
         };
         for (unsigned i = 0; i < ARRAY_SIZE(map); i++) {
            if (match_layout_qualifier((yyvsp[0].identifier), map[i].s, state) == 0) {
               (yyval.type_qualifier).flags.q.prim_type = 1;
               (yyval.type_qualifier).prim_type = map[i].e;
               break;
            }
         }

         if ((yyval.type_qualifier).flags.i && !state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "primitive mode qualifier `%s' requires "
                             "GLSL 4.00 or ARB_tessellation_shader", (yyvsp[0].identifier));
         }
      }
      if (!(yyval.type_qualifier).flags.i) {
         static const struct {
            const char *s;
            enum gl_tess_spacing e;
         } map[] = {
                 { "equal_spacing", TESS_SPACING_EQUAL },
                 { "fractional_odd_spacing", TESS_SPACING_FRACTIONAL_ODD },
                 { "fractional_even_spacing", TESS_SPACING_FRACTIONAL_EVEN },
         };
         for (unsigned i = 0; i < ARRAY_SIZE(map); i++) {
            if (match_layout_qualifier((yyvsp[0].identifier), map[i].s, state) == 0) {
               (yyval.type_qualifier).flags.q.vertex_spacing = 1;
               (yyval.type_qualifier).vertex_spacing = map[i].e;
               break;
            }
         }

         if ((yyval.type_qualifier).flags.i && !state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "vertex spacing qualifier `%s' requires "
                             "GLSL 4.00 or ARB_tessellation_shader", (yyvsp[0].identifier));
         }
      }
      if (!(yyval.type_qualifier).flags.i) {
         if (match_layout_qualifier((yyvsp[0].identifier), "cw", state) == 0) {
            (yyval.type_qualifier).flags.q.ordering = 1;
            (yyval.type_qualifier).ordering = GL_CW;
         } else if (match_layout_qualifier((yyvsp[0].identifier), "ccw", state) == 0) {
            (yyval.type_qualifier).flags.q.ordering = 1;
            (yyval.type_qualifier).ordering = GL_CCW;
         }

         if ((yyval.type_qualifier).flags.i && !state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "ordering qualifier `%s' requires "
                             "GLSL 4.00 or ARB_tessellation_shader", (yyvsp[0].identifier));
         }
      }
      if (!(yyval.type_qualifier).flags.i) {
         if (match_layout_qualifier((yyvsp[0].identifier), "point_mode", state) == 0) {
            (yyval.type_qualifier).flags.q.point_mode = 1;
            (yyval.type_qualifier).point_mode = true;
         }

         if ((yyval.type_qualifier).flags.i && !state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "qualifier `point_mode' requires "
                             "GLSL 4.00 or ARB_tessellation_shader");
         }
      }

      if (!(yyval.type_qualifier).flags.i) {
         static const struct {
            const char *s;
            uint32_t mask;
         } map[] = {
                 { "blend_support_multiply",       BLEND_MULTIPLY },
                 { "blend_support_screen",         BLEND_SCREEN },
                 { "blend_support_overlay",        BLEND_OVERLAY },
                 { "blend_support_darken",         BLEND_DARKEN },
                 { "blend_support_lighten",        BLEND_LIGHTEN },
                 { "blend_support_colordodge",     BLEND_COLORDODGE },
                 { "blend_support_colorburn",      BLEND_COLORBURN },
                 { "blend_support_hardlight",      BLEND_HARDLIGHT },
                 { "blend_support_softlight",      BLEND_SOFTLIGHT },
                 { "blend_support_difference",     BLEND_DIFFERENCE },
                 { "blend_support_exclusion",      BLEND_EXCLUSION },
                 { "blend_support_hsl_hue",        BLEND_HSL_HUE },
                 { "blend_support_hsl_saturation", BLEND_HSL_SATURATION },
                 { "blend_support_hsl_color",      BLEND_HSL_COLOR },
                 { "blend_support_hsl_luminosity", BLEND_HSL_LUMINOSITY },
                 { "blend_support_all_equations",  BLEND_ALL },
         };
         for (unsigned i = 0; i < ARRAY_SIZE(map); i++) {
            if (match_layout_qualifier((yyvsp[0].identifier), map[i].s, state) == 0) {
               (yyval.type_qualifier).flags.q.blend_support = 1;
               state->fs_blend_support |= map[i].mask;
               break;
            }
         }

         if ((yyval.type_qualifier).flags.i &&
             !state->KHR_blend_equation_advanced_enable &&
             !state->is_version(0, 320)) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "advanced blending layout qualifiers require "
                             "ESSL 3.20 or KHR_blend_equation_advanced");
         }

         if ((yyval.type_qualifier).flags.i && state->stage != MESA_SHADER_FRAGMENT) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "advanced blending layout qualifiers only "
                             "valid in fragment shaders");
         }
      }

      /* Layout qualifiers for ARB_compute_variable_group_size. */
      if (!(yyval.type_qualifier).flags.i) {
         if (match_layout_qualifier((yyvsp[0].identifier), "local_size_variable", state) == 0) {
            (yyval.type_qualifier).flags.q.local_size_variable = 1;
         }

         if ((yyval.type_qualifier).flags.i && !state->ARB_compute_variable_group_size_enable) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "qualifier `local_size_variable` requires "
                             "ARB_compute_variable_group_size");
         }
      }

      /* Layout qualifiers for ARB_bindless_texture. */
      if (!(yyval.type_qualifier).flags.i) {
         if (match_layout_qualifier((yyvsp[0].identifier), "bindless_sampler", state) == 0)
            (yyval.type_qualifier).flags.q.bindless_sampler = 1;
         if (match_layout_qualifier((yyvsp[0].identifier), "bound_sampler", state) == 0)
            (yyval.type_qualifier).flags.q.bound_sampler = 1;

         if (state->has_shader_image_load_store()) {
            if (match_layout_qualifier((yyvsp[0].identifier), "bindless_image", state) == 0)
               (yyval.type_qualifier).flags.q.bindless_image = 1;
            if (match_layout_qualifier((yyvsp[0].identifier), "bound_image", state) == 0)
               (yyval.type_qualifier).flags.q.bound_image = 1;
         }

         if ((yyval.type_qualifier).flags.i && !state->has_bindless()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "qualifier `%s` requires "
                             "ARB_bindless_texture", (yyvsp[0].identifier));
         }
      }

      if (!(yyval.type_qualifier).flags.i &&
          state->EXT_shader_framebuffer_fetch_non_coherent_enable) {
         if (match_layout_qualifier((yyvsp[0].identifier), "noncoherent", state) == 0)
            (yyval.type_qualifier).flags.q.non_coherent = 1;
      }

      /* Layout qualifier for NV_viewport_array2. */
      if (!(yyval.type_qualifier).flags.i && state->stage != MESA_SHADER_FRAGMENT) {
         if (match_layout_qualifier((yyvsp[0].identifier), "viewport_relative", state) == 0) {
            (yyval.type_qualifier).flags.q.viewport_relative = 1;
         }

         if ((yyval.type_qualifier).flags.i && !state->NV_viewport_array2_enable) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "qualifier `%s' requires "
                             "GL_NV_viewport_array2", (yyvsp[0].identifier));
         }

         if ((yyval.type_qualifier).flags.i && state->NV_viewport_array2_warn) {
            _mesa_glsl_warning(& (yylsp[0]), state,
                               "GL_NV_viewport_array2 layout "
                               "identifier `%s' used", (yyvsp[0].identifier));
         }
      }

      if (!(yyval.type_qualifier).flags.i) {
         _mesa_glsl_error(& (yylsp[0]), state, "unrecognized layout identifier "
                          "`%s'", (yyvsp[0].identifier));
         YYERROR;
      }
   }
#line 4224 "generated/glsl/glsl_parser.cpp"
    break;

  case 151: /* layout_qualifier_id: any_identifier '=' constant_expression  */
#line 1684 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      void *ctx = state->linalloc;

      if ((yyvsp[0].expression)->oper != ast_int_constant &&
          (yyvsp[0].expression)->oper != ast_uint_constant &&
          !state->has_enhanced_layouts()) {
         _mesa_glsl_error(& (yylsp[-2]), state,
                          "compile-time constant expressions require "
                          "GLSL 4.40 or ARB_enhanced_layouts");
      }

      if (match_layout_qualifier("align", (yyvsp[-2].identifier), state) == 0) {
         if (!state->has_enhanced_layouts()) {
            _mesa_glsl_error(& (yylsp[-2]), state,
                             "align qualifier requires "
                             "GLSL 4.40 or ARB_enhanced_layouts");
         } else {
            (yyval.type_qualifier).flags.q.explicit_align = 1;
            (yyval.type_qualifier).align = (yyvsp[0].expression);
         }
      }

      if (match_layout_qualifier("location", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.explicit_location = 1;

         if ((yyval.type_qualifier).flags.q.attribute == 1 &&
             state->ARB_explicit_attrib_location_warn) {
            _mesa_glsl_warning(& (yylsp[-2]), state,
                               "GL_ARB_explicit_attrib_location layout "
                               "identifier `%s' used", (yyvsp[-2].identifier));
         }
         (yyval.type_qualifier).location = (yyvsp[0].expression);
      }

      if (match_layout_qualifier("component", (yyvsp[-2].identifier), state) == 0) {
         if (!state->has_enhanced_layouts()) {
            _mesa_glsl_error(& (yylsp[-2]), state,
                             "component qualifier requires "
                             "GLSL 4.40 or ARB_enhanced_layouts");
         } else {
            (yyval.type_qualifier).flags.q.explicit_component = 1;
            (yyval.type_qualifier).component = (yyvsp[0].expression);
         }
      }

      if (match_layout_qualifier("index", (yyvsp[-2].identifier), state) == 0) {
         if (state->es_shader && !state->EXT_blend_func_extended_enable) {
            _mesa_glsl_error(& (yylsp[0]), state, "index layout qualifier requires EXT_blend_func_extended");
            YYERROR;
         }

         (yyval.type_qualifier).flags.q.explicit_index = 1;
         (yyval.type_qualifier).index = (yyvsp[0].expression);
      }

      if ((state->has_420pack_or_es31() ||
           state->has_atomic_counters() ||
           state->has_shader_storage_buffer_objects()) &&
          match_layout_qualifier("binding", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.explicit_binding = 1;
         (yyval.type_qualifier).binding = (yyvsp[0].expression);
      }

      if ((state->has_atomic_counters() ||
           state->has_enhanced_layouts()) &&
          match_layout_qualifier("offset", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.explicit_offset = 1;
         (yyval.type_qualifier).offset = (yyvsp[0].expression);
      }

      if (match_layout_qualifier("max_vertices", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.max_vertices = 1;
         (yyval.type_qualifier).max_vertices = new(ctx) ast_layout_expression((yylsp[-2]), (yyvsp[0].expression));
         if (!state->has_geometry_shader()) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "#version 150 max_vertices qualifier "
                             "specified", (yyvsp[0].expression));
         }
      }

      if (state->stage == MESA_SHADER_GEOMETRY) {
         if (match_layout_qualifier("stream", (yyvsp[-2].identifier), state) == 0 &&
             state->check_explicit_attrib_stream_allowed(& (yylsp[0]))) {
            (yyval.type_qualifier).flags.q.stream = 1;
            (yyval.type_qualifier).flags.q.explicit_stream = 1;
            (yyval.type_qualifier).stream = (yyvsp[0].expression);
         }
      }

      if (state->has_enhanced_layouts()) {
         if (match_layout_qualifier("xfb_buffer", (yyvsp[-2].identifier), state) == 0) {
            (yyval.type_qualifier).flags.q.xfb_buffer = 1;
            (yyval.type_qualifier).flags.q.explicit_xfb_buffer = 1;
            (yyval.type_qualifier).xfb_buffer = (yyvsp[0].expression);
         }

         if (match_layout_qualifier("xfb_offset", (yyvsp[-2].identifier), state) == 0) {
            (yyval.type_qualifier).flags.q.explicit_xfb_offset = 1;
            (yyval.type_qualifier).offset = (yyvsp[0].expression);
         }

         if (match_layout_qualifier("xfb_stride", (yyvsp[-2].identifier), state) == 0) {
            (yyval.type_qualifier).flags.q.xfb_stride = 1;
            (yyval.type_qualifier).flags.q.explicit_xfb_stride = 1;
            (yyval.type_qualifier).xfb_stride = (yyvsp[0].expression);
         }
      }

      static const char * const local_size_qualifiers[3] = {
         "local_size_x",
         "local_size_y",
         "local_size_z",
      };
      for (int i = 0; i < 3; i++) {
         if (match_layout_qualifier(local_size_qualifiers[i], (yyvsp[-2].identifier),
                                    state) == 0) {
            if (!state->has_compute_shader()) {
               _mesa_glsl_error(& (yylsp[0]), state,
                                "%s qualifier requires GLSL 4.30 or "
                                "GLSL ES 3.10 or ARB_compute_shader",
                                local_size_qualifiers[i]);
               YYERROR;
            } else {
               (yyval.type_qualifier).flags.q.local_size |= (1 << i);
               (yyval.type_qualifier).local_size[i] = new(ctx) ast_layout_expression((yylsp[-2]), (yyvsp[0].expression));
            }
            break;
         }
      }

      if (match_layout_qualifier("invocations", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.invocations = 1;
         (yyval.type_qualifier).invocations = new(ctx) ast_layout_expression((yylsp[-2]), (yyvsp[0].expression));
         if (!state->is_version(400, 320) &&
             !state->ARB_gpu_shader5_enable &&
             !state->OES_geometry_shader_enable &&
             !state->EXT_geometry_shader_enable) {
            _mesa_glsl_error(& (yylsp[0]), state,
                             "GL_ARB_gpu_shader5 invocations "
                             "qualifier specified", (yyvsp[0].expression));
         }
      }

      /* Layout qualifiers for tessellation control shaders. */
      if (match_layout_qualifier("vertices", (yyvsp[-2].identifier), state) == 0) {
         (yyval.type_qualifier).flags.q.vertices = 1;
         (yyval.type_qualifier).vertices = new(ctx) ast_layout_expression((yylsp[-2]), (yyvsp[0].expression));
         if (!state->has_tessellation_shader()) {
            _mesa_glsl_error(& (yylsp[-2]), state,
                             "vertices qualifier requires GLSL 4.00 or "
                             "ARB_tessellation_shader");
         }
      }

      /* If the identifier didn't match any known layout identifiers,
       * emit an error.
       */
      if (!(yyval.type_qualifier).flags.i) {
         _mesa_glsl_error(& (yylsp[-2]), state, "unrecognized layout identifier "
                          "`%s'", (yyvsp[-2].identifier));
         YYERROR;
      }
   }
#line 4393 "generated/glsl/glsl_parser.cpp"
    break;

  case 152: /* layout_qualifier_id: interface_block_layout_qualifier  */
#line 1849 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      /* Layout qualifiers for ARB_uniform_buffer_object. */
      if ((yyval.type_qualifier).flags.q.uniform && !state->has_uniform_buffer_objects()) {
         _mesa_glsl_error(& (yylsp[0]), state,
                          "#version 140 / GL_ARB_uniform_buffer_object "
                          "layout qualifier `%s' is used", (yyvsp[0].type_qualifier));
      } else if ((yyval.type_qualifier).flags.q.uniform && state->ARB_uniform_buffer_object_warn) {
         _mesa_glsl_warning(& (yylsp[0]), state,
                            "#version 140 / GL_ARB_uniform_buffer_object "
                            "layout qualifier `%s' is used", (yyvsp[0].type_qualifier));
      }
   }
#line 4411 "generated/glsl/glsl_parser.cpp"
    break;

  case 153: /* interface_block_layout_qualifier: ROW_MAJOR  */
#line 1875 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.row_major = 1;
   }
#line 4420 "generated/glsl/glsl_parser.cpp"
    break;

  case 154: /* interface_block_layout_qualifier: PACKED_TOK  */
#line 1880 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.packed = 1;
   }
#line 4429 "generated/glsl/glsl_parser.cpp"
    break;

  case 155: /* interface_block_layout_qualifier: SHARED  */
#line 1885 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.shared = 1;
   }
#line 4438 "generated/glsl/glsl_parser.cpp"
    break;

  case 156: /* subroutine_qualifier: SUBROUTINE  */
#line 1893 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.subroutine = 1;
   }
#line 4447 "generated/glsl/glsl_parser.cpp"
    break;

  case 157: /* subroutine_qualifier: SUBROUTINE '(' subroutine_type_list ')'  */
#line 1898 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.subroutine = 1;
      (yyval.type_qualifier).subroutine_list = (yyvsp[-1].subroutine_list);
   }
#line 4457 "generated/glsl/glsl_parser.cpp"
    break;

  case 158: /* subroutine_type_list: any_identifier  */
#line 1907 "mesa-imported/glsl/glsl_parser.yy"
   {
        void *ctx = state->linalloc;
        ast_declaration *decl = new(ctx)  ast_declaration((yyvsp[0].identifier), NULL, NULL);
        decl->set_location((yylsp[0]));

        (yyval.subroutine_list) = new(ctx) ast_subroutine_list();
        (yyval.subroutine_list)->declarations.push_tail(&decl->link);
   }
#line 4470 "generated/glsl/glsl_parser.cpp"
    break;

  case 159: /* subroutine_type_list: subroutine_type_list ',' any_identifier  */
#line 1916 "mesa-imported/glsl/glsl_parser.yy"
   {
        void *ctx = state->linalloc;
        ast_declaration *decl = new(ctx)  ast_declaration((yyvsp[0].identifier), NULL, NULL);
        decl->set_location((yylsp[0]));

        (yyval.subroutine_list) = (yyvsp[-2].subroutine_list);
        (yyval.subroutine_list)->declarations.push_tail(&decl->link);
   }
#line 4483 "generated/glsl/glsl_parser.cpp"
    break;

  case 160: /* interpolation_qualifier: SMOOTH  */
#line 1928 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.smooth = 1;
   }
#line 4492 "generated/glsl/glsl_parser.cpp"
    break;

  case 161: /* interpolation_qualifier: FLAT  */
#line 1933 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.flat = 1;
   }
#line 4501 "generated/glsl/glsl_parser.cpp"
    break;

  case 162: /* interpolation_qualifier: NOPERSPECTIVE  */
#line 1938 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.noperspective = 1;
   }
#line 4510 "generated/glsl/glsl_parser.cpp"
    break;

  case 163: /* type_qualifier: INVARIANT  */
#line 1947 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.invariant = 1;
   }
#line 4519 "generated/glsl/glsl_parser.cpp"
    break;

  case 164: /* type_qualifier: PRECISE  */
#line 1952 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.precise = 1;
   }
#line 4528 "generated/glsl/glsl_parser.cpp"
    break;

  case 171: /* type_qualifier: precision_qualifier  */
#line 1963 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(&(yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).precision = (yyvsp[0].n);
   }
#line 4537 "generated/glsl/glsl_parser.cpp"
    break;

  case 172: /* type_qualifier: PRECISE type_qualifier  */
#line 1981 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).flags.q.precise)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate \"precise\" qualifier");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).flags.q.precise = 1;
   }
#line 4549 "generated/glsl/glsl_parser.cpp"
    break;

  case 173: /* type_qualifier: INVARIANT type_qualifier  */
#line 1989 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).flags.q.invariant)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate \"invariant\" qualifier");

      if (!state->has_420pack_or_es31() && (yyvsp[0].type_qualifier).flags.q.precise)
         _mesa_glsl_error(&(yylsp[-1]), state,
                          "\"invariant\" must come after \"precise\"");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).flags.q.invariant = 1;

      /* GLSL ES 3.00 spec, section 4.6.1 "The Invariant Qualifier":
       *
       * "Only variables output from a shader can be candidates for invariance.
       * This includes user-defined output variables and the built-in output
       * variables. As only outputs can be declared as invariant, an invariant
       * output from one shader stage will still match an input of a subsequent
       * stage without the input being declared as invariant."
       *
       * On the desktop side, this text first appears in GLSL 4.30.
       */
      if (state->is_version(430, 300) && (yyval.type_qualifier).flags.q.in)
         _mesa_glsl_error(&(yylsp[-1]), state, "invariant qualifiers cannot be used with shader inputs");
   }
#line 4578 "generated/glsl/glsl_parser.cpp"
    break;

  case 174: /* type_qualifier: interpolation_qualifier type_qualifier  */
#line 2014 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* Section 4.3 of the GLSL 1.40 specification states:
       * "...qualified with one of these interpolation qualifiers"
       *
       * GLSL 1.30 claims to allow "one or more", but insists that:
       * "These interpolation qualifiers may only precede the qualifiers in,
       *  centroid in, out, or centroid out in a declaration."
       *
       * ...which means that e.g. smooth can't precede smooth, so there can be
       * only one after all, and the 1.40 text is a clarification, not a change.
       */
      if ((yyvsp[0].type_qualifier).has_interpolation())
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate interpolation qualifier");

      if (!state->has_420pack_or_es31() &&
          ((yyvsp[0].type_qualifier).flags.q.precise || (yyvsp[0].type_qualifier).flags.q.invariant)) {
         _mesa_glsl_error(&(yylsp[-1]), state, "interpolation qualifiers must come "
                          "after \"precise\" or \"invariant\"");
      }

      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 4606 "generated/glsl/glsl_parser.cpp"
    break;

  case 175: /* type_qualifier: layout_qualifier type_qualifier  */
#line 2038 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* In the absence of ARB_shading_language_420pack, layout qualifiers may
       * appear no later than auxiliary storage qualifiers. There is no
       * particularly clear spec language mandating this, but in all examples
       * the layout qualifier precedes the storage qualifier.
       *
       * We allow combinations of layout with interpolation, invariant or
       * precise qualifiers since these are useful in ARB_separate_shader_objects.
       * There is no clear spec guidance on this either.
       */
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(& (yylsp[-1]), state, (yyvsp[0].type_qualifier), false, (yyvsp[0].type_qualifier).has_layout());
   }
#line 4624 "generated/glsl/glsl_parser.cpp"
    break;

  case 176: /* type_qualifier: subroutine_qualifier type_qualifier  */
#line 2052 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 4633 "generated/glsl/glsl_parser.cpp"
    break;

  case 177: /* type_qualifier: auxiliary_storage_qualifier type_qualifier  */
#line 2057 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).has_auxiliary_storage()) {
         _mesa_glsl_error(&(yylsp[-1]), state,
                          "duplicate auxiliary storage qualifier (centroid or sample)");
      }

      if (!state->has_420pack_or_es31() &&
          ((yyvsp[0].type_qualifier).flags.q.precise || (yyvsp[0].type_qualifier).flags.q.invariant ||
           (yyvsp[0].type_qualifier).has_interpolation() || (yyvsp[0].type_qualifier).has_layout())) {
         _mesa_glsl_error(&(yylsp[-1]), state, "auxiliary storage qualifiers must come "
                          "just before storage qualifiers");
      }
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 4653 "generated/glsl/glsl_parser.cpp"
    break;

  case 178: /* type_qualifier: storage_qualifier type_qualifier  */
#line 2073 "mesa-imported/glsl/glsl_parser.yy"
   {
      /* Section 4.3 of the GLSL 1.20 specification states:
       * "Variable declarations may have a storage qualifier specified..."
       *  1.30 clarifies this to "may have one storage qualifier".
       */
      if ((yyvsp[0].type_qualifier).has_storage())
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate storage qualifier");

      if (!state->has_420pack_or_es31() &&
          ((yyvsp[0].type_qualifier).flags.q.precise || (yyvsp[0].type_qualifier).flags.q.invariant || (yyvsp[0].type_qualifier).has_interpolation() ||
           (yyvsp[0].type_qualifier).has_layout() || (yyvsp[0].type_qualifier).has_auxiliary_storage())) {
         _mesa_glsl_error(&(yylsp[-1]), state, "storage qualifiers must come after "
                          "precise, invariant, interpolation, layout and auxiliary "
                          "storage qualifiers");
      }

      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 4677 "generated/glsl/glsl_parser.cpp"
    break;

  case 179: /* type_qualifier: precision_qualifier type_qualifier  */
#line 2093 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].type_qualifier).precision != ast_precision_none)
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate precision qualifier");

      if (!(state->has_420pack_or_es31()) &&
          (yyvsp[0].type_qualifier).flags.i != 0)
         _mesa_glsl_error(&(yylsp[-1]), state, "precision qualifiers must come last");

      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).precision = (yyvsp[-1].n);
   }
#line 4693 "generated/glsl/glsl_parser.cpp"
    break;

  case 180: /* type_qualifier: memory_qualifier type_qualifier  */
#line 2105 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      (yyval.type_qualifier).merge_qualifier(&(yylsp[-1]), state, (yyvsp[0].type_qualifier), false);
   }
#line 4702 "generated/glsl/glsl_parser.cpp"
    break;

  case 181: /* auxiliary_storage_qualifier: CENTROID  */
#line 2113 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.centroid = 1;
   }
#line 4711 "generated/glsl/glsl_parser.cpp"
    break;

  case 182: /* auxiliary_storage_qualifier: SAMPLE  */
#line 2118 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.sample = 1;
   }
#line 4720 "generated/glsl/glsl_parser.cpp"
    break;

  case 183: /* auxiliary_storage_qualifier: PATCH  */
#line 2123 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.patch = 1;
   }
#line 4729 "generated/glsl/glsl_parser.cpp"
    break;

  case 184: /* storage_qualifier: CONST_TOK  */
#line 2130 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.constant = 1;
   }
#line 4738 "generated/glsl/glsl_parser.cpp"
    break;

  case 185: /* storage_qualifier: ATTRIBUTE  */
#line 2135 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.attribute = 1;
   }
#line 4747 "generated/glsl/glsl_parser.cpp"
    break;

  case 186: /* storage_qualifier: VARYING  */
#line 2140 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.varying = 1;
   }
#line 4756 "generated/glsl/glsl_parser.cpp"
    break;

  case 187: /* storage_qualifier: IN_TOK  */
#line 2145 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.in = 1;
   }
#line 4765 "generated/glsl/glsl_parser.cpp"
    break;

  case 188: /* storage_qualifier: OUT_TOK  */
#line 2150 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.out = 1;

      if (state->stage == MESA_SHADER_GEOMETRY &&
          state->has_explicit_attrib_stream()) {
         /* Section 4.3.8.2 (Output Layout Qualifiers) of the GLSL 4.00
          * spec says:
          *
          *     "If the block or variable is declared with the stream
          *     identifier, it is associated with the specified stream;
          *     otherwise, it is associated with the current default stream."
          */
          (yyval.type_qualifier).flags.q.stream = 1;
          (yyval.type_qualifier).flags.q.explicit_stream = 0;
          (yyval.type_qualifier).stream = state->out_qualifier->stream;
      }

      if (state->has_enhanced_layouts()) {
          (yyval.type_qualifier).flags.q.xfb_buffer = 1;
          (yyval.type_qualifier).flags.q.explicit_xfb_buffer = 0;
          (yyval.type_qualifier).xfb_buffer = state->out_qualifier->xfb_buffer;
      }
   }
#line 4794 "generated/glsl/glsl_parser.cpp"
    break;

  case 189: /* storage_qualifier: INOUT_TOK  */
#line 2175 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.in = 1;
      (yyval.type_qualifier).flags.q.out = 1;

      if (!state->has_framebuffer_fetch() ||
          !state->is_version(130, 300) ||
          state->stage != MESA_SHADER_FRAGMENT)
         _mesa_glsl_error(&(yylsp[0]), state, "A single interface variable cannot be "
                          "declared as both input and output");
   }
#line 4810 "generated/glsl/glsl_parser.cpp"
    break;

  case 190: /* storage_qualifier: UNIFORM  */
#line 2187 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.uniform = 1;
   }
#line 4819 "generated/glsl/glsl_parser.cpp"
    break;

  case 191: /* storage_qualifier: BUFFER  */
#line 2192 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.buffer = 1;
   }
#line 4828 "generated/glsl/glsl_parser.cpp"
    break;

  case 192: /* storage_qualifier: SHARED  */
#line 2197 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.shared_storage = 1;
   }
#line 4837 "generated/glsl/glsl_parser.cpp"
    break;

  case 193: /* memory_qualifier: COHERENT  */
#line 2205 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.coherent = 1;
   }
#line 4846 "generated/glsl/glsl_parser.cpp"
    break;

  case 194: /* memory_qualifier: VOLATILE  */
#line 2210 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q._volatile = 1;
   }
#line 4855 "generated/glsl/glsl_parser.cpp"
    break;

  case 195: /* memory_qualifier: RESTRICT  */
#line 2215 "mesa-imported/glsl/glsl_parser.yy"
   {
      STATIC_ASSERT(sizeof((yyval.type_qualifier).flags.q) <= sizeof((yyval.type_qualifier).flags.i));
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.restrict_flag = 1;
   }
#line 4865 "generated/glsl/glsl_parser.cpp"
    break;

  case 196: /* memory_qualifier: READONLY  */
#line 2221 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.read_only = 1;
   }
#line 4874 "generated/glsl/glsl_parser.cpp"
    break;

  case 197: /* memory_qualifier: WRITEONLY  */
#line 2226 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.write_only = 1;
   }
#line 4883 "generated/glsl/glsl_parser.cpp"
    break;

  case 198: /* array_specifier: '[' ']'  */
#line 2234 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.array_specifier) = new(ctx) ast_array_specifier((yylsp[-1]), new(ctx) ast_expression(
                                                  ast_unsized_array_dim, NULL,
                                                  NULL, NULL));
      (yyval.array_specifier)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 4895 "generated/glsl/glsl_parser.cpp"
    break;

  case 199: /* array_specifier: '[' constant_expression ']'  */
#line 2242 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.array_specifier) = new(ctx) ast_array_specifier((yylsp[-2]), (yyvsp[-1].expression));
      (yyval.array_specifier)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 4905 "generated/glsl/glsl_parser.cpp"
    break;

  case 200: /* array_specifier: array_specifier '[' ']'  */
#line 2248 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.array_specifier) = (yyvsp[-2].array_specifier);

      if (state->check_arrays_of_arrays_allowed(& (yylsp[-2]))) {
         (yyval.array_specifier)->add_dimension(new(ctx) ast_expression(ast_unsized_array_dim, NULL,
                                                   NULL, NULL));
      }
   }
#line 4919 "generated/glsl/glsl_parser.cpp"
    break;

  case 201: /* array_specifier: array_specifier '[' constant_expression ']'  */
#line 2258 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.array_specifier) = (yyvsp[-3].array_specifier);

      if (state->check_arrays_of_arrays_allowed(& (yylsp[-3]))) {
         (yyval.array_specifier)->add_dimension((yyvsp[-1].expression));
      }
   }
#line 4931 "generated/glsl/glsl_parser.cpp"
    break;

  case 203: /* type_specifier: type_specifier_nonarray array_specifier  */
#line 2270 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_specifier) = (yyvsp[-1].type_specifier);
      (yyval.type_specifier)->array_specifier = (yyvsp[0].array_specifier);
   }
#line 4940 "generated/glsl/glsl_parser.cpp"
    break;

  case 204: /* type_specifier_nonarray: basic_type_specifier_nonarray  */
#line 2278 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[0].type));
      (yyval.type_specifier)->set_location((yylsp[0]));
   }
#line 4950 "generated/glsl/glsl_parser.cpp"
    break;

  case 205: /* type_specifier_nonarray: struct_specifier  */
#line 2284 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[0].struct_specifier));
      (yyval.type_specifier)->set_location((yylsp[0]));
   }
#line 4960 "generated/glsl/glsl_parser.cpp"
    break;

  case 206: /* type_specifier_nonarray: TYPE_IDENTIFIER  */
#line 2290 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.type_specifier) = new(ctx) ast_type_specifier((yyvsp[0].identifier));
      (yyval.type_specifier)->set_location((yylsp[0]));
   }
#line 4970 "generated/glsl/glsl_parser.cpp"
    break;

  case 207: /* basic_type_specifier_nonarray: VOID_TOK  */
#line 2298 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.type) = glsl_type::void_type; }
#line 4976 "generated/glsl/glsl_parser.cpp"
    break;

  case 208: /* basic_type_specifier_nonarray: BASIC_TYPE_TOK  */
#line 2299 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.type) = (yyvsp[0].type); }
#line 4982 "generated/glsl/glsl_parser.cpp"
    break;

  case 209: /* precision_qualifier: HIGHP  */
#line 2304 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->check_precision_qualifiers_allowed(&(yylsp[0]));
      (yyval.n) = ast_precision_high;
   }
#line 4991 "generated/glsl/glsl_parser.cpp"
    break;

  case 210: /* precision_qualifier: MEDIUMP  */
#line 2309 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->check_precision_qualifiers_allowed(&(yylsp[0]));
      (yyval.n) = ast_precision_medium;
   }
#line 5000 "generated/glsl/glsl_parser.cpp"
    break;

  case 211: /* precision_qualifier: LOWP  */
#line 2314 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->check_precision_qualifiers_allowed(&(yylsp[0]));
      (yyval.n) = ast_precision_low;
   }
#line 5009 "generated/glsl/glsl_parser.cpp"
    break;

  case 212: /* struct_specifier: STRUCT any_identifier '{' struct_declaration_list '}'  */
#line 2322 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.struct_specifier) = new(ctx) ast_struct_specifier((yyvsp[-3].identifier), (yyvsp[-1].declarator_list));
      (yyval.struct_specifier)->set_location_range((yylsp[-3]), (yylsp[0]));
      state->symbols->add_type((yyvsp[-3].identifier), glsl_type::void_type);
   }
#line 5020 "generated/glsl/glsl_parser.cpp"
    break;

  case 213: /* struct_specifier: STRUCT '{' struct_declaration_list '}'  */
#line 2329 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;

      /* All anonymous structs have the same name. This simplifies matching of
       * globals whose type is an unnamed struct.
       *
       * It also avoids a memory leak when the same shader is compiled over and
       * over again.
       */
      (yyval.struct_specifier) = new(ctx) ast_struct_specifier("#anon_struct", (yyvsp[-1].declarator_list));

      (yyval.struct_specifier)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 5038 "generated/glsl/glsl_parser.cpp"
    break;

  case 214: /* struct_declaration_list: struct_declaration  */
#line 2346 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declarator_list) = (yyvsp[0].declarator_list);
      (yyvsp[0].declarator_list)->link.self_link();
   }
#line 5047 "generated/glsl/glsl_parser.cpp"
    break;

  case 215: /* struct_declaration_list: struct_declaration_list struct_declaration  */
#line 2351 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declarator_list) = (yyvsp[-1].declarator_list);
      (yyval.declarator_list)->link.insert_before(& (yyvsp[0].declarator_list)->link);
   }
#line 5056 "generated/glsl/glsl_parser.cpp"
    break;

  case 216: /* struct_declaration: fully_specified_type struct_declarator_list ';'  */
#line 2359 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_fully_specified_type *const type = (yyvsp[-2].fully_specified_type);
      type->set_location((yylsp[-2]));

      if (state->has_bindless()) {
         ast_type_qualifier input_layout_mask;

         /* Allow to declare qualifiers for images. */
         input_layout_mask.flags.i = 0;
         input_layout_mask.flags.q.coherent = 1;
         input_layout_mask.flags.q._volatile = 1;
         input_layout_mask.flags.q.restrict_flag = 1;
         input_layout_mask.flags.q.read_only = 1;
         input_layout_mask.flags.q.write_only = 1;
         input_layout_mask.flags.q.explicit_image_format = 1;

         if ((type->qualifier.flags.i & ~input_layout_mask.flags.i) != 0) {
            _mesa_glsl_error(&(yylsp[-2]), state,
                             "only precision and image qualifiers may be "
                             "applied to structure members");
         }
      } else {
         if (type->qualifier.flags.i != 0)
            _mesa_glsl_error(&(yylsp[-2]), state,
                             "only precision qualifiers may be applied to "
                             "structure members");
      }

      (yyval.declarator_list) = new(ctx) ast_declarator_list(type);
      (yyval.declarator_list)->set_location((yylsp[-1]));

      (yyval.declarator_list)->declarations.push_degenerate_list_at_head(& (yyvsp[-1].declaration)->link);
   }
#line 5095 "generated/glsl/glsl_parser.cpp"
    break;

  case 217: /* struct_declarator_list: struct_declarator  */
#line 2397 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declaration) = (yyvsp[0].declaration);
      (yyvsp[0].declaration)->link.self_link();
   }
#line 5104 "generated/glsl/glsl_parser.cpp"
    break;

  case 218: /* struct_declarator_list: struct_declarator_list ',' struct_declarator  */
#line 2402 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declaration) = (yyvsp[-2].declaration);
      (yyval.declaration)->link.insert_before(& (yyvsp[0].declaration)->link);
   }
#line 5113 "generated/glsl/glsl_parser.cpp"
    break;

  case 219: /* struct_declarator: any_identifier  */
#line 2410 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.declaration) = new(ctx) ast_declaration((yyvsp[0].identifier), NULL, NULL);
      (yyval.declaration)->set_location((yylsp[0]));
   }
#line 5123 "generated/glsl/glsl_parser.cpp"
    break;

  case 220: /* struct_declarator: any_identifier array_specifier  */
#line 2416 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.declaration) = new(ctx) ast_declaration((yyvsp[-1].identifier), (yyvsp[0].array_specifier), NULL);
      (yyval.declaration)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 5133 "generated/glsl/glsl_parser.cpp"
    break;

  case 222: /* initializer: '{' initializer_list '}'  */
#line 2426 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[-1].expression);
   }
#line 5141 "generated/glsl/glsl_parser.cpp"
    break;

  case 223: /* initializer: '{' initializer_list ',' '}'  */
#line 2430 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.expression) = (yyvsp[-2].expression);
   }
#line 5149 "generated/glsl/glsl_parser.cpp"
    break;

  case 224: /* initializer_list: initializer  */
#line 2437 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.expression) = new(ctx) ast_aggregate_initializer();
      (yyval.expression)->set_location((yylsp[0]));
      (yyval.expression)->expressions.push_tail(& (yyvsp[0].expression)->link);
   }
#line 5160 "generated/glsl/glsl_parser.cpp"
    break;

  case 225: /* initializer_list: initializer_list ',' initializer  */
#line 2444 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyvsp[-2].expression)->expressions.push_tail(& (yyvsp[0].expression)->link);
   }
#line 5168 "generated/glsl/glsl_parser.cpp"
    break;

  case 227: /* statement: compound_statement  */
#line 2456 "mesa-imported/glsl/glsl_parser.yy"
                             { (yyval.node) = (ast_node *) (yyvsp[0].compound_statement); }
#line 5174 "generated/glsl/glsl_parser.cpp"
    break;

  case 235: /* compound_statement: '{' '}'  */
#line 2471 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.compound_statement) = new(ctx) ast_compound_statement(true, NULL);
      (yyval.compound_statement)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 5184 "generated/glsl/glsl_parser.cpp"
    break;

  case 236: /* $@2: %empty  */
#line 2477 "mesa-imported/glsl/glsl_parser.yy"
   {
      state->symbols->push_scope();
   }
#line 5192 "generated/glsl/glsl_parser.cpp"
    break;

  case 237: /* compound_statement: '{' $@2 statement_list '}'  */
#line 2481 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.compound_statement) = new(ctx) ast_compound_statement(true, (yyvsp[-1].node));
      (yyval.compound_statement)->set_location_range((yylsp[-3]), (yylsp[0]));
      state->symbols->pop_scope();
   }
#line 5203 "generated/glsl/glsl_parser.cpp"
    break;

  case 238: /* statement_no_new_scope: compound_statement_no_new_scope  */
#line 2490 "mesa-imported/glsl/glsl_parser.yy"
                                   { (yyval.node) = (ast_node *) (yyvsp[0].compound_statement); }
#line 5209 "generated/glsl/glsl_parser.cpp"
    break;

  case 240: /* compound_statement_no_new_scope: '{' '}'  */
#line 2496 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.compound_statement) = new(ctx) ast_compound_statement(false, NULL);
      (yyval.compound_statement)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 5219 "generated/glsl/glsl_parser.cpp"
    break;

  case 241: /* compound_statement_no_new_scope: '{' statement_list '}'  */
#line 2502 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.compound_statement) = new(ctx) ast_compound_statement(false, (yyvsp[-1].node));
      (yyval.compound_statement)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 5229 "generated/glsl/glsl_parser.cpp"
    break;

  case 242: /* statement_list: statement  */
#line 2511 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].node) == NULL) {
         _mesa_glsl_error(& (yylsp[0]), state, "<nil> statement");
         assert((yyvsp[0].node) != NULL);
      }

      (yyval.node) = (yyvsp[0].node);
      (yyval.node)->link.self_link();
   }
#line 5243 "generated/glsl/glsl_parser.cpp"
    break;

  case 243: /* statement_list: statement_list statement  */
#line 2521 "mesa-imported/glsl/glsl_parser.yy"
   {
      if ((yyvsp[0].node) == NULL) {
         _mesa_glsl_error(& (yylsp[0]), state, "<nil> statement");
         assert((yyvsp[0].node) != NULL);
      }
      (yyval.node) = (yyvsp[-1].node);
      (yyval.node)->link.insert_before(& (yyvsp[0].node)->link);
   }
#line 5256 "generated/glsl/glsl_parser.cpp"
    break;

  case 244: /* expression_statement: ';'  */
#line 2533 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_expression_statement(NULL);
      (yyval.node)->set_location((yylsp[0]));
   }
#line 5266 "generated/glsl/glsl_parser.cpp"
    break;

  case 245: /* expression_statement: expression ';'  */
#line 2539 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_expression_statement((yyvsp[-1].expression));
      (yyval.node)->set_location((yylsp[-1]));
   }
#line 5276 "generated/glsl/glsl_parser.cpp"
    break;

  case 246: /* selection_statement: IF '(' expression ')' selection_rest_statement  */
#line 2548 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = new(state->linalloc) ast_selection_statement((yyvsp[-2].expression), (yyvsp[0].selection_rest_statement).then_statement,
                                                        (yyvsp[0].selection_rest_statement).else_statement);
      (yyval.node)->set_location_range((yylsp[-4]), (yylsp[0]));
   }
#line 5286 "generated/glsl/glsl_parser.cpp"
    break;

  case 247: /* selection_rest_statement: statement ELSE statement  */
#line 2557 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.selection_rest_statement).then_statement = (yyvsp[-2].node);
      (yyval.selection_rest_statement).else_statement = (yyvsp[0].node);
   }
#line 5295 "generated/glsl/glsl_parser.cpp"
    break;

  case 248: /* selection_rest_statement: statement  */
#line 2562 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.selection_rest_statement).then_statement = (yyvsp[0].node);
      (yyval.selection_rest_statement).else_statement = NULL;
   }
#line 5304 "generated/glsl/glsl_parser.cpp"
    break;

  case 249: /* condition: expression  */
#line 2570 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = (ast_node *) (yyvsp[0].expression);
   }
#line 5312 "generated/glsl/glsl_parser.cpp"
    break;

  case 250: /* condition: fully_specified_type any_identifier '=' initializer  */
#line 2574 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_declaration *decl = new(ctx) ast_declaration((yyvsp[-2].identifier), NULL, (yyvsp[0].expression));
      ast_declarator_list *declarator = new(ctx) ast_declarator_list((yyvsp[-3].fully_specified_type));
      decl->set_location_range((yylsp[-2]), (yylsp[0]));
      declarator->set_location((yylsp[-3]));

      declarator->declarations.push_tail(&decl->link);
      (yyval.node) = declarator;
   }
#line 5327 "generated/glsl/glsl_parser.cpp"
    break;

  case 251: /* switch_statement: SWITCH '(' expression ')' switch_body  */
#line 2592 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = new(state->linalloc) ast_switch_statement((yyvsp[-2].expression), (yyvsp[0].switch_body));
      (yyval.node)->set_location_range((yylsp[-4]), (yylsp[0]));
   }
#line 5336 "generated/glsl/glsl_parser.cpp"
    break;

  case 252: /* switch_body: '{' '}'  */
#line 2600 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.switch_body) = new(state->linalloc) ast_switch_body(NULL);
      (yyval.switch_body)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 5345 "generated/glsl/glsl_parser.cpp"
    break;

  case 253: /* switch_body: '{' case_statement_list '}'  */
#line 2605 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.switch_body) = new(state->linalloc) ast_switch_body((yyvsp[-1].case_statement_list));
      (yyval.switch_body)->set_location_range((yylsp[-2]), (yylsp[0]));
   }
#line 5354 "generated/glsl/glsl_parser.cpp"
    break;

  case 254: /* case_label: CASE expression ':'  */
#line 2613 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.case_label) = new(state->linalloc) ast_case_label((yyvsp[-1].expression));
      (yyval.case_label)->set_location((yylsp[-1]));
   }
#line 5363 "generated/glsl/glsl_parser.cpp"
    break;

  case 255: /* case_label: DEFAULT ':'  */
#line 2618 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.case_label) = new(state->linalloc) ast_case_label(NULL);
      (yyval.case_label)->set_location((yylsp[0]));
   }
#line 5372 "generated/glsl/glsl_parser.cpp"
    break;

  case 256: /* case_label_list: case_label  */
#line 2626 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_case_label_list *labels = new(state->linalloc) ast_case_label_list();

      labels->labels.push_tail(& (yyvsp[0].case_label)->link);
      (yyval.case_label_list) = labels;
      (yyval.case_label_list)->set_location((yylsp[0]));
   }
#line 5384 "generated/glsl/glsl_parser.cpp"
    break;

  case 257: /* case_label_list: case_label_list case_label  */
#line 2634 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.case_label_list) = (yyvsp[-1].case_label_list);
      (yyval.case_label_list)->labels.push_tail(& (yyvsp[0].case_label)->link);
   }
#line 5393 "generated/glsl/glsl_parser.cpp"
    break;

  case 258: /* case_statement: case_label_list statement  */
#line 2642 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_case_statement *stmts = new(state->linalloc) ast_case_statement((yyvsp[-1].case_label_list));
      stmts->set_location((yylsp[0]));

      stmts->stmts.push_tail(& (yyvsp[0].node)->link);
      (yyval.case_statement) = stmts;
   }
#line 5405 "generated/glsl/glsl_parser.cpp"
    break;

  case 259: /* case_statement: case_statement statement  */
#line 2650 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.case_statement) = (yyvsp[-1].case_statement);
      (yyval.case_statement)->stmts.push_tail(& (yyvsp[0].node)->link);
   }
#line 5414 "generated/glsl/glsl_parser.cpp"
    break;

  case 260: /* case_statement_list: case_statement  */
#line 2658 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_case_statement_list *cases= new(state->linalloc) ast_case_statement_list();
      cases->set_location((yylsp[0]));

      cases->cases.push_tail(& (yyvsp[0].case_statement)->link);
      (yyval.case_statement_list) = cases;
   }
#line 5426 "generated/glsl/glsl_parser.cpp"
    break;

  case 261: /* case_statement_list: case_statement_list case_statement  */
#line 2666 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.case_statement_list) = (yyvsp[-1].case_statement_list);
      (yyval.case_statement_list)->cases.push_tail(& (yyvsp[0].case_statement)->link);
   }
#line 5435 "generated/glsl/glsl_parser.cpp"
    break;

  case 262: /* iteration_statement: WHILE '(' condition ')' statement_no_new_scope  */
#line 2674 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_while,
                                            NULL, (yyvsp[-2].node), NULL, (yyvsp[0].node));
      (yyval.node)->set_location_range((yylsp[-4]), (yylsp[-1]));
   }
#line 5446 "generated/glsl/glsl_parser.cpp"
    break;

  case 263: /* iteration_statement: DO statement WHILE '(' expression ')' ';'  */
#line 2681 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_do_while,
                                            NULL, (yyvsp[-2].expression), NULL, (yyvsp[-5].node));
      (yyval.node)->set_location_range((yylsp[-6]), (yylsp[-1]));
   }
#line 5457 "generated/glsl/glsl_parser.cpp"
    break;

  case 264: /* iteration_statement: FOR '(' for_init_statement for_rest_statement ')' statement_no_new_scope  */
#line 2688 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_iteration_statement(ast_iteration_statement::ast_for,
                                            (yyvsp[-3].node), (yyvsp[-2].for_rest_statement).cond, (yyvsp[-2].for_rest_statement).rest, (yyvsp[0].node));
      (yyval.node)->set_location_range((yylsp[-5]), (yylsp[0]));
   }
#line 5468 "generated/glsl/glsl_parser.cpp"
    break;

  case 268: /* conditionopt: %empty  */
#line 2704 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = NULL;
   }
#line 5476 "generated/glsl/glsl_parser.cpp"
    break;

  case 269: /* for_rest_statement: conditionopt ';'  */
#line 2711 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.for_rest_statement).cond = (yyvsp[-1].node);
      (yyval.for_rest_statement).rest = NULL;
   }
#line 5485 "generated/glsl/glsl_parser.cpp"
    break;

  case 270: /* for_rest_statement: conditionopt ';' expression  */
#line 2716 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.for_rest_statement).cond = (yyvsp[-2].node);
      (yyval.for_rest_statement).rest = (yyvsp[0].expression);
   }
#line 5494 "generated/glsl/glsl_parser.cpp"
    break;

  case 271: /* jump_statement: CONTINUE ';'  */
#line 2725 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_continue, NULL);
      (yyval.node)->set_location((yylsp[-1]));
   }
#line 5504 "generated/glsl/glsl_parser.cpp"
    break;

  case 272: /* jump_statement: BREAK ';'  */
#line 2731 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_break, NULL);
      (yyval.node)->set_location((yylsp[-1]));
   }
#line 5514 "generated/glsl/glsl_parser.cpp"
    break;

  case 273: /* jump_statement: RETURN ';'  */
#line 2737 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, NULL);
      (yyval.node)->set_location((yylsp[-1]));
   }
#line 5524 "generated/glsl/glsl_parser.cpp"
    break;

  case 274: /* jump_statement: RETURN expression ';'  */
#line 2743 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_return, (yyvsp[-1].expression));
      (yyval.node)->set_location_range((yylsp[-2]), (yylsp[-1]));
   }
#line 5534 "generated/glsl/glsl_parser.cpp"
    break;

  case 275: /* jump_statement: DISCARD ';'  */
#line 2749 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.node) = new(ctx) ast_jump_statement(ast_jump_statement::ast_discard, NULL);
      (yyval.node)->set_location((yylsp[-1]));
   }
#line 5544 "generated/glsl/glsl_parser.cpp"
    break;

  case 276: /* external_declaration: function_definition  */
#line 2757 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = (yyvsp[0].function_definition); }
#line 5550 "generated/glsl/glsl_parser.cpp"
    break;

  case 277: /* external_declaration: declaration  */
#line 2758 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = (yyvsp[0].node); }
#line 5556 "generated/glsl/glsl_parser.cpp"
    break;

  case 278: /* external_declaration: pragma_statement  */
#line 2759 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = (yyvsp[0].node); }
#line 5562 "generated/glsl/glsl_parser.cpp"
    break;

  case 279: /* external_declaration: layout_defaults  */
#line 2760 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = (yyvsp[0].node); }
#line 5568 "generated/glsl/glsl_parser.cpp"
    break;

  case 280: /* external_declaration: ';'  */
#line 2761 "mesa-imported/glsl/glsl_parser.yy"
                            { (yyval.node) = NULL; }
#line 5574 "generated/glsl/glsl_parser.cpp"
    break;

  case 281: /* function_definition: function_prototype compound_statement_no_new_scope  */
#line 2766 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      (yyval.function_definition) = new(ctx) ast_function_definition();
      (yyval.function_definition)->set_location_range((yylsp[-1]), (yylsp[0]));
      (yyval.function_definition)->prototype = (yyvsp[-1].function);
      (yyval.function_definition)->body = (yyvsp[0].compound_statement);

      state->symbols->pop_scope();
   }
#line 5588 "generated/glsl/glsl_parser.cpp"
    break;

  case 282: /* interface_block: basic_interface_block  */
#line 2780 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = (yyvsp[0].interface_block);
   }
#line 5596 "generated/glsl/glsl_parser.cpp"
    break;

  case 283: /* interface_block: layout_qualifier interface_block  */
#line 2784 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_interface_block *block = (ast_interface_block *) (yyvsp[0].node);

      if (!(yyvsp[-1].type_qualifier).merge_qualifier(& (yylsp[-1]), state, block->layout, false,
                              block->layout.has_layout())) {
         YYERROR;
      }

      block->layout = (yyvsp[-1].type_qualifier);

      (yyval.node) = block;
   }
#line 5613 "generated/glsl/glsl_parser.cpp"
    break;

  case 284: /* interface_block: memory_qualifier interface_block  */
#line 2797 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_interface_block *block = (ast_interface_block *)(yyvsp[0].node);

      if (!block->default_layout.flags.q.buffer) {
            _mesa_glsl_error(& (yylsp[-1]), state,
                             "memory qualifiers can only be used in the "
                             "declaration of shader storage blocks");
      }
      if (!(yyvsp[-1].type_qualifier).merge_qualifier(& (yylsp[-1]), state, block->layout, false)) {
         YYERROR;
      }
      block->layout = (yyvsp[-1].type_qualifier);
      (yyval.node) = block;
   }
#line 5632 "generated/glsl/glsl_parser.cpp"
    break;

  case 285: /* basic_interface_block: interface_qualifier NEW_IDENTIFIER '{' member_list '}' instance_name_opt ';'  */
#line 2815 "mesa-imported/glsl/glsl_parser.yy"
   {
      ast_interface_block *const block = (yyvsp[-1].interface_block);

      if ((yyvsp[-6].type_qualifier).flags.q.uniform) {
         block->default_layout = *state->default_uniform_qualifier;
      } else if ((yyvsp[-6].type_qualifier).flags.q.buffer) {
         block->default_layout = *state->default_shader_storage_qualifier;
      }
      block->block_name = (yyvsp[-5].identifier);
      block->declarations.push_degenerate_list_at_head(& (yyvsp[-3].declarator_list)->link);

      _mesa_ast_process_interface_block(& (yylsp[-6]), state, block, (yyvsp[-6].type_qualifier));

      (yyval.interface_block) = block;
   }
#line 5652 "generated/glsl/glsl_parser.cpp"
    break;

  case 286: /* interface_qualifier: IN_TOK  */
#line 2834 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.in = 1;
   }
#line 5661 "generated/glsl/glsl_parser.cpp"
    break;

  case 287: /* interface_qualifier: OUT_TOK  */
#line 2839 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.out = 1;
   }
#line 5670 "generated/glsl/glsl_parser.cpp"
    break;

  case 288: /* interface_qualifier: UNIFORM  */
#line 2844 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.uniform = 1;
   }
#line 5679 "generated/glsl/glsl_parser.cpp"
    break;

  case 289: /* interface_qualifier: BUFFER  */
#line 2849 "mesa-imported/glsl/glsl_parser.yy"
   {
      memset(& (yyval.type_qualifier), 0, sizeof((yyval.type_qualifier)));
      (yyval.type_qualifier).flags.q.buffer = 1;
   }
#line 5688 "generated/glsl/glsl_parser.cpp"
    break;

  case 290: /* interface_qualifier: auxiliary_storage_qualifier interface_qualifier  */
#line 2854 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (!(yyvsp[-1].type_qualifier).flags.q.patch) {
         _mesa_glsl_error(&(yylsp[-1]), state, "invalid interface qualifier");
      }
      if ((yyvsp[0].type_qualifier).has_auxiliary_storage()) {
         _mesa_glsl_error(&(yylsp[-1]), state, "duplicate patch qualifier");
      }
      (yyval.type_qualifier) = (yyvsp[0].type_qualifier);
      (yyval.type_qualifier).flags.q.patch = 1;
   }
#line 5703 "generated/glsl/glsl_parser.cpp"
    break;

  case 291: /* instance_name_opt: %empty  */
#line 2868 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.interface_block) = new(state->linalloc) ast_interface_block(NULL, NULL);
   }
#line 5711 "generated/glsl/glsl_parser.cpp"
    break;

  case 292: /* instance_name_opt: NEW_IDENTIFIER  */
#line 2872 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.interface_block) = new(state->linalloc) ast_interface_block((yyvsp[0].identifier), NULL);
      (yyval.interface_block)->set_location((yylsp[0]));
   }
#line 5720 "generated/glsl/glsl_parser.cpp"
    break;

  case 293: /* instance_name_opt: NEW_IDENTIFIER array_specifier  */
#line 2877 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.interface_block) = new(state->linalloc) ast_interface_block((yyvsp[-1].identifier), (yyvsp[0].array_specifier));
      (yyval.interface_block)->set_location_range((yylsp[-1]), (yylsp[0]));
   }
#line 5729 "generated/glsl/glsl_parser.cpp"
    break;

  case 294: /* member_list: member_declaration  */
#line 2885 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declarator_list) = (yyvsp[0].declarator_list);
      (yyvsp[0].declarator_list)->link.self_link();
   }
#line 5738 "generated/glsl/glsl_parser.cpp"
    break;

  case 295: /* member_list: member_declaration member_list  */
#line 2890 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.declarator_list) = (yyvsp[-1].declarator_list);
      (yyvsp[0].declarator_list)->link.insert_before(& (yyval.declarator_list)->link);
   }
#line 5747 "generated/glsl/glsl_parser.cpp"
    break;

  case 296: /* member_declaration: fully_specified_type struct_declarator_list ';'  */
#line 2898 "mesa-imported/glsl/glsl_parser.yy"
   {
      void *ctx = state->linalloc;
      ast_fully_specified_type *type = (yyvsp[-2].fully_specified_type);
      type->set_location((yylsp[-2]));

      if (type->qualifier.flags.q.attribute) {
         _mesa_glsl_error(& (yylsp[-2]), state,
                          "keyword 'attribute' cannot be used with "
                          "interface block member");
      } else if (type->qualifier.flags.q.varying) {
         _mesa_glsl_error(& (yylsp[-2]), state,
                          "keyword 'varying' cannot be used with "
                          "interface block member");
      }

      (yyval.declarator_list) = new(ctx) ast_declarator_list(type);
      (yyval.declarator_list)->set_location((yylsp[-1]));

      (yyval.declarator_list)->declarations.push_degenerate_list_at_head(& (yyvsp[-1].declaration)->link);
   }
#line 5772 "generated/glsl/glsl_parser.cpp"
    break;

  case 297: /* layout_uniform_defaults: layout_qualifier layout_uniform_defaults  */
#line 2922 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      if (!(yyval.type_qualifier).merge_qualifier(& (yylsp[-1]), state, (yyvsp[0].type_qualifier), false, true)) {
         YYERROR;
      }
   }
#line 5783 "generated/glsl/glsl_parser.cpp"
    break;

  case 299: /* layout_buffer_defaults: layout_qualifier layout_buffer_defaults  */
#line 2933 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      if (!(yyval.type_qualifier).merge_qualifier(& (yylsp[-1]), state, (yyvsp[0].type_qualifier), false, true)) {
         YYERROR;
      }
   }
#line 5794 "generated/glsl/glsl_parser.cpp"
    break;

  case 301: /* layout_in_defaults: layout_qualifier layout_in_defaults  */
#line 2944 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      if (!(yyval.type_qualifier).merge_qualifier(& (yylsp[-1]), state, (yyvsp[0].type_qualifier), false, true)) {
         YYERROR;
      }
      if (!(yyval.type_qualifier).validate_in_qualifier(& (yylsp[-1]), state)) {
         YYERROR;
      }
   }
#line 5808 "generated/glsl/glsl_parser.cpp"
    break;

  case 302: /* layout_in_defaults: layout_qualifier IN_TOK ';'  */
#line 2954 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (!(yyvsp[-2].type_qualifier).validate_in_qualifier(& (yylsp[-2]), state)) {
         YYERROR;
      }
   }
#line 5818 "generated/glsl/glsl_parser.cpp"
    break;

  case 303: /* layout_out_defaults: layout_qualifier layout_out_defaults  */
#line 2963 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.type_qualifier) = (yyvsp[-1].type_qualifier);
      if (!(yyval.type_qualifier).merge_qualifier(& (yylsp[-1]), state, (yyvsp[0].type_qualifier), false, true)) {
         YYERROR;
      }
      if (!(yyval.type_qualifier).validate_out_qualifier(& (yylsp[-1]), state)) {
         YYERROR;
      }
   }
#line 5832 "generated/glsl/glsl_parser.cpp"
    break;

  case 304: /* layout_out_defaults: layout_qualifier OUT_TOK ';'  */
#line 2973 "mesa-imported/glsl/glsl_parser.yy"
   {
      if (!(yyvsp[-2].type_qualifier).validate_out_qualifier(& (yylsp[-2]), state)) {
         YYERROR;
      }
   }
#line 5842 "generated/glsl/glsl_parser.cpp"
    break;

  case 305: /* layout_defaults: layout_uniform_defaults  */
#line 2982 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = NULL;
      if (!state->default_uniform_qualifier->
             merge_qualifier(& (yylsp[0]), state, (yyvsp[0].type_qualifier), false)) {
         YYERROR;
      }
      if (!state->default_uniform_qualifier->
             push_to_global(& (yylsp[0]), state)) {
         YYERROR;
      }
   }
#line 5858 "generated/glsl/glsl_parser.cpp"
    break;

  case 306: /* layout_defaults: layout_buffer_defaults  */
#line 2994 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = NULL;
      if (!state->default_shader_storage_qualifier->
             merge_qualifier(& (yylsp[0]), state, (yyvsp[0].type_qualifier), false)) {
         YYERROR;
      }
      if (!state->default_shader_storage_qualifier->
             push_to_global(& (yylsp[0]), state)) {
         YYERROR;
      }

      /* From the GLSL 4.50 spec, section 4.4.5:
       *
       *     "It is a compile-time error to specify the binding identifier for
       *     the global scope or for block member declarations."
       */
      if (state->default_shader_storage_qualifier->flags.q.explicit_binding) {
         _mesa_glsl_error(& (yylsp[0]), state,
                          "binding qualifier cannot be set for default layout");
      }
   }
#line 5884 "generated/glsl/glsl_parser.cpp"
    break;

  case 307: /* layout_defaults: layout_in_defaults  */
#line 3016 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = NULL;
      if (!(yyvsp[0].type_qualifier).merge_into_in_qualifier(& (yylsp[0]), state, (yyval.node))) {
         YYERROR;
      }
      if (!state->in_qualifier->push_to_global(& (yylsp[0]), state)) {
         YYERROR;
      }
   }
#line 5898 "generated/glsl/glsl_parser.cpp"
    break;

  case 308: /* layout_defaults: layout_out_defaults  */
#line 3026 "mesa-imported/glsl/glsl_parser.yy"
   {
      (yyval.node) = NULL;
      if (!(yyvsp[0].type_qualifier).merge_into_out_qualifier(& (yylsp[0]), state, (yyval.node))) {
         YYERROR;
      }
      if (!state->out_qualifier->push_to_global(& (yylsp[0]), state)) {
         YYERROR;
      }
   }
#line 5912 "generated/glsl/glsl_parser.cpp"
    break;


#line 5916 "generated/glsl/glsl_parser.cpp"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      {
        yypcontext_t yyctx
          = {yyssp, yytoken, &yylloc};
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == -1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *,
                             YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (yymsg)
              {
                yysyntax_error_status
                  = yysyntax_error (&yymsg_alloc, &yymsg, &yyctx);
                yymsgp = yymsg;
              }
            else
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = YYENOMEM;
              }
          }
        yyerror (&yylloc, state, yymsgp);
        if (yysyntax_error_status == YYENOMEM)
          YYNOMEM;
      }
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, state);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, state);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, state, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, state);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, state);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
  return yyresult;
}

