/*
 * This file is part of pkg.
 *
 * Copyright (C) 2012-2024 Mateusz Viste
 */

#ifndef loadrepolist_sentinel
#define loadrepolist_sentinel

struct customdirs {
  struct customdirs *next;
  char name[9];
  char location[1]; /* extended at alloc time */
};

/* Loads the list of custom directories from the config file, as well as the
 * bootdrive (defaults to 'C' if not found)
 * Returns 0 on success, or -1 on failure. */
int loadconf(const char *dosdir, struct customdirs **dirlist, char *bootdrive);

/* Free the memory allocated at configuration load. */
void freeconf(struct customdirs **dirlist);

#endif
