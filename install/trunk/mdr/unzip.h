/*
 * Function to iterate through files in a ZIP archive.
 *
 * This file is part of the Mateusz' DOS Routines (MDR): http://mdr.osdn.io
 * Published under the terms of the MIT License, as stated below.
 *
 * Copyright (C) 2012-2022 Mateusz Viste
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef MDR_UNZIP
#define MDR_UNZIP

#include <stdio.h> /* FILE * */

#define ZIP_FLAG_ISADIR    1
#define ZIP_FLAG_ENCRYPTED 2

#define ZIP_METH_STORE 0
#define ZIP_METH_DEFLATE 8

struct mdr_zip_item {
  unsigned long filelen;
  unsigned long compressedfilelen;
  unsigned long crc32;
  unsigned long dataoffset;    /* offset in the file where compressed data starts */
  unsigned long nextidxoffset; /* offset in the file of the next zip record, used by mdr_zip_iter() */
  unsigned short dosdate;      /* datestamp of the file (DOS packed format) */
  unsigned short dostime;      /* timestamp of the file (DOS packed format) */
  unsigned short compmethod;   /* compression method (ZIP_METH_xxx) */
  unsigned char flags;         /* see ZIP_FLAG_xxx above */
  char fname[256];             /* filename */
};

/* returns next item found in zip file. this is supposed to be called
 * iteratively, passing the previous mdr_zipitem struct each time (z must be
 * all zeroed out on first call).
 * returns 0 on success, neg on error, 1 on end of archive */
int mdr_zip_iter(struct mdr_zip_item *z, FILE *fd);

#endif
