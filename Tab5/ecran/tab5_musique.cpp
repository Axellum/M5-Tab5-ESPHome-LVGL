/**
 * [AI-CONTEXT]
 * @file tab5_musique.cpp
 * @role Musique de toute la maison (ADR-0067, 18/10/2026, demande d'Axel) : les pages du
 *       popup Musique (lecteur_popup.yaml, ADR-0050) au-delà de « Lecture », peintes d'après
 *       ce que pousse Home Assistant (lu par musique_pieces_lire() / musique_liste_lire(),
 *       tab5_parse.h, section 9 bis) :
 *         - les pages et leurs noms sur la ligne du titre : la brique des popups à pages
 *           (tab5_pages.cpp, ADR-0046 : un seul glissement, pages_brancher), chaque page
 *           seulement quand elle a quelque chose à montrer — Lecture toujours ; Playlists
 *           dès une playlist favorite (ADR-0068, la grille peinte par tab5_lecteur.cpp, le
 *           retour à Lecture 600 ms après un appui) ; Pièces dès deux pièces ; Bibliothèque avec Music Assistant (ou,
 *           sans lui, le temps d'y choisir une source : « Sources ») ;
 *         - Pièces : une carte par lecteur (8 au plus, 4 × 2), volume (envoyé au relâcher),
 *           grouper au lecteur montré / l'en séparer, marche, « y envoyer la musique » ;
 *           un toucher montre la pièce sur la page Lecture, un appui long ouvre sa roue
 *           (ADR-0036 : source, muet, musique ici, séparer) ; « Tout ici » et « Tout
 *           séparer » avec Music Assistant ;
 *         - Bibliothèque : sept catégories, une grille de 5 × 2 éléments (pochette, nom,
 *           2e ligne), un toucher qui joue (titre, radio, épisode, livre) ou qui ouvre
 *           (artiste, album, playlist, podcast : bouton retour, pile de kPileMax niveaux),
 *           un appui long qui propose maintenant, à la suite, en fin de file, en radio ;
 *           « Chargement… » puis, sans réponse en kAttenteMs, une phrase courte ;
 *         - les pochettes : dix online_image (musique_pochette.yaml), une à la fois depuis
 *           le premier élément, seulement page montrée ; un échec garde l'icône du genre ;
 *         - sur la page Lecture : « À suivre · titre — artiste » et « n titres » sous
 *           l'album (le morceau remonte un peu pour lui faire place), la puce « + n »
 *           après le nom du lecteur quand il mène un groupe.
 *       Rien en NVS : HA repousse les pièces à l'ouverture (événement « ouvrir »).
 * @architecture_constraint Push-only, events-only (ADR-0001, ADR-0025) : la tablette dit
 *       « la pièce de rang i » (son rang dans la dernière liste poussée, « -1 » le lecteur
 *       montré) et renvoie la ref que HA lui a donnée ; jamais une entité ni une action.
 *       Couleurs par la palette (UIColor.X) et musique_rejouer_theme() (ADR-0029). Glyphes
 *       posés d'ici (règle 7) : glyphe_piece, glyphe_element, glyphe_son, glyphe_lien
 *       (MDI_CODE_TARGETS de tools/check_tab5_code_rules.py). Changement de page INSTANTANÉ
 *       (préférence d'Axel) : on cache et on montre, rien ne glisse ; jamais un second
 *       gestionnaire de geste (pages_brancher seulement).
 * @ai_warning [AI-WARNING] Une pochette ne se télécharge que dans un emplacement dont
 *       l'image est MASQUÉE : online_image réalloue son tampon au téléchargement, une
 *       lv_image visible qui le montrerait lirait de la mémoire libérée. Même règle pour la
 *       libération (emplacements vides). Un résultat arrivé pour une liste remplacée est
 *       jeté (l'URL demandée n'est plus celle de l'élément) et l'emplacement retéléchargé.
 * @ai_instruction Un widget de plus = son champ dans MusiqueUI (tab5_musique.h) et sa ligne
 *       dans le script tab5_musique_lier (tab5-musique.yaml). Une page de plus = une valeur
 *       de Page, son cas dans page_disponible() et page_nom(), un onglet de plus
 *       (kMusiqueOnglets, musique_calques.yaml).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl_private.h"  // lv_image_cache_drop() (cache d'images, hors de lvgl.h en 9.5)
#include <algorithm>
#include <cstdio>
#include <cstring>

MusiqueUI g_musique_ui;

namespace {

static_assert(kMusiquePieces == kMusiquePiecesMax, "une carte par pièce poussée");
static_assert(kMusiqueElements == kMusiqueElementsMax, "une case de la grille par élément poussé");

constexpr const char* kTag = "tab5.musique";

// ─── Géométrie (coordonnées dans la carte du lecteur, 940 × 536 sur les deux cartes) ───
// Noms des pages sur la ligne du titre, collés à la croix (modal_header.yaml : 80 px à 10 px
// du bord ; ils finissent 12 px avant elle, comme kOngletsFin des popups à pages), chacun à
// la largeur de son nom ; la pastille de l'application passe à leur gauche.
constexpr int32_t kOngletsFinX = kLecteurCarteL - 102;
constexpr int32_t kOngletEcart = 10;
constexpr int32_t kOngletMarge = 16;  // de chaque côté du nom
constexpr int32_t kOngletMinL = 96;
constexpr int32_t kOngletMaxL = 200;
// Puce « + n » après le nom du lecteur : le titre de l'en-tête est à x 52 (modal_header.yaml),
// la puce à kGroupeEcart de son texte, kGroupeMarge de chaque côté de « + n ».
constexpr int32_t kTitreX = 52;
constexpr int32_t kGroupeEcart = 12;
constexpr int32_t kGroupeMarge = 12;
// Ligne « À suivre » : la colonne de droite du popup (x 408, 508 px, lecteur_popup.yaml).
// Le morceau remonte pour lui faire place sous l'album, au-dessus de la barre (y 208).
constexpr int32_t kColonneL = 508;
constexpr int32_t kFileEcart = 16;
constexpr int32_t kTitreY[2] = {84, 72};     // sans / avec la ligne « À suivre »
constexpr int32_t kArtisteY[2] = {130, 116};
constexpr int32_t kAlbumY[2] = {162, 146};
// Pièces : la rangée d'actions à y 72 (« Tout ici », « Tout séparer », 196 px chacun, le mot
// à x 50 : 136 px, mesurés assez pour les sept langues en Roboto 22), la
// grille dessous (4 × 2 cartes de 214 × 188, 12 px d'écart), centrée.
constexpr int32_t kMarge = 24;
constexpr int32_t kActionsL = 196;
constexpr int32_t kActionTexteX = 50;  // musique_bouton.yaml
constexpr int32_t kPieceL = 214;
constexpr int32_t kPieceH = 188;
constexpr int32_t kPieceEcart = 12;
constexpr int kPiecesParLigne = 4;
constexpr int32_t kGrilleY = 128;
constexpr int32_t kGrilleH = kLecteurCarteH - 20 - kGrilleY;  // 20 px d'air en bas
constexpr int32_t kPieceNomL = 146;   // musique_piece.yaml : x 54, 14 px à droite
constexpr int32_t kPieceEtatL = 186;
constexpr int32_t kConseilL = kLecteurCarteL - kMarge - 2 * (kActionsL + 12) - kMarge;
// Bibliothèque : catégories, ou retour + titre, à y 72 ; flèches des pages à droite ; la
// grille de 5 × 2 éléments de 168 × 196 dessous (pochette de 144, nom, 2e ligne).
constexpr int32_t kRangeeL = kLecteurCarteL - 2 * kMarge;  // 892
constexpr int32_t kFlecheL = 48;
constexpr int32_t kFlechesL = 2 * kFlecheL + 8;
// Écart et marge mesurés : avec les flèches, les sept catégories tiennent en français,
// anglais, allemand, italien et espagnol ; coupées avec « … » en néerlandais et en turc.
constexpr int32_t kCategorieEcart = 6;
constexpr int32_t kCategorieMarge = 10;
constexpr int32_t kCategorieMinL = 72;
constexpr int32_t kTitreBiblioX = kMarge + kFlecheL + 12;
constexpr int32_t kElementL = 168;
constexpr int32_t kElementH = 196;
constexpr int32_t kElementPasX = (kRangeeL - kElementL) / 4;  // 181
constexpr int32_t kElementPasY = kElementH + 8;
constexpr int32_t kElementsY = 124;
constexpr int kElementsParLigne = 5;
constexpr int32_t kElementTexteL = 160;
static_assert(kElementsY + kElementPasY + kElementH <= kLecteurCarteH - 8, "deux rangées dans la carte");
static_assert(kGrilleY + 2 * kPieceH + kPieceEcart <= kLecteurCarteH - 8, "deux rangées de cartes dans la carte");

// ─── Temps ───
constexpr uint32_t kAttenteMs = 8000;      // « Chargement… » au-delà : la phrase d'erreur
constexpr uint32_t kRetourMs = 600;        // l'élément touché vu en accent, puis la page suivante
constexpr int kPileMax = 4;                // catégorie, puis trois niveaux ouverts (artiste → album)
constexpr size_t kUrlMax = 384;            // pochette (proxy de MA, googleusercontent…)

// Pages du popup, dans l'ordre de leurs onglets.
enum class Page : uint8_t { LECTURE,
                            PLAYLISTS,
                            PIECES,
                            BIBLIOTHEQUE,
                            NB };
constexpr int kPagesNb = static_cast<int>(Page::NB);
static_assert(kPagesNb == kMusiqueOnglets, "un onglet par page");

// Les sept catégories de la bibliothèque, dans l'ordre de leurs boutons.
constexpr MusiqueVue kCategories[kMusiqueCategories] = {
    MusiqueVue::FAVORIS, MusiqueVue::PLAYLISTS, MusiqueVue::ALBUMS, MusiqueVue::ARTISTES,
    MusiqueVue::RADIOS, MusiqueVue::PODCASTS, MusiqueVue::RECENTS};
constexpr const char* kCategoriesNoms[kMusiqueCategories] = {
    tr_noop("Favoris"), tr_noop("Playlists"), tr_noop("Albums"), tr_noop("Artistes"),
    tr_noop("Radios"), tr_noop("Podcasts"), tr_noop("Récents")};

// ─── État ───
struct Piece {
    MusiquePieceLue lu;  // Champ vides : seuls les textes ci-dessous sont lus
    char nom[kMusiqueNomMax + 1] = "";
    char titre[kMusiqueTitreMax + 1] = "";
};
struct Pieces {
    bool ma = false;
    int montre = -1;
    int file = 0;
    char suivant[kMusiqueTitreMax + 1] = "";
    int n = 0;
    Piece p[kMusiquePiecesMax];
};
Pieces s_p;
bool s_glisse[kMusiquePieces] = {};  // curseur de volume tenu : la poussée ne le bouge pas
int s_glisse_v[kMusiquePieces] = {};

struct Element {
    char nom[kMusiqueNomMax + 1] = "";
    char sous[kMusiqueNomMax + 1] = "";
    MusiqueElementGenre genre = MusiqueElementGenre::AUTRE;
    char ref[kMusiqueRefMax + 1] = "";
    char url[kUrlMax] = "";  // "" sans pochette (ou refusée)
};
struct Liste {
    MusiqueVue vue = MusiqueVue::INCONNUE;
    char titre[kMusiqueListeTitreMax + 1] = "";
    int page = 0;
    bool suite = false;
    bool retour = false;
    int n = 0;
    Element e[kMusiqueElementsMax];
};
Liste s_l;
bool s_liste_recue = false;

// Un niveau de la bibliothèque : une catégorie, ou le contenu d'une ref ouverte.
struct Niveau {
    MusiqueVue vue = MusiqueVue::FAVORIS;  // une catégorie, ou DETAIL
    char ref[kMusiqueRefMax + 1] = "";
    int page = 0;
};
Niveau s_pile[kPileMax];
int s_pile_n = 1;  // s_pile[0] : la catégorie, Favoris d'abord
// Sources d'une pièce (sa roue) : montrées dans la grille, hors de la pile.
bool s_sources = false;
int s_src_piece = -1;  // rang HA de la pièce
int s_src_page = 0;

// Demande en cours (« Chargement… ») et son genre attendu.
bool s_attente = false;
uint32_t s_attente_ms = 0;
MusiqueVue s_attendu = MusiqueVue::INCONNUE;
bool s_sans_reponse = false;  // kAttenteMs passées sans réponse
bool s_biblio_a_jour = false;  // demandée depuis l'ouverture du popup
int s_lance = -1;              // élément touché, en accent jusqu'au retour
lv_timer_t* s_retour = nullptr;
Page s_retour_depuis = Page::BIBLIOTHEQUE;  // la page qui doit encore être montrée
Page s_retour_page = Page::LECTURE;

// Pochettes : l'URL demandée par emplacement, prête ou en échec, le téléchargement en cours.
char s_img_url[kMusiqueElementsMax][kUrlMax] = {};
bool s_img_prete[kMusiqueElementsMax] = {};
bool s_img_echec[kMusiqueElementsMax] = {};
int s_dl = -1;

Page s_page = Page::LECTURE;
int s_roue_piece = -1;    // rang dans la grille de la pièce dont la roue est ouverte
int s_roue_element = -1;  // élément dont la roue est ouverte

const MusiqueUI& ui() { return g_musique_ui; }
bool popup_ouvert() {
    return g_lecteur_ui.popup != nullptr && !lv_obj_has_flag(g_lecteur_ui.popup, LV_OBJ_FLAG_HIDDEN);
}
bool lie() { return ui().calque != nullptr; }

void copier(char* dst, size_t cap, const Champ& c) { texte_ha_copier(dst, cap, c.p, c.n); }
// Copie brute d'un champ ASCII (ref, URL), coupée à `cap` - 1.
void copier_brut(char* dst, size_t cap, const Champ& c) {
    const size_t n = std::min(c.n, cap - 1);
    if (n > 0) std::memcpy(dst, c.p, n);
    dst[n] = '\0';
}

Piece* piece_rang(int index) {
    for (int k = 0; k < s_p.n; k++)
        if (s_p.p[k].lu.index == index) return &s_p.p[k];
    return nullptr;
}
Piece* montre() { return piece_rang(s_p.montre); }
bool a(const Piece& p, uint8_t f) { return (p.lu.fonctions & f) != 0; }
bool allumee(const Piece& p) { return p.lu.etat == MusiqueEtat::ALLUME || p.lu.etat == MusiqueEtat::PAUSE || p.lu.etat == MusiqueEtat::LECTURE; }

// Les mêmes règles que musique_membres / musique_meme_groupe (tab5_parse.h), sur l'état gardé.
int membres(int index) {
    if (piece_rang(index) == nullptr) return 0;
    int n = 0;
    for (int k = 0; k < s_p.n; k++)
        if (s_p.p[k].lu.index != index && s_p.p[k].lu.groupe == index) n++;
    return n;
}
bool meme_groupe(int a, int b) {
    const Piece* pa = piece_rang(a);
    const Piece* pb = piece_rang(b);
    return a != b && pa != nullptr && pb != nullptr && pa->lu.groupe >= 0 && pa->lu.groupe == pb->lu.groupe;
}

// ─── Glyphes (règle 7 : chacun rattaché à ses labels dans MDI_CODE_TARGETS) ───

const char* glyphe_piece(MusiqueGenre g) {
    switch (g) {
        case MusiqueGenre::TV: return "\U000F0502";        // television
        case MusiqueGenre::ENCEINTE: return "\U000F04C3";  // speaker
        case MusiqueGenre::ECRAN: return "\U000F04F6";     // tablet
        case MusiqueGenre::GROUPE: return "\U000F0D38";    // speaker-multiple
        default: return "\U000F0387";                      // music-note
    }
}

const char* glyphe_element(MusiqueElementGenre g) {
    switch (g) {
        case MusiqueElementGenre::ALBUM: return "\U000F0025";     // album
        case MusiqueElementGenre::ARTISTE: return "\U000F0803";   // account-music
        case MusiqueElementGenre::PLAYLIST: return "\U000F0CB8";  // playlist-music
        case MusiqueElementGenre::RADIO: return "\U000F0439";     // radio
        case MusiqueElementGenre::PODCAST: return "\U000F0994";   // podcast
        case MusiqueElementGenre::LIVRE: return "\U000F0067";     // book-music
        case MusiqueElementGenre::SOURCE: return "\U000F02FA";    // import
        default: return "\U000F0387";                             // music-note
    }
}

const char* glyphe_son(bool muet) {
    return muet ? "\U000F0581"   // volume-off
                : "\U000F057E";  // volume-high
}

const char* glyphe_lien(bool lie) {
    return lie ? "\U000F033A"   // link-variant-off : séparer
               : "\U000F0339";  // link-variant : grouper
}

// ─── Événements ───

void envoyer(const char* action, int piece, const char* valeur = "") {
    if (ui().envoyer == nullptr) return;
    char p[12];  // un int entier : aucune troncature possible (-Wformat-truncation)
    std::snprintf(p, sizeof(p), "%d", piece);
    ui().envoyer(action, p, valeur);
}

// ─── Pages ───

bool page_disponible(Page p) {
    switch (p) {
        case Page::LECTURE: return true;
        case Page::PLAYLISTS: return lecteur_playlists_nb() > 0;  // ADR-0068
        case Page::PIECES: return s_p.n >= 2;
        case Page::BIBLIOTHEQUE: return s_p.ma || s_sources;
        default: return false;
    }
}

int pages_visibles(Page out[kPagesNb]) {
    int n = 0;
    for (int i = 0; i < kPagesNb; i++)
        if (page_disponible(static_cast<Page>(i))) out[n++] = static_cast<Page>(i);
    return n;
}

const char* page_nom(Page p) {
    switch (p) {
        case Page::LECTURE: return tr_ctx("lecteur", "Lecture");
        case Page::PLAYLISTS: return tr("Playlists");
        case Page::PIECES: return tr("Pièces");
        // Sans Music Assistant, la page n'existe que pour choisir une source.
        case Page::BIBLIOTHEQUE: return s_p.ma ? tr("Bibliothèque") : tr("Sources");
        default: return "";
    }
}

int nombre_pages() {
    Page v[kPagesNb];
    return pages_visibles(v);
}

int page_courante() {
    Page v[kPagesNb];
    const int n = pages_visibles(v);
    for (int i = 0; i < n; i++)
        if (v[i] == s_page) return i;
    return 0;
}

void afficher(Page p);

void afficher_rang(int i) {
    Page v[kPagesNb];
    const int n = pages_visibles(v);
    if (i >= 0 && i < n) afficher(v[i]);
}

// Glissement gauche / droite du popup (tab5_pages.cpp), branché à l'ouverture.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_rang};

// ─── Peinture : en-tête et page Lecture ───

// L'en-tête (onglets, « + n ») et la ligne « À suivre » sont repeints avec le popup
// (lecteur_popup_repeindre → musique_onglets_peindre, musique_lecture_peindre).
void repeindre_lecteur() {
    if (popup_ouvert()) lecteur_popup_repeindre();
}

// ─── Peinture : page Pièces ───

// « Volume 45 % » dans la langue de l'écran, sans std::string (règle 4 : écrit à chaque pas
// du curseur) : le modèle traduit, son « {v} » remplacé par le nombre.
void texte_volume(char* buf, size_t n, int v) {
    const char* modele = tr("Volume {v} %");
    const char* trou = std::strstr(modele, "{v}");
    if (trou == nullptr) {
        std::snprintf(buf, n, "%d %%", v);
        return;
    }
    std::snprintf(buf, n, "%.*s%d%s", static_cast<int>(trou - modele), modele, v, trou + 3);
}

// Pendant un glissement : la seule ligne d'état (le reste de la carte ne bouge pas).
void peindre_volume_glisse(int k) {
    char t[48];
    texte_volume(t, sizeof(t), s_glisse_v[k]);
    ui_text(ui().piece_etat[k], t);
}

void peindre_piece(int k) {
    const MusiqueUI& u = ui();
    lv_obj_t* const carte = u.piece[k];
    const bool montree = k < s_p.n;
    ui_hidden(carte, !montree);
    if (!montree || carte == nullptr) return;
    const Piece& p = s_p.p[k];
    // Place : chaque rangée centrée, le bloc centré dans la hauteur de la grille.
    const int rangees = (s_p.n + kPiecesParLigne - 1) / kPiecesParLigne;
    const int rangee = k / kPiecesParLigne;
    const int dans = std::min(kPiecesParLigne, s_p.n - rangee * kPiecesParLigne);
    const int32_t bloc_h = rangees * kPieceH + (rangees - 1) * kPieceEcart;
    const int32_t x0 = (kLecteurCarteL - (dans * kPieceL + (dans - 1) * kPieceEcart)) / 2;
    ui_x(carte, x0 + (k % kPiecesParLigne) * (kPieceL + kPieceEcart));
    ui_y(carte, kGrilleY + (kGrilleH - bloc_h) / 2 + rangee * (kPieceH + kPieceEcart));

    const bool joue = p.lu.etat == MusiqueEtat::LECTURE;
    const bool absente = p.lu.etat == MusiqueEtat::INDISPONIBLE;
    ui_text(u.piece_icone[k], glyphe_piece(p.lu.genre));
    ui_text_color(u.piece_icone[k], joue ? UIColor.ACCENT : (allumee(p) ? UIColor.TEXT_SOFT : UIColor.TEXT_DIM));
    texte_ha_coupe(u.piece_nom[k], p.nom, kPieceNomL);
    ui_text_color(u.piece_nom[k], absente ? UIColor.TEXT_DIM : UIColor.TEXT_PRIMARY);

    // Curseur masqué par une poussée pendant un glissement (pièce éteinte, volume perdu) :
    // son RELEASED ne viendra pas, le glissement s'arrête ici.
    const bool vol = a(p, MUSIQUE_F_VOLUME) && p.lu.volume >= 0 && allumee(p);
    if (!vol) s_glisse[k] = false;

    // Ligne d'état : le volume pendant un glissement, le morceau, sinon l'état en mots.
    const char* etat = "";
    if (s_glisse[k]) {
        peindre_volume_glisse(k);
    } else {
        if ((joue || p.lu.etat == MusiqueEtat::PAUSE) && p.titre[0] != '\0') {
            etat = p.titre;
        } else {
            switch (p.lu.etat) {
                case MusiqueEtat::LECTURE: etat = tr("En lecture"); break;
                case MusiqueEtat::PAUSE: etat = tr("En pause"); break;
                case MusiqueEtat::ALLUME: etat = tr("Allumé"); break;
                case MusiqueEtat::ETEINT: etat = tr("Éteint"); break;
                default: etat = tr("Indisponible"); break;
            }
        }
        texte_ha_coupe(u.piece_etat[k], etat, kPieceEtatL);
    }
    ui_text_color(u.piece_etat[k], joue ? UIColor.TEXT_SOFT : UIColor.TEXT_DIM);

    // Volume : un appareil éteint ou absent ne se règle pas.
    const bool muet = p.lu.muet == 1;
    const bool son = (a(p, MUSIQUE_F_VOLUME) || a(p, MUSIQUE_F_MUET)) && allumee(p);
    ui_hidden(u.piece_son[k], !son);
    ui_text(u.piece_son[k], glyphe_son(muet || p.lu.volume == 0));
    ui_text_color(u.piece_son[k], muet ? UIColor.ACCENT : UIColor.TEXT_DIM);
    ui_hidden(u.piece_volume[k], !vol);
    if (vol && !s_glisse[k] && lv_slider_get_value(u.piece_volume[k]) != p.lu.volume)
        lv_slider_set_value(u.piece_volume[k], p.lu.volume, LV_ANIM_OFF);

    // Boutons : seulement ce que la pièce et le lecteur montré savent faire.
    const Piece* m = montre();
    const bool est_montree = p.lu.index == s_p.montre;
    const bool groupee = meme_groupe(p.lu.index, s_p.montre);
    const bool g = !est_montree && m != nullptr && a(p, MUSIQUE_F_GROUPE) && a(*m, MUSIQUE_F_GROUPE) && !absente;
    ui_hidden(u.piece_btn[k][MUSIQUE_PIECE_GROUPE], !g);
    ui_text(u.piece_lien[k], glyphe_lien(groupee));
    ui_text_color(u.piece_lien[k], groupee ? UIColor.ACCENT : UIColor.TEXT_SOFT);
    const bool tr_ok = !est_montree && m != nullptr && a(p, MUSIQUE_F_TRANSFERT) && a(*m, MUSIQUE_F_TRANSFERT) && !absente;
    ui_hidden(u.piece_btn[k][MUSIQUE_PIECE_TRANSFERT], !tr_ok);
    ui_hidden(u.piece_btn[k][MUSIQUE_PIECE_MARCHE], !a(p, MUSIQUE_F_MARCHE) || absente);
    lv_obj_t* const marche = u.piece_btn[k][MUSIQUE_PIECE_MARCHE];
    if (marche != nullptr) ui_text_color(lv_obj_get_child(marche, 0), allumee(p) ? UIColor.ACCENT : UIColor.TEXT_DIM);

    // Bord : accent pour le lecteur montré, plus fin pour son groupe.
    if (est_montree) highlight_button_border(carte, true, UIColor.ACCENT, 3);
    else if (groupee) highlight_button_border(carte, true, UIColor.ACCENT, 2);
    else highlight_button_border(carte, false, UIColor.ACCENT, 3);
}

void peindre_pieces() {
    const MusiqueUI& u = ui();
    if (!lie()) return;
    const bool page = s_page == Page::PIECES && popup_ouvert();
    ui_hidden(u.pieces, !page);
    if (!page) return;
    texte_ha_coupe(u.conseil, tr("Toucher : sa lecture · Appui long : plus de choix"), kConseilL);
    // « Tout ici » : une pièce au moins peut rejoindre le lecteur montré ; « Tout séparer » :
    // un groupe existe. Music Assistant seulement (ADR-0067).
    const Piece* m = montre();
    bool ici = false, separer = false;
    for (int k = 0; k < s_p.n; k++) {
        const Piece& p = s_p.p[k];
        if (m != nullptr && a(*m, MUSIQUE_F_GROUPE) && p.lu.index != s_p.montre && a(p, MUSIQUE_F_GROUPE) &&
            p.lu.etat != MusiqueEtat::INDISPONIBLE && !meme_groupe(p.lu.index, s_p.montre))
            ici = true;
        if (membres(p.lu.index) > 0) separer = true;
    }
    ui_hidden(u.tout_ici, !(s_p.ma && ici));
    ui_hidden(u.tout_separer, !(s_p.ma && separer));
    if (u.tout_ici != nullptr) texte_ha_coupe(lv_obj_get_child(u.tout_ici, 1), tr("Tout ici"), kActionsL - kActionTexteX - 10);
    if (u.tout_separer != nullptr)
        texte_ha_coupe(lv_obj_get_child(u.tout_separer, 1), tr("Tout séparer"), kActionsL - kActionTexteX - 10);
    for (int k = 0; k < kMusiquePieces; k++) peindre_piece(k);
}

// ─── Peinture : page Bibliothèque ───

// Phrases de la vue « err » de packages/tab5_musique.yaml, écrites en français par HA :
// traduites à l'affichage. Une autre phrase s'affiche telle quelle.
const char* erreur_ha(const char* phrase) {
    static const char* const kPhrases[] = {
        tr_noop("Music Assistant n'est pas installé."),
        tr_noop("Music Assistant ne répond pas."),
    };
    for (const char* p : kPhrases) {
        if (std::strcmp(phrase, p) == 0) return tr(p);
    }
    return phrase;
}

// Largeurs d'une rangée de boutons au nom (catégories) : chacun à la largeur de son nom
// tant que la rangée suffit ; sinon ceux qui dépassent leur part se partagent le reste (la
// recette des pastilles du lecteur, peindre_puces de tab5_lecteur.cpp).
void partager(int32_t* largeurs, int n, int32_t total) {
    bool fixe[kMusiqueCategories] = {};
    int32_t reste = total;
    int libres = n;
    bool change = true;
    while (change && libres > 0) {
        change = false;
        const int32_t part = reste / libres;
        for (int i = 0; i < n; i++) {
            if (fixe[i] || largeurs[i] > part) continue;
            fixe[i] = true;
            reste -= largeurs[i];
            libres--;
            change = true;
        }
    }
    if (libres > 0)
        for (int i = 0; i < n; i++)
            if (!fixe[i]) largeurs[i] = reste / libres;
}

int categorie_courante() {
    for (int i = 0; i < kMusiqueCategories; i++)
        if (kCategories[i] == s_pile[0].vue) return i;
    return -1;
}

// La rangée du haut : les catégories (bibliothèque, au niveau d'une catégorie), sinon le
// bouton retour et le titre ; les flèches des pages à droite quand il y en a.
void peindre_rangee() {
    const MusiqueUI& u = ui();
    const bool niveau_cat = !s_sources && s_pile_n <= 1;
    const bool cats = niveau_cat && s_p.ma;
    const bool fleches_avant = !s_attente && s_liste_recue && s_l.page > 0;
    const bool fleches_apres = !s_attente && s_liste_recue && s_l.suite;
    ui_hidden(u.precedents, !fleches_avant);
    ui_hidden(u.suivants, !fleches_apres);
    // Place des flèches montrées : « précédente » (x 812) prend les deux places, « suivante »
    // seule (x 868, la première page) une seule.
    const int32_t fleches = fleches_avant ? kFlechesL + kCategorieEcart : (fleches_apres ? kFlecheL + kCategorieEcart : 0);
    // Catégories.
    if (cats) {
        int32_t largeurs[kMusiqueCategories];
        for (int i = 0; i < kMusiqueCategories; i++) {
            lv_obj_t* lbl = u.categorie[i] != nullptr ? lv_obj_get_child(u.categorie[i], 0) : nullptr;
            largeurs[i] = std::max(kCategorieMinL, ui_largeur_texte(lbl, tr(kCategoriesNoms[i])) + 2 * kCategorieMarge);
        }
        partager(largeurs, kMusiqueCategories, kRangeeL - fleches - (kMusiqueCategories - 1) * kCategorieEcart);
        int32_t x = kMarge;
        for (int i = 0; i < kMusiqueCategories; i++) {
            lv_obj_t* b = u.categorie[i];
            ui_hidden(b, false);
            if (b == nullptr) continue;
            ui_x(b, x);
            lv_obj_set_width(b, largeurs[i]);  // LVGL 9.5.0 compare lui-même (lv_obj_pos.c)
            texte_ha_coupe(lv_obj_get_child(b, 0), tr(kCategoriesNoms[i]), largeurs[i] - 2 * kCategorieMarge + 8);
            x += largeurs[i] + kCategorieEcart;
        }
        choix_peindre(u.categorie, kMusiqueCategories, categorie_courante());
    } else {
        for (lv_obj_t* b : u.categorie) ui_hidden(b, true);
    }
    // Retour et titre : un niveau ouvert, les sources d'une pièce.
    const bool retour = !cats && (s_sources || s_pile_n > 1);
    ui_hidden(u.retour, !retour);
    ui_hidden(u.titre, cats);
    if (!cats) {
        const char* titre = s_liste_recue && s_l.vue != MusiqueVue::ERREUR ? s_l.titre : "";
        const int32_t x = retour ? kTitreBiblioX : kMarge;
        ui_x(u.titre, x);
        texte_ha_coupe(u.titre, titre, kLecteurCarteL - kMarge - fleches - x);
    }
}

// Cadre d'une pochette en accent (l'élément touché), ou rendu à son style (bord GLASS_RIM
// de musique_element.yaml : les propriétés locales retirées, comme choix_peindre).
void cadre_accent(lv_obj_t* cadre, bool on) {
    if (cadre == nullptr) return;
    if (on) {
        ui_style_couleur(cadre, LV_STYLE_BORDER_COLOR, UIColor.ACCENT);
        ui_style_num(cadre, LV_STYLE_BORDER_WIDTH, 3);
    } else {
        for (lv_style_prop_t p : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_WIDTH})
            lv_obj_remove_local_style_prop(cadre, p, LV_PART_MAIN);
    }
}

void peindre_element(int k) {
    const MusiqueUI& u = ui();
    lv_obj_t* const b = u.element[k];
    const bool montre = !s_attente && !s_sans_reponse && s_liste_recue && s_l.vue != MusiqueVue::ERREUR && k < s_l.n;
    ui_hidden(b, !montre);
    if (!montre || b == nullptr) return;
    const Element& e = s_l.e[k];
    ui_x(b, kMarge + (k % kElementsParLigne) * kElementPasX);
    ui_y(b, kElementsY + (k / kElementsParLigne) * kElementPasY);
    const bool img = s_img_prete[k] && e.url[0] != '\0';
    ui_hidden(u.element_img[k], !img);
    ui_hidden(u.element_icone[k], img);
    ui_text(u.element_icone[k], glyphe_element(e.genre));
    texte_ha_coupe(u.element_nom[k], e.nom, kElementTexteL);
    ui_hidden(u.element_sous[k], e.sous[0] == '\0');
    texte_ha_coupe(u.element_sous[k], e.sous, kElementTexteL);
    cadre_accent(u.element_cadre[k], k == s_lance);
}

void peindre_biblio() {
    const MusiqueUI& u = ui();
    if (!lie()) return;
    const bool page = s_page == Page::BIBLIOTHEQUE && popup_ouvert();
    ui_hidden(u.biblio, !page);
    if (!page) return;
    peindre_rangee();
    // Message à la place de la grille : en attente, sans réponse, erreur de HA, rien.
    const char* msg = "";
    if (s_attente || !s_liste_recue) msg = tr("Chargement...");
    else if (s_sans_reponse) msg = s_p.ma ? tr("Music Assistant ne répond pas") : tr("Home Assistant ne répond pas");
    else if (s_l.vue == MusiqueVue::ERREUR) msg = s_l.titre[0] != '\0' ? erreur_ha(s_l.titre) : tr("Music Assistant ne répond pas");
    else if (s_l.n == 0) msg = s_l.vue == MusiqueVue::FAVORIS ? tr("Aucun favori pour l'instant") : tr("Rien à montrer ici");
    ui_hidden(u.message, msg[0] == '\0');
    if (msg[0] != '\0') ui_text(u.message, msg);
    for (int k = 0; k < kMusiqueElements; k++) peindre_element(k);
}

// ─── Pochettes : une à la fois, page montrée ───

void telecharger_suivante() {
    if (s_dl >= 0 || ui().telecharger == nullptr || s_page != Page::BIBLIOTHEQUE || !popup_ouvert()) return;
    if (s_attente || !s_liste_recue) return;
    for (int k = 0; k < s_l.n && k < kMusiqueElementsMax; k++) {
        const Element& e = s_l.e[k];
        if (e.url[0] == '\0' || s_img_prete[k] || s_img_echec[k]) continue;
        // L'image de l'emplacement est masquée tant qu'elle n'est pas prête ([AI-WARNING]).
        ui_hidden(ui().element_img[k], true);
        std::snprintf(s_img_url[k], sizeof(s_img_url[k]), "%s", e.url);
        s_dl = k;
        ui().telecharger(k, e.url);
        return;
    }
}

// Une liste neuve : un emplacement garde son image si son URL ne change pas ; les
// emplacements vides libèrent la leur (masquée d'abord).
void pochettes_nouvelle_liste() {
    for (int k = 0; k < kMusiqueElementsMax; k++) {
        const char* url = k < s_l.n ? s_l.e[k].url : "";
        if (std::strcmp(url, s_img_url[k]) == 0 && k != s_dl) continue;
        s_img_echec[k] = false;
        if (k == s_dl) continue;  // en cours : son résultat sera jeté (URL changée), puis retéléchargé
        s_img_prete[k] = false;
        ui_hidden(ui().element_img[k], true);
        if (url[0] == '\0' && s_img_url[k][0] != '\0' && ui().telecharger != nullptr) ui().telecharger(k, "");
        s_img_url[k][0] = '\0';
    }
}

// ─── Bibliothèque : demandes ───

void demander() {
    char v[kMusiqueRefMax + 16];
    s_sans_reponse = false;
    s_attente = true;
    s_attente_ms = esphome::millis();
    if (s_sources) {
        std::snprintf(v, sizeof(v), "%d", s_src_page);
        s_attendu = MusiqueVue::SOURCES;
        envoyer("sources", s_src_piece, v);
    } else {
        const Niveau& n = s_pile[s_pile_n - 1];
        if (n.vue == MusiqueVue::DETAIL) {
            std::snprintf(v, sizeof(v), "%s|%d", n.ref, n.page);
            envoyer("detail", -1, v);
        } else {
            std::snprintf(v, sizeof(v), "%s|%d", musique_vue_code(n.vue), n.page);
            envoyer("parcourir", -1, v);
        }
        s_attendu = n.vue;
    }
    s_biblio_a_jour = true;
    peindre_biblio();
}

// Une réponse attendue ? Une catégorie doit être celle demandée (deux touchers rapides :
// la première réponse est jetée), une ref ouverte un « det », des sources un « src ». Rien
// hors d'une demande (en attente, ou après son délai : une réponse lente s'affiche encore) ;
// « err » répond à toute demande, un contexte illisible (INCONNUE) à aucune.
bool reponse_attendue(MusiqueVue vue) {
    if (!s_attente && !s_sans_reponse) return false;
    if (vue == MusiqueVue::ERREUR) return true;
    return vue != MusiqueVue::INCONNUE && vue == s_attendu;
}

// Plus rien d'attendu (les sources quittées) : une réponse tardive sera jetée.
void attente_oublier() {
    s_attente = false;
    s_sans_reponse = false;
    s_attendu = MusiqueVue::INCONNUE;
}

// ─── Roues (ADR-0036) ───

// Ce que fait chaque bouton de la roue ouverte.
enum class Geste : uint8_t { SOURCE,
                             MUET,
                             TRANSFERT,
                             SEPARER,
                             LIRE,
                             SUIVANT,
                             FIN,
                             RADIO,
                             OUVRIR };
Geste s_roue_gestes[kRoueBoutons];
int s_roue_n = 0;

void rejouer_roue_piece();
void rejouer_roue_element();

void choisir_piece(int i);
void choisir_element(int i);

bool peindre_roue_piece(bool garder) {
    const MusiqueUI& u = ui();
    if (s_roue_piece < 0 || s_roue_piece >= s_p.n) return false;
    const Piece& p = s_p.p[s_roue_piece];
    if (p.lu.etat == MusiqueEtat::INDISPONIBLE) return false;
    RoueBouton b[kRoueBoutons];
    s_roue_n = 0;
    auto ajouter = [&](Geste g, RoueIcone icone, const char* mot, bool courant) {
        if (s_roue_n >= kRoueBoutons) return;
        b[s_roue_n] = RoueBouton{};
        b[s_roue_n].icone = icone;
        b[s_roue_n].genre = RoueGenre::ACTION;
        b[s_roue_n].courant = courant;
        b[s_roue_n].legende = mot;
        s_roue_gestes[s_roue_n++] = g;
    };
    const bool muet = p.lu.muet == 1;
    const bool est_montree = p.lu.index == s_p.montre;
    const Piece* m = montre();
    if (a(p, MUSIQUE_F_SOURCE)) ajouter(Geste::SOURCE, RoueIcone::SOURCE, tr("Source"), false);
    if (a(p, MUSIQUE_F_MUET))
        ajouter(Geste::MUET, muet ? RoueIcone::SON : RoueIcone::MUET, muet ? tr("Son") : tr("Muet"), muet);
    if (!est_montree && m != nullptr && a(p, MUSIQUE_F_TRANSFERT) && a(*m, MUSIQUE_F_TRANSFERT))
        ajouter(Geste::TRANSFERT, RoueIcone::TRANSFERT, tr("Musique ici"), false);
    if (p.lu.groupe >= 0) ajouter(Geste::SEPARER, RoueIcone::SEPARER, tr("Séparer"), false);
    if (s_roue_n == 0) return false;
    char valeur[16] = "";
    if (p.lu.volume >= 0) std::snprintf(valeur, sizeof(valeur), "%d %%", p.lu.volume);
    RoueTete tete;
    tete.icone = glyphe_piece(p.lu.genre);
    tete.valeur = valeur;
    tete.nom = p.nom;
    tete.jauge = a(p, MUSIQUE_F_VOLUME) ? p.lu.volume : -1;
    tete.couleur = p.lu.etat == MusiqueEtat::LECTURE ? UIColor.ACCENT : UIColor.TEXT_SOFT;
    tete.mots = true;
    RoueRappels r;
    r.choisir = choisir_piece;
    r.rejouer = rejouer_roue_piece;
    return roue_ouvrir(u.piece_icone[s_roue_piece], tete, b, s_roue_n, r, garder);
}

void rejouer_roue_piece() {
    if (!peindre_roue_piece(true)) roue_actions_fermer();
}

void choisir_piece(int i) {
    if (i < 0 || i >= s_roue_n || s_roue_piece < 0 || s_roue_piece >= s_p.n) return;
    Piece& p = s_p.p[s_roue_piece];
    const int index = p.lu.index;
    switch (s_roue_gestes[i]) {
        case Geste::SOURCE:
            // Les sources dans la grille de la page Bibliothèque (« Sources » sans MA).
            s_sources = true;
            s_src_piece = index;
            s_src_page = 0;
            demander();
            afficher(Page::BIBLIOTHEQUE);
            return;
        case Geste::MUET:
            p.lu.muet = p.lu.muet == 1 ? 0 : 1;
            envoyer("muet", index);
            break;
        case Geste::TRANSFERT:
            envoyer("transferer", index);
            s_p.montre = index;  // HA en fait le lecteur montré ; sa poussée le confirme
            break;
        case Geste::SEPARER:
            envoyer("separer", index);
            p.lu.groupe = -1;
            break;
        default: return;
    }
    peindre_pieces();
    repeindre_lecteur();
}

bool peindre_roue_element(bool garder) {
    const MusiqueUI& u = ui();
    if (s_roue_element < 0 || s_roue_element >= s_l.n) return false;
    const Element& e = s_l.e[s_roue_element];
    MusiqueElementLu lu;  // pour musique_element_jouable / conteneur : genre et ref seuls
    lu.genre = e.genre;
    lu.ref = Champ{e.ref, std::strlen(e.ref)};
    const bool conteneur = musique_element_conteneur(lu);
    if (!conteneur && !musique_element_jouable(lu)) return false;
    RoueBouton b[kRoueBoutons];
    s_roue_n = 0;
    auto ajouter = [&](Geste g, RoueIcone icone, const char* mot) {
        b[s_roue_n] = RoueBouton{};
        b[s_roue_n].icone = icone;
        b[s_roue_n].genre = RoueGenre::ACTION;
        b[s_roue_n].legende = mot;
        s_roue_gestes[s_roue_n++] = g;
    };
    ajouter(Geste::LIRE, RoueIcone::LIRE, tr("Lire"));
    ajouter(Geste::SUIVANT, RoueIcone::A_LA_SUITE, tr("À la suite"));
    ajouter(Geste::FIN, RoueIcone::EN_FIN, tr("En fin de file"));
    ajouter(Geste::RADIO, RoueIcone::RADIO, tr("Radio"));
    if (conteneur) ajouter(Geste::OUVRIR, RoueIcone::LISTE, tr("Ouvrir"));
    RoueTete tete;
    tete.icone = glyphe_element(e.genre);
    tete.valeur = "";
    tete.nom = e.nom;
    tete.jauge = -1;
    tete.couleur = UIColor.ACCENT;
    tete.mots = true;
    RoueRappels r;
    r.choisir = choisir_element;
    r.rejouer = rejouer_roue_element;
    return roue_ouvrir(u.element_cadre[s_roue_element], tete, b, s_roue_n, r, garder);
}

void rejouer_roue_element() {
    if (!peindre_roue_element(true)) roue_actions_fermer();
}

void retour_arreter() {
    if (s_retour != nullptr) lv_timer_delete(s_retour);
    s_retour = nullptr;
}

// Après un élément joué ou une source choisie : l'élément en accent a été vu, la page
// suivante (Lecture, ou Pièces pour une source) ; après une playlist (ADR-0068) : Lecture.
void retour_fin(lv_timer_t*) {
    s_retour = nullptr;  // un seul tour : LVGL supprime le timer après ce rappel
    s_lance = -1;
    if (popup_ouvert() && s_page == s_retour_depuis) {
        afficher(s_retour_page);
    } else {
        peindre_biblio();
    }
}

// La page `vers` dans kRetourMs, si `depuis` est encore montrée (afficher() l'annule).
void retour_programmer(Page depuis, Page vers) {
    s_retour_depuis = depuis;
    s_retour_page = vers;
    retour_arreter();
    s_retour = lv_timer_create(retour_fin, kRetourMs, nullptr);
    lv_timer_set_repeat_count(s_retour, 1);
}

void lancer_retour(int k, Page vers) {
    s_lance = k;
    peindre_biblio();
    retour_programmer(Page::BIBLIOTHEQUE, vers);
}

void jouer(int k, char mode) {
    if (k < 0 || k >= s_l.n) return;
    char v[kMusiqueRefMax + 4];
    std::snprintf(v, sizeof(v), "%c|%s", mode, s_l.e[k].ref);
    envoyer("jouer", -1, v);
    lancer_retour(k, Page::LECTURE);
}

void ouvrir(int k) {
    if (k < 0 || k >= s_l.n) return;
    if (s_pile_n >= kPileMax) {  // la catégorie reste, le plus ancien niveau ouvert s'en va
        for (int i = 1; i + 1 < kPileMax; i++) s_pile[i] = s_pile[i + 1];
        s_pile_n = kPileMax - 1;
    }
    Niveau& n = s_pile[s_pile_n++];
    n.vue = MusiqueVue::DETAIL;
    std::snprintf(n.ref, sizeof(n.ref), "%s", s_l.e[k].ref);
    n.page = 0;
    demander();
}

void choisir_element(int i) {
    if (i < 0 || i >= s_roue_n) return;
    const int k = s_roue_element;
    switch (s_roue_gestes[i]) {
        case Geste::LIRE: jouer(k, 'm'); break;
        case Geste::SUIVANT: jouer(k, 's'); break;
        case Geste::FIN: jouer(k, 'f'); break;
        case Geste::RADIO: jouer(k, 'r'); break;
        case Geste::OUVRIR: ouvrir(k); break;
        default: break;
    }
}

// ─── Affichage d'une page ───

void afficher(Page p) {
    if (!page_disponible(p)) p = Page::LECTURE;
    // Quitter les sources (sans MA, la page « Sources » disparaît avec elles).
    if (p != Page::BIBLIOTHEQUE && s_sources) {
        s_sources = false;
        if (s_attendu == MusiqueVue::SOURCES) attente_oublier();
        s_liste_recue = false;  // la grille montrait les sources
        s_biblio_a_jour = false;
    }
    s_page = p;
    retour_arreter();
    s_lance = -1;
    roue_actions_fermer();
    lecteur_popup_repeindre();  // l'en-tête (onglets) et le corps de la page Lecture
    peindre_pieces();
    if (p == Page::BIBLIOTHEQUE && !s_biblio_a_jour && !s_sources && s_p.ma) demander();
    peindre_biblio();
    telecharger_suivante();
    ui_mark_activity();
}

// Une page qui n'a plus rien à montrer (Playlists sans playlist, Pièces sous deux pièces,
// Bibliothèque sans MA) : retour à la page Lecture.
void pages_revoir() {
    if (page_disponible(s_page)) return;
    if (popup_ouvert()) afficher(Page::LECTURE);
    else s_page = Page::LECTURE;
}

}  // namespace

// ─── Pour le popup Musique (tab5_lecteur.cpp) ───

void musique_ouvrir() {
    // Le popup s'ouvre sur sa page Lecture ; la bibliothèque sera redemandée.
    retour_arreter();
    s_lance = -1;
    s_page = Page::LECTURE;
    s_biblio_a_jour = false;
    if (s_sources) {
        s_sources = false;
        s_liste_recue = false;
        if (s_attendu == MusiqueVue::SOURCES) attente_oublier();
    }
    if (s_pages.popup == nullptr && g_lecteur_ui.popup != nullptr) {
        s_pages.popup = g_lecteur_ui.popup;
        pages_brancher(&s_pages);
    }
    peindre_pieces();
    peindre_biblio();
    // HA pousse les pièces (et leur contexte) en réponse.
    envoyer("ouvrir", -1);
}

bool musique_page_lecture() { return s_page == Page::LECTURE; }

bool musique_page_playlists() { return s_page == Page::PLAYLISTS; }

void musique_pages_revoir() { pages_revoir(); }

void musique_retour_lecture() { retour_programmer(Page::PLAYLISTS, Page::LECTURE); }

int32_t musique_onglets_peindre() {
    const MusiqueUI& u = ui();
    Page v[kPagesNb];
    const int n = pages_visibles(v);
    const bool montres = n >= 2 && lie();
    int32_t x = kOngletsFinX;
    for (int i = kMusiqueOnglets - 1; i >= 0; i--) {
        lv_obj_t* const b = u.onglet[i];
        const bool vis = montres && i < n;
        ui_hidden(b, !vis);
        if (!vis || b == nullptr) continue;
        lv_obj_t* const lbl = lv_obj_get_child(b, 0);
        const char* nom = page_nom(v[i]);
        const int32_t w = std::clamp(ui_largeur_texte(lbl, nom) + 2 * kOngletMarge, kOngletMinL, kOngletMaxL);
        x -= w;
        ui_x(b, x);
        lv_obj_set_width(b, w);  // LVGL 9.5.0 compare lui-même (lv_obj_pos.c)
        texte_ha_coupe(lbl, nom, w - 2 * kOngletMarge);
        x -= kOngletEcart;
    }
    if (!montres) return kLecteurAppFinX;
    choix_peindre(u.onglet, n, page_courante());
    return x;
}

int32_t musique_entete_reserve() {
    const MusiqueUI& u = ui();
    const int n = membres(s_p.montre);
    if (n <= 0 || u.groupe_texte == nullptr) return 0;
    char t[16];
    std::snprintf(t, sizeof(t), "+ %d", n);
    return kGroupeEcart + ui_largeur_texte(u.groupe_texte, t) + 2 * kGroupeMarge;
}

void musique_entete_peindre(lv_obj_t* nom) {
    const MusiqueUI& u = ui();
    const int n = membres(s_p.montre);
    ui_hidden(u.groupe, n <= 0 || nom == nullptr);
    if (n <= 0 || nom == nullptr || u.groupe == nullptr) return;
    char t[16];
    std::snprintf(t, sizeof(t), "+ %d", n);
    ui_text(u.groupe_texte, t);
    ui_x(u.groupe, kTitreX + ui_largeur_texte(nom, lv_label_get_text(nom)) + kGroupeEcart);
}

void musique_lecture_peindre(bool morceau) {
    const MusiqueUI& u = ui();
    if (!lie()) return;
    // Avec Music Assistant : le titre suivant et la longueur de la file sous l'album.
    const bool ligne = morceau && s_page == Page::LECTURE && s_p.ma && (s_p.file > 0 || s_p.suivant[0] != '\0');
    ui_hidden(u.suivant, !ligne);
    ui_hidden(u.file, !ligne || s_p.file <= 0);
    const int l = ligne ? 1 : 0;
    const LecteurUI& lu = g_lecteur_ui;
    ui_y(lu.titre, kTitreY[l]);
    ui_y(lu.artiste, kArtisteY[l]);
    ui_y(lu.album, kAlbumY[l]);
    if (!ligne) return;
    int32_t file_l = 0;
    if (s_p.file > 0) {
        char n[12];
        std::snprintf(n, sizeof(n), "%d", s_p.file);
        const std::string t = s_p.file == 1 ? std::string(tr("1 titre")) : tr_fill("{n} titres", {{"n", n}});
        ui_text(u.file, t.c_str());
        file_l = ui_largeur_texte(u.file, t.c_str()) + kFileEcart;
    }
    if (s_p.suivant[0] != '\0') {
        const std::string t = tr_fill("À suivre · {titre}", {{"titre", s_p.suivant}});
        texte_ha_coupe(u.suivant, t.c_str(), kColonneL - file_l);
    } else {
        ui_text(u.suivant, "");
    }
}

void musique_rejouer_theme() {
    if (!lie()) return;
    peindre_pieces();
    peindre_biblio();
}

// ─── Poussées de Home Assistant ───

void musique_pieces_recu(const std::string& pieces, const std::string& contexte) {
    if (payload_trop_long(kTag, pieces.size() + contexte.size())) return;
    MusiquePiecesLues lu;
    musique_pieces_lire(Champ{pieces.data(), pieces.size()}, Champ{contexte.data(), contexte.size()}, lu);
    s_p.ma = lu.ma;
    s_p.montre = lu.montre;
    s_p.file = lu.file;
    copier(s_p.suivant, sizeof(s_p.suivant), lu.suivant);
    s_p.n = lu.n;
    for (int k = 0; k < lu.n; k++) {
        Piece& p = s_p.p[k];
        copier(p.nom, sizeof(p.nom), lu.pieces[k].nom);
        copier(p.titre, sizeof(p.titre), lu.pieces[k].titre);
        p.lu = lu.pieces[k];
        p.lu.nom = p.lu.titre = Champ{nullptr, 0};
    }
    ESP_LOGD(kTag, "%d pièce(s), Music Assistant %s", s_p.n, s_p.ma ? "oui" : "non");
    pages_revoir();
    if (s_roue_piece >= s_p.n) s_roue_piece = -1;
    if (!lie()) return;
    peindre_pieces();
    repeindre_lecteur();
    // La roue d'une carte après les cartes (son ancre, mesurée à l'ouverture, est à sa
    // nouvelle place), popup montré sur Pièces seulement : garé, il est à 10 000 px.
    if (roue_ouverte_par(rejouer_roue_piece)) {
        if (popup_ouvert() && s_page == Page::PIECES) rejouer_roue_piece();
        else roue_actions_fermer();
    }
}

void musique_liste_recue(const std::string& contexte, const std::string& elements) {
    if (payload_trop_long(kTag, contexte.size() + elements.size())) return;
    MusiqueListeLue lu;
    musique_liste_lire(Champ{contexte.data(), contexte.size()}, Champ{elements.data(), elements.size()}, lu);
    if (!reponse_attendue(lu.vue)) {
        ESP_LOGD(kTag, "liste « %s » non attendue, ignorée", musique_vue_code(lu.vue));
        return;
    }
    s_attente = false;
    s_sans_reponse = false;
    s_l.vue = lu.vue;
    copier(s_l.titre, sizeof(s_l.titre), lu.titre);
    s_l.page = lu.page;
    s_l.suite = lu.suite;
    s_l.retour = lu.retour;
    s_l.n = lu.n;
    for (int k = 0; k < lu.n; k++) {
        Element& e = s_l.e[k];
        const MusiqueElementLu& l = lu.elements[k];
        copier(e.nom, sizeof(e.nom), l.nom);
        copier(e.sous, sizeof(e.sous), l.sous);
        e.genre = l.genre;
        copier_brut(e.ref, sizeof(e.ref), l.ref);
        if (!ha_image_url(l.image, nullptr, e.url, sizeof(e.url)) && l.image.n > 0)
            payload_refuse(kTag, "pochette illisible", l.image.n);
    }
    // La page lue devient celle du niveau (sa flèche suivante part de là).
    if (s_sources) s_src_page = lu.page;
    else s_pile[s_pile_n - 1].page = lu.page;
    s_liste_recue = true;
    s_lance = -1;
    if (s_roue_element >= s_l.n) s_roue_element = -1;
    if (roue_ouverte_par(rejouer_roue_element)) roue_actions_fermer();  // la liste a changé sous elle
    pochettes_nouvelle_liste();
    peindre_biblio();
    telecharger_suivante();
}

void musique_image_prete(int n, const lv_image_dsc_t* dsc) {
    if (n < 0 || n >= kMusiqueElementsMax) return;
    if (n == s_dl) s_dl = -1;
    const bool valide = dsc != nullptr && dsc->data != nullptr && dsc->header.w > 0 && dsc->header.h > 0;
    // Jeté si la liste a changé pendant le téléchargement : l'emplacement est redemandé.
    const bool a_jour = n < s_l.n && std::strcmp(s_img_url[n], s_l.e[n].url) == 0;
    if (valide && a_jour && ui().element_img[n] != nullptr) {
        // Mêmes pixels neufs dans le même descripteur : LVGL ne garde rien de l'image d'avant.
        // Taille d'origine, centrée dans le cadre de 144 px (clip_corner) : une pochette de
        // 160 est rognée de 8 px de chaque côté, sans mise à l'échelle (aucune transformation
        // à chaque image).
        lv_image_cache_drop(dsc);
        lv_image_set_inner_align(ui().element_img[n], LV_IMAGE_ALIGN_CENTER);
        lv_image_set_src(ui().element_img[n], dsc);
        s_img_prete[n] = true;
    } else {
        s_img_prete[n] = false;
        if (!a_jour) s_img_url[n][0] = '\0';
    }
    peindre_element(n);
    telecharger_suivante();
}

void musique_image_erreur(int n) {
    if (n < 0 || n >= kMusiqueElementsMax) return;
    if (n == s_dl) s_dl = -1;
    const bool a_jour = n < s_l.n && std::strcmp(s_img_url[n], s_l.e[n].url) == 0;
    // Une pochette absente est normale (logo de radio parti, taille refusée) : l'icône reste.
    s_img_prete[n] = false;
    s_img_echec[n] = a_jour;
    if (!a_jour) s_img_url[n][0] = '\0';
    peindre_element(n);
    telecharger_suivante();
}

void musique_tic() {
    if (s_attente && esphome::millis() - s_attente_ms >= kAttenteMs) {
        s_attente = false;
        s_sans_reponse = true;
        ESP_LOGW(kTag, "pas de réponse en %u s", static_cast<unsigned>(kAttenteMs / 1000));
        peindre_biblio();
    }
}

// ─── Widgets ───

void musique_onglet(int i) { afficher_rang(i); }

void musique_piece_appui(int i) {
    if (ui_appui_glisse() || i < 0 || i >= s_p.n) return;
    const int index = s_p.p[i].lu.index;
    if (index != s_p.montre) {
        envoyer("montrer", index);
        s_p.montre = index;  // tout de suite ; la poussée de HA confirme
    }
    afficher(Page::LECTURE);
}

void musique_piece_long(int i) {
    if (ui_appui_glisse() || i < 0 || i >= s_p.n) return;
    s_roue_piece = i;
    peindre_roue_piece(false);
}

void musique_piece_bouton(int i, int bouton) {
    if (ui_appui_glisse() || i < 0 || i >= s_p.n) return;
    Piece& p = s_p.p[i];
    const int index = p.lu.index;
    switch (bouton) {
        case MUSIQUE_PIECE_GROUPE: {
            Piece* const m = montre();
            if (m == nullptr || index == s_p.montre) return;
            if (meme_groupe(index, s_p.montre)) {
                envoyer("separer", index);
                p.lu.groupe = -1;
                if (membres(m->lu.index) == 0 && m->lu.groupe == m->lu.index) m->lu.groupe = -1;
            } else {
                envoyer("grouper", index);
                const int meneur = m->lu.groupe >= 0 ? m->lu.groupe : m->lu.index;
                m->lu.groupe = meneur;
                p.lu.groupe = meneur;
            }
            break;
        }
        case MUSIQUE_PIECE_TRANSFERT:
            if (index == s_p.montre) return;
            envoyer("transferer", index);
            s_p.montre = index;
            break;
        case MUSIQUE_PIECE_MARCHE:
            if (p.lu.etat == MusiqueEtat::INDISPONIBLE) return;
            envoyer("marche", index);
            p.lu.etat = allumee(p) ? MusiqueEtat::ETEINT : MusiqueEtat::ALLUME;
            break;
        default: return;
    }
    peindre_pieces();
    repeindre_lecteur();  // « + n » de l'en-tête
}

void musique_piece_volume(int i, bool relache) {
    const MusiqueUI& u = ui();
    if (i < 0 || i >= s_p.n || u.piece_volume[i] == nullptr) return;
    Piece& p = s_p.p[i];
    const int v = tab5_float_vers_int(static_cast<float>(lv_slider_get_value(u.piece_volume[i])), 0, 100, 0);
    const bool debut = !s_glisse[i];
    s_glisse[i] = !relache;
    s_glisse_v[i] = v;
    // Chaque pas du glissement : la ligne d'état seule (règle 4) ; toute la carte au
    // premier pas (couleur de la ligne) et au relâcher.
    if (!relache && !debut) {
        peindre_volume_glisse(i);
        return;
    }
    if (relache && a(p, MUSIQUE_F_VOLUME)) {
        char t[8];
        std::snprintf(t, sizeof(t), "%d", v);
        envoyer("volume", p.lu.index, t);
        p.lu.volume = v;
        if (v > 0 && p.lu.muet == 1) p.lu.muet = 0;  // HA rétablit le son en réglant le volume
    }
    peindre_piece(i);
}

void musique_bouton(int bouton) {
    if (ui_appui_glisse()) return;
    switch (bouton) {
        case MUSIQUE_BTN_TOUT_ICI:
            if (!s_p.ma) return;
            envoyer("tout_grouper", -1);
            break;
        case MUSIQUE_BTN_TOUT_SEPARER:
            if (!s_p.ma) return;
            envoyer("tout_separer", -1);
            for (int k = 0; k < s_p.n; k++) s_p.p[k].lu.groupe = -1;
            peindre_pieces();
            repeindre_lecteur();
            break;
        case MUSIQUE_BTN_RETOUR:
            if (s_sources) {
                afficher(Page::PIECES);  // quitte les sources
            } else if (s_pile_n > 1) {
                s_pile_n--;
                demander();
            }
            break;
        case MUSIQUE_BTN_PRECEDENTS:
        case MUSIQUE_BTN_SUIVANTS: {
            const int pas = bouton == MUSIQUE_BTN_SUIVANTS ? 1 : -1;
            if (pas > 0 && !s_l.suite) return;
            if (s_sources) {
                s_src_page = std::max(0, s_l.page + pas);
            } else {
                s_pile[s_pile_n - 1].page = std::max(0, s_l.page + pas);
            }
            demander();
            break;
        }
        default: return;
    }
    ui_mark_activity();
}

void musique_categorie(int i) {
    if (ui_appui_glisse() || i < 0 || i >= kMusiqueCategories || !s_p.ma) return;
    s_sources = false;
    s_pile_n = 1;
    s_pile[0].vue = kCategories[i];
    s_pile[0].ref[0] = '\0';
    s_pile[0].page = 0;
    demander();
    ui_mark_activity();
}

void musique_element_appui(int i) {
    if (ui_appui_glisse() || i < 0 || i >= s_l.n || s_attente) return;
    const Element& e = s_l.e[i];
    if (e.ref[0] == '\0') return;
    if (e.genre == MusiqueElementGenre::SOURCE) {
        envoyer("source", s_src_piece, e.ref);
        lancer_retour(i, Page::PIECES);
        return;
    }
    MusiqueElementLu lu;
    lu.genre = e.genre;
    lu.ref = Champ{e.ref, std::strlen(e.ref)};
    if (musique_element_conteneur(lu)) ouvrir(i);
    else if (musique_element_jouable(lu)) jouer(i, 'm');
    ui_mark_activity();
}

void musique_element_long(int i) {
    if (ui_appui_glisse() || i < 0 || i >= s_l.n || s_attente) return;
    if (s_l.e[i].genre == MusiqueElementGenre::SOURCE) return;  // une source : son toucher suffit
    s_roue_element = i;
    peindre_roue_element(false);
}
