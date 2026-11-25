// [z]f — file explorer per Windows. Win32 + Direct3D 11, nessun framework UI.
//
// Unity build: questo è l'unico file che si compila; include i moduli nell'ordine in cui
// dipendono l'uno dall'altro. Compilazione veloce, ottimizzazione su tutto il programma,
// nessun costo di collegamento. Le dichiarazioni condivise stanno in app.h.
//
// Regola d'oro: il thread dell'interfaccia non tocca mai il disco.

#include "base.h"     // include di sistema, tempo, registro dei rallentamenti
#include "theme.h"    // colori, misure e tempi dell'interfaccia (regole in DESIGN.md)
#include "gfx.cpp"    // D3D11: device, swapchain, shader, quad
#include "text.cpp"   // glifi, testo, casella di testo, appunti
#include "listing.cpp"// lettura cartelle e ordinamento
#include "zip.cpp"    // archivi zip in sola lettura: elenco, inflate, estrazione
#include "lnk.cpp"    // lettore dei collegamenti (.lnk), senza la shell
#include "app.h"      // stato e dichiarazioni condivise
#include "app.cpp"    // layout, selezione, navigazione, ricariche
#include "dirsize.cpp" // dimensione totale delle cartelle (tooltip, anteprima, colonna)
#include "links.cpp"  // collegamenti (.lnk): destinazione letta su un thread, colonna Destinazione
#include "watch.cpp"  // aggiornamento in tempo reale
#include "icons.cpp"  // icone della shell
#include "render.cpp" // disegno di un frame
#include "shell.cpp"  // comandi e menu della shell, operazioni su disco
#include "settings.cpp" // impostazioni e salvataggio dei file di [z]f
#include "tabs.cpp"   // schede
#include "sidebar.cpp"// barra laterale: preferiti e unità
#include "integration.cpp" // [z]f al posto di Explorer (registro HKCU)
#include "filedlg.cpp" // dialoghi Apri e Salva nella cartella di [z]f
#include "crumbs.cpp" // percorso a segmenti cliccabili
#include "chrome.cpp" // cornice: barra del titolo con le schede, pulsanti
#include "tooltip.cpp" // informazioni su ciò che sta sotto il mouse
#include "preview.cpp" // pannello di anteprima: testo, immagini, cartelle
#include "thumbs.cpp" // vista a griglia: miniature, vista per cartella
#include "fileops.cpp" // operazioni sui file con l'esito di ogni elemento, annulla e ripeti
#include "menu.cpp"   // menu contestuale nostro
#include "options.cpp" // pannello delle opzioni, scorciatoie, versione
#include "goto.cpp"   // "Vai a": ricerca fuzzy su cartelle recenti, preferiti, sottocartelle
#include "drag.cpp"   // trascinamento dei file, da e verso [z]f
#include "zipout.cpp" // dallo zip verso fuori: aprire, copiare, trascinare (estraendo su un thread)
#include "listmouse.cpp" // selezione col rettangolo, colonne ridimensionabili
#include "hybrid.cpp" // WARP all'avvio, poi GPU
#include "input.cpp"  // tastiera, mouse, messaggi
#include "main.cpp"   // avvio
