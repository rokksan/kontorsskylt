# Kontorsskylt -- bygga Windows-version

Den här instruktionen beskriver hur den fungerande Python-applikationen
för **Kontorsskylt** testas och paketeras som en fristående Windows
11-`.exe`.

## Utgångsläge

Projektet använder:

-   Python
-   Tkinter
-   Bleak för Bluetooth Low Energy
-   ESP32-C3 som BLE-enhet
-   `kontorsskylt.py` som desktopapplikation

Den fungerande BLE/state-logiken ska **inte ändras bara för
Windows-paketeringen**.

Målet är att slutanvändaren ska kunna köra programmet på Windows 11 utan
att själv installera Python, skapa en venv eller installera paket.

> Viktigt: Windows-`.exe` ska byggas på Windows. Bygg inte
> Windows-versionen med PyInstaller på Linux.

------------------------------------------------------------------------

## 1. Hämta projektet med Git

På Windows-datorn, klona repot och gå in i projektmappen.

Exempel:

``` powershell
git clone <REPO-URL>
cd kontorsskylt
```

Om repot redan finns:

``` powershell
git pull
```

Kontrollera att den fungerande filen finns:

``` text
kontorsskylt.py
```

------------------------------------------------------------------------

## 2. Kontrollera Python

Python behövs på **byggdatorn**, men inte på datorerna som senare bara
kör den färdiga `.exe`-filen.

Öppna PowerShell i projektmappen och kör:

``` powershell
python --version
```

Om kommandot inte finns, prova:

``` powershell
py --version
```

Om inget av dem fungerar behöver Python 3 installeras på byggdatorn.

När Python installeras på Windows är det praktiskt att aktivera
alternativet som lägger Python i `PATH`.

------------------------------------------------------------------------

## 3. Skapa en ren virtuell miljö

Om `python` fungerar:

``` powershell
python -m venv .venv
```

Om Windows använder `py`:

``` powershell
py -m venv .venv
```

Aktivera miljön i PowerShell:

``` powershell
.\.venv\Scripts\Activate.ps1
```

Prompten bör därefter visa ungefär:

``` text
(.venv) PS C:\...\kontorsskylt>
```

### Om PowerShell blockerar aktiveringsskriptet

Kör för den aktuella PowerShell-sessionen:

``` powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

och försök sedan igen:

``` powershell
.\.venv\Scripts\Activate.ps1
```

------------------------------------------------------------------------

## 4. Installera beroenden

Med `.venv` aktiverad:

``` powershell
python -m pip install --upgrade pip
python -m pip install bleak pyinstaller
```

Kontrollera installationen:

``` powershell
python -m pip show bleak
python -m PyInstaller --version
```

------------------------------------------------------------------------

## 5. Testa Python-programmet på Windows först

Innan någon `.exe` byggs ska originalprogrammet testas direkt med
Python:

``` powershell
python kontorsskylt.py
```

Kontrollera att GUI:t öppnas.

Med Kontorsskylt-enheten igång, kontrollera sedan:

1.  att programmet hittar `Kontorsskylt` via BLE,
2.  att **SKYLTEN VISAR NU** läser det verkliga läget,
3.  att **På kontoret** fungerar,
4.  att **Arbetar hemifrån** fungerar,
5.  att CUSTOM/egen text fungerar,
6.  att svenska tecken som `å`, `ä` och `ö` fungerar,
7.  att GUI:t uppdaterar **SKYLTEN VISAR NU** först efter att skylten
    har slutfört displayuppdateringen.

### Om Python-versionen inte fungerar

Bygg inte `.exe` ännu.

Felsök först Windows/Bluetooth/Bleak tills:

``` powershell
python kontorsskylt.py
```

fungerar korrekt.

Det gör det mycket lättare att skilja BLE-/Windowsproblem från
PyInstallerproblem.

------------------------------------------------------------------------

## 6. Första PyInstaller-bygget

När Python-versionen fungerar på Windows kan programmet paketeras.

Börja med ett enkelt bygge:

``` powershell
python -m PyInstaller --noconfirm --clean --onefile --windowed --name Kontorsskylt kontorsskylt.py
```

Flaggorna betyder:

-   `--noconfirm` -- ersätt tidigare byggresultat utan extra fråga.
-   `--clean` -- rensa PyInstallers cache inför bygget.
-   `--onefile` -- skapa en ensam `.exe`.
-   `--windowed` -- öppna inte ett separat konsolfönster tillsammans med
    Tkinter-GUI:t.
-   `--name Kontorsskylt` -- den färdiga filen heter `Kontorsskylt.exe`.

PyInstaller skapar bland annat:

``` text
build\
dist\
Kontorsskylt.spec
```

Den färdiga applikationen ska ligga här:

``` text
dist\Kontorsskylt.exe
```

------------------------------------------------------------------------

## 7. Testa den färdiga EXE-filen

Starta:

``` powershell
.\dist\Kontorsskylt.exe
```

Gör samma funktionstest som med Python-versionen:

-   programmet öppnas,
-   BLE-enheten hittas,
-   aktuellt skyltläge läses,
-   OFFICE fungerar,
-   HOME fungerar,
-   CUSTOM fungerar,
-   svenska tecken fungerar,
-   **SKYLTEN VISAR NU** visar bekräftat läge efter
    displayuppdateringen.

Testa helst även `.exe`-filen på en annan Windows 11-dator som **inte**
har projektets Python-miljö installerad. Det är det viktiga
distributionstestet.

------------------------------------------------------------------------

## 8. Om EXE-versionen inte fungerar men Python-versionen fungerar

Då är problemet sannolikt kopplat till paketeringen snarare än den
grundläggande applikationslogiken.

Bygg då tillfälligt en felsökningsversion **utan** `--windowed`:

``` powershell
python -m PyInstaller --noconfirm --clean --onefile --name Kontorsskylt-debug kontorsskylt.py
```

Starta den från PowerShell:

``` powershell
.\dist\Kontorsskylt-debug.exe
```

Eftersom konsolfönstret finns kvar kan Python-/Bleak-fel visas där.

Ändra inte den fungerande BLE-logiken innan felet faktiskt har
identifierats.

------------------------------------------------------------------------

## 9. Git och filer som bör ignoreras

Den virtuella miljön och PyInstallers byggmappar ska normalt inte
checkas in.

Lägg exempelvis detta i `.gitignore`:

``` gitignore
.venv/
build/
dist/
__pycache__/
*.pyc
```

`Kontorsskylt.spec` kan antingen versionshanteras eller ignoreras. När
byggprocessen har stabiliserats kan det vara användbart att
versionshantera `.spec`-filen så att Windows-bygget blir reproducerbart.

Den färdiga `.exe`-filen behöver normalt inte ligga direkt i
Git-historiken. För distribution är en release/artifact ofta lämpligare
när projektet kommit så långt.

------------------------------------------------------------------------

## 10. Rekommenderad arbetsordning på den nya datorn

Kortversion:

``` powershell
git clone <REPO-URL>
cd kontorsskylt

py -m venv .venv
.\.venv\Scripts\Activate.ps1

python -m pip install --upgrade pip
python -m pip install bleak pyinstaller

python kontorsskylt.py
```

När Python-versionen är verifierad:

``` powershell
python -m PyInstaller --noconfirm --clean --onefile --windowed --name Kontorsskylt kontorsskylt.py
```

Testa:

``` powershell
.\dist\Kontorsskylt.exe
```

------------------------------------------------------------------------

## Nuvarande BLE-konfiguration

Applikationen använder följande BLE-identitet:

``` text
Device name:
Kontorsskylt

Service UUID:
7d230001-5475-4a28-a975-8a03cafe0001

Command/status characteristic:
7d230002-5475-4a28-a975-8a03cafe0001

State characteristic:
7d230003-5475-4a28-a975-8a03cafe0001
```

Command/status används för kommandon och slutbekräftelse.
State-characteristic används för att läsa vad e-paper-skylten faktiskt
visar.

------------------------------------------------------------------------

## Viktig princip

Den nuvarande fungerande kedjan är:

``` text
GUI
  ↓
BLE-kommando
  ↓
ESP32-C3
  ↓
e-paper uppdateras
  ↓
slutbekräftelse
  ↓
STATE läses tillbaka
  ↓
"SKYLTEN VISAR NU" uppdateras
```

GUI:t ska alltså inte anta att skylten har ändrats bara för att ett
kommando har skickats. Det bekräftade STATE-värdet är det som ska
representera skyltens verkliga läge.

Behåll denna princip vid framtida ändringar.
