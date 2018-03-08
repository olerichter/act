/*************************************************************************
 *
 *  Copyright (c) 2015-2018 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <stdio.h>
#include <stdarg.h>
#include <act/types.h>
#include <act/inst.h>
#include <act/act_parse_int.h>
#include <act/act_walk.extra.h>


/*
 *
 * Typechecking code
 *
 */

static char typecheck_errmsg[10240];

static void typecheck_err (const char *s, ...)
{
  va_list ap;

  va_start (ap, s);
  vsnprintf (typecheck_errmsg, 10239,  s, ap);
  typecheck_errmsg[10239] = '\0';
  va_end (ap);
}

const char *act_type_errmsg (void)
{
  return typecheck_errmsg;
}

int act_type_var (Scope *s, ActId *id)
{
  InstType *it;
  UserDef *u;
  Data *d;
  ActId *orig = id;
  int is_strict;

  /*printf ("[scope=%x] check: var ", s); id->Print (stdout); printf ("\n");*/

  Assert (s, "Scope?");
  Assert (id, "Identifier?");
  it = s->Lookup (id->getName());
  if (!it) {
    it = s->FullLookup (id->getName());
    is_strict = T_STRICT;
  }
  else {
    is_strict = 0;
  }
  Assert (it, "This should have been caught during parsing!");

  if (id->Rest ()) {
    while (id->Rest()) {
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught during parsing!");
      id = id->Rest();
      it = u->Lookup (id);
    }
  }
  else {
    /*printf ("strict check: %s\n", id->getName ());*/
    /* this may be a strict port param */
    u = s->getUserDef();
    if (u && u->isStrictPort (id->getName ())) {
      is_strict = T_STRICT;
      /*printf ("this is strict [%s]!\n", id->getName ());*/
    }
  }

  Assert (!(id->isDeref () && !it->arrayInfo()), "Check during parsing!");
  
  Type *t = it->BaseType ();

  if (TypeFactory::isPIntType (t)) {
    return T_INT|T_PARAM|is_strict;
  }
  if (TypeFactory::isPBoolType (t)) {
    return T_BOOL|T_PARAM|is_strict;
  }
  if (TypeFactory::isPRealType (t)) {
    return T_REAL|T_PARAM|is_strict;
  }
  if (TypeFactory::isIntType (t)) {
    return T_INT;
  }
  if (TypeFactory::isBoolType (t)) {
    return T_BOOL;
  }
  if (!TypeFactory::isDataType (t)) {
    if (TypeFactory::isChanType (t)) {
      return T_CHAN;
    }
    if (TypeFactory::isProcessType (t)) {
      return T_PROC;
    }
    typecheck_err ("Identifier `%s' is not a data type", id->getName ());
    return T_ERR;
  }
  d = dynamic_cast<Data *>(t);
  if (d->isEnum()) {
    return T_INT;
  }
#if 0
  fatal_error ("FIX this. Check that the data type is an int or a bool");
  /* XXX: FIX THIS PLEASE */
  typecheck_err ("FIX this. Check data type.");
#endif
  return T_DATA;
}


/**
 *  Typecheck expression
 *
 *  @param s is the scope in which all identifiers are evaluated
 *  @param e is the expression to be typechecked
 *  @return -1 on an error
 */
