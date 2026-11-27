// [z]f — tema: colori, misure e tempi dell'interfaccia. Le regole d'uso sono in DESIGN.md.
//
// Ogni valore visivo ha un nome qui. Il codice nuovo usa questi nomi, mai numeri sparsi;
// i valori sparsi rimasti nel codice più vecchio si migrano quando si tocca quel codice.
// Le misure sono pixel a scala 100%: nel codice si usano con px() (moltiplica per la scala DPI
// e arrotonda per difetto, così i bordi cadono su pixel interi).

// --- colori -----------------------------------------------------------------------------
// Superfici, dalla più scura: barre di contorno → sfondo → barre → menu
static const uint32_t C_INPUT      = rgba(0x14, 0x15, 0x17); // fondo delle caselle di testo
static const uint32_t C_TABBAR     = rgba(0x17, 0x18, 0x1a); // barra schede e barra laterale
static const uint32_t C_BG         = rgba(0x1b, 0x1c, 0x1e); // sfondo della lista
static const uint32_t C_HEADER     = rgba(0x20, 0x21, 0x24); // intestazione delle colonne
static const uint32_t C_BAR        = rgba(0x23, 0x24, 0x27); // barra del percorso, di stato, scheda attiva
static const uint32_t C_MENU       = rgba(0x27, 0x28, 0x2c); // menu, tooltip, pannelli sovrapposti
static const uint32_t C_LINE       = rgba(0x30, 0x32, 0x36); // separatori e bordi (1 px)
static const uint32_t C_SHADOW     = rgba(0, 0, 0, 90);      // ombra dei pannelli sovrapposti
// Stati
static const uint32_t C_HOVER      = rgba(0x29, 0x2b, 0x2f); // sotto il mouse
static const uint32_t C_HOVER_STRONG = rgba(0x3a, 0x3d, 0x42); // sotto il mouse, su un controllo piccolo (×)
static const uint32_t C_HOVER_PANEL = rgba(0x33, 0x35, 0x39); // sotto il mouse nei pannelli sovrapposti: su C_MENU si vede quanto C_HOVER su C_BG (1,2:1)
static const uint32_t C_SEL        = rgba(0x1c, 0x3d, 0x5a); // selezionato / corrente
static const uint32_t C_SEL_HOVER  = rgba(0x23, 0x4a, 0x6b); // selezionato e sotto il mouse
static const uint32_t C_SEL_EDGE   = rgba(0x3a, 0x8e, 0xd0); // cursore da tastiera
static const uint32_t C_SEL_TEXT   = rgba(0x2a, 0x5f, 0x93); // selezione dentro una casella di testo
static const uint32_t C_MARQUEE    = rgba(0x3a, 0x8e, 0xd0, 48); // fondo del rettangolo di selezione (C_SEL_EDGE trasparente)
static const uint32_t C_SCROLL     = rgba(0x4a, 0x4d, 0x52); // barra di scorrimento, barra d'uso
// Testo: la gerarchia si fa con il colore, non con dimensione o grassetto
static const uint32_t C_TEXT       = rgba(0xdc, 0xdc, 0xdc); // primario
static const uint32_t C_DIM        = rgba(0x8c, 0x8f, 0x94); // secondario (date, etichette, scorciatoie)
static const uint32_t C_DIM_SEL    = rgba(0xb0, 0xb3, 0xb8); // secondario su una voce selezionata: C_DIM lì è 3,5:1 (2,9 sotto il mouse), questo 5,4 e 4,4
static const uint32_t C_FAINT      = rgba(0x62, 0x65, 0x6a); // terziario, disattivato, file nascosti
static const uint32_t C_ACCENT     = rgba(0x4a, 0xa3, 0xff); // attivo, informazione, filtro
static const uint32_t C_ERR        = rgba(0xff, 0x6b, 0x6b); // errore, spazio quasi esaurito
// Segnaposto delle icone (prima che arrivi l'icona vera)
static const uint32_t C_FOLDER     = rgba(0xe8, 0xb8, 0x4a);
static const uint32_t C_FILE       = rgba(0x8a, 0x90, 0x98);
// Icone e immagini (texture): il colore le moltiplica, quindi il bianco le disegna come sono.
// I file nascosti hanno l'icona al 43% di opacità.
static const uint32_t C_ICON        = rgba(0xff, 0xff, 0xff);
static const uint32_t C_ICON_HIDDEN = rgba(0xff, 0xff, 0xff, 110);
// Elementi singoli
static const uint32_t C_CLOSE      = rgba(0xc4, 0x2b, 0x1c); // pulsante chiudi della finestra sotto il mouse (il rosso di Windows)
static const uint32_t C_ON_CLOSE   = rgba(0xff, 0xff, 0xff); // il suo simbolo
static const uint32_t C_KNOB_ON    = rgba(0x10, 0x10, 0x10); // pallino di un interruttore acceso, sul fondo C_ACCENT
static const uint32_t C_SEL_EDGE_PARENT = rgba(0x3a, 0x8e, 0xd0, 153); // pillola dell'unità che contiene la cartella aperta (C_SEL_EDGE al 60%)

