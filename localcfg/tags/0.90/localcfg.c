/*
 * Locales configuration for DOS
 * Copyright (C) Mateusz Viste 2015
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */

#include <stdio.h>
#include <stdlib.h> /* atoi() */
#include <string.h> /* strchr */

#include "country.h"

#define PVER "0.90"
#define PDATE "2015"


static void about(void) {
  puts("localcfg v" PVER " - locales configuration for DOS\n"
       "Copyright (C) Mateusz Viste 2015 / http://localcfg.sourceforge.net\n"
       "\n"
       "localcfg creates a custom COUNTRY.SYS-like file matching your preferences.\n"
       "\n"
       "usage: localcfg myprefs.sys [options]\n"
       "\n"
       "options allow to configure country locales to your likening, as follows:\n"
       "  /country:XX indicates your country code is XX (1 for USA, 33 for France, etc)\n"
       "  /cp:XXX     adapts country data for codepage XXX (example: '437')\n"
       "  /decim:X    reconfigures the decimal symbol to be 'X'");
  puts("  /thous:X    reconfigures the thousands symbol to be 'X'\n"
       "  /datesep:X  sets the date separator to 'X' (for example '/')\n"
       "  /datefmt:X  sets the date format, can be: MDY, DMY or YMD\n"
       "  /timesep:X  sets the time separator to 'X' (for example ':')\n"
       "  /timefmt:X  sets the time format: 0=12h with AM/PM or 1=24h\n"
       "  /curr:XXX   sets the currency to XXX (a string of 1 to 4 characters)\n"
       "  /currpos:X  sets the currency symbol position to X, where X is either");
  puts("              0=currency precedes the value, 1=currency follows the value and\n"
       "              2=currency replaces the decimal sign\n"
       "  /currspc:X  space between the currency and the value (0=no, 1=yes)\n"
       "  /currprec:X currency's precision (number of decimal digits, 0..9)\n"
       "  /yesno:XY   sets the 'Yes/No' letter to XY (default: YN)\n"
      );
}


static char *datestring(struct country *c) {
  static char result[16];
  switch (c->datefmt) {
    case COUNTRY_DATE_MDY:
      sprintf(result, "12%c31%c1990", c->datesep, c->datesep);
      break;
    case COUNTRY_DATE_DMY:
      sprintf(result, "31%c12%c1990", c->datesep, c->datesep);
      break;
    case COUNTRY_DATE_YMD:
    default:
      sprintf(result, "1990%c12%c31", c->datesep, c->datesep);
      break;
  }
  return(result);
}


static char *timestring(struct country *c) {
  static char result[16];
  if (c->timefmt == COUNTRY_TIME12) {
    sprintf(result, "11%c59%c59 PM", c->timesep, c->timesep);
  } else {
    sprintf(result, "23%c59%c59", c->timesep, c->timesep);
  }
  return(result);
}


static char *currencystring(struct country *c) {
  static char result[16];
  char decimalpart[16];
  char space[2] = {0, 0};
  char decsym[8];
  char cursym[8];
  decimalpart[0] = '1';
  decimalpart[1] = '2';
  decimalpart[2] = '3';
  decimalpart[3] = '4';
  decimalpart[4] = '5';
  decimalpart[5] = '6';
  decimalpart[6] = '7';
  decimalpart[7] = '8';
  decimalpart[8] = '9';
  decimalpart[9] = 0;
  /* prepare the decimal string first */
  if (c->currencyprec < 9) {
    decimalpart[c->currencyprec] = 0;
  }
  /* prepare the currency space string */
  if (c->currencyspace != 0) {
    space[0] = ' ';
  }
  /* prepare the currency and decimal symbols */
  if (c->currencydecsym != 0) { /* currency replaces the decimal point */
    sprintf(decsym, "%s", c->currency);
    cursym[0] = 0;
  } else {
    sprintf(decsym, "%c", c->decimal);
    sprintf(cursym, "%s", c->currency);
  }
  if (c->currencyprec == 0) decsym[0] = 0;
  /* compute the final string */
  if (c->currencypos == 0) { /* currency precedes value */
    sprintf(result, "%s%s99%s%s", cursym, space, decsym, decimalpart);
  } else { /* currency follows value or replaces decimal symbol */
    sprintf(result, "99%s%s%s%s", decsym, decimalpart, space, cursym);
  }
  return(result);
}


/* checks if str starts with prefix. returns 0 if so, non-zero otherwise. */
static int stringstartswith(char *str, char *prefix) {
  for (;;) {
    /* end of prefix means success */
    if (*prefix == 0) return(0);
    /* otherwise there is no match */
    if (*str != *prefix) return(-1);
    /* if match good so far, look at next char */
    str += 1;
    prefix += 1;
  }
}


