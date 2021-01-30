/*
 * This file is part of pkginst (SvarDOS)
 * Copyright (C) 2013-2021 Mateusz Viste
 */

#ifndef showinst_h_sentinel
  #define showinst_h_sentinel
  void pkg_freeflist(struct flist_t *flist);
  struct flist_t *pkg_loadflist(char *pkgname, char *dosdir);
  void showinstalledpkgs(char *filterstr, char *dosdir);
  void listfilesofpkg(char *pkgname, char *dosdir);
#endif
