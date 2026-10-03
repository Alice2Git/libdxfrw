# Patch-urile dxfrw_c pentru libdxfrw

Baza: `codelibs/libdxfrw`, commit `bf80b74` („fix(dwg): add AC1032 version support…”).
Toate modificările din cod sunt marcate cu comentariul `patch dxfrw_c`. Cele 13 teste proprii ale libdxfrw trec.

## Unde se află sursele

Sursele libdxfrw cu toate corecturile se distribuie separat, ca depozit pregătit pentru git (arhiva `libdxfrw-dxfrw_c-<data>.zip`):
CMake ca singur sistem de build, fișierele de build învechite (autotools, Visual Studio 2013, `makefile.mingw`) eliminate,
README și ChangeLog actualizate. Fișierul vechi `libdxfrw-dxfrw_c.patch` (primele 24 de patch-uri) a fost retras: nu mai
conținea corecturile din septembrie–octombrie 2026. Diferența completă față de original se obține într-o clonă:

```
git clone https://github.com/codelibs/libdxfrw.git
cd libdxfrw
git checkout -b dxfrw_c bf80b74
(se copiază peste ea conținutul arhivei libdxfrw)
git add -A
git commit -m "dxfrw_c: corecturile și completările descrise în DXFRW_C_PATCHES.md"
git diff bf80b74
```

Pachetul shim-ului conține libdxfrw doar compilată (`libdxfrw-win64/`, cu antetele necesare pentru compilarea shim-ului).

## Corecturi, grupate după impact

### Date corupte sau pierdute la salvare

| Problemă | Efect înainte de patch | Fișiere |
|---|---|---|
| Stare globală (`static int min_ver`) în codec-ul de text | După un singur fișier R2000/2004 citit sau scris, toate salvările 2007+ din același proces corupeau textul non-ASCII | `intern/drw_textcodec.cpp` |
| Tabelul ANSI_1252 nefolosit, conversie prin iconv cu „SJIS” | Textul accentuat din DXF ≤ 2004 era convertit greșit | `intern/drw_textcodec.cpp` |
| Valori booleene (coduri 290–299) scrise pe 2 octeți în DXF binar | Fișierele binare scrise nu puteau fi citite de alte programe și nici de libdxfrw | `intern/dxfwriter.cpp` |
| Întregi pe 16 biți citiți cu semn greșit din DXF binar | Valori ≥ 128 în octetul inferior deveneau negative (flag 192 → -64, culori ACI > 127) | `intern/dxfreader.cpp` |
| Grosimea (39) și extrudarea (210) nescrise la POINT, LINE, CIRCLE, ARC, ELLIPSE, LWPOLYLINE | Geometria oglindită (extrudare 0,0,-1) ieșea întoarsă | `libdxfrw.cpp` |
| Scara de linetype (48), vizibilitatea (60), numele de culoare (430) și transparența (440) nescrise | Proprietățile se pierdeau la salvare | `libdxfrw.cpp` |
| Vizibilitatea citită inversat din DXF și ignorată din DWG | Entitățile invizibile apăreau vizibile și invers | `drw_entities.cpp` |
| Transparența (440) necitită din DXF | Valoarea se pierdea | `drw_entities.cpp` |
| Flag-urile vertecșilor POLYLINE scrise cu flag-urile polilinei | Polyface mesh-urile deveneau invalide | `libdxfrw.cpp` |
| Numărul de vertecși și fețe (71/72) nescris la polyface | Polyface invalid după salvare | `libdxfrw.cpp` |
| Marcaje de subclasă POLYLINE/VERTEX inversate sau lipsă | 2D etichetat 3D; cititorii stricți pot respinge fișierul | `libdxfrw.cpp` |
| Fit points, toleranța și tangentele SPLINE nescrise | Spline-urile definite prin fit points pierdeau geometria | `libdxfrw.cpp` |
| Normala SPLINE (0,0,0) scrisă pentru spline-uri neplanare | Vector invalid în fișier | `libdxfrw.cpp` |
| MTEXT > 250 octeți tăiat în mijlocul caracterelor UTF-8 sau al secvențelor `\U+XXXX` | Diacritice corupte în textele lungi | `libdxfrw.cpp` |
| Stilul MTEXT, numele de pattern HATCH și numele blocului de cotă scrise fără conversie de code page | Nume non-ASCII corupte în DXF ≤ 2004 | `libdxfrw.cpp` |
| LWPOLYLINE omisă la salvarea R12 („TODO”) | Poliliniile dispăreau din fișierele R12; acum sunt scrise ca POLYLINE 2D | `libdxfrw.cpp` |
| `write()` întorcea succes chiar dacă fișierul nu putea fi creat | Eroare de scriere nesemnalată | `libdxfrw.cpp` |
| Scurgere de memorie la conversia ELLIPSE → POLYLINE (R12) | Memorie pierdută la fiecare elipsă | `libdxfrw.cpp` |

