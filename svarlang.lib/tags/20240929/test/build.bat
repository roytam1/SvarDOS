@echo off

IF EXIST OUT.LNG DEL OUT.LNG
IF EXIST OUTC.LNG DEL OUTC.LNG

..\tlumacz en pl
rename out.lng outc.lng

..\tlumacz /nocomp en pl

wcl -0 -wx -ox -we -ms test.c deflang.c ..\svarlngs.lib
