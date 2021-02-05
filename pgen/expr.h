/*************************************************************************
 *
 *  Standard expression parsing library
 *
 *  Copyright (c) 1999-2010, 2019 Rajit Manohar
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *
 **************************************************************************
 */
#ifndef __EXPR_H__
#define __EXPR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "file.h"
#include "pp.h"
#include "id.h"

/**
 * @defgroup E_TYPES Expression types/tokens
 * Expression tokens used in @ref expr . 
 * The parser will look for ['<char>'], 
 * for ambigus parcing options the look at the structure of the equation.
 * 
 * for precence/priority for the tokens @ref act/expr2.cc
 * 
 * some of the tokens are indicates as parser only or data structure only
 * parser only tokens are used to guide the parcing process to build the correct data structure e.g. parentesis
 * data only tokens are used to represent properties and units like function/variable names and other objets like numbers
 * 
 * most tokens are defined in @ref act/expr.h ( pgen/expr.h ) :
 * some more tokens are defined in @ref act/types.h:
 * 
 * @{
 */

#define  E_AND 	 0 /**< and ['&']*/
#define  E_OR 	 1 /**< or ['|']*/
#define  E_NOT 	 2 /**< not ['~'], single argument left only*/
#define  E_PLUS  3 /**< addition ['+']*/
#define  E_MINUS 4 /**< signed subtraction ['-'], left - right */
#define  E_MULT	 5 /**< multiplication ['*'] */
#define  E_DIV	 6 /**< division ['/'] */
#define  E_MOD	 7 /**< modulo - remainder ['\%'] */
#define  E_LSL	 8 /**< logic shift left ['<<'] */
#define  E_LSR	 9 /**< logic shift right ['>>'], only active in integer and real parser mode */
#define  E_ASR	 10 /**< arithmetic shift right ['>>>'] */
#define  E_UMINUS 11 /**< subtraction enclosed expression ['-'], single argument left only */
#define  E_INT	 12  /**< unsigned integer, data structure only type. */
#define  E_VAR	 13  /**< variable pointer? pID, data structure only type. @TODO confirm */
#define  E_QUERY 14 /**< Query or if statement, left is condition, right should be a E_COLON */
#define  E_LPAR	 15 /**< open parantesis, parser only type. */
#define  E_RPAR	 16 /**< close parantesis, parser only type. */
#define  E_XOR	 17 /**< xor ['^']*/
#define  E_LT	 18 /**< less than comparator ['<']*/
#define  E_GT    19 /**< greater than comparator ['>']*/
#define  E_LE    20 /**< less or equal comparator ['<=']*/
#define  E_GE	 21 /**< greater or equal comparator ['>=']*/
#define  E_EQ    22 /**< equivilance comaparator ['=']. compares value => hardware */
#define  E_NE    23 /**< difference comaparator['!=']. compares value => hardware */
#define  E_TRUE  24  /**< TRUE 1, data structure only type. */
#define  E_FALSE 25  /**< FALSE 0, data structure only type. */
#define  E_COLON 26 /**< excecuts a decision of a E_QUERY [':']. @TODO check if used without E_QUERY */
#define  E_PROBE 27 /**< Inidicade a probe ['#'], single argument left only */
#define  E_COMMA 28 /**< comma [','], parser only type @TODO check. */
#define  E_CONCAT 29 /**< start concatonate ['{'] */
#define  E_BITFIELD 30 /**< BITFIELD ['..']. @TODO describe */
#define  E_COMPLEMENT 31 /**< bitwise complement, single argument left only */
#define  E_REAL 32  /**< float value, data structure only type. */

#define  E_RAWFREE 33  /**< place holder?, data structure only type. used in @see act/expr_extra.c in case of a loop @TODO investigate */
  
#define  E_END   34 /**< end concatonate ['}'] */

#define E_NUMBER 35 /**< # of E_xxx things, not a token. number of tokens defined, for array sizing and in range checks */

#define E_FUNCTION 100 /**< function with name, data structure only type. */
/** @} */