### Windows și build

| Problemă | Efect | Fișiere |
|---|---|---|
| Căi deschise cu `char*` (code page ANSI) | Fișierele cu diacritice în cale nu se deschideau pe Windows; acum căile UTF-8 sunt convertite la UTF-16 | `intern/drw_utf8path.h` (nou), `libdxfrw.cpp`, `libdwgr.cpp` |
| Dependență de iconv (inexistent în Visual Studio / MinGW standard) | Biblioteca nu se compila fără pachetul NuGet libiconv | `intern/drw_textcodec.*`, `CMakeLists.txt` |
| Mesaj pe `std::cerr` în cititorul DWG | Ieșire nedorită în consolă | `intern/dwgreader15.cpp` |

### Fișiere corupte sau trunchiate (crash, blocaj)

Găsite prin fuzzing pe 640 de fișiere derivate din DWG R14–2018 și DXF (trunchieri, octeți alterați, antete stricate).
După patch: 640/640 fără crash sau blocaj; sub AddressSanitizer + LeakSanitizer, zero erori de memorie și zero scurgeri.

| Problemă | Efect | Fișiere |
|---|---|---|
| Buclele de dispecerizare a entităților/obiectelor nu se opreau la sfârșitul fluxului | Blocaj infinit pe DXF trunchiat sau corupt | `libdxfrw.cpp`, `intern/dxfreader.*` |
| Fișier fără nicio secțiune DXF acceptat ca valid | Date aleatoare sau text „încărcate” ca document gol | `libdxfrw.cpp` |
| Lipsa marcajului EOF nesemnalată | Nou: `dxfRW::eofFound()` | `libdxfrw.h`, `libdxfrw.cpp` |
| VERTEX adăugat de două ori când nu urma `SEQEND` | Double free (crash) | `libdxfrw.cpp` |
| Înregistrarea `SEQEND` citită în polilinie | Handle-ul și layerul polilinei suprascrise și pe fișiere valide | `libdxfrw.cpp` |
| Contor de înregistrări DWG R2000 necontrolat | Blocaj infinit (citire după EOF) | `intern/dwgreader15.cpp` |
| Decompresorul R2004+ fără verificări de limite | Citire/scriere în afara bufferelor (crash) | `intern/dwgutil.*` |
| Dimensiuni și offset-uri de secțiune DWG nevalidate | Alocări de gigaocteți, scriere în afara bufferului | `intern/dwgreader18.*` |
| 34 de bucle cu contor citit din fișier | Blocaje de minute sau ore pe valori corupte | `drw_entities.cpp`, `drw_objects.cpp`, `intern/dwgreader.cpp` |
| Citirile după sfârșitul datelor lăsau octeți neinițializați | Bucle „citește până la 0” infinite, valori aleatoare | `intern/dwgbuffer.cpp` |
| `getBytes` cu dimensiune negativă, dimensiune grafică nevalidată | Scriere în afara bufferului | `intern/dwgbuffer.cpp`, `drw_entities.cpp` |
| Contor de handle-uri de bloc folosit direct pentru `reserve` | Alocare de 13 GB | `drw_objects.cpp` |
| Citiri binare prin cast de pointer | Comportament nedefinit (aliniere/aliasing) | `intern/dxfreader.cpp` |
| Buffere temporare neeliberate pe căile de eroare (șiruri, CRC, harta secțiunilor R2007) | Scurgeri de memorie la fiecare fișier corupt | `intern/dwgbuffer.cpp`, `intern/dwgreader21.cpp` |
| Variabile de header, clase DWG și intrări de tabel cu nume/handle duplicat suprascrise fără eliberare | Scurgeri de memorie | `drw_header.cpp`, `intern/dwgreader*.cpp` |
| LWPOLYLINE neterminată la sfârșitul fluxului | Vertecși pierduți (scurgere) | `libdxfrw.cpp` |
| Bucla de citire a claselor DWG R2000 fără verificarea stării bufferului | Blocaj posibil pe date corupte | `intern/dwgreader15.cpp` |
| Pointeri și grad neinițializați în `DRW_Spline` | Crash posibil pe fișiere cu ordine neobișnuită a codurilor | `drw_entities.h` |

