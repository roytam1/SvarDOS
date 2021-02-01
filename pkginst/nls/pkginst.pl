#
# FDNPKG language file
#
# Language..: Polish
# Codepage..: MAZOVIA
# Translator: Mateusz Viste
#


#### Help ####

1.0:Sieciowy menad�er pakiet�w dla FreeDOS.
1.1:Sk�adnia: FDNPKG akcja [parametry]
1.2:Gdzie akcja to jedno z poni�szych:
1.3: search [string]   - wyszukuje w repozytoriach pakiety zawieraj�ce 'string'
1.4: vsearch [string]  - to samo co 'search', ale wy�wietla tak�e repozytoria
1.5: install pkg       - instaluje pakiet 'pkg' (lub lokalny plik zip)
1.6: remove pkg        - usuwa pakiet 'pkg'
1.7: dumpcfg           - wy�wietla konfiguracj� wczytan� z pliku cfg
1.8: license           - wy�wietla licencj� programu
1.9:FDNPKG jest podlinkowany z WatTCP w poni�szej wersji:
1.10: install-nosrc pkg - instaluje pakiet 'pkg' (lub lokalny plik zip) bez �r�de�
1.11: install-wsrc pkg  - instaluje pakiet 'pkg' (lub lokalny plik zip) ze �r�d�ami
1.12: showinstalled [str] - wy�wietla zainstalowane pakiety zawieraj�ce 'str'
1.13: checkupdates      - sprawdza dost�pne uaktualnienia pakiet�w i je wy�wietla
1.14: update pkg        - uaktualnia pakiet 'pkg' do nowszej wersji
1.15: update [pkg]      - uaktualnia pakiet 'pkg' (lub wszystkie pakiety)
1.16: listlocal [str]   - wy�wietla zainstalowane pakiety zawieraj�ce 'str'
1.17:FDNPKG jest podlinkowany z Watt-32 w poni�szej wersji:
1.18: listfiles pkg     - wy�wietla pliki nale��ce do pakietu 'pkg'


### General stuff ####

2.0:%TEMP% nie ustawione! Ustaw by wskazywa�o na tymczasowy katalog.
2.1:Przyk�ad: SET TEMP=C:\\TEMP
2.2:%DOSDIR% nie ustawione! Ustaw by wskazywa�o na katalog instalacji FreeDOS.
2.3:Przyk�ad: SET DOSDIR=C:\\FDOS
2.4:Nieprawid�owa liczba parametr�w. Uruchom bez parametr�w by uzyska� pomoc.
2.5:Brak skonfigurowanych repozytori�w. Ustaw przynajmniej jeden.
2.6:Dodaj do pliku konfiguracyjnego przynajmniej jeden wpis w takiej formie:
2.7:REPO www.freedos.org/repo
2.8:Poni�ej lista skonfigurowanych repozytori�w fdnpkg:
2.9:Od�wie�anie %s...
2.10:�ci�ganie repozytorium nie powiod�o si�!
2.11:B��d podczas �adowania repozytorium z pliku tmp...
2.12:UWAGA: %TZ% nie ustawione! daty na instalowanych plikach mog� by� nie�cis�e.
2.13:Baza danych pakiet�w za�adowana z pami�ci podr�cznej.
2.14:Brak pami�ci! (%s)
2.15:B��D: inicjalizacja TCP/IP nie powiod�a si�!
2.16:�adowanie %s...
2.17:UWAGA: Niski poziom pami�ci wirtualnej. FDNPKG mo�e zachowywa� si� b��dnie.
2.18:B��D: Nie mo�na pisa� w katalogu '%s'. Sprawd� ustawienie zmiennej %%TEMP%%.


#### Installing package ####

3.0:Pakiet %s jest ju� zainstalowany! Usu� go najpierw je�li chcesz uaktualni�.
3.1:Nie znaleziono pakietu '%s' w repozytoriach.
3.2:Pakiet '%s' jest niedost�pny w repozytoriach.
3.3:%s jest dost�pny z kilku repozytori�w. Wybierz kt�ry u�y�:
3.4:Tw�j wyb�r:
3.5:Nieprawid�owy wyb�r!
3.6:�ci�ganie pakietu %s...
3.7:B��d podczas �ci�gania pakietu.
3.8:B��d: Nieprawid�owe archiwum zip! Pakiet nie zosta� zainstalowany.
3.9:B��d: Pakiet zawiera plik kt�ry ju� istnieje lokalnie:
3.10:B��d: Nie uda�o si� stworzy� %s!
3.11:Pakiet %s zosta� zainstalowany.
3.12:B��d: Pakiet nie zawiera pliku %s! Nieprawid�owy pakiet FreeDOS.
3.13:B��d: �ci�gni�ty pakiet zawiera b��dne CRC. Instalacja przerwana.
3.14:B��d: Nie uda�o si� otworzy� �ci�gni�tego pakietu. Instalacja przerwana.
3.15:B��d: Brak pami�ci podczas obliczania CRC pakietu!
3.16:Pakiet %s zosta� zainstalowany (wraz ze �r�d�ami, je�li dost�pne).
3.17:Pakiet %s zosta� zainstalowany (bez �r�de�).
3.18:Pakiet %s jest ju� zainstalowany! Zobacz akcj� 'update'.
3.19:Pakiet %s zosta� zainstalowany: rozpakowano %d plik�w, %d b��d�w.
3.20:B��d: Pakiet zawiera zaszyfrowany plik:
3.21:B��d: Nie uda�o si� otworzy� pliku link '%s' dla odczytu.
3.22:B��d: Nie uda�o si� otworzy� pliku link '%s' dla zapisu.
3.23:B��d: Pakiet zawiera plik o nieprawid�owej nazwie:


#### Removing package ####

4.0:Pakiet %s nie jest zainstalowany, wi�c nie wykasowany.
4.1:B��d podczas dost�pu do pliku lst!
4.2:Limit dirlist osi�gni�ty. Katalog %s nie zostanie usuni�ty!
4.3:Brak pami�ci! Nie zapami�tano katalogu %s!
4.4:usuwanie %s
4.5:Pakiet %s zosta� usuni�ty.


#### Searching package ####

5.0:�aden pakiet nie pasuje do wyszukiwania.
5.1:Brak pami�ci podczas przetwarzania opisu pakietu!


#### Package database handling ####

6.0:B��d: Nieprawid�owy plik indeksu (b��dny nag��wek)! Repozytorium zignorowane.
6.1:B��d: Nieprawid�owy plik indeksu! Repozytorium zignorowane.
6.2:B��d: Brak pami�ci podczas �adowania bazy danych!
6.3:B��d: Nie zdo�ano otworzy� pliku danych '%s'.
6.4:Uwaga: Nie zdo�ano otworzy� pliku pami�ci podr�cznej %s!


#### Loading configuration ####

7.0:B��d: repozytorium '%s' jest skonfigurowane dwa razy!
7.1:B��d: nie zdo�ano otworzy� pliku konfiguracyjnego '%s'!
7.2:Uwaga: token bez warto�ci w linii #%d
7.3:Uwaga: zbyt d�ugi token konfiguracyjny w linii #%d
7.4:Uwaga: token z pust� warto�ci� w linii #%d
7.5:Uwaga: spacja po warto�ci w linii #%d
7.6:Odrzucono repozytorium: zbyt wiele skonfigurowanych (maks=%d)
7.8:Uwaga: Nieznany token '%s' w linii #%d
7.9:Uwaga: zbyt d�uga warto�� w pliku konfiguracyjnym w linii #%d
7.10:Uwaga: Nieprawid�owa warto�� '%s' w linii #%d
7.11:Uwaga: Nieprawid�owe polecenie 'DIR' w linii #%d
7.12:B��d: za d�uga �cie�ka DIR w linii #%d
7.13:B��d: Nieistniej�ca zmienna �rodowiskowa '%s' w linii #%d
7.14:B��d: repozytorium '%s' jest skonfigurowane dwa razy!
7.15:B��d: katalog specjalny '%s' nie jest prawid�ow� �cie�k�!
7.16:B��d: katalog specjalny '%s' jest zarezerwowan� nazw�!


#### Unziping package ####

8.0:Brak pami�ci!
8.1:nieznana sygnatura zip: 0x%08lx
8.2:B��d: Pakiet zawiera plik skompresowany nieznan� metod� (%d):
8.3:B��d podczas rozpakowywania '%s' do '%s'!


#### Handling the local list of installed packages ####

9.0:B��d: Dost�p do katalogu %s nie powi�d� si�.
9.1:B��d: Nie znaleziono lokalnego pakietu %s.


#### Package updates ####

10.0:%s (wersja lokalna: %s)
10.1:wersja %s pod %s
10.2:Nie znaleziono aktualizacji dla pakietu '%s'.
10.3:Znaleziono aktualizacj� dla pakietu '%s'. Aktualizacja w toku...
10.4:Zaktualizowano %d pakiet�w, %d b��dnych pakiet�w.
10.5:Znaleziono aktualizacje dla %d pakiet�w.
10.6:Pakiet %s nie jest zainstalowany.
10.7:Poszukiwanie aktualizacji...


#### Downloading ####

11.0:�ci�ganie %s... %ld bajt�w