/* processes an argument. returns 0 on success, non-zero otherwise. */
static int processarg(char *arg, struct country *c) {
  char *value;
  int intvalue;
  /* an option must start with a '/' */
  if (arg[0] != '/') return(-1);
  arg += 1; /* skip the slash */
  /* find where the value starts */
  value = strchr(arg, ':');
  /* if no value present, fail */
  if (value == NULL) return(-2);
  value += 1;
  if (*value == 0) return(-3);
  /* interpret the option now */
  if (stringstartswith(arg, "country:") == 0) {
    intvalue = atoi(value);
    if ((intvalue > 0) && (intvalue < 1000)) {
      c->id = intvalue;
      return(0);
    }
  } else if (stringstartswith(arg, "cp:") == 0) {
    intvalue = atoi(value);
    if ((intvalue > 0) && (intvalue < 1000)) {
      c->codepage = intvalue;
      return(0);
    }
  } else if (stringstartswith(arg, "decim:") == 0) {
    if (value[1] == 0) { /* value must be exactly one character */
      c->decimal = *value;
      return(0);
    }
  } else if (stringstartswith(arg, "thous:") == 0) {
    if (value[1] == 0) { /* value must be exactly one character */
      c->thousands = *value;
      return(0);
    }
  } else if (stringstartswith(arg, "datesep:") == 0) {
    if (value[1] == 0) { /* value must be exactly one character */
      c->datesep = *value;
      return(0);
    }
  } else if (stringstartswith(arg, "timesep:") == 0) {
    if (value[1] == 0) { /* value must be exactly one character */
      c->timesep = *value;
      return(0);
    }
  } else if (stringstartswith(arg, "datefmt:") == 0) {
    if (strcmp(value, "MDY") == 0) {
      c->datefmt = COUNTRY_DATE_MDY;
      return(0);
    } else if (strcmp(value, "DMY") == 0) {
      c->datefmt = COUNTRY_DATE_DMY;
      return(0);
    } else if (strcmp(value, "YMD") == 0) {
      c->datefmt = COUNTRY_DATE_YMD;
      return(0);
    }
  } else if (stringstartswith(arg, "timefmt:") == 0) {
    if (value[1] == 0) {
      if ((value[0] >= '0') && (value[0] <= '1')) {
        c->timefmt = value[0] - '0';
        return(0);
      }
    }
  } else if (stringstartswith(arg, "curr:") == 0) {
    if (strlen(value) <= 4) {
      strcpy(c->currency, value);
      return(0);
    }
  } else if (stringstartswith(arg, "currpos:") == 0) {
    if (value[1] == 0) {
      if (value[0] == '0') {
        c->currencypos = 0;
        return(0);
      } else if (value[0] == '1') {
        c->currencypos = 1;
        return(0);
      } else if (value[0] == '2') {
        c->currencypos = 0;
        c->currencydecsym = 1;
        return(0);
      }
    }
  } else if (stringstartswith(arg, "currspc:") == 0) {
    if (value[1] == 0) {
      if ((value[0] >= '0') && (value[0] <= '1')) {
        c->currencyspace = value[0] - '0';
        return(0);
      }
    }
  } else if (stringstartswith(arg, "currprec:") == 0) {
    if (value[1] == 0) {
      if ((value[0] >= '0') && (value[0] <= '9')) {
        c->currencyprec = value[0] - '0';
        return(0);
      }
    }
  } else if (stringstartswith(arg, "yesno:") == 0) {
    /* string must be exactly 2 characters long */
    if ((value[0] != 0) && (value[1] != 0) && (value[2] == 0)) {
      c->yes = value[0];
      c->no = value[1];
      return(0);
    }
  }
  /* if I'm here, something went wrong */
  return(-4);
}


int main(int argc, char **argv) {
  struct country cntdata;
  int changedflag;
  int x;
  char *fname;

  if ((argc < 2) || (argv[1][0] == '/')) {
    about();
    return(1);
  }

  fname = argv[1];

  x = country_read(&cntdata, fname);
  if (x != 0) {
    printf("ERROR: failed to read the preference file [%d]\n", x);
    return(2);
  }

  changedflag = argc - 2;

  /* process command line arguments */
  while (--argc > 1) {
    if (processarg(argv[argc], &cntdata) != 0) {
      about();
      return(3);
    }
  }

  printf("Country intl code.....: %03d\n", cntdata.id);
  printf("Codepage..............: %d\n", cntdata.codepage);
  printf("Decimal separator.....: %c\n", cntdata.decimal);
  printf("Thousands separator...: %c\n", cntdata.thousands);
  printf("Date format...........: %s\n", datestring(&cntdata));
  printf("Time format...........: %s\n", timestring(&cntdata));
  printf("Yes/No letters........: %c/%c\n", cntdata.yes, cntdata.no);
  printf("Currency example......: %s\n", currencystring(&cntdata));

  printf("\n"
         "Note: Please make sure your CONFIG.SYS contains a COUNTRY directive pointing\n"
         "      to your custom preferences file. Example:\n"
         "      COUNTRY=%03d,,C:\\MYPREFS.SYS\n\n", cntdata.id);

  /* if anything changed, write the new file */
  if (changedflag != 0) country_write(fname, &cntdata);

  return(0);
}
