/*************************************************************************
 *
 *  Copyright (c) 2011 Rajit Manohar
 *  All Rights Reserved
 *
 **************************************************************************
 */
#include <iostream>
#include <act/namespaces.h>
#include <act/types.h>
#include <act/inst.h>
#include <string.h>
#include "misc.h"

ActNamespace *ActNamespace::global = NULL;
int ActNamespace::creating_global = 0;

/**
 * Initialize namespaces. This function creates the ``Global''
 * namespace, and initializes static member function global
 */
void ActNamespace::Init ()
{
  if (global) {
    /* this function has already been called */
    return;
  }
  else {
    creating_global = 1;
    global = new ActNamespace ("Global");
    creating_global = 0;
  }
}
      
void ActNamespace::_init (ActNamespace *ns, const char *s)
{
  if (creating_global) {
    /* skip the checks */
  }
  else {
    ActNamespace::Init();
    if (!ns) {
      ns = global;
    }
    if (ns->findNS (s)) {
      fatal_error ("Cannot create duplicate namespace `%s'\n", s);
    }
  }
  N = hash_new (2);
  if (!creating_global) {
    Assert (ns, "This should not happen");
    Link (ns, s);
  }
  else {
    parent = ns;
  }
  T = hash_new (4);
  xT = hash_new (4);
  if (creating_global) {
    I = new Scope  (NULL);
  }
  else {
    I = new Scope(ns->CurScope());
  }
  B = NULL;
  exported = 0;
}

void ActNamespace::AppendBody (ActBody *b)
{
  if (B) { 
    B->Append (b); 
  } else { 
    B = b;
  }
}


/**
 * Create a new namespace within the specified namespace
 */
ActNamespace::ActNamespace (ActNamespace *ns, const char *s)
{
  _init (ns, s);
}

/**
 *  Create a new namespace within the global namespace
 */
ActNamespace::ActNamespace (const char *s)
{
  _init (NULL, s);
}


/**
 * Find a namespace in the current context
 */
ActNamespace *ActNamespace::findNS (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (N, s);
  if (b) {
    return (ActNamespace *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Unlink namespace
 */
void ActNamespace::Unlink ()
{
  ActNamespace *ns = parent;

  Assert (ns, "No parent to this namespace?");
  hash_delete (ns->N, self_bucket->key);
  self_bucket = NULL;
  parent = NULL;
}


/**
 * Link namespace to parent
 */
void ActNamespace::Link (ActNamespace *up, const char *name)
{
  self_bucket = hash_add (up->N, name);
  self_bucket->v = this;
  parent = up;
}


/**
 * String name
 */
char *ActNamespace::Name ()
{
  int sz = 1, len;
  ActNamespace *ns;
  char *ret;

  ns = this;

  if (ns == global) {
    return Strdup ("::");
  }
  
  while (ns->parent) {
    sz += strlen (ns->self_bucket->key);
    sz += 2;
    ns = ns->parent;
  }

  MALLOC (ret, char, sz);
  ns = this;
  ret[sz-1] = '\0';
  sz--;
  while (ns->parent) {
    len = strlen (ns->self_bucket->key);
    strncpy (&ret[sz]-len, ns->self_bucket->key, len);
    sz -= len;
    ret[--sz] = ':';
    ret[--sz] = ':';
    ns = ns->parent;
  }
  Assert (sz == 0, "Eh?");
  return ret;
}

/**
 * Functions for class ActOpen
 *
 */

ActOpen::ActOpen ()
{
  search_path = list_new ();
}


ActOpen::~ActOpen ()
{
  list_free (search_path);
}


int ActOpen::Open(ActNamespace *ns, const char *newname)
{
  listitem_t *li;

  if (newname) {
    if (ActNamespace::Global()->findNS (newname)) {
      return 0;
    }
    ns->Unlink ();
    ns->Link (ActNamespace::Global(), newname);
  }
  else {
    /* add to the search path */
    for (li = list_first (search_path); li; li = list_next (li)) {
      if (list_value (li) == (void *)ns)
	return 1;
    }
    stack_push (search_path, ns);
  }
  return 1;
}


/**
 * Find namespace in the current context, given the context of opens
 *
 */
ActNamespace *ActOpen::find (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  if (cur) {
    ns = cur->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    Assert (ns, "What?");
    ns = ns->findNS (s);
    if (ns) {
      return ns;
    }
  }

  /* Look in the global namespace */
  ns = ActNamespace::Global()->findNS (s);

  return ns;
}

/**
 * Find namespace containing the type in the current context, given
 * the context of opens
 *
 */
ActNamespace *ActOpen::findType (ActNamespace *cur, const char *s)
{
  listitem_t *li;
  ActNamespace *ns;

  /* Search in the current namespace */
  while (cur) {
    if (cur->findType (s)) {
      return cur;
    }
    cur = cur->Parent ();
  }

  /* Search in the path */
  Assert (search_path, "Constructor didn't get called?");
  for (li = list_first (search_path); li; li = list_next (li)) {
    ns = (ActNamespace *) list_value (li);
    if (ns->findType (s)) {
      return ns;
    }
  }

  /* Look in the global namespace */
  if (ActNamespace::Global()->findType (s)) {
    return ActNamespace::Global();
  }
  
  return NULL;
}


/**
 * Find a type in the current namespace
 */
UserDef *ActNamespace::findType (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    return (UserDef *)b->v;
  }
  else {
    return NULL;
  }
}


/**
 * Find an instance in the current namespace 
 */
InstType *ActNamespace::findInstance (const char *s)
{
  return I->Lookup (s);
}


/**
 * Create a new type
 */
int ActNamespace::CreateType (const char *s, UserDef *u)
{
  hash_bucket_t *b;

  b = hash_lookup (T, s);
  if (b) {
    /* failure: name already exists */
    return 0;
  }
  else {
    b = hash_add (T, s);
    b->v = u;
    u->setName (b->key);

    return 1;
  }
}


/**
 *  Check if a name is free
 */
int ActNamespace::findName (const char *s)
{
  if (findNS (s)) {
    return 2;
  }
  else if (findType (s)) {
    return 1;
  }
  else if (findInstance (s)) {
    return 3;
  }
  return 0;
}


Scope::Scope (Scope *parent, int is_expanded)
{
  expanded = is_expanded;
  H = hash_new (2);
  u = NULL;
  up = parent;

  A_INIT (vpint);
  A_INIT (vpints);
  A_INIT (vpreal);
  A_INIT (vptype);
  vpbool = NULL;

  vpint_set = NULL;
  vpints_set = NULL;
  vpreal_set = NULL;
  vptype_set = NULL;
  vpbool_set = NULL;

}

InstType *Scope::Lookup (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (b) {
    if (!expanded) {
      return (InstType *)b->v;
    }
    else {
      return ((ValueIdx *)b->v)->t;
    }
  }
  else {
    return NULL;
  }
}

ValueIdx *Scope::LookupVal (const char *s)
{
  hash_bucket_t *b;

  if (!expanded) {
    return NULL;
  }

  b = hash_lookup (H, s);
  if (!b) {
    return NULL;
  }
  return (ValueIdx *)b->v;
}

InstType *Scope::FullLookup (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (b) {
    if (!expanded) {
      return (InstType *)b->v;
    }
    else {
      return ((ValueIdx *)b->v)->t;
    }
  }
  else {
    if (up) {
      return up->FullLookup (s);
    }
    return NULL;
  }
}


Scope::~Scope ()
{
  hash_free (H);
  H = NULL;

  A_FREE (vpint);
  A_FREE (vpints);
  A_FREE (vpreal);
  A_FREE (vptype);
  if (vpbool) { bitset_free (vpbool); }
  if (vpint_set) { bitset_free (vpint_set); }
  if (vpints_set) { bitset_free (vpints_set); }
  if (vpreal_set) { bitset_free (vpreal_set); }
  if (vptype_set) { bitset_free (vptype_set); }
  if (vpbool_set) { bitset_free (vpbool_set); }
}

InstType *Scope::Lookup (ActId *id, int err)
{
  InstType *it;
  Scope *s;

  s = this;
  it = s->Lookup (id->getName ());
  if (!it) { 
    return NULL; 
  }
  if (err) {
    if (id->Rest()) {
      fatal_error ("Illegal call to Scope::Lookup() with dotted identifier");
    }
  }
  return it;
}

/**
 *  Add a new identifier to the scope.
 *
 *  @param s is a string corresponding to the identifier being added
 *  to the scope
 *  @param it is the instantiation type 
 *
 *  @return 0 on a failure, 1 on success.
 */
int Scope::Add (const char *s, InstType *it)
{
  hash_bucket_t *b;
  
  if (Lookup (s)) {
    /* failure */
    return 0;
  }

  b = hash_add (H, s);

  if (expanded == 0) {
    b->v = it;
  }
  else {
    ValueIdx *v = new ValueIdx;

    if (it->isExpanded() == 0) {
      fatal_error ("Scope::Add(): Scope is expanded, but instance type is not!");
    }
    
    v->t = it;
    v->init = 0;
    b->v = v;
  }
  return 1;
}

void Scope::Del (const char *s)
{
  hash_bucket_t *b;

  b = hash_lookup (H, s);
  if (!b) {
    fatal_error ("Del() called, failed!");
  }
  if (expanded) {
    ValueIdx *v = (ValueIdx *)b->v;
    delete v;
  }
  hash_delete (H, s);
}

/**
 *  Flush the table
 */
void Scope::FlushExpand ()
{
  if (expanded) {
    int i;
    hash_bucket_t *b;
    ValueIdx *v;
    
    for (i=0; i < H->size; i++) {
      for (b = H->head[i]; b; b = b->next) {
	v = (ValueIdx *)b->v;
	delete v;
      }
    }
    A_FREE (vpint);
    A_FREE (vpints);
    A_FREE (vpreal);
    A_FREE (vptype);
    if (vpbool) { bitset_free (vpbool); }
    if (vpint_set) { bitset_free (vpint_set); }
    if (vpints_set) { bitset_free (vpints_set); }
    if (vpreal_set) { bitset_free (vpreal_set); }
    if (vptype_set) { bitset_free (vptype_set); }
    if (vpbool_set) { bitset_free (vpbool_set); }
  }
  hash_clear (H);
  expanded = 1;

  /* value storage */
  A_INIT (vpint);
  A_INIT (vpints);
  A_INIT (vpreal);
  A_INIT (vptype);
  vpbool = NULL;

  vpint_set = NULL;
  vpints_set = NULL;
  vpreal_set = NULL;
  vptype_set = NULL;
  vpbool_set = NULL;
}

/**
 * Merge in instances from another scope
 */
void Scope::Merge (Scope *s)
{
  if (expanded && !s->expanded) {
    fatal_error ("Scope::Merge(): can't merge expanded scope into unexpanded one");
  }
  if (!expanded && s->expanded) {
    fatal_error ("Scope::Merge(): can't merge unexpanded scope into expanded one");
  }
  
  int i;
  hash_bucket_t *b, *tmp;

  for (i=0; i < s->H->size; i++) {
    for (b = s->H->head[i]; b; b = b->next) {
      if (hash_lookup (H, b->key)) {
	fatal_error ("Scope::Merge(): id `%s' already exists!", b->key);
      }
      tmp = hash_add (H, b->key);
      tmp->v = b->v;
    }
  }
}



/*------------------------------------------------------------------------
 *
 * Expand a namespace
 *
 *------------------------------------------------------------------------
 */
void ActNamespace::Expand ()
{
  ActBody *b;
  int i;
  ActNamespace *ns;
  hash_bucket_t *bkt;

  if (this == global) {
    act_error_push ("::<Global>", NULL, 0);
  }
  else {
    act_error_push (getName(), NULL, 0);
  }

  Assert (I, "No scope?");

  /* flush the scope, and re-create it! */
  I->FlushExpand ();

  /* 1. Expand all meta parameters at the top level of the namespace. */
  for (b = B; b; b = b->Next ()) {
    b->Expand (this, I, 1 /* meta-only */);
  }

  /* 2. Expand all namespaces that are nested within me */
  for (i=0; i < N->size; i++) {
    for (bkt = N->head[i]; bkt; bkt = bkt->next) {
      ns = (ActNamespace *) bkt->v;
      ns->Expand ();
    }
  }

  /* 3. Expand the rest. Note that expanding meta parameters is
     idempotent for a namespace. */
  for (b = B; b; b = b->Next()) {
    b->Expand (this, I);
  }

  act_error_pop ();
}


/*------------------------------------------------------------------------
 *
 *   Functions to handle parameter values in a scope
 *
 *------------------------------------------------------------------------
 */

/**----- pint -----**/

unsigned long Scope::AllocPInt(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPInt(): count must be >0!");
  }
  A_NEWP (vpint, unsigned long, count);
  ret = A_LEN (vpint);
  if (!vpint_set) { vpint_set = bitset_new (count); }
  else {
    bitset_expand (vpint_set, ret + count);
  }
  A_LEN (vpint) += count;
  return ret;
}

void Scope::setPInt(unsigned long id, unsigned long val)
{
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  vpint[id] = val;
  bitset_set (vpint_set, id);
}

int Scope::issetPInt(unsigned long id)
{
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  return bitset_tst (vpint_set, id);
}

unsigned long Scope::getPInt(unsigned long id)
{
  if (id >= A_LEN (vpint)) {
    fatal_error ("Scope::setPInt(): invalid identifier!");
  }
  return vpint[id];
}

/**----- pints -----**/

unsigned long Scope::AllocPInts(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPInts(): count must be >0!");
  }
  A_NEWP (vpints, long, count);
  ret = A_LEN (vpints);
  if (!vpints_set) { vpints_set = bitset_new (count); }
  else {
    bitset_expand (vpints_set, A_LEN (vpints)+count);
  }
  A_LEN (vpints) += count;
  return A_LEN (vpints)-1;
}

void Scope::setPInts(unsigned long id, long val)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  vpints[id] = val;
  bitset_set (vpints_set, id);
}

int Scope::issetPInts(unsigned long id)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  return bitset_tst (vpints_set, id);
}

long Scope::getPInts(unsigned long id)
{
  if (id >= A_LEN (vpints)) {
    fatal_error ("Scope::setPInts(): invalid identifier!");
  }
  return vpints[id];
}

/**----- preal -----**/

unsigned long Scope::AllocPReal(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPReal(): count must be >0!");
  }
  A_NEWP (vpreal, double, count);
  ret = A_LEN (vpreal);
  if (!vpreal_set) { vpreal_set = bitset_new (count); }
  else {
    bitset_expand (vpreal_set, A_LEN (vpreal)+count);
  }
  A_LEN (vpreal) += count;
  return ret;
}

void Scope::setPReal(unsigned long id, double val)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPPReal(): invalid identifier!");
  }
  vpreal[id] = val;
  bitset_set (vpreal_set, id);
}

int Scope::issetPReal(unsigned long id)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPReal(): invalid identifier!");
  }
  return bitset_tst (vpreal_set, id);
}

double Scope::getPReal(unsigned long id)
{
  if (id >= A_LEN (vpreal)) {
    fatal_error ("Scope::setPReal(): invalid identifier!");
  }
  return vpreal[id];
}



/**----- ptype -----**/

unsigned long Scope::AllocPType(int count)
{
  unsigned long ret;
  if (count <= 0) {
    fatal_error ("Scope::AllocPType(): count must be >0!");
  }
  A_NEWP (vptype, InstType *, count);
  ret = A_LEN (vptype);
  if (!vptype_set) { vptype_set = bitset_new (count); }
  else {
    bitset_expand (vptype_set, A_LEN (vptype)+count);
  }
  A_LEN (vptype) += count;
  return ret;
}

void Scope::setPType(unsigned long id, InstType *val)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPPType(): invalid identifier!");
  }
  vptype[id] = val;
  bitset_set (vptype_set, id);
}

int Scope::issetPType(unsigned long id)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPType(): invalid identifier!");
  }
  return bitset_tst (vptype_set, id);
}

InstType *Scope::getPType(unsigned long id)
{
  if (id >= A_LEN (vptype)) {
    fatal_error ("Scope::setPType(): invalid identifier!");
  }
  return vptype[id];
}


/**----- pbool -----**/

unsigned long Scope::AllocPBool(int count)
{
  if (count <= 0) {
    fatal_error ("Scope::AllocPBool(): count must be >0!");
  }
  if (!vpbool) {
    vpbool = bitset_new (count);
    vpbool_set = bitset_new (count);
    vpbool_len = count;
    return 0;
  }
  else {
    bitset_expand (vpbool, vpbool_len + count);
    bitset_expand (vpbool_set, vpbool_len + count);
    vpbool_len += count;
    return vpbool_len - count;
  }
}

void Scope::setPBool(unsigned long id, int val)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  if (val) {
    bitset_set (vpbool, id);
  }
  else {
    bitset_clr (vpbool, id);
  }
  bitset_set (vpreal_set, id);
}

int Scope::issetPBool(unsigned long id)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  return bitset_tst (vpbool_set, id);
}

int Scope::getPBool(unsigned long id)
{
  if (id >= vpbool_len) {
    fatal_error ("Scope::setPBool(): invalid identifier!");
  }
  return bitset_tst (vpbool, id);
}
