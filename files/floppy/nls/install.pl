#
# This is a localization file for the Svarog386 INSTALL program
#
# Language..: Polish
# Translator: Mateusz Viste
#

### COMMON STUFF: TITLE BAR AND MULTIPLE CHOICE STRINGS ###
0.0:INSTALACJA SVAROG386
0.1:Instaluj Svarog386
0.2:Wyjd� do DOS
0.3:Stw�rz partycj� automatycznie
0.4:Uruchom narz�dzie partycjonowania FDISK
0.5:Naci�nij cokolwiek...
0.6:Formatuj

### LANGUAGE SELECTION SCREEN ###
1.0:Witaj w systemie Svarog386
1.1:Wybierz tw�j j�zyk z poni�szej listy:

### WELCOME SCREEN ###
2.0:You are about to install Svarog386: a free, MSDOS-compatible operating system
2.1:based on the FreeDOS kernel. Svarog386 targets 386+ computers and comes with a
2.2:variety of third-party applications.
# Note: line 2.4 is indented as to be nicely aligned with 2.3:
2.3:UWAGA: Je�li tw�j komputer posiada ju� inny system operacyjny, system ten
2.4:       mo�e nie zdo�a� si� uruchomi� po instalacji Svarog386.
#

### DISK SETUP ###
# lines 3.0 to 3.3 are a single (multi-line) message and need to be indented nicely
# also, no line shall be longer than 76 characters
3.0:ERROR: Drive %c: could not be found. Perhaps your hard disk needs to be
3.1:       partitioned first. Please create at least one partition on your
3.2:       hard disk, so Svarog386 can be installed on it. Note, that
3.3:       Svarog386 requires at least %d MiB of available disk space.
# lines 3.4 to 3.7 are also one multi-line message. each line max. 76 characters
3.4:You can use the FDISK partitioning tool for creating the required partition
3.5:manually, or you can let the installer partitioning your disk
3.6:automatically. You can also abort the installation to use any other
3.7:partition manager of your choice.
#
3.8:Your computer will reboot now.
# the line below must not be longer than 70 characters
3.9:ERROR: Drive %c: is a removable device. Installation aborted.
# the two lines below are a multi-line message. 76 chars max per line.
3.10:ERROR: Drive %c: seems to be unformated.
3.11:       Do you wish to format it?
# the two lines below are a multi-line message. 76 chars max per line.
3.12:ERROR: Drive %c: is not big enough!
3.13:       Svarog386 requires a disk of at least %d MiB.
# the three lines below are a single multi-line message. 76 chars max per line.
3.14:ERROR: Drive %c: is not empty. Svarog386 must be installed on an empty disk.
3.15:       You can format the disk now, to make it empty. Note however, that
3.16:       this will ERASE ALL CURRENT DATA on your disk.
#
3.17:The installation of Svarog386 to %c: is about to begin.

### PACKAGES INSTALLATION ###
4.0:Installing package %d/%d: %s

### END SCREEN ###
5.0:Svarog386 installation is over. Your computer will reboot now.
5.1:Please remove the installation disk from your drive.