int act_type_expr (Scope *s, Expr *e)
{
  int lt, rt;
  int flgs;
  if (!e) return T_ERR;

  /*  
  printf ("check: %s\n", expr_operator_name (e->type));
  printf (" lt: %x  rt: %x\n", lt, rt);      
  */

#define EQUAL_LT_RT2(f,g)			\
  do {						\
    lt = act_type_expr (s, e->u.e.l);		\
    rt = act_type_expr (s, e->u.e.r);		\
    if (lt == T_ERR || rt == T_ERR) {		\
      return T_ERR;				\
    }						\
    flgs = lt & rt & (T_PARAM|T_STRICT);        \
    if ((lt & (f)) == (rt & (f))) {		\
      return ((g) & ~(T_PARAM|T_STRICT))|flgs;	\
    }						\
  } while (0)

#define EQUAL_LT_RT(f)	 EQUAL_LT_RT2((f),lt)

#define INT_OR_REAL						\
  do {								\
    lt = act_type_expr (s, e->u.e.l);				\
    rt = act_type_expr (s, e->u.e.r);				\
    if (lt == T_ERR || rt == T_ERR) {				\
      return T_ERR;						\
    }								\
    flgs = lt & rt & (T_PARAM|T_STRICT);			\
    if ((lt & (T_INT|T_REAL)) && (rt & (T_INT|T_REAL))) {	\
      if ((lt & T_REAL) || (rt & T_REAL)) {			\
	return T_REAL|flgs;					\
      }								\
      else {							\
	return T_INT|flgs;					\
      }								\
    }								\
  } while (0)
  
  
  switch (e->type) {
    /* Boolean, binary or int */
  case E_AND:
  case E_OR:
    EQUAL_LT_RT(T_BOOL|T_INT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs bool/bool or int/int", expr_operator_name (e->type));
    return T_ERR;
    break;

    /* Boolean, unary */
  case E_NOT:
    lt = act_type_expr (s, e->u.e.l);
    if (lt == T_ERR) return T_ERR;
    if (lt & T_BOOL) {
      return lt;
    }
    typecheck_err ("`~' operator applied to a non-Boolean");
    return T_ERR;
    break;

    /* Binary, real or integer */
  case E_PLUS:
  case E_MINUS:
  case E_MULT:
  case E_DIV:
  case E_MOD:
    INT_OR_REAL;
    /*EQUAL_LT_RT(T_INT|T_REAL);*/
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/real for both", expr_operator_name (e->type));
    /*typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int or real/real", expr_operator_name (e->type));*/
    return T_ERR;
    break;

  case E_LT:
  case E_GT:
  case E_LE:
  case E_GE:
    EQUAL_LT_RT2(T_INT|T_REAL,T_BOOL);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int or real/real", expr_operator_name (e->type));
    return T_ERR;

    /* Unary, real or integer */
  case E_UMINUS:
    lt = act_type_expr (s, e->u.e.l);
    if (lt == T_ERR) return T_ERR;
    if (lt & (T_REAL|T_INT)) {
      return lt;
    }
    typecheck_err ("`!' operator applied to a Boolean");
    return T_ERR;
    break;

    /* Binary, integer only */
  case E_LSL:
  case E_LSR:
  case E_ASR:
  case E_COMPLEMENT:
  case E_XOR:
    EQUAL_LT_RT(T_INT);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs int/int", expr_operator_name (e->type));
    return T_ERR;

    /* weird */
  case E_QUERY:
  case E_COLON:

    /* binary, Integer or Bool or real */
  case E_EQ:
  case E_NE:
    EQUAL_LT_RT2(T_INT|T_BOOL|T_REAL,T_BOOL);
    typecheck_err ("`%s': inconsistent/invalid types for the two arguments; needs real/real, int/int, or bool/bool", expr_operator_name (e->type));
    return T_ERR;

  case E_CONCAT:
    fatal_error ("Should not be here");
    do {
      lt = act_type_expr (s, e->u.e.l);
      if (lt == T_ERR) return T_ERR;
      if (!(lt & (T_INT|T_BOOL))) {
	typecheck_err ("{ } concat: all items must be int or bool");
      }
      if (e->u.e.r) {
	e = e->u.e.r;
      }
    } while (e->type == E_CONCAT);
    return T_INT;
    break;

  case E_BITFIELD:
    /* XXX: HERE */
    typecheck_err ("Bitfield! Implement me please");
    return T_ERR;

    /* UMMMM */
  case E_FUNCTION:
    typecheck_err ("Function! Implement me please");
    return T_ERR;
    break;

    /* leaves */
  case E_VAR:
    lt = act_type_var (s, (ActId *)e->u.e.l);
    return lt;
    break;

  case E_PROBE:
    /* XXX: if the variable is a channel, then */
    return T_BOOL;

  case E_REAL:
    return (T_REAL|T_PARAM|T_STRICT);
    break;

  case E_INT:
    return (T_INT|T_PARAM|T_STRICT);
    break;

  case E_TRUE:
  case E_FALSE:
    return (T_BOOL|T_PARAM|T_STRICT);
    break;

  case E_SELF:
    return T_SELF;
    break;

  default:
    typecheck_err ("`%d' is an unknown type for an expression", e->type);
    return T_ERR;
  }
  typecheck_err ("Should not be here");
  return T_ERR;
#undef EQUAL_LT_RT
}


static InstType *actual_insttype (Scope *s, ActId *id)
{
  InstType *it;

  it = s->FullLookup (id->getName());
  Assert (it, "This should have been caught earlier!");
  while (id->Rest()) {
      /* this had better be an array deref if there is a array'ed type
	 involved */
      if (it->arrayInfo()) {
	if (!id->arrayInfo()) {
	  typecheck_err ("Port `.%s' access for an arrayed type", id->getName ());
	  return NULL;
	}
	if (it->arrayInfo()->nDims () != id->arrayInfo()->nDims ()) {
	  typecheck_err ("Port `.%s': number of dimensions don't match type (%s v/s %s)", id->arrayInfo()->nDims(), it->arrayInfo()->nDims ());
	  return NULL;
	}
      }
      else {
	if (id->arrayInfo()) {
	  typecheck_err ("Array de-reference for a non-arrayed type, port `.%s'", id->getName ());
	  return NULL;
	}
      }
      UserDef *u;
      u = dynamic_cast<UserDef *>(it->BaseType ());
      Assert (u, "This should have been caught during parsing!");
      id = id->Rest ();
      it = u->Lookup (id);
  }
  /* we have insttype, and id: now handle any derefs */
  if (!id->arrayInfo ()) {
    /* easy case */
    return it;
  }
  if (!it->arrayInfo()) {
    typecheck_err ("Array de-reference for a non-arrayed type, port `.%s'", id->getName ());
    return NULL;
  }
  if (it->arrayInfo ()->nDims () != id->arrayInfo()->nDims ()) {
    typecheck_err ("Port `.%s': number of dimensions don't match type (%s v/s %s)", id->arrayInfo()->nDims (), it->arrayInfo()->nDims ());
    return NULL;
  }
  /* type is an array, there is a deref specified here */
  /* XXX: currently two cases handled correctly
       subrange specifier, full dimensions, return original type
       OR
       full deref
  */
  if (id->arrayInfo()->effDims() == id->arrayInfo()->nDims()) {
    InstType *it2 = new InstType (it, 1);
    it2->MkArray (id->arrayInfo()->Clone ());
    it = it2;
  }
  else if (id->arrayInfo()->effDims() == 0) {
    InstType *it2 = new InstType (it, 1);
    /* no array! */
    it = it2;
  }
  else {
    InstType *it2 = new InstType (it, 1);
    Array *a = NULL;
    for (int i=0; i < id->arrayInfo()->effDims(); i++) {
      if (!a) {
	a = new Array (const_expr (-1));
	a->mkArray ();
      }
      else {
	a->Concat (new Array (const_expr (-1)));
      }
    }
    it2->MkArray (a);
    it = it2;
  }
  return it;
}

InstType *act_expr_insttype (Scope *s, Expr *e)
{
  int ret;
  InstType *it;

  if (!e) {
    typecheck_err ("NULL expression??");
    return NULL;
  }
  if (e->type == E_VAR) {
    /* special case */
    it = actual_insttype (s, (ActId *)e->u.e.l);
    return it;
  }
  ret = act_type_expr (s, e);

  if (ret == T_ERR) {
    return NULL;
  }
  ret &= ~T_STRICT;
  if (ret == (T_INT|T_PARAM)) {
    return TypeFactory::Factory()->NewPInt();
  }
  else if (ret == T_INT) {
    return TypeFactory::Factory()->NewInt (s, Type::NONE, 0, const_expr (32));
  }
  else if (ret == (T_BOOL|T_PARAM)) {
    return TypeFactory::Factory()->NewPBool ();
  }
  else if (ret == T_BOOL) {
    return TypeFactory::Factory()->NewBool (Type::NONE);
  }
  else if (ret == (T_REAL|T_PARAM)) {
    return TypeFactory::Factory()->NewPReal ();
  }
  else {
    fatal_error ("Not sure what to do now");
  }
  return NULL;
}

static int stype_line_no, stype_col_no;
static char *stype_file_name;

void type_set_position (int l, int c, char *n)
{
  stype_line_no = l;
  stype_col_no = c;
  stype_file_name = n;
}

/* 
   are these two types compatible? connectable?
*/
int type_connectivity_check (InstType *lhs, InstType *rhs)
{
  struct act_position p;
  if (lhs == rhs) return 1;

#if 0
  printf ("check: ");
  lhs->Print (stdout);
  printf (" -vs- ");
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
#endif

  if (lhs->isExpanded ()) {
    /* the connectivity check is now strict */
    if (lhs->isEqual (rhs, 2)) {
      return 1;
    }
  }
  else {
    /* check dim compatibility */
    if (lhs->isEqual (rhs, 1)) {
      return 1;
    }
  }

  p.l = stype_line_no;
  p.c = stype_col_no;
  p.f = stype_file_name;

  act_parse_msg (&p, "Type checking failed in connection\n");
  fprintf (stderr, "\tTypes `");
  lhs->Print (stderr);
  fprintf (stderr, "' and `");
  rhs->Print (stderr);
  fprintf (stderr, "' are not compatible\n");
  exit (1);

  return 0;
}

InstType *AExpr::getInstType (Scope *s)
{
  InstType *cur, *tmp;
  AExpr *ae;

  switch (t) {
  case AExpr::CONCAT:
  case AExpr::COMMA:
    cur = l->getInstType (s);
    if (!cur) return NULL;
    ae = r;
    while (ae) {
      Assert (ae->GetLeft (), "Hmm");
      tmp = ae->GetLeft ()->getInstType (s);
      if (!tmp) {
	delete cur;
	return NULL;
      }
      if (!type_connectivity_check (cur, tmp)) {
	delete cur;
	delete tmp;
	return NULL;
      }
      delete tmp;
      ae = ae->GetRight ();
    }
    if (t == AExpr::CONCAT) {
      return cur;
    }
    else {
      Array *a;

      tmp = new InstType (cur, 1);
      a = cur->arrayInfo ();
      if (!a) {
	a = new Array (const_expr (1));
      }
      else {
	int i;
	i = a->nDims () + 1;
	a = new Array (const_expr (1));
	a->mkArray ();
	i--;
	for (; i > 0; i--) {
	  a->Concat (new Array (const_expr (1)));
	}
      }
      tmp->MkArray (a);

      return tmp;
    }
    break;

  case AExpr::EXPR:
    return act_expr_insttype (s, (Expr *)l);
    break;

#if 0
  case AExpr::SUBRANGE:
    fatal_error ("Should not be here");
    return actual_insttype (s, (ActId *)l);
#endif

  default:
    fatal_error ("Unimplemented?, %d", t);
    break;
  }
  return NULL;
}


int act_type_conn (Scope *s, AExpr *ae, AExpr *rae)
{
  InstType *it;
  int ret;

  Assert (s, "NULL scope");
  Assert (ae, "NULL AExpr");
  Assert (rae, "NULL AExpr");

  /*
  printf ("Here: checking: ");
  ae->Print (stdout);
  printf (" v/s ");
  rae->Print (stdout);
  printf ("\n");
  */

  InstType *lhs = ae->getInstType (s);
  if (!lhs) return T_ERR;

  InstType *rhs = rae->getInstType (s);
  if (!rhs) {
    delete lhs;
    return T_ERR;
  }
  
  /*
  printf ("Types: ");
  lhs->Print (stdout);
  printf (" -vs- ");
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
  */
  
  /* now check type compatibility between it, id, and rhs */
  ret = type_connectivity_check (lhs, rhs);

  /*
  printf ("Ok, here\n");
  fflush (stdout);
  */

  delete lhs;
  delete rhs;

  return ret;
}

int act_type_conn (Scope *s, ActId *id, AExpr *rae)
{
  InstType *it;
  int ret;

  Assert (s, "NULL scope");
  Assert (id, "NULL id");
  Assert (rae, "NULL AExpr");

  /*
  printf ("Here2: checking: ");
  id->Print (stdout);
  printf (" v/s ");
  rae->Print (stdout);
  printf ("\n");
  */

  InstType *lhs = actual_insttype (s, id);
  if (!lhs) return T_ERR;

  InstType *rhs = rae->getInstType (s);
  if (!rhs) {
    delete lhs;
    return T_ERR;
  }

  /*
  printf ("Types: "); fflush (stdout);
  lhs->Print (stdout);
  printf (" -vs- "); fflush (stdout);
  rhs->Print (stdout);
  printf ("\n");
  fflush (stdout);
  */

  /* now check type compatibility between it, id, and rhs */
  ret = type_connectivity_check (lhs, rhs);

  delete lhs;
  delete rhs;

  /*
  printf ("Ok, here2\n");
  fflush (stdout);
  */

  return ret;
}