/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"

#include "list_walk.h"

#if !defined(U3_CPU_DEBUG) && !defined(U3_MEMORY_DEBUG)
// Axes rooted at the sample traverse each distinct internal node once per
// row. Other axes may read the unchanged gate battery or lexical context.
// https://yfilter.cs.berkeley.edu/doc/yfilter-tods-2003.pdf
// Each split assigns adjacent registers to two borrowed fields. Constants
// borrow from the gate. Each constructed cell has one consumer.
enum { turn_splits = 63, turn_const = 128, turn_cell = 160, turn_pairs = 31 };

typedef struct {
  u3_noun vals[turn_cell + turn_pairs];
  c3_y children[2 * turn_splits + 1], parent[turn_splits];
  struct { c3_y left, right; } pairs[turn_pairs];
  c3_w splits, constants, cells, budget;
} _turn_layout;

static int
_turn_layout_field(_turn_layout* p, c3_w axis, c3_w bits)
{
  c3_y field = 0;
  while ( bits ) {
    c3_y child = p->children[field];
    if ( !child ) {
      if ( p->splits == turn_splits ) return -1;
      p->parent[p->splits] = field;
      child = 2 * p->splits++ + 1;
      p->children[field] = child;
    }
    field = child + ((axis >> --bits) & 1);
  }
  return field;
}

static int
_turn_layout_read(_turn_layout* p, u3_noun gate, u3_noun fol)
{
  u3_noun op, arg;
  if ( !p->budget || c3n == u3r_cell(fol, &op, &arg) ) return -1;
  --p->budget;
  if ( c3y == u3du(op) ) {
    int l = _turn_layout_read(p, gate, op);
    if ( l < 0 ) return -1;
    int r = _turn_layout_read(p, gate, arg);
    if ( r < 0 ) return -1;
    // Reading both children already proves their parent is a cell.
    if ( l < turn_const && (l & 1) && r == l + 1 ) {
      return p->parent[(l - 1) / 2];
    }
    if ( p->cells == turn_pairs ) return -1;
    p->pairs[p->cells].left = l;
    p->pairs[p->cells].right = r;
    return turn_cell + p->cells++;
  }
  if ( op == 0 && arg >= 2 && arg <= 0x7fffffff ) {
    c3_w depth = u3x_dep(arg);
    if ( depth >= 2 && (arg >> (depth - 2)) == 6 ) {
      return _turn_layout_field(p, arg, depth - 2);
    }
    // Axis 3 includes the sample. Every other nonsample axis below the
    // root selects part of the battery or context.
    if ( arg == 3 ) return -1;
    arg = u3r_at(arg, gate);
    if ( arg == u3_none ) return -1;
  }
  else if ( op != 1 ) return -1;
  if ( p->constants == turn_cell - turn_const ) return -1;
  p->vals[turn_const + p->constants] = arg;
  return turn_const + p->constants++;
}

static inline u3_noun
_turn_layout_take(_turn_layout* p, c3_y index)
{
  u3_noun value = p->vals[index];
  return index >= turn_cell ? value : u3k(value);
}

static __attribute__((noinline)) u3_weak
_turn_layout_run(u3_noun list, u3_noun gate)
{
  _turn_layout plan;
  plan.splits = plan.cells = plan.constants = 0;
  plan.budget = 2 * turn_pairs + 1;
  memset(plan.children, 0, sizeof(plan.children));
  int root = _turn_layout_read(&plan, gate, u3h(gate));
  if ( root < 0 ) return u3_none;

  u3_noun result, *end = &result;
  for ( u3_noun at = list; at; at = u3t(at) ) {
    if ( c3n == u3du(at) ) goto punt;
    plan.vals[0] = u3h(at);
    for ( c3_w i = 0; i < plan.splits; ++i ) {
      u3_noun n = plan.vals[plan.parent[i]];
      if ( c3n == u3du(n) ) goto punt;
      u3a_cell* c = u3a_to_ptr(n);
      plan.vals[2*i + 1] = c->hed;
      plan.vals[2*i + 2] = c->tel;
    }
    // A reconstruction of the whole row returns the original list after
    // every row passes its cell checks.
    if ( !root ) continue;
    for ( c3_w i = 0; i < plan.cells; ++i ) {
      u3_noun l = _turn_layout_take(&plan, plan.pairs[i].left);
      u3_noun r = _turn_layout_take(&plan, plan.pairs[i].right);
      plan.vals[turn_cell + i] = u3nc(l, r);
    }
    u3_noun value = _turn_layout_take(&plan, root);
    u3_noun *hed, *tel;
    *end = u3i_defcons(&hed, &tel);
    *hed = value;
    end = tel;
  }
  *end = root ? u3_nul : u3k(list);
  return result;
punt:
  *end = u3_nul;
  u3z(result);
  return u3_none;
}
#endif

u3_noun
u3qb_turn(u3_noun a, u3_noun b)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  if ( u3_nul != a ) {
    u3_noun*   hed;
    u3_noun*   tel;
    u3_noun   i, t;
    u3j_site sit_u;

    u3j_gate_prep(&sit_u, u3k(b));
#if !defined(U3_CPU_DEBUG) && !defined(U3_MEMORY_DEBUG)
    // Amortize the plan over at least eight rows.
    if ( !(u3C.wag_h & (u3o_trace | u3o_debug_cpu)) &&
         (sit_u.loc == u3_none || sit_u.jet_o == c3n) &&
         c3y == u3du(u3h(b)) && c3y == u3du(u3h(u3h(b))) &&
         u3r_at(511, a) != u3_none ) {
      pro = _turn_layout_run(a, b);
      if ( pro != u3_none ) {
        u3j_gate_lose(&sit_u);
        return pro;
      }
    }
#endif
    if ( c3y == u3j_gate_read_only(&sit_u) ) {
      pro = _walk_list(a, &sit_u, _walk_turn);
      u3j_gate_lose(&sit_u);
      return pro;
    }
    u3k(a);

    do {
      i = u3h(a);

      *lit = u3i_defcons(&hed, &tel);
      *hed = u3j_gate_slam(&sit_u, u3k(i));
      lit  = tel;

      t = u3k(u3t(a));
      u3z(a), a = t;
    }
    while ( u3_nul != a );

    u3j_gate_lose(&sit_u);
  }

  *lit = u3_nul;

  return pro;
}

u3_noun
u3wb_turn(u3_noun cor)
{
  u3_noun a, b;
  u3x_mean(cor, {u3x_sam_2, &a}, {u3x_sam_3, &b});
  return u3qb_turn(a, b);
}