namespace ui {
// --- misure (px a scala 100%) -----------------------------------------------------------
constexpr float GAP_XS = 2, GAP_S = 4, GAP_M = 8, GAP_L = 12, GAP_XL = 16; // spaziature
constexpr float EDGE = 2;          // barre d'accento: cursore, scheda attiva, barra d'uso
// Angoli arrotondati come in Windows 11 (Fluent): 8 px per i contenitori sovrapposti (menu,
// pannelli, schede), 4 px per gli elementi nella pagina (righe, pulsanti, campi) e per i tooltip
constexpr float R_S = 4, R_M = 8;
constexpr float PILL_W = 3;        // indicatore del cursore: pillola verticale come in Windows 11
// Altezza riga = altezza del testo + ROW_EXTRA. Eccezione voluta a px(): si arrotonda al più vicino
// (layout, app.cpp), che dà comunque pixel interi; per difetto le righe sarebbero 1 px più basse a
// 150%, 175% e 250%, rispetto a come [z]f è sempre stato
constexpr float ROW_EXTRA = 9;
constexpr float PATH_BAR = 1.45f;  // altezza della riga del percorso, in righe
constexpr float TAB_BAR = 1.35f;   // altezza della barra del titolo (schede), in righe
constexpr float TAB_TOP = 5;       // spazio sopra le schede, nella barra del titolo
constexpr float CAPTION_W = 46;    // pulsanti riduci/ingrandisci/chiudi (come in Windows)
constexpr float NAV_W = 1.25f;     // pulsanti della riga del percorso (▯ ← → ↑ ☆ ◨), in righe
constexpr float NAV_MENU_W = 0.75f; // ˅ accanto a ◨ (posizione dell'anteprima), in righe: metà pulsante doppio
constexpr float DRAG_MIN = 48;     // spazio minimo per trascinare la finestra nella barra del titolo
constexpr float PAD_X = 12;        // margine orizzontale dei contenuti
constexpr float SCROLL_W = 10, THUMB_MIN = 24;
// Barra laterale: larghezza predefinita e minima (impostazioni.txt la cambia), massima in frazione della finestra
constexpr float SIDE_W = 210, SIDE_MIN = 150, SIDE_MAX = 0.35f;
constexpr float SIDE_PAD = 10;     // barra laterale: margine orizzontale delle voci
constexpr float SIDE_SECTION_H = 1.25f; // barra laterale: titolo di sezione ("Preferiti"), in righe
constexpr float SIDE_INDENT = 16;  // barra laterale: rientro dei preferiti sotto il titolo e delle unità sotto "Questo PC" (un livello dell'albero)
constexpr float DATE_W = 150, SIZE_W = 100;     // colonne: larghezze predefinite (impostazioni.txt le cambia); DATE_W anche Data creazione
constexpr float TYPE_W = 110, ATTR_W = 80;      // colonne Tipo (ci sta "Percorso di rete" di Questo PC) e Attributi
constexpr float LINK_W = 300;      // colonna Destinazione (cartelle con collegamenti): larghezza predefinita
constexpr float LINK_MAX = 0.5f;   // colonna Destinazione: al massimo questa frazione dello spazio tra l'inizio della lista e la colonna dopo (mai più larga di Nome)
constexpr float COL_MIN = 48;      // colonne dopo Nome: larghezza minima
constexpr float COL_NAME_MIN = 160; // spazio minimo della colonna Nome, dall'inizio della lista
constexpr float COL_GRIP = 4;      // bordo tra due colonne: fascia per afferrarlo, per lato
constexpr float GROUP_GAP = 8;     // gruppi della lista: spazio prima di ogni gruppo tranne il primo, lungo l'asse dello scorrimento
constexpr float TAB_MIN = 70, TAB_MAX = 220;    // larghezza di una scheda
constexpr float TAB_TEXT_X = 10;   // titolo della scheda: margine a sinistra
constexpr float TAB_SEP_INSET = 6; // separatore tra due schede: staccato di tanto da sopra e da sotto
constexpr float TAB_PLUS_W = 1.2f, TAB_CLOSE_W = 0.9f; // pulsanti + e × delle schede, in righe
constexpr float TAB_CLOSE_INSET_Y = 3; // fondo della × sotto il mouse: rientro in alto e in basso
constexpr float MENU_PAD_X = 14, MENU_PAD_Y = 5, MENU_SEP = 9, MENU_KEYS_GAP = 40;
constexpr float TIP_PAD = 10, TIP_MAX_W = 520, TIP_COL_GAP = 14, TIP_OFFSET_X = 12, TIP_OFFSET_Y = 22;
constexpr float TIP_ROWS_GAP = 6, TIP_OFFSET_ABOVE = 10; // tooltip: tra titolo e righe; sopra il mouse se sotto non c'è posto
constexpr float STATS_W = 440;     // pannello F12
// Pannello delle opzioni: misura massima, spazio dal bordo della finestra, distanza minima dall'alto
constexpr float OPT_W = 640, OPT_H = 720, OPT_MARGIN = 48, OPT_TOP_MIN = 24;
constexpr float OPT_LABEL_RESERVE = 200; // a destra dell'etichetta, lo spazio dei controlli
constexpr float OPT_VALUE_W = 56, OPT_FACE_W = 170, OPT_SEARCH_W = 240; // "13 px", nome del carattere, ricerca
constexpr float SWITCH_W = 32, SWITCH_H = 16, SWITCH_KNOB_INSET = 3; // interruttore (come Windows 11)
constexpr float FIELD_PAD = 6;     // margine interno delle caselle di testo
constexpr float GOTO_W = 600;      // pannello "Vai a": larghezza massima
constexpr int GOTO_ROWS = 12;      // pannello "Vai a": voci visibili prima di scorrere
constexpr float GOTO_NAME_MAX = 0.6f; // pannello "Vai a": spazio massimo del nome, il resto al percorso
constexpr float SHADOW_X = 2, SHADOW_Y = 3;     // ombra dei pannelli sovrapposti
constexpr float ENTER_DY = 4;      // pannelli che compaiono: partono qualche pixel più in alto/basso
// Cartella nuova che entra: parte LIST_ENTER_DY più in basso e al 25% di opacità (lo sfondo la copre
// al 75%); cambiando scheda la dissolvenza parte da LIST_FADE_TAB (55% di opacità)
constexpr float LIST_ENTER_DY = 6, LIST_ENTER_COVER = 0.75f, LIST_FADE_TAB = 0.4f;
constexpr float FILTER_W = 300, FILTER_MAX = 0.4f; // campo del filtro: larghezza, massimo in frazione della barra di stato
constexpr float FILTER_MIN = 2;   // sotto questa larghezza, in altezze del campo, il filtro non si disegna (lista stretta)
constexpr float MENU_KEYBOARD_X = 48; // menu aperto da tastiera sullo sfondo o in elenco: distanza dall'inizio della lista
constexpr float WINDOW_W = 1000, WINDOW_H = 680;   // finestra nuova
constexpr float WINDOW_MIN_W = 420, WINDOW_MIN_H = 240;
constexpr float ICON_FONT = 1.15f; // font delle icone (⚙, ←, ×…): un po' più grande del testo, come nelle app di Windows
constexpr float LINE_GAP = 3;      // interlinea del testo su più righe (tooltip, testo nell'anteprima)
constexpr float LIST_ICON = 16;    // icone della shell in lista, barra laterale e menu (arrotondate alla scala DPI)
// Segnaposto delle icone in lista (prima che arrivi l'icona vera), in frazioni del lato dell'icona:
// cartella = linguetta (da PH_TAB_Y, larga PH_TAB_W, alta PH_TAB_H) più corpo (da PH_BODY_Y, largo
// tutto il lato, alto il lato meno PH_BODY_CUT); file = foglio (da PH_FILE_X, largo PH_FILE_W, alto tutto)
constexpr float PH_TAB_Y = 0.12f, PH_TAB_W = 0.45f, PH_TAB_H = 0.2f;
constexpr float PH_BODY_Y = 0.25f, PH_BODY_CUT = 0.3f;
constexpr float PH_FILE_X = 0.15f, PH_FILE_W = 0.7f;
// Pannello di anteprima: larghezza (a destra) e altezza (in basso) predefinite e minime
// (impostazioni.txt le cambia), massime in frazione della finestra; in basso la lista sopra tiene
// almeno PREVIEW_LIST_ROWS righe; icona grande per i file senza anteprima
constexpr float PREVIEW_W = 360, PREVIEW_MIN = 220, PREVIEW_MAX = 0.5f;
constexpr float PREVIEW_H = 280, PREVIEW_HMIN = 140;
constexpr int PREVIEW_LIST_ROWS = 4;
constexpr float PREVIEW_ICON = 96;
// Vista a icone: lato del riquadro delle miniature per ogni livello di zoom, da 20% a 100% (Ctrl+rotellina;
// Ctrl+Shift+4/5/6 portano a 40, 70 e 100%: ZOOM_SMALL, ZOOM_MEDIUM, ZOOM_LARGE in app.h); icona
// dei file senza miniatura in frazione del riquadro; righe del nome sotto la miniatura
constexpr float THUMB_PX[9] = { 64, 80, 96, 112, 128, 160, 192, 224, 256 };
constexpr float THUMB_ICON = 0.5f;
constexpr int GRID_NAME_LINES = 2;
constexpr float LIST_COL_W = 300;  // vista Elenco (zoom 10%): larghezza di una colonna di voci
// Anello di punti di un lavoro in corso (controllo dell'aggiornamento): punti fermi, ne gira solo
// la luminosità, come l'anello di avanzamento di Windows. Diametro in frazione dell'icona
constexpr int SPIN_DOTS = 8;
constexpr float SPIN_DOT = 3, SPIN_SIZE = 1.0f, SPIN_MIN_ALPHA = 0.2f;
// Pallino su ⚙ quando c'è una versione nuova pronta: diametro, distanza dall'angolo in alto a destra
constexpr float BADGE = 6, BADGE_INSET = 8;

// --- testo ------------------------------------------------------------------------------
constexpr int FONT_PX = 13, FONT_MIN = 10, FONT_MAX = 24; // una sola dimensione in tutta l'interfaccia

// --- movimento (ms) ---------------------------------------------------------------------
// Costanti di tempo per gli avvicinamenti esponenziali (al 95% in circa 3×)
constexpr float TAU_HOVER_IN = 20, TAU_HOVER_OUT = 50; // evidenziazione al passaggio del mouse
constexpr float TAU_CURSOR = 24;   // cursore che scivola tra le righe
constexpr float TAU_LIST_IN = 35;  // cartella nuova che entra
constexpr float TAU_TAB_LINE = 30; // indicatore della scheda attiva
constexpr float TAU_SCROLL = 45;   // scorrimento con la rotellina
constexpr int WHEEL_LINES = 3;     // righe per scatto della rotellina (lista, menu, pannelli; non Icone ed Elenco, che scorrono di una riga o colonna di voci)
// Durate fisse (ease-out)
constexpr float DUR_MENU = 90, DUR_TIP = 80, DUR_ICON = 110;
// Unica animazione continua (DESIGN.md §5): un giro dell'anello di punti, mentre un lavoro è in
// corso e almeno per un giro (un lavoro breve non è un lampo)
constexpr float SPIN_PERIOD = 900;
// Tooltip e avvisi
constexpr int TIP_DELAY = 500, TIP_DELAY_NEAR = 60, TIP_INFO_WAIT = 150, TIP_NEAR_WINDOW = 400;
constexpr int NOTICE_INFO = 2500, NOTICE_ERROR = 4000;
constexpr int NOTICE_RENEW = NOTICE_INFO / 2; // un avviso che vale finché dura un lavoro ("Estrazione…") si rinnova prima di scadere
constexpr int LOADING_AFTER = 150; // "caricamento…" solo se la lettura supera questo tempo
// Anteprima: una lettura che dura più di così (rete) non trattiene la successiva, che parte su
// un altro thread
constexpr int PREVIEW_SLOW = 300;
// Trascinamento dei file
constexpr int DRAG_SPRING = 500;   // fermi su una scheda: si apre
constexpr int DRAG_TICK = 16;      // passo del timer durante il trascinamento (scorrimento, disegno)
constexpr float DRAG_SCROLL = 0.35f; // scorrimento automatico vicino ai bordi, in righe per passo
constexpr float DRAG_GAP = 0.25f;  // tra i preferiti: fascia in alto e in basso di una voce che inserisce
// Selezione col rettangolo: scorre come il trascinamento (DRAG_SCROLL), più veloce quanto più il
// mouse va oltre il bordo, fino a questo multiplo
constexpr float MARQUEE_SCROLL_MAX = 6;
// Lavoro in background
constexpr int WATCH_BATCH = 50;    // notifiche della cartella aperta raggruppate: al massimo una ricarica ogni tanto
constexpr int SIDE_RELOAD = 5000;  // tornando a [z]f, unità e preferiti si rileggono se più vecchi di così
constexpr int ZIP_TEMP_KEEP = 3 * 24 * 3600 * 1000; // file estratti da uno zip (%TEMP%\z-f): tolti all'estrazione dopo questo tempo
// Dimensione delle cartelle (dirsize.cpp): un totale vale DS_FRESH, poi una richiesta lo riconta;
// il tooltip vede il parziale ogni DS_PROGRESS; la cache oltre il suo limite si pulisce
// al massimo ogni DS_TRIM_EVERY
constexpr int DS_FRESH = 60000, DS_PROGRESS = 100, DS_TRIM_EVERY = 15000;
// Salto dei dialoghi Apri e Salva (filedlg.cpp, sul suo thread): ogni FD_POLL si guarda se il
// dialogo è pronto, per al massimo FD_WAIT_MAX; poi FD_SETTLE di margine (prima, il comando fa
// cadere il programma del dialogo). Attesa massima di una risposta del dialogo: FD_MSG_WAIT per il
// campo del nome, FD_OK_WAIT per Apri/Salva (che entra nella cartella)
constexpr int FD_POLL = 25, FD_WAIT_MAX = 3000, FD_SETTLE = 150;
constexpr int FD_MSG_WAIT = 500, FD_OK_WAIT = 2000;
// Appunti aperti da un altro programma (rdpclip, un gestore degli appunti): i thread che scrivono e
// leggono il testo (text.cpp) riprovano ogni CLIP_RETRY, per al massimo CLIP_WAIT_MAX, come fanno
// Windows Forms e WPF (10 volte, 100 ms)
constexpr int CLIP_RETRY = 50, CLIP_WAIT_MAX = 1000;
// Avvio con la finestra principale non ancora creata (due avvii quasi insieme, main.cpp): chi arriva
// secondo la cerca ogni FWD_POLL, per al massimo FWD_WAIT, poi diventa lui la principale
constexpr int FWD_POLL = 10, FWD_WAIT = 3000;
// Aggiornamento (update.cpp): primo controllo così tanto dopo il primo frame, poi al massimo uno
// ogni UPD_EVERY secondi (anche a [z]f aperto: la condizione si guarda ogni UPD_TICK ms)
constexpr int UPD_DELAY = 30000, UPD_TICK = 3600 * 1000;
constexpr int UPD_DELAY_TEST = 2000; // con --aggiorna-da (server di prova)
constexpr int UPD_OLD_WAIT = 10000;  // dopo "Aggiorna": attesa massima che la versione di prima esca (sul thread)
constexpr long long UPD_EVERY = 24 * 3600;
constexpr int UPD_TIMEOUT = 10000, UPD_TIMEOUT_READ = 30000; // rete: risoluzione, connessione, invio · lettura
constexpr int UPD_JSON_MAX = 2 << 20, UPD_EXE_MAX = 64 << 20; // risposte più grandi si scartano

// --- registro dei rallentamenti (z-f-log.txt, ms) -----------------------------------------
// Oltre queste soglie un tempo si scrive con l'ora. Il riferimento è un frame a 60 Hz.
constexpr double LOG_FRAME = 16;   // frame (CPU), Present, atlante delle miniature
constexpr double LOG_VSYNC = 34;   // attesa del vsync: due frame persi
constexpr double LOG_MSG = 25;     // un messaggio, la shell sul thread UI, un passo del trascinamento
constexpr double LOG_MSG_F12 = 8;  // messaggio contato come lento in F12 (non scritto nel registro)
constexpr double LOG_MENU = 50;    // menu completo della shell e suoi comandi, sul loro thread (costano sempre: si segna solo se esagera)
constexpr double LOG_GOTO = 8;     // ricerca di "Vai a"
constexpr double LOG_ZIP = 100;   // zip: lettura della directory centrale, estrazione (sul loro thread)
} // namespace ui
