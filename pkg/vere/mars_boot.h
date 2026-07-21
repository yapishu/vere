#ifndef U3_VERE_MARS_BOOT_H
#define U3_VERE_MARS_BOOT_H

#include "c3/c3.h"
#include "noun.h"

#include <sys/time.h>

  /** Data types.
  **/
    /* u3_mars_boot_meta: diskless boot metadata.
    */
      typedef struct _u3_mars_boot_meta {
        c3_w ver_w;                       //  event-log version
        c3_d who_d[2];                    //  identity
        c3_o fak_o;                       //  fake bit
        c3_w lif_w;                       //  lifecycle length
      } u3_mars_boot_meta;

    /* u3_mars_boot_opts: diskless boot parameters.
    */
      typedef struct _u3_mars_boot_opts {
        c3_w           eny_w[16];         //  entropy
        c3_o           veb_o;             //  verbose
        c3_o           lit_o;             //  lite
        c3_o           sev_l;             //  instance number
        struct timeval tim_u;             //  time
        struct {                          //  kelvin
          c3_m         nam_m;             //    label
          c3_w         ver_w;             //    version
        } ver_u;
      } u3_mars_boot_opts;

  /** Functions.
  **/
    /* u3_mars_boot_make(): construct timestamped boot event list.
    */
      c3_o
      u3_mars_boot_make(u3_mars_boot_opts* inp_u,
                        u3_noun           com,
                        u3_noun*          ova,
                        u3_noun*          xac,
                        u3_mars_boot_meta* met_u);

#endif /* ifndef U3_VERE_MARS_BOOT_H */
