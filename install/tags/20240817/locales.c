/*
 * This program creates a header file for the SvarDOS installer, that
 * contains the list of all supported keyboard layouts, along with a way to
 * select sets of keyboard layouts that apply to only a specific region.
 *
 * Copyright (C) 2016-2024 Mateusz Viste
 */

/* the kblayouts list is a NULL-terminated array that contains entries in the
 * following format:
 * human description string <0> layout code string <0> <codepage number as a 16bit value> <ega.sys file number as a single byte> <keyboard.sys file number as a single byte> <sub keyb ID as a 16bit value, zero if none>

    char *kblayouts[] = {
    "English (US)\0xxxx",
    "Polish\0pl\0xxxx"};
  };
 */


#include <stdio.h>
#include <string.h>


/* global file pointers */
FILE *fdkeyb, *fdoff;


/* Converts a positive base_10 into base_8 */
static unsigned int dec2oct(int n) {
  int res=0, digitPos=1;
  while (n) {
    res += (n & 7) * digitPos;
    n >>= 3;
    digitPos *= 10;
  }
  return(res);
}


static void addnew(char *countrycode, unsigned short countryid, char *humanlang, char *keybcode, unsigned short cp, unsigned char egafile, unsigned char keybfile, unsigned int subid) {
  static char lastcountry[4] = {0};
  static int curoffset = 0, curcountryoffset = 0;
  /* if new country, declare an offset */
  if (strcmp(countrycode, lastcountry) != 0) {
    /* close previous one, if any */
    if (lastcountry[0] != 0) {
      fprintf(fdoff, "#define OFFLEN_%s %d\r\n", lastcountry, curoffset - curcountryoffset);
    } else {
      fprintf(fdkeyb, "const char *kblayouts[] = {\r\n");
    }
    /* open new one, if any */
    if (countrycode[0] != 0) {
      fprintf(fdkeyb, "  /****** %s ******/\r\n", countrycode);
      curcountryoffset = curoffset;
      fprintf(fdoff, "#define OFFLOC_%s %d\r\n", countrycode, curoffset);
      strcpy(lastcountry, countrycode);
    } else {
      fprintf(fdoff, "#define OFFCOUNT %d\r\n", curoffset);
    }
  }
  /* */
  if (countrycode[0] != 0) {
    fprintf(fdkeyb, "  \"%s\\0%s\\0\\%d\\%d\\%d\\%d\\%d\\%d\\%d\\%d\",\r\n", humanlang, keybcode, dec2oct(cp >> 8), dec2oct(cp & 0xff), dec2oct(egafile), dec2oct(keybfile), dec2oct(subid >> 8), dec2oct(subid & 0xff), dec2oct(countryid >> 8), dec2oct(countryid & 0xff));
  } else {
    fprintf(fdkeyb, "  NULL};\r\n");
  }
  curoffset++;
}


int main(void) {

  /*** open files ***/
  fdkeyb = fopen("keylay.h", "wb");
  fdoff = fopen("keyoff.h", "wb");
  fprintf(fdkeyb, "/* DO NOT EDIT THIS FILE, IT IS AUTO-GENERATED BY LOCALES.EXE */\r\n");
  fprintf(fdoff, "/* DO NOT EDIT THIS FILE, IT IS AUTO-GENERATED BY LOCALES.EXE */\r\n");

  /******************* LAYOUTS LIST START *******************/

  /* addnew(countrycode, countryid, humanlang, keybcode, cp, egafile, keybfile, subid) */

  /* English */
  addnew("EN",  1, "English (US)", "en", 437, 0, 0, 0);
  addnew("EN", 44, "English (UK)", "uk", 437, 0, 1, 0);

  /* Armenian */
  addnew("HY", 374, "Armenian", "hy", 899, 6, 3, 0);

  /* Brazilian */
  addnew("BR", 55, "Brazilian", "br", 850, 1, 1, 0);
  addnew("BR", 55, "Brazilian (US layout)", "br274", 850, 1, 1, 0);

  /* Bulgarian */
  addnew("BG", 359, "Bulgarian", "bg", 872, 3, 2, 0);

  /* French */
  addnew("FR", 33, "French (France)", "fr", 858, 1, 1, 0);
  addnew("FR", 1, "French (Canada, standard)", "cf", 863, 9, 1, 0);
  addnew("FR", 1, "French (Canada, legacy)", "cf", 863, 9, 1, 501);
  addnew("FR", 41, "French (Switzerland)", "sf", 858, 1, 1, 0);

  /* German */
  addnew("DE", 49, "German", "de", 858, 1, 1, 0);

  /* Hungarian */
  addnew("HU", 36, "Hungarian", "hu", 852, 1, 1, 208);

  /* Italian */
  addnew("IT", 39, "Italian", "it", 858, 1, 1, 0);

  /* Latin-American */
  addnew("LA", 54, "Latin-American", "la", 437, 0, 1, 0);

  /* Norvegian */
  addnew("NO", 47, "Norvegian", "no", 858, 9, 1, 0);

  /* Polish */
  addnew("PL", 48, "Polish (Programmer)", "pl", 991, 10, 1, 0);
  addnew("PL", 48, "Polish (Typewriter)", "pl", 991, 10, 1, 214);

  /* Russian */
  addnew("RU", 7, "Russian (Standard)", "ru", 866, 3, 2, 0);
  addnew("RU", 7, "Russian (Typewriter)", "ru", 866, 3, 2, 443);

  /* Slovenian */
  addnew("SI", 386, "Slovenian", "si", 852, 1, 1, 0);

  /* Spanish */
  addnew("ES", 34, "Spanish", "es", 858, 1, 1, 0);

  /* Swedish */
  addnew("SV", 46, "Swedish", "sv", 858, 1, 1, 0);

  /* Turkish */
  addnew("TR", 90, "Turkish", "tr", 857, 1, 2, 0);

  /******************* LAYOUTS LIST STOP *******************/

  /* end of list - DO NOT REMOVE */
  addnew("", 0, "", "", 0, 0, 0, 0);

  /* close files */
  fclose(fdoff);
  fclose(fdkeyb);

  return(0);
}