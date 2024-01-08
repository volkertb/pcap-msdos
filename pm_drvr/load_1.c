#include "pmdrvr.h"

#ifndef __HIGHC__
#error This file is for Metaware HighC and Pharlap DOSX only
#endif

extern int call_child (LDEXP_BLK *ldblkp, ULONG num_params, ...);
#pragma Alias (call_child,"_@call_child")

#undef memset

int _dx_run_module (char *name, struct ModuleParams *mod)
{
  LDEXP_BLK  ldBlk;
  CONFIG_INF cfg;
  UCHAR     *mod_name;
  int        rc;

  if (!mod)
     return (-1);

  memset (mod, 0, sizeof(*mod));

  _dx_config_inf (&cfg,(UCHAR*)&cfg);  /* get config block */
  mod->parent_cs  = cfg.c_cs_sel;
  mod->parent_ds  = cfg.c_ds_sel;
  mod->loader_err = mod->dos_err = 0;
  mod_name = (UCHAR*)name;

  if (cfg.c_vmmf && !(cfg.c_flags1 & CF1_NOVM))
       rc = _dx_ld_novm (mod_name, &ldBlk,
                         &mod->loader_err, &mod->dos_err);
  else rc = _dx_ld_flat (mod_name, &ldBlk, 0, &mod->module_handle,
                         &mod->loader_err, &mod->dos_err);
  if (rc)
  {
    printf ("_dx_ld_flat(): error %d, dos-error %d\n",
            mod->loader_err, mod->dos_err);
    return (-1);
  }
  mod->child_cs = ldBlk.cs;
  mod->child_ds = ldBlk.ds;

#if 0
  printf ("Initial CS:EIP = %02X:%08Xh, SS:ESP = %02X:%08Xh,\n"
          "        DS = %02Xh, ES = %02Xh, GS = %02Xh, FS = %02Xh\n",
          ldBlk.cs, ldBlk.eip, ldBlk.ss, ldBlk.esp,
          ldBlk.ds, ldBlk.es,  ldBlk.gs, ldBlk.fs);
#endif

  mod->child_code = call_child (&ldBlk,sizeof(*mod)/sizeof(ULONG),*mod);

  return (0);
}