/** 
 * the Expr data structure.
 * the Expression data type is a recursive data structure that is a hybrid of a binary tree and a recursive list, 
 * it consists out its *type* as int @ref E_TYPES depending on the type you have the following variables inside the struct 
 * (mutualy exclusive because union *u*):
 * 
 * the **e** sub struct:
 * 
 * for *type*: @ref E_AND, @ref E_OR, @ref E_PLUS, @ref E_MINUS, @ref E_MULT, @ref E_DIV, @ref E_MOD, @ref E_LSL, @ref E_XOR,
 * @ref E_LT, @ref E_GT, @ref E_LE, @ref E_GE, @ref E_EQ, @ref E_NE, @ref E_LSR, @ref E_ASR, @ref E_NOT (left only), 
 * @ref E_COMPLEMENT (left only), @ref E_UMINUS (left only), @ref E_QUERY (assumes a E_COLON on right), @ref E_PROBE (left only),  you have:
 *  - an expr pointer *u.e.l* (left) 
 *  - and *u.e.r* (right)
 * 
 * the **fn** sub struct:
 * 
 * for *type*: @ref E_FUNCTION you have:
 *  - a char pointer *u.fn.s* that points to the begining of a char array 
 *  - and an expr pointer *u.fn.r* (right)
 * 
 * the **x** sub struct:
 * 
 * for *type*: you have:
 *  - an unsigned int *u.x.val* 
 *  - and a unsigned long *u.x.extra*
 * 
 * the variables:
 * 
 * for *type*: @ref E_REAL you have:
 *  - a double *u.f*
 * 
 * for *type*: @ref E_INT you have:
 *  - an unsigned int *u.v*
 * 
 * and some special *type* :
 *  - for @ref E_VAR *u.e.l* is miss used as a pointer of the data type pID? expr_parse_id , void? expr_free_id + expr_print_id (cast needed), left only, @TODO check source of pgen
 * 
 *  - for @ref E_PROBE *u.e.l* is miss used as a pointer of the data type pID? expr_parse_id , void? expr_free_id + expr_print_probe (cast needed), left only, @TODO check source of pgen
 * 
 *  - for @ref E_BITFIELD the *u.e.l* is missused as a pointer of the data type pID? expr_parse_id , void? expr_free_id + expr_print_id (cast needed)
 *    and *u.e.r* used normal for a expr, this expression is assumed to hold an other E_BITFIELD
 *    with *u.e.l* as an unsiged long holding the start and *u.e.r* the end @TODO double check if correct
 * 
 * examples:
 * Function calls are represented as:
 *  @code
 *  type = E_FUNCTION
 *
 *  l = function name
 *  
 *  E_FUNCTION
 *    /      |
 *   name    E_LT 
 *          /  \
 *        arg1  E_LT
 *             /  \
 *           arg2  ...
 *
 *
 *  A ? B : C is represented as:
 *
 *       E_QUERY
 *      /      \
 *      A      E_COLON
 *              /  \
 *             B    C
 *
 *
 *  type = E_CONCAT
 *
 *      E_CONCAT
 *       /     |
 *     expr   E_CONCAT
 *             /    |
 *           expr   ...
 *
 *
 *  type = E_BITFIELD
 *
 *      E_BITFIELD
 *      /       |
 *    var     E_BITFIELD
 *              /    |
 *             lo    hi
 * @endcode
 */
typedef struct expr {
  int type; /**< one of @ref E_TYPES */
  union {
    struct {
      struct expr *l, *r;
    } e;
    struct {
      char *s;
      struct expr *r;
    } fn;
    struct {
      unsigned int val;
      unsigned long extra;
    } x;
    double f;
    unsigned int v;
  } u;
} Expr;


void expr_settoken (int name, int value);
  /* 
     Set the lexical id for token name (from the E_xxx list above) to
     value. "value" is the integer that lex_addtoken() returns.
     To delete expression operators, simply set the value of a token
     to -1.
  */

  
int expr_parse_isany (LFILE *l);
  /* return 0 if this is definitely not an expression */

Expr *expr_parse_any (LFILE *l);
  /*
    Parse any expression
  */

Expr *expr_parse_int (LFILE *l);
  /*
    Parse an integer expression and returns its parse tree.
  */

Expr *expr_parse_bool (LFILE *l);
 /*
   Parse a Boolean expression and return its parse tree.
 */

Expr *expr_parse_real (LFILE *l);
 /*
   Parse a real expression and return its parse tree.
 */

void expr_free (Expr *e);
 /*
   Free expression tree
 */

int expr_init (LFILE *l);
 /*
   Initialize all tokens with standard symbols, and add tokens to the
   lexer. Returns the number of tokens added. This number might be 
   less than the # of tokens used by the expression evaluation package
   because the lexer may already know about some of the tokens.
 */

void expr_clear (void);
  /*
    Clears expressions from expression parser
  */

void expr_endgtmode (int m);

extern pId *(*expr_parse_id)(LFILE *l);
  /* The function must do the following:
       - if there is a valid identifier, then create a data structure
         for it and return a pointer to it.
       - if there isn't, don't do anything, return a NULL pointer. The
         lexer state must be unchanged in this case.
  */
extern void (*expr_free_id) (void *);
  /* Free an allocated id structure */

extern void (*expr_print_id) (pp_t *, void *);
  /* Print an allocated id structure */

extern void (*expr_print_probe) (pp_t *, void *);
  /* Print #chan */

  
extern Expr *(*expr_parse_basecase_num)(LFILE *l);
  /* if you want to support your own basecase for numerical expr */

extern Expr *(*expr_parse_basecase_bool)(LFILE *l);
  /* if you want to support your own basecase for bool expr */

extern int (*expr_parse_newtokens)(LFILE *l);

extern void expr_inc_parens (void);  
extern void expr_dec_parens (void);  


extern int expr_gettoken (int t);
  /* needed for external parsing support */

extern void expr_print (pp_t *, Expr *);
/*  Print expression */

extern const char *expr_operator_name (int type);
/* return string corresponding to the operator token */

#ifdef __cplusplus
}
#endif

#endif /* __EXPR_H__ */
