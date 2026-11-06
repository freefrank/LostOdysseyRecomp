/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY__MESA_GLSL_GENERATED_GLSL_GLSL_PARSER_H_INCLUDED
# define YY__MESA_GLSL_GENERATED_GLSL_GLSL_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int _mesa_glsl_debug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    ATTRIBUTE = 258,               /* ATTRIBUTE  */
    CONST_TOK = 259,               /* CONST_TOK  */
    BASIC_TYPE_TOK = 260,          /* BASIC_TYPE_TOK  */
    BREAK = 261,                   /* BREAK  */
    BUFFER = 262,                  /* BUFFER  */
    CONTINUE = 263,                /* CONTINUE  */
    DO = 264,                      /* DO  */
    ELSE = 265,                    /* ELSE  */
    FOR = 266,                     /* FOR  */
    IF = 267,                      /* IF  */
    DISCARD = 268,                 /* DISCARD  */
    RETURN = 269,                  /* RETURN  */
    SWITCH = 270,                  /* SWITCH  */
    CASE = 271,                    /* CASE  */
    DEFAULT = 272,                 /* DEFAULT  */
    CENTROID = 273,                /* CENTROID  */
    IN_TOK = 274,                  /* IN_TOK  */
    OUT_TOK = 275,                 /* OUT_TOK  */
    INOUT_TOK = 276,               /* INOUT_TOK  */
    UNIFORM = 277,                 /* UNIFORM  */
    VARYING = 278,                 /* VARYING  */
    SAMPLE = 279,                  /* SAMPLE  */
    NOPERSPECTIVE = 280,           /* NOPERSPECTIVE  */
    FLAT = 281,                    /* FLAT  */
    SMOOTH = 282,                  /* SMOOTH  */
    IMAGE1DSHADOW = 283,           /* IMAGE1DSHADOW  */
    IMAGE2DSHADOW = 284,           /* IMAGE2DSHADOW  */
    IMAGE1DARRAYSHADOW = 285,      /* IMAGE1DARRAYSHADOW  */
    IMAGE2DARRAYSHADOW = 286,      /* IMAGE2DARRAYSHADOW  */
    COHERENT = 287,                /* COHERENT  */
    VOLATILE = 288,                /* VOLATILE  */
    RESTRICT = 289,                /* RESTRICT  */
    READONLY = 290,                /* READONLY  */
    WRITEONLY = 291,               /* WRITEONLY  */
    SHARED = 292,                  /* SHARED  */
    STRUCT = 293,                  /* STRUCT  */
    VOID_TOK = 294,                /* VOID_TOK  */
    WHILE = 295,                   /* WHILE  */
    IDENTIFIER = 296,              /* IDENTIFIER  */
    TYPE_IDENTIFIER = 297,         /* TYPE_IDENTIFIER  */
    NEW_IDENTIFIER = 298,          /* NEW_IDENTIFIER  */
    FLOATCONSTANT = 299,           /* FLOATCONSTANT  */
    DOUBLECONSTANT = 300,          /* DOUBLECONSTANT  */
    INTCONSTANT = 301,             /* INTCONSTANT  */
    UINTCONSTANT = 302,            /* UINTCONSTANT  */
    BOOLCONSTANT = 303,            /* BOOLCONSTANT  */
    INT64CONSTANT = 304,           /* INT64CONSTANT  */
    UINT64CONSTANT = 305,          /* UINT64CONSTANT  */
    FIELD_SELECTION = 306,         /* FIELD_SELECTION  */
    LEFT_OP = 307,                 /* LEFT_OP  */
    RIGHT_OP = 308,                /* RIGHT_OP  */
    INC_OP = 309,                  /* INC_OP  */
    DEC_OP = 310,                  /* DEC_OP  */
    LE_OP = 311,                   /* LE_OP  */
    GE_OP = 312,                   /* GE_OP  */
    EQ_OP = 313,                   /* EQ_OP  */
    NE_OP = 314,                   /* NE_OP  */
    AND_OP = 315,                  /* AND_OP  */
    OR_OP = 316,                   /* OR_OP  */
    XOR_OP = 317,                  /* XOR_OP  */
    MUL_ASSIGN = 318,              /* MUL_ASSIGN  */
    DIV_ASSIGN = 319,              /* DIV_ASSIGN  */
    ADD_ASSIGN = 320,              /* ADD_ASSIGN  */
    MOD_ASSIGN = 321,              /* MOD_ASSIGN  */
    LEFT_ASSIGN = 322,             /* LEFT_ASSIGN  */
    RIGHT_ASSIGN = 323,            /* RIGHT_ASSIGN  */
    AND_ASSIGN = 324,              /* AND_ASSIGN  */
    XOR_ASSIGN = 325,              /* XOR_ASSIGN  */
    OR_ASSIGN = 326,               /* OR_ASSIGN  */
    SUB_ASSIGN = 327,              /* SUB_ASSIGN  */
    INVARIANT = 328,               /* INVARIANT  */
    PRECISE = 329,                 /* PRECISE  */
    LOWP = 330,                    /* LOWP  */
    MEDIUMP = 331,                 /* MEDIUMP  */
    HIGHP = 332,                   /* HIGHP  */
    SUPERP = 333,                  /* SUPERP  */
    PRECISION = 334,               /* PRECISION  */
    VERSION_TOK = 335,             /* VERSION_TOK  */
    EXTENSION = 336,               /* EXTENSION  */
    LINE = 337,                    /* LINE  */
    COLON = 338,                   /* COLON  */
    EOL = 339,                     /* EOL  */
    INTERFACE = 340,               /* INTERFACE  */
    OUTPUT = 341,                  /* OUTPUT  */
    PRAGMA_DEBUG_ON = 342,         /* PRAGMA_DEBUG_ON  */
    PRAGMA_DEBUG_OFF = 343,        /* PRAGMA_DEBUG_OFF  */
    PRAGMA_OPTIMIZE_ON = 344,      /* PRAGMA_OPTIMIZE_ON  */
    PRAGMA_OPTIMIZE_OFF = 345,     /* PRAGMA_OPTIMIZE_OFF  */
    PRAGMA_WARNING_ON = 346,       /* PRAGMA_WARNING_ON  */
    PRAGMA_WARNING_OFF = 347,      /* PRAGMA_WARNING_OFF  */
    PRAGMA_INVARIANT_ALL = 348,    /* PRAGMA_INVARIANT_ALL  */
    LAYOUT_TOK = 349,              /* LAYOUT_TOK  */
    DOT_TOK = 350,                 /* DOT_TOK  */
    ASM = 351,                     /* ASM  */
    CLASS = 352,                   /* CLASS  */
    UNION = 353,                   /* UNION  */
    ENUM = 354,                    /* ENUM  */
    TYPEDEF = 355,                 /* TYPEDEF  */
    TEMPLATE = 356,                /* TEMPLATE  */
    THIS = 357,                    /* THIS  */
    PACKED_TOK = 358,              /* PACKED_TOK  */
    GOTO = 359,                    /* GOTO  */
    INLINE_TOK = 360,              /* INLINE_TOK  */
    NOINLINE = 361,                /* NOINLINE  */
    PUBLIC_TOK = 362,              /* PUBLIC_TOK  */
    STATIC = 363,                  /* STATIC  */
    EXTERN = 364,                  /* EXTERN  */
    EXTERNAL = 365,                /* EXTERNAL  */
    LONG_TOK = 366,                /* LONG_TOK  */
    SHORT_TOK = 367,               /* SHORT_TOK  */
    HALF = 368,                    /* HALF  */
    FIXED_TOK = 369,               /* FIXED_TOK  */
    UNSIGNED = 370,                /* UNSIGNED  */
    INPUT_TOK = 371,               /* INPUT_TOK  */
    HVEC2 = 372,                   /* HVEC2  */
    HVEC3 = 373,                   /* HVEC3  */
    HVEC4 = 374,                   /* HVEC4  */
    FVEC2 = 375,                   /* FVEC2  */
    FVEC3 = 376,                   /* FVEC3  */
    FVEC4 = 377,                   /* FVEC4  */
    SAMPLER3DRECT = 378,           /* SAMPLER3DRECT  */
    SIZEOF = 379,                  /* SIZEOF  */
    CAST = 380,                    /* CAST  */
    NAMESPACE = 381,               /* NAMESPACE  */
    USING = 382,                   /* USING  */
    RESOURCE = 383,                /* RESOURCE  */
    PATCH = 384,                   /* PATCH  */
    SUBROUTINE = 385,              /* SUBROUTINE  */
    ERROR_TOK = 386,               /* ERROR_TOK  */
    COMMON = 387,                  /* COMMON  */
    PARTITION = 388,               /* PARTITION  */
    ACTIVE = 389,                  /* ACTIVE  */
    FILTER = 390,                  /* FILTER  */
    ROW_MAJOR = 391,               /* ROW_MAJOR  */
    THEN = 392                     /* THEN  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 98 "mesa-imported/glsl/glsl_parser.yy"

   int n;
   int64_t n64;
   float real;
   double dreal;
   const char *identifier;

   struct ast_type_qualifier type_qualifier;

   ast_node *node;
   ast_type_specifier *type_specifier;
   ast_array_specifier *array_specifier;
   ast_fully_specified_type *fully_specified_type;
   ast_function *function;
   ast_parameter_declarator *parameter_declarator;
   ast_function_definition *function_definition;
   ast_compound_statement *compound_statement;
   ast_expression *expression;
   ast_declarator_list *declarator_list;
   ast_struct_specifier *struct_specifier;
   ast_declaration *declaration;
   ast_switch_body *switch_body;
   ast_case_label *case_label;
   ast_case_label_list *case_label_list;
   ast_case_statement *case_statement;
   ast_case_statement_list *case_statement_list;
   ast_interface_block *interface_block;
   ast_subroutine_list *subroutine_list;
   struct {
      ast_node *cond;
      ast_expression *rest;
   } for_rest_statement;

   struct {
      ast_node *then_statement;
      ast_node *else_statement;
   } selection_rest_statement;

   const glsl_type *type;

#line 242 "generated/glsl/glsl_parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif




int _mesa_glsl_parse (struct _mesa_glsl_parse_state *state);


#endif /* !YY__MESA_GLSL_GENERATED_GLSL_GLSL_PARSER_H_INCLUDED  */
