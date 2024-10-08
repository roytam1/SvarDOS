/*
 *  This file is part of SvarDOS
 *  Copyright (C) 2012-2024 Mateusz Viste
 */

#ifndef pkginst_sentinel
#define pkginst_sentinel

#include "loadconf.h" /* required for struct customdirs */

#define PKGINST_UPDATE    2
#define PKGINST_HIDEWARN  4

int is_package_installed(const char *pkgname, const char *dosdir);
struct ziplist *pkginstall_preparepackage(char *pkgname, const char *localfile, unsigned char flags, FILE **zipfd, const char *dosdir, const struct customdirs *dirlist, char bootdrive);
int pkginstall_installpackage(const char *pkgname, const char *dosdir, const struct customdirs *dirlist, struct ziplist *ziplinkedlist, FILE *zipfd, char bootdrive, unsigned char *buff15k, unsigned char flags);

#endif