### Compatibilitate AutoCAD (găsite cu DWG TrueView, patch-urile 8–16)

Fișiere pe care ezdxf le accepta, dar pe care DWG TrueView (motorul AutoCAD) le deschidea fără să le afișeze, cu procesorul blocat. Cauzele au fost izolate prin variante cu câte o singură modificare, testate în TrueView, și confirmate în ambele direcții (eliminarea cauzei repară fișierul; introducerea ei într-un fișier bun îl strică).

| Problemă | Efect | Fișiere |
|---|---|---|
| R12: intrările de tabel și blocurile fără handle | Blocaj la orice fișier R12 | `libdxfrw.cpp` |
| `$HANDSEED` scris constant | Blocaj când handle-urile depășeau valoarea (R12); acum calculat | `drw_header.cpp`, shim |
| `$TDCREATE` / `$TDUPDATE` nescrise în nicio versiune | Blocaj în R12; acum scrise în toate versiunile (data creării păstrată din sursă) | `drw_header.cpp` |
| Codul 91 (identificator de vertex, 2010+) scris cu 0 pe fețele polyface, inclusiv în R12 | Blocaj la orice polyface | `libdxfrw.cpp` |
| RAY și XLINE scrise în R12 (au apărut în R13) | Fișier R12 invalid | `libdxfrw.cpp` |
| Codul 49 (DIMFXL, 2007+) scris în toate versiunile | Cod inexistent în versiunile vechi | `libdxfrw.cpp` |
| DWG 2007+: parserele STYLE, VPORT, APPID citeau un câmp inexistent (xrefindex) | Stiluri de text cu valori aberante (lățime −3,7·10¹⁵⁴): blocaj la orice text | `drw_objects.cpp` |
| HATCH cu model: `78=N` (linii de definiție) scris fără liniile propriu-zise | Blocaj; acum `78=0`, hașură neasociativă | `libdxfrw.cpp` |
| R12: nume cu spații sau `*` la început (`Tavolo 2`, `*ADSK_SYSTEM_LIGHTS`, `*T13`) | Blocaj; acum caracterele nepermise devin `_`, consecvent în tabele, entități, blocuri și header (ca AutoCAD „Salvare ca R12”) | `intern/dxfwriter.*`, `libdxfrw.cpp`, `drw_header.cpp` |
| R12: flag-uri de bloc contradictorii (anonim pentru un nume redenumit, „are atribute” fără ATTDEF) | Blocaj | `libdxfrw.cpp` |
| DWG R14: bitul „blocat” al layerelor citit pe poziția 8 în loc de 4 | Flag de layer nedefinit | `drw_objects.cpp` |
| Layerul `Defpoints` scris plotabil (DWG R14 nu stochează flag-ul) | Blocaj; acum `Defpoints` e mereu neplotabil | `libdxfrw.cpp` |
| DXF binar: mărimea valorii era dată de funcția apelată, nu de codul de grup (de ex. codul 91 al hașurii scris pe 16 biți) | Restul fișierului citit decalat, entități pierdute la recitire; acum formatul se alege după codul de grup | `intern/dxfwriter.cpp` |

