/// @file
///
/// Build a home-road set from a canonical checkpoint. For an acyclic
/// noun graph, uniqueness of atom values and of pairs of child handles
/// implies structural uniqueness, by induction from the leaves. The walk
/// checks both conditions without rewriting the graph. If either fails,
/// the ordinary bottom-up walk canonicalizes the checkpoint.
///
/// Group cells by ranges of home buckets in the destination table itself.
/// Hash partitioning for cache locality: https://ir.cwi.nl/pub/11189/

#define _CM_RANGE  (16 * _CS_PER)
_Static_assert(_CM_RANGE < 255 && _CM_RANGE - 1 <= _CS_DIS_MAX,
               "range offsets and probe distances fit in a byte");

typedef struct {
  c3_y*    num_y;  //  entries staged in each range
  u3_noun* ext;    //  entries that overflow their range, in the result yard
  size_t   len_i;
  c3_d     cel_d;  //  distinct cell handles seen
  c3_t     ok_t;
  c3_t     key_t;  //  walk only the key of a bytecode entry
} _cm_build;

static c3_t
_cm_build_spill(_cm_build* bil_u, u3_noun som)
{
  //  Bound scratch use even for a stale size hint or skewed mugs.
  //
  if ( bil_u->len_i >= (_cm_sole_u.cel_u.cap_d / 16 + _CM_RANGE) ) return 0;
  u3_noun* ent = u3a_yard_take(&u3a_Yard_res, sizeof(*ent), __alignof__(u3_noun));
  if ( !ent ) return 0;
  if ( !bil_u->len_i ) bil_u->ext = ent;
  *ent = som;
  bil_u->len_i++;
  return 1;
}

/* Stage a key and its local home bucket in the next slot of its range.
*/
static c3_t
_cm_build_cell(_cm_build* bil_u, u3_noun som, c3_h mug_h)
{
  _cs_tab* tab_u = &_cm_sole_u.cel_u;
  c3_d     hom_d = _cs_home(tab_u, mug_h);
  c3_d     ran_d = hom_d / _CM_RANGE;
  c3_y     num_y = bil_u->num_y[ran_d];
  c3_d     bas_d = ran_d * _CM_RANGE;
  c3_d     pos_d = bas_d + num_y;

  bil_u->cel_d++;
  if ( (_CM_RANGE == num_y) || (pos_d >= tab_u->cap_d) ) {
    return _cm_build_spill(bil_u, som);
  }
  _cs_key_put(tab_u, pos_d, _cs_member(tab_u, som, mug_h));
  *_cs_dis(tab_u, pos_d) = (c3_y)(hom_d - bas_d);
  bil_u->num_y[ran_d]++;
  return 1;
}

static c3_t
_cm_build_walk(_cm_build* bil_u, u3_noun som)
{
  size_t bot_i = u3a_Yard_wok.use_i;

  while ( 1 ) {
    if ( c3n == u3a_is_cat(som) ) {
      c3_w idx_w = _cm_sole_idx(som);
      if ( !_cm_vis_bit(_cm_vis.mar_w, idx_w) ) {
        c3_w mug_w = ((u3a_noun*)u3a_to_ptr(som))->mug_w;
        if ( !mug_w || (mug_w >> 31) || !_cs_reaches(som) ) break;
        _cm_vis_bit_set(_cm_vis.mar_w, idx_w);

        if ( c3y == u3a_is_cell(som) ) {
          u3a_cell* cel_u = u3a_to_ptr(som);
          if ( !_cm_build_cell(bil_u, som, (c3_h)mug_w) ) break;
          if ( c3y == u3a_is_cat(cel_u->hed) ) {
            som = cel_u->tel;
          }
          else {
            if ( c3n == u3a_is_cat(cel_u->tel) ) {
              _cm_frame* fam_u = _cm_walk_push();
              if ( !fam_u ) break;
              *fam_u = cel_u->tel;
            }
            som = cel_u->hed;
          }
          continue;
        }
        else {
          c3_d pos_d;
          if ( som != _cs_put(&_cm_sole_u.vat_u, som, (c3_h)mug_w,
                              _cm_cs_atom_eq, &pos_d) ) break;
        }
      }
    }
    if ( bot_i == u3a_Yard_wok.use_i ) return 1;
    som = *_cm_walk_peek();
    u3a_yard_give(&u3a_Yard_wok, sizeof(som));
  }

  u3a_yard_back(&u3a_Yard_wok, bot_i);
  return 0;
}

static void
_cm_build_kev(u3_noun kev, void* ptr_v)
{
  _cm_build* bil_u = ptr_v;
  if ( bil_u->ok_t ) {
    bil_u->ok_t = _cm_build_walk(bil_u, u3h(kev)) &&
                  (bil_u->key_t || _cm_build_walk(bil_u, u3t(kev)));
  }
}

/* Visit home buckets in order. Each key goes in the first free slot
** at or beyond its home, exactly the order of a Robin Hood run. Keys
** crossing the range boundary go through ordinary insertion afterwards.
*/
static c3_t
_cm_build_range(_cm_build* bil_u, _cs_tab* tab_u, c3_d bas_d)
{
  c3_y hed_y[_CM_RANGE], nex_y[_CM_RANGE];
  c3_w key_w[_CM_RANGE];
  c3_w len_w = (c3_w)c3_min((c3_d)_CM_RANGE, tab_u->cap_d - bas_d);
  c3_w num_w = bil_u->num_y[bas_d / _CM_RANGE];
  c3_w pos_w = 0;

  memset(hed_y, 255, sizeof(hed_y));
  for ( c3_w i_w = 0; i_w < num_w; i_w++ ) {
    c3_y hom_y = *_cs_dis(tab_u, bas_d + i_w);
    key_w[i_w] = _cs_entry(tab_u, bas_d + i_w);
    nex_y[i_w] = hed_y[hom_y];
    hed_y[hom_y] = (c3_y)i_w;
  }
  memset(&tab_u->lin_u[bas_d / _CS_PER], 0, len_w / _CS_PER * sizeof(_cs_line));

  for ( c3_w hom_w = 0; hom_w < len_w; hom_w++ ) {
    pos_w = c3_max(pos_w, hom_w);
    for ( c3_y i_y = hed_y[hom_w]; 255 != i_y; i_y = nex_y[i_y] ) {
      c3_w    ent_w = key_w[i_y];
      u3_noun som   = _cs_noun(tab_u, ent_w);
      for ( c3_y j_y = nex_y[i_y]; 255 != j_y; j_y = nex_y[j_y] ) {
        c3_w alt_w = key_w[j_y];
        if ( !((ent_w ^ alt_w) & _cs_tag(tab_u, ent_w) & _cs_tag(tab_u, alt_w)) &&
             _canon_cmp_cells(som, _cs_noun(tab_u, alt_w)) ) return 0;
      }
      if ( pos_w == len_w ) {
        if ( !_cm_build_spill(bil_u, som) ) return 0;
      }
      else {
        c3_y dis_y = (c3_y)(pos_w - hom_w);
        _cs_key_put(tab_u, bas_d + pos_w, ent_w);
        *_cs_dis(tab_u, bas_d + pos_w) = dis_y;
        tab_u->dmx_y = c3_max(tab_u->dmx_y, dis_y);
        tab_u->len_d++;
        pos_w++;
      }
    }
  }
  return 1;
}

/* An unmugged root already requires the bottom-up walk. Check the
** table entries before touching the graph under them.
*/
static void
_cm_build_check(u3_noun kev, void* ptr_v)
{
  _cm_build* bil_u = ptr_v;
  u3_noun    hed = u3h(kev), tel = u3t(kev);

  if ( ((c3n == u3a_is_cat(hed)) && !((u3a_noun*)u3a_to_ptr(hed))->mug_w) ||
       (!bil_u->key_t && (c3n == u3a_is_cat(tel)) &&
        !((u3a_noun*)u3a_to_ptr(tel))->mug_w) ) {
    bil_u->ok_t = 0;
  }
}

static c3_t
_cm_sole_build(u3p(u3h_root)* roo_p)
{
  _cs_tab*  tab_u = &_cm_sole_u.cel_u;
  size_t    end_i = u3a_yard_here(&u3a_Yard_res);
  size_t    num_i = (tab_u->cap_d + _CM_RANGE - 1) / _CM_RANGE;
  _cm_build bil_u = { .ok_t = 1 };

  for ( c3_w i_w = 0; i_w < 7; i_w++ ) {
    bil_u.key_t = (2 == i_w);
    u3h_walk_with(roo_p[i_w], _cm_build_check, &bil_u);
    if ( !bil_u.ok_t ) return 0;
  }
  bil_u.num_y = u3a_yard_take(&u3a_Yard_res, num_i, 1);
  if ( !bil_u.num_y ) return 0;
  memset(bil_u.num_y, 0, num_i);

  for ( c3_w i_w = 0; bil_u.ok_t && (i_w < 7); i_w++ ) {
    c3_d pre_d = bil_u.cel_d;
    bil_u.key_t = (2 == i_w);
    u3h_walk_with(roo_p[i_w], _cm_build_kev, &bil_u);
    u3e_Meter.hcc_d[i_w] = bil_u.cel_d - pre_d;
  }
  //  An underestimated size hint can require growth. Only the ordinary
  //  table can resize: its staged keys still carry local home buckets.
  //
  if ( bil_u.cel_d * 100 > tab_u->cap_d * _CS_LIM ) bil_u.ok_t = 0;
  for ( c3_d bas_d = 0; bil_u.ok_t && (bas_d < tab_u->cap_d); bas_d += _CM_RANGE ) {
    bil_u.ok_t = _cm_build_range(&bil_u, tab_u, bas_d);
  }
  for ( size_t i_i = 0; bil_u.ok_t && (i_i < bil_u.len_i); i_i++ ) {
    u3_noun som   = bil_u.ext[i_i];
    c3_h    mug_h = (c3_h)((u3a_cell*)u3a_to_ptr(som))->mug_w;
    c3_d    pos_d;
    bil_u.ok_t = (som == _cs_put(tab_u, som, mug_h, _cm_cs_cell_eq, &pos_d));
  }
  u3a_yard_back(&u3a_Yard_res, end_i);

  if ( bil_u.ok_t ) {
    //  The private reachability bitmap is now the exact membership map.
    //
    c3_w* map_w = u3a_Sole_w;
    u3a_Sole_w = _cm_vis.mar_w;
    _cm_vis.mar_w = map_w;
  }
  else {
    //  Drop the temporary marks without faulting in unused bitmap pages.
    //
    c3_free(_cm_vis.mar_w);
    _cm_vis.mar_w = c3_calloc(sizeof(c3_w) * _cm_vis.mal_w);
    _cs_reset(&_cm_sole_u.vat_u);
    _cs_reset(tab_u);
  }
  return bil_u.ok_t;
}

#undef _CM_RANGE
