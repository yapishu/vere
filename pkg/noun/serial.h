/// @file

#ifndef U3_SERIAL_H
#define U3_SERIAL_H

#include "c3/c3.h"
#include "imprison.h"
#include "types.h"
    /*  constants
    */
      /* u3_dit_y: digit table for @ux/@uv/@uw.
      */
        extern const c3_y u3s_dit_y[64];

    /*  opaque handles
    */
      /* u3_cue_xeno: handle for cue-ing with an off-loom dictionary.
      */
        typedef struct _u3_cue_xeno u3_cue_xeno;

      /* u3_cue_view: index for reading fields from a jam on the home road.
      */
        typedef struct _u3_cue_view u3_cue_view;

      /* u3_jam_query: compiled patterns for searching jammed nouns.
      */
        typedef struct _u3_jam_query u3_jam_query;

    /*  Noun serialization. All noun arguments RETAINED.
    */

      /* u3s_jam_fib(): jam into a slab and return the length in bits. */
        c3_w
        u3s_jam_fib(u3i_slab* sab_u, u3_noun a);

      /* u3s_jam_xeno(): jam into an off-loom byte buffer.
      */
        c3_d
        u3s_jam_xeno(u3_noun a, c3_d* len_d, c3_y** byt_y);

      /* Jam after pre_y header bytes. Return jam bits and total buffer bytes. */
        c3_d
        u3s_jam_xeno_prefix(u3_noun a, c3_y pre_y, c3_d* len_d, c3_y** byt_y);

      /* u3s_cue(): cue [a]
      */
        u3_noun
        u3s_cue(u3_atom a);

      /* u3s_cue_view_new(): allocate an empty index.
      */
        u3_cue_view*
        u3s_cue_view_new(void);

      /* u3s_cue_view_open(): index a jam. The caller keeps the bytes live and
      ** immutable until the next open or free. A failed open clears the view.
      */
        c3_o
        u3s_cue_view_open(u3_cue_view* view, c3_d len, const c3_y* bytes);

      /* u3s_cue_view_at(): produce the fragment at axis, or u3_none if absent.
      */
        u3_weak
        u3s_cue_view_at(u3_cue_view* view, u3_atom axis);

      /* u3s_cue_view_free(): release the index.
      */
        void
        u3s_cue_view_free(u3_cue_view* view);

      /* u3s_jam_query_compile(): compile 1-64 patterns for use off the loom.
      ** Patterns are _, @, decimal uint64 atoms, %terms or [head tail].
      ** A query supports 62 distinct literal/pair rules and 255 states.
      ** Returns NULL and sets error to a static message on failure.
      */
        u3_jam_query*
        u3s_jam_query_compile(size_t count, const c3_c* const* patterns,
                              const c3_c** error);

      /* u3s_jam_query_scan(): set bit i if pattern i occurs anywhere in the
      ** noun. Return c3n and clear matches on an invalid jam.
      */
        c3_o
        u3s_jam_query_scan(const u3_jam_query* query, size_t len,
                           const c3_y* bytes, c3_d* matches);

      /* u3s_jam_query_free(): release the compiled query and its literals.
      */
        void
        u3s_jam_query_free(u3_jam_query* query);

      /* u3s_cue_xeno_init_with(): initialize a cue_xeno handle as specified.
      */
        u3_cue_xeno*
        u3s_cue_xeno_init_with(c3_d pre_d, c3_d siz_d);

      /* u3s_cue_xeno_init(): initialize a cue_xeno handle.
      */
        u3_cue_xeno*
        u3s_cue_xeno_init(void);

      /* u3s_cue_xeno_init(): cue on-loom, with off-loom dictionary in handle.
      */
        u3_weak
        u3s_cue_xeno_with(u3_cue_xeno* sil_u,
                          c3_d         len_d,
                          const c3_y*  byt_y);

      /* u3s_cue_xeno_init(): dispose cue_xeno handle.
      */
        void
        u3s_cue_xeno_done(u3_cue_xeno* sil_u);

      /* u3s_cue_xeno(): cue on-loom, with off-loom dictionary.
      */
        u3_weak
        u3s_cue_xeno(c3_d        len_d,
                     const c3_y* byt_y);

      /* u3s_cue_bytes(): cue bytes onto the loom.
      */
        u3_noun
        u3s_cue_bytes(c3_d len_d, const c3_y* byt_y);

      /* u3s_cue_atom(): cue atom.
      */
        u3_noun
        u3s_cue_atom(u3_atom a);

      /* u3s_cue_plan_read(): read a senior atom on a child road. Returns NULL
      ** outside the supported sizes. The child road owns all scratch.
      */
        u3_cue_plan*
        u3s_cue_plan_read(u3_atom a);

      /* u3s_cue_plan_take(): construct the checked product on the parent.
      ** The atom and the child heap must remain live until this returns.
      */
        u3_noun
        u3s_cue_plan_take(u3_cue_plan* plan, u3_atom a);

      /* u3s_etch_ud_smol(): c3_d to @ud
      **
      **   =(26 (met 3 (scot %ud (dec (bex 64)))))
      */
        c3_y*
        u3s_etch_ud_smol(c3_d a_d, c3_y hun_y[26]);

      /* u3s_etch_ud(): atom to @ud.
      */
        u3_atom
        u3s_etch_ud(u3_atom a);

      /* u3s_etch_ud_c(): atom to @ud, as a malloc'd c string.
      */
        size_t
        u3s_etch_ud_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_ux(): atom to @ux.
      */
        u3_atom
        u3s_etch_ux(u3_atom a);

      /* u3s_etch_ux_c(): atom to @ux, as a malloc'd c string.
      */
        size_t
        u3s_etch_ux_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_uv(): atom to @uv.
      */
        u3_atom
        u3s_etch_uv(u3_atom a);

      /* u3s_etch_uv_c(): atom to @uv, as a malloc'd c string.
      */
        size_t
        u3s_etch_uv_c(u3_atom a, c3_c** out_c);

      /* u3s_etch_uw(): atom to @uw.
      */
        u3_atom
        u3s_etch_uw(u3_atom a);

      /* u3s_etch_uw_c(): atom to @uw, as a malloc'd c string.
      */
        size_t
        u3s_etch_uw_c(u3_atom a, c3_c** out_c);

      /* u3s_sift_ud_bytes: parse @ud.
      */
        u3_weak
        u3s_sift_ud_bytes(c3_w len_w, c3_y* byt_y);

      /* u3s_sift_ud: parse @ud.
      */
        u3_weak
        u3s_sift_ud(u3_atom a);

#endif /* ifndef U3_SERIAL_H */