Rezultat confirmat în TrueView: desenul de test și un DWG real se deschid salvați în **toate** versiunile de ieșire (R12, R14, 2000, 2004, 2007, 2010, 2013, 2018), iar DWG-urile R14, 2000 și 2018 convertite în DXF se deschid.

### Citirea fișierelor DWG reale (patch-urile 17–20)

Găsite pe un lot de producție de 11.606 desene (9734 DWG, 18,2 milioane de entități).

| Problemă | Efect |
|---|---|
| Structurile care descriu paginile și secțiunile DWG aveau câmpuri neinițializate; bufferele de secțiune și citirile incomplete lăsau octeți neinițializați | Același fișier dădea rezultate diferite la fiecare rulare, de obicei zero entități |
| `case 7` fără `break` în decompresorul R2007 (`copyCompBytes21`) | Scria 15 octeți în loc de 7: corupere de memorie și oprirea programului |
| Decompresorul R2007 fără verificări de limite; numărul de pagini al unei secțiuni folosit fără validare | Depășiri de buffer, citiri la nesfârșit, hărți umplute cu pagini inexistente |
| Harta de obiecte era declarată invalidă după orice citire la limită, deși fusese citită integral | Fișierele 2018 raportau „eroare la citirea hărții de obiecte” și pierdeau tabelele |
| Punctele de control ale unui spline care formează conturul unei hașuri erau adăugate de două ori | Geometrie dublată și eliberare dublă de memorie: proces oprit brusc |
| Harta de obiecte nu se oprea la înregistrarea goală care marchează sfârșitul | Reziduurile unei salvări anterioare (valide, deci trecând și verificarea sumei de control) suprascriau pozițiile reale ale obiectelor: fișierul ieșea complet gol |
| Mesajele de depanare construiau șiruri la fiecare apel, chiar dezactivate | Citire de circa două ori mai lentă (se elimină cu `DRW_NO_DEBUG`, păstrând apelurile cu efect) |
| DXF binar: mărimea valorii era dată de funcția apelată, nu de codul de grup (de ex. codul 91 al hașurii) | Restul fișierului citit decalat, entități pierdute la recitire |

Cinci desene din lot ieșeau cu zero entități; după corecturi citesc între 1769 și 103.166 de entități.

### Cote și hașuri (tranșa C, patch-urile 21–23)

| Problemă | Efect |
|---|---|
| Handle-ul blocului de geometrie al unei cote era citit din DWG, dar niciodată tradus în nume (pas pe care biblioteca îl face pentru INSERT) | Cotele din DWG rămâneau fără referință, deci invalide, și se pierdeau la salvare |
| Conversia din DWG lăsa în codul 70 un bit inexistent în DXF, iar cititorul extrăgea tipul din 4 biți în loc de 3 | Tipul cotei ieșea nerecunoscut, iar cota era ignorată la recitire |
| Biblioteca reținea doar numărul liniilor de definiție a modelului de hașură, nu și conținutul lor | Hașurile se salvau fără desenul propriu-zis |
| Unghiurile hașurii și ale liniilor modelului nu erau convertite din radiani în grade la citirea DWG | Model rotit greșit (de exemplu 1,57 în loc de 90°) |
| `DRW_Dimension` nu expunea tipul (codul 70) și returna șirurile prin copie | Necesar pentru interfața C |

### Citire incompletă raportată corect (patch-ul 24)

| Problemă | Efect |
|---|---|
| O intrare la care un tabel trimite, dar care nu mai există în desen (element șters), era tratată ca eroare de citire | Fișiere citite integral erau marcate „incomplete”; acum intrarea moartă se sare, cu avertisment |
| Numărul obiectelor care nu au putut fi decodate nu era cunoscut de apelant | Se expune prin `dxfrw_document_failed_objects`, apare în mesaj și în coloana `obiecte_nedecodate` din `rezumat.csv` |

### Tranșele B și C (septembrie 2026)

Găsite la completarea shim-ului (blocuri, INSERT, SOLID/TRACE, 3DFACE, RAY/XLINE, LEADER, IMAGE, VIEWPORT, XDATA, bucle-polilinie și muchii spline la hașuri, tabelele APPID/VPORT/DIMSTYLE).

| Problemă | Efect | Fișiere |
|---|---|---|
| Conturul-polilinie al hașurilor nu se scria deloc (codul 92 urmat direct de 75), iar codul 93 număra și muchiile nescrise | **Aproape toate hașurile din DWG-urile lotului (1.446) se salvau fără contur** | `libdxfrw.cpp` (`writeHatch`) |
| Muchiile spline ale hașurilor din DXF nu erau citite (94, 73, 74, 95–97, 40, 10/20, 42, 11/21, 12/22, 13/23); sensul muchiilor elipsă (73) ignorat | Hașurile cu spline sau arce eliptice se pierdeau la recitire | `drw_entities.cpp` |
| Membri neinițializați în `DRW_Hatch`, `DRW_Image`, `DRW_Leader`, `DRW_Viewport` | Valori aleatoare scrise în fișier | `drw_entities.h` |
| VIEWPORT: codurile 13/23, 14/24, 16–17, 42–45, 50, 51 necitite din DXF; unghiurile din DWG lăsate în radiani | Ferestre de vizualizare greșite | `drw_entities.cpp` |
| Șirurile XDATA citite fără decodare (în fișierele până la R2004 rămâneau `\U+XXXX`) | Diacritice corupte în XDATA | `drw_entities.cpp`, `drw_objects.cpp` |
| XDATA entităților nu se scria deloc; nou `dxfRW::writeEntityExtData` (POLYLINE o scrie înaintea vertecșilor) | XDATA pierdut la salvare | `libdxfrw.h`, `libdxfrw.cpp` |
| SOLID/TRACE fără grosime și extrudare la scriere; INSERT fără extrudare | Blocurile inserate oglindit ajungeau în alt loc | `libdxfrw.cpp` |
| LEADER: codul 76 scris de două ori, vectorii 210–213 lipsă; VIEWPORT: câmpuri nescrise; layerul blocului scris mereu „0” | Entități incomplete sau greșite | `libdxfrw.cpp` |
| IMAGEDEF fără dimensiuni, rezoluție și proprietar; obiectele `DRW_ImageDef` neeliberate | Imagini invalide, scurgeri de memorie | `libdxfrw.cpp` |

### Finalizarea bibliotecii (octombrie 2026)

Găsite prin măsurarea a ceea ce rămânea necitit în cele 9.735 de DWG-uri ale lotului.

| Problemă | Efect | Fișiere |
|---|---|---|
| Verificarea de limite din `readInstructions21` (un patch al nostru anterior) cerea 4 octeți pentru orice instrucțiune, deși unele au nevoie de 1–3 | Ultima copiere a unei pagini DWG 2007 lipsea, pagina se termina cu zerouri: **cauza tuturor celor 65 de fișiere PARTIAL R2007** (și a unor polilinii 3D fără vertecși) | `intern/dwgutil.cpp` |
| Tangentele de capăt ale muchiilor spline din hașuri (DWG 2010+) citite și când muchia nu are puncte de potrivire | Hașura nedecodată; ultimul fișier PARTIAL din lot | `drw_entities.cpp` |
| Datele extinse (XDATA) din DWG erau sărite; acum se decodează toate tipurile (șiruri pe code page sau UTF-16, `{`/`}`, layer, binar, handle, puncte, reale, întregi), iar aplicația și layerul se rezolvă în nume | XDATA pierdut la conversia DWG → DXF (12.759 de blocuri în 1.102 fișiere, decodate exact până la ultimul octet) | `drw_entities.*`, `intern/dwgreader.cpp` |
| Numele complet al blocurilor anonime („*D834”) era cunoscut abia când se ajungea la bloc; cotele și INSERT-urile din blocurile citite înainte primeau doar prefixul („*D”) | 150 de cote și 2 INSERT din 27 de fișiere trimiteau spre blocuri inexistente | `intern/dwgreader.cpp` |
| Două blocuri anonime distincte cu același nume în DWG (permis acolo, referințele sunt prin handle) | În DXF al doilea bloc se scria peste primul (handle-uri duplicate); acum primește următorul număr liber | `intern/dwgreader.cpp` |
| ATTRIB / ATTDEF nesuportate deloc (nici citire, nici scriere) | Valorile atributelor (de ex. textele din cartușe) se pierdeau; acum `DRW_Attrib`, `DRW_Attdef`, `DRW_Insert::attributes`, `DRW_Interface::addAttdef` | `drw_entities.*`, `drw_interface.h`, `libdxfrw.*`, `intern/dwgreader.*` |
| Handle-urile atributelor unei insertii citite cu un contor pe 8 biți și ca absolute | Insertii cu peste 255 de atribute / referințe relative greșite | `drw_entities.cpp` |
| Dimensiunea graficii proxy citită ca RL și în DWG 2010+, unde este BLL | Orice entitate cu grafică proxy din fișierele 2010+ era nedecodată (de ex. toate MULTILEADER) | `drw_entities.cpp` |
| `get3Bits` lua un singur bit din octetul următor când cei 3 biți începeau pe ultimul bit al unui octet; `getBitLongLong` asambla octeții în ordine inversă | Valori BLL greșite și citire decalată | `intern/dwgbuffer.cpp` |
| MULTILEADER nesuportat; acum citire DWG (validată pe toate cele 1.501 din lot: datele se termină exact la începutul fluxului de șiruri), citire și scriere DXF, cu stilul „Standard” (MLEADERSTYLE) și clasele scrise automat | ~1.500 de indicatoare (texte, numere de poziție) pierdute | `drw_entities.*`, `drw_classes.cpp`, `drw_interface.h`, `libdxfrw.*`, `intern/dwgreader.*` |
| În DXF, la prima entitate din fiecare bloc se pierdea primul cod de după nume (de regulă handle-ul, în R12 fără handle-uri layerul) | Referințe spre acea entitate rupte; în R12, entitatea ajungea pe layerul „0” | `libdxfrw.cpp` (`processEntities`) |
| Constructorul de copiere `DRW_Entity(const DRW_Entity&)` nu copia numele culorii (430), datele de aplicație (102) și grafica proxy (310) și lăsa neinițializate câmpurile numerice folosite la citirea DWG | Atributele unui INSERT (ținute prin copiere) își pierdeau numele culorii; găsit de testele stratului .NET | `drw_entities.h` |
| VERTEX 2D din DWG: din cauza unor acolade lipsă, ID-ul vertexului (BL, existent doar din 2010) se citea la toate versiunile | Direcția tangentei (cod 50) citită decalat în DWG R2000–R2007 | `drw_entities.cpp` |
| DIMSTYLE: DIMLWD și DIMLWE (371, 372) erau scrise, dar nu și citite din DXF | Grosimile liniilor de cotă se pierdeau la recitire | `drw_objects.cpp` |

Rezultat pe lot: **toate cele 9.735 de DWG-uri se citesc complet** (0 PARTIAL, față de 66), fără referințe de bloc rupte și fără blocuri duplicate.

### Găsite prin aplicația BatchPrint (2 octombrie 2026)

Găsite la trecerea aplicației BatchPrint de la IxMilia.Dxf la DxfRw: prima prin compararea desenelor randate cu cele două biblioteci, a doua prin compararea unei planșe tipărite cu AutoCAD.

| Problemă | Efect | Fișiere |
|---|---|---|
| `DRW_LType::reset()` nu golea lista elementelor (codul 49), iar `dxfRW::processLType` folosește același obiect pentru tot tabelul LTYPE | Fiecare tip de linie dintr-un DXF moștenea elementele tuturor tipurilor de dinaintea lui: model și lungime (40) greșite, iar un tip fără elemente (linie continuă) devenea întrerupt. Afecta 597 din cele 1.871 de DXF-uri ale lotului; DWG-ul nu era afectat (acolo fiecare tip de linie e un obiect nou) | `drw_objects.h` |
| MTEXT din DWG: direcția axei X era citită, dar steagul `haveXAxis` (pus doar la citirea DXF, codul 11) rămânea fals, așa că `updateAngle()` nu calcula unghiul | Orice MTEXT citit dintr-un DWG avea rotația 0: textele verticale ieșeau orizontale, iar numerele din bulinele de poziție ieșeau în afara lor | `drw_entities.cpp` |

La scriere, rotația MTEXT rămâne în codul 50, în grade. Referința DXF spune radiani, dar AutoCAD folosește grade (la fel tratează codul și ezdxf); AutoCAD însuși scrie vectorul axei X (11/21/31).

### Găsite de testul de rescriere al nivelului 2 FreeBASIC (3 octombrie 2026)

Testul `lot_oop ... rescriere` scrie înapoi fiecare entitate (prin clasele FreeBASIC) și o compară cu cea recitită. Pe lotul de 7.049 de desene a găsit 1.439 de entități diferite după rescriere. Toate cele afișate erau polilinii cu un vertex a cărui direcție a tangentei era NaN, deși fișierele nu conțin codul 50.

| Problemă | Efect | Fișiere |
|---|---|---|
| Membri neinițializați: direcția tangentei vertecșilor POLYLINE (50), raza CIRCLE (40), unghiurile ARC (50/51), raportul și parametrii ELLIPSE (40/41/42), înălțimea TEXT/MTEXT/ATTRIB (40), lungimea indicatorului la cote (40) și `hdir`, `vertexnum` la LWPOLYLINE, handle-ul intrărilor de tabel | Codul 50 apare doar la vertecșii curve-fit, iar din DWG se citește doar la vertecșii 2D. Aproape orice vertex POLYLINE din DXF și orice vertex 3D sau polyface din DWG primea o valoare aleatoare din memorie, uneori NaN. La celelalte câmpuri erau afectate doar fișierele cărora le lipsește codul. Acum valorile implicite sunt 0, iar elipsa este completă (raport 1, parametri 0…2π) | `drw_entities.h`, `drw_objects.h` |
| `DRW_Vertex(x, y, z, bulge)` (folosit la conversia ELLIPSE → POLYLINE în R12) nu seta tipul VERTEX | Tipul rămânea POINT | `drw_entities.h` |

Valorile aleatoare care nu erau NaN treceau neobservate: o valoare copiată exact se compară egal cu ea însăși. Grupul de teste „coduri de grup lipsă” din `freebasic/tests` citește un DXF scris de mână fără aceste coduri și găsea, cu DLL-ul vechi, de exemplu `TangentDirection = 1.0e-316`.

### Teste ale bibliotecii actualizate

`tests/test_attributes.cpp` verifica explicit vechea limitare („scara de linetype nu se scrie, trebuie să rămână 1.0”). Acum verifică faptul că valoarea 2.5 se păstrează.

`tests/test_tables.cpp` (`testLineTypes`) număra doar tipurile de linie recitite; acum verifică și modelul fiecăruia (cu vechiul `reset()`, `DOTTED` ieșea cu elementele lui `DASHED`).

`tests/test_text.cpp` are un test nou, `testDwgMTextRotation`: construiește bit cu bit două entități MTEXT în format DWG R2000 (verticală și la 30°) și verifică rotația citită, fără să aibă nevoie de un fișier DWG.

`tests/test_entities.cpp` are un test nou, `testDefaultValues`: construiește entitățile (și o intrare de tabel) peste memorie umplută cu un tipar nenul și verifică valorile implicite ale câmpurilor citite din coduri opționale. Fără corectura membrilor neinițializați pică la toate cele 13 verificări. Umplerea se face prin scrieri `volatile`, pentru că gcc consideră moarte scrierile obișnuite făcute înaintea constructorului (`-flifetime-dse`) și le poate elimina.

## Corecturi făcute în shim (nu în bibliotecă)

Acestea compensează comportamente ale libdxfrw pe care patch-ul nu le schimbă, pentru compatibilitate cu alți utilizatori ai bibliotecii:

- `DRW_Dimension(const DRW_Dimension&)` forțează tipul generic `DIMENSION`, așa că toate cotele copiate își pierdeau tipul și erau omise la salvare (aceeași problemă o are și `dwg2dxf`). Shim-ul restaurează tipul real.
- `DRW_Entity(const DRW_Entity&)` nu copia `colorName` și `appData` (corectat acum și în bibliotecă; compensarea din shim a rămas, fără efect).
- `DRW_Polyline`, `DRW_Spline`, `DRW_Hatch` și `DRW_Leader` au copiere superficială și destructori care nu eliberează sub-obiectele. Shim-ul preia proprietatea explicit.
- Unele șiruri din DWG R14–2004 (nume de blocuri, INSERT, HATCH, cote, text) conțin terminatorul NUL.
- `*Model_Space` și `*Paper_Space` (în R12: `$Model_Space` și `$Paper_Space`) sunt scrise de bibliotecă; shim-ul nu le mai trimite a doua oară. Înainte, un fișier R12 salvat ca 2018 ajungea cu blocurile de layout duplicate.
- În R12, layout-urile suplimentare (`*Paper_Space0`…) nu se scriu (nu au echivalent în R12).
- Cotele fără bloc de geometrie (create prin API) primesc la salvare un bloc anonim gol, regenerat de programul CAD la deschidere.
- În R12, flag-ul „are atribute” al blocurilor se potrivește cu prezența ATTDEF-urilor.
- În DXF, referințele MULTILEADER (stiluri, tipuri de linie, blocuri, ATTDEF-uri) sunt handle-uri; shim-ul le rezolvă în nume după citire.
- La salvare se calculează extinderea reală a spațiului model (`$EXTMIN`/`$EXTMAX`, cu transformarea OCS → WCS). Dacă documentul nu are o vedere activă (desene create prin API), se scrie una centrată pe desen, în locul vederii implicite de 5 unități a bibliotecii.

## Limitări rămase ale libdxfrw (nemodificate)

- Entități încă nerecunoscute (și absente din lotul de producție): MLINE, 3DSOLID/REGION/BODY (date ACIS criptate), TABLE, WIPEOUT, TOLERANCE, SHAPE, OLE2FRAME.
- Secțiunea OBJECTS e în mare parte nepăstrată: layout-urile suplimentare (`*Paper_Space0`…) rămân orfane; grupurile, dicționarele, ordinea de desenare (SORTENTSTABLE), decuparea blocurilor (XCLIP), asocierea cotelor (DIMASSOC), stilurile MLEADERSTYLE și parametrii blocurilor dinamice se pierd.
- MULTILEADER: săgețile per braț din DWG ≤ 2007 și atributele multi-linie (R2018, cu MTEXT încorporat) nu se păstrează; atributele multi-linie se citesc ca text pe un rând.
- XDATA se citește doar pe entități (nu și pe intrările de tabel), iar pe ATTRIB nu este expus în API-ul C (se păstrează la salvare).
- DWG este doar citit, nu scris.
- Din DWG, la stilurile de cotă (DIMSTYLE) se citește doar numele; variabilele DIM* rămân la valorile implicite (din DXF se citesc toate).
- Ponderile spline-urilor raționale (cod 41) nu sunt suportate.
- Grosimea TEXT nu se scrie.
- La salvare se scriu doar variabilele de header cunoscute de bibliotecă.
- Handle-urile entităților sunt renumerotate la salvare.
- În R12 nu se scriu RAY, XLINE, VIEWPORT, SPLINE, MTEXT, HATCH, LEADER, IMAGE, MULTILEADER (inexistente în R12); LWPOLYLINE și ELLIPSE devin POLYLINE. MULTILEADER nu se scrie nici în R14–2004.
- Din DWG-urile R14 nu se citesc LWPOLYLINE și HATCH (observat pe mostra de test; lotul nu conține DWG R14).
