/**
 * [AI-CONTEXT]
 * @file tab5_lecteur.cpp
 * @role Lecteur de musique (ADR-0050, 10/10/2026) : le popup « Musique » (lecteur_popup.yaml),
 *       la mini-barre « en lecture » de l'accueil (lecteur_mini.yaml) et le lecteur compact
 *       de la zone à gauche de l'horloge (lecteur_zone.yaml, ADR-0051 lot 2), peints d'après
 *       ce que pousse tab5_maj_lecteur (lu par lecteurs_lire() / lecteur_etat_lire(),
 *       tab5_parse.h, section 9).
 *         - lecteur_recu() garde la liste et l'état (textes copiés par texte_ha_copier),
 *           repeint, et ne télécharge la pochette que si son adresse change ;
 *         - lecteur_tic() (1 s) avance la position en lecture (lecteur_position()) et masque
 *           la mini-barre 5 min après une pause ;
 *         - le lecteur compact (peindre_zone) n'est peint que montré (s_zone_montre, posé par
 *           lecteur_zone_montrer() depuis zone_gauche_appliquer()), sinon marqué « sale » ;
 *         - les commandes partent en événement esphome.tab5_lecteur (ADR-0025) : la tablette
 *           montre tout de suite l'effet attendu (lecture / pause, aléatoire, répétition,
 *           muet, volume, position), HA confirme par sa poussée suivante.
 * @architecture_constraint Push-only, events-only (ADR-0001, ADR-0025) : aucune entité
 *       nommée ici, la tablette dit « l'index i de la liste » ou « la tuile tRT ». Couleurs
 *       par la palette (UIColor.X) et lecteur_rejouer_theme() (ADR-0029). Glyphes posés
 *       d'ici (règle 7) : glyphe_lecture, glyphe_aleatoire, glyphe_repetition, glyphe_volume,
 *       glyphe_genre (MDI_CODE_TARGETS de tools/check_tab5_code_rules.py).
 * @ai_instruction La mini-barre couvre le cadre « Ok Nabu » quand elle se montre
 *       (nabu_masquer(), tab5_rangee.cpp) : le panneau revient tel qu'il était quand elle
 *       se masque. Elle ne se montre pas tant que le lecteur compact est dans la zone
 *       gauche (choix d'Axel, 10/10/2026). Un widget de plus = son champ dans LecteurUI (tab5_lecteur.h) et sa
 *       ligne dans le script tab5_lecteur_lier (tab5-lecteur.yaml).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl_private.h"  // lv_image_cache_drop() (cache d'images, hors de lvgl.h en 9.5)
#include <cmath>
#include <cstdio>
#include <cstring>

LecteurUI g_lecteur_ui;

namespace {

static_assert(kLecteurPuces == kLecteursMax, "une pastille par lecteur de la liste");

constexpr const char* kTag = "tab5.lecteur";
// Largeurs où les textes sont coupés par « … » (lecteur_popup.yaml, lecteur_mini.yaml).
constexpr int32_t kTexteL = 508;          // colonne de droite du popup
constexpr int32_t kNomL = 520;            // en-tête, à côté de la pastille de l'application
constexpr int32_t kNomSeulL = 740;        // en-tête sans pastille
constexpr int32_t kAppL = 180;            // pastille de l'application
// Pastilles des lecteurs : chacune à la largeur de son nom (icône de 32 px à 12 px du
// bord, nom à 48 px, 16 px de marge à droite), entre kPuceMinL et la part de la rangée
// qui lui revient ; « … » seulement quand la rangée entière ne suffit pas.
constexpr int32_t kPuceEcart = 12;
constexpr int32_t kPuceNomX = 48;         // lecteur_puce.yaml : x du nom
constexpr int32_t kPuceMargeD = 16;
constexpr int32_t kPuceMinL = 96;
constexpr int32_t kPucesL = kLecteurCarteL - 2 * 24;   // rangée : les marges de la pochette
constexpr int32_t kPucesCentreX = kLecteurCarteL / 2;  // milieu de la carte
constexpr int32_t kMiniTexteL = 252;      // mini-barre : entre la vignette et le bouton
constexpr uint32_t kPauseVisibleMs = 5u * 60u * 1000u;  // mini-barre après une pause
// Lecteur compact de la zone à gauche de l'horloge (lecteur_zone.yaml, ADR-0051 lot 2) :
// pochette carrée sur la hauteur de la carte moins kZoneMarge d'air, colonne à sa droite
// (titre, artiste, barre, boutons centrés sous elle).
constexpr int32_t kZoneMarge = 16;
constexpr int32_t kZoneLigne = 28;        // du haut du titre au haut de l'artiste
constexpr int32_t kZoneBarreEcart = 12;   // sous l'artiste
constexpr int32_t kZoneBarreH = 4;
constexpr int32_t kZoneBtnL = 52;         // précédent, suivant
constexpr int32_t kZoneLectureL = 60;     // lecture / pause
constexpr int32_t kZoneBtnEcart = 12;
constexpr int32_t kZoneRayonMin = 10;     // pochette : rayon du cadre moins la marge, au moins

struct Lecteur {
    char nom[kLecteurNomMax + 1] = "";
    LecteurGenre genre = LecteurGenre::AUTRE;
};

// Ce que montre le popup : l'état lu, ses textes copiés (le payload ne vit pas).
struct Etat {
    LecteurEtatLu lu;  // Champ vides : seuls les textes ci-dessous sont lus
    char nom[kLecteurNomMax + 1] = "";
    char titre[kLecteurTexteMax + 1] = "";
    char artiste[kLecteurTexteMax + 1] = "";
    char album[kLecteurTexteMax + 1] = "";
    char app[kLecteurNomMax + 1] = "";
    char image[kLecteurUrlMax] = "";  // URL complète, "" sans pochette
};

Lecteur s_lecteurs[kLecteursMax];
int s_n = 0;
Etat s_e;
bool s_recu = false;             // une poussée de HA est arrivée depuis le démarrage
uint32_t s_recu_ms = 0;          // réception de la position (millis)
// Passage en pause (millis) et son drapeau : jamais « millis() | 1 », qui dépasse millis() d'un
// quand la milliseconde est paire ; la différence lue dans la même milliseconde valait alors
// 2^32 - 1 et la mini-barre restait masquée (une pause sur deux ; rendu clair, 10/10/2026).
uint32_t s_pause_ms = 0;
bool s_en_pause = false;
bool s_image_prete = false;      // pochette décodée pour s_e.image
char s_url_demandee[kLecteurUrlMax] = "";
char s_base[64] = "";            // http://hôte:8123, d'après le client API de HA
bool s_glisse[2] = {false, false};  // un curseur est tenu : la poussée ne le bouge pas
bool s_zone_montre = false;      // le lecteur compact est le contenu de la zone gauche
bool s_zone_sale = true;         // à repeindre à sa prochaine apparition

const LecteurUI& ui() { return g_lecteur_ui; }

bool popup_ouvert() { return ui().popup != nullptr && !lv_obj_has_flag(ui().popup, LV_OBJ_FLAG_HIDDEN); }

void copier(char* dst, size_t cap, const Champ& c) { texte_ha_copier(dst, cap, c.p, c.n); }

bool a(uint16_t f) { return (s_e.lu.fonctions & f) != 0; }

bool en_lecture() { return s_e.lu.etat == LecteurEtat::LECTURE || s_e.lu.etat == LecteurEtat::CHARGEMENT; }

// Allumé : quelque chose à montrer et à commander (lecture, pause, inactif).
bool allume() {
    switch (s_e.lu.etat) {
        case LecteurEtat::LECTURE:
        case LecteurEtat::PAUSE:
        case LecteurEtat::CHARGEMENT:
        case LecteurEtat::INACTIF: return true;
        default: return false;
    }
}

// ─── Glyphes (règle 7 : chacun rattaché à ses labels dans MDI_CODE_TARGETS) ───

const char* glyphe_lecture(bool lecture) {
    return lecture ? "\U000F03E4"   // pause
                   : "\U000F040A";  // play
}

const char* glyphe_aleatoire(bool actif) {
    return actif ? "\U000F049D"   // shuffle
                 : "\U000F049E";  // shuffle-disabled
}

const char* glyphe_repetition(LecteurRepetition r) {
    switch (r) {
        case LecteurRepetition::TOUT: return "\U000F0456";  // repeat
        case LecteurRepetition::UNE: return "\U000F0458";   // repeat-once
        default: return "\U000F0457";                       // repeat-off
    }
}

const char* glyphe_volume(bool muet) {
    return muet ? "\U000F0581"   // volume-off
                : "\U000F057E";  // volume-high
}

const char* glyphe_genre(LecteurGenre g) {
    switch (g) {
        case LecteurGenre::TV: return "\U000F0502";        // television
        case LecteurGenre::ENCEINTE: return "\U000F04C3";  // speaker
        case LecteurGenre::AMPLI: return "\U000F0030";     // amplifier
        default: return "\U000F0387";                      // music-note
    }
}

// ─── Événements ───

void envoyer(const char* action, const char* valeur = "") {
    if (ui().envoyer == nullptr) return;
    char lecteur[8];
    std::snprintf(lecteur, sizeof(lecteur), "%d", s_e.lu.actif);
    ui().envoyer(action, lecteur, valeur);
}

// ─── Peinture ───

// Message du popup quand rien ne se lit, ou "" s'il y a un morceau à montrer.
const char* message() {
    if (s_e.lu.etat == LecteurEtat::AUCUN) return s_recu ? tr("Aucun lecteur choisi") : tr("En attente de Home Assistant");
    switch (s_e.lu.etat) {
        case LecteurEtat::ETEINT:
        case LecteurEtat::VEILLE: return tr("Lecteur éteint");
        case LecteurEtat::INDISPONIBLE: return tr("Lecteur indisponible");
        default: break;
    }
    return s_e.titre[0] == '\0' && s_e.artiste[0] == '\0' ? tr("Rien en lecture") : "";
}

void peindre_pochette() {
    const LecteurUI& u = ui();
    const bool img = s_image_prete && s_e.image[0] != '\0';
    ui_hidden(u.pochette, !img);
    ui_hidden(u.pochette_vide, img);
    ui_hidden(u.mini_pochette, !img);
    ui_hidden(u.mini_vide, img);
    ui_hidden(u.zone_pochette, !img);
    ui_hidden(u.zone_vide, img);
}

void peindre_position() {
    const LecteurUI& u = ui();
    // Un direct (durée inconnue) n'a pas de barre.
    ui_hidden(u.position, std::isnan(s_e.lu.duree));
    const float ecoule = static_cast<float>(esphome::millis() - s_recu_ms) / 1000.0f;
    const float p = lecteur_position(s_e.lu, ecoule);
    char t[16];
    lecteur_temps_texte(p, t, sizeof(t));
    ui_text(u.ecoule, t);
    lecteur_temps_texte(s_e.lu.duree, t, sizeof(t));
    ui_text(u.duree, t);
    ui_hidden(u.ecoule, std::isnan(p));
    ui_hidden(u.duree, std::isnan(s_e.lu.duree));
    if (u.position != nullptr && !s_glisse[LECTEUR_CURSEUR_POSITION] && !std::isnan(s_e.lu.duree) && !std::isnan(p)) {
        const int32_t v = tab5_float_vers_int(p * 1000.0f / s_e.lu.duree, 0, 1000, 0);
        if (lv_slider_get_value(u.position) != v) lv_slider_set_value(u.position, v, LV_ANIM_OFF);
    }
    // Sans seek, le curseur montre la position sans se laisser déplacer.
    if (u.position != nullptr) {
        if (a(LECTEUR_F_POSITION)) lv_obj_add_flag(u.position, LV_OBJ_FLAG_CLICKABLE);
        else lv_obj_remove_flag(u.position, LV_OBJ_FLAG_CLICKABLE);
    }
}

void peindre_volume() {
    const LecteurUI& u = ui();
    const bool muet = s_e.lu.muet == 1;
    ui_text(u.ico_muet, glyphe_volume(muet || s_e.lu.volume == 0));
    ui_text_color(u.ico_muet, muet ? UIColor.ACCENT : UIColor.TEXT_SOFT);
    ui_hidden(u.btn[LECTEUR_BTN_MUET], !a(LECTEUR_F_MUET) && !a(LECTEUR_F_VOLUME));
    const bool vol = a(LECTEUR_F_VOLUME) && s_e.lu.volume >= 0;
    ui_hidden(u.volume, !vol);
    ui_hidden(u.volume_texte, !vol);
    if (!vol) return;
    if (u.volume != nullptr && !s_glisse[LECTEUR_CURSEUR_VOLUME] && lv_slider_get_value(u.volume) != s_e.lu.volume)
        lv_slider_set_value(u.volume, s_e.lu.volume, LV_ANIM_OFF);
    if (!s_glisse[LECTEUR_CURSEUR_VOLUME]) {
        char t[8];
        std::snprintf(t, sizeof(t), "%d %%", s_e.lu.volume);
        ui_text(u.volume_texte, t);
    }
}

void peindre_commandes() {
    const LecteurUI& u = ui();
    ui_hidden(u.btn[LECTEUR_BTN_LECTURE], !a(LECTEUR_F_LECTURE));
    ui_hidden(u.btn[LECTEUR_BTN_PRECEDENT], !a(LECTEUR_F_PRECEDENT));
    ui_hidden(u.btn[LECTEUR_BTN_SUIVANT], !a(LECTEUR_F_SUIVANT));
    ui_hidden(u.btn[LECTEUR_BTN_ALEATOIRE], !a(LECTEUR_F_ALEATOIRE));
    ui_hidden(u.btn[LECTEUR_BTN_REPETITION], !a(LECTEUR_F_REPETITION));
    ui_text(u.ico_lecture, glyphe_lecture(en_lecture()));
    ui_text(u.mini_ico, glyphe_lecture(en_lecture()));
    const bool aleatoire = s_e.lu.aleatoire == 1;
    ui_text(u.ico_aleatoire, glyphe_aleatoire(aleatoire));
    ui_text_color(u.ico_aleatoire, aleatoire ? UIColor.ACCENT : UIColor.TEXT_SOFT);
    const bool repete = s_e.lu.repetition == LecteurRepetition::TOUT || s_e.lu.repetition == LecteurRepetition::UNE;
    ui_text(u.ico_repetition, glyphe_repetition(s_e.lu.repetition));
    ui_text_color(u.ico_repetition, repete ? UIColor.ACCENT : UIColor.TEXT_SOFT);
}

// Largeur du texte dans la police du label (une ligne).
int32_t largeur_texte(lv_obj_t* lbl, const char* txt) {
    const lv_font_t* f = lbl != nullptr ? lv_obj_get_style_text_font(lbl, LV_PART_MAIN) : nullptr;
    if (f == nullptr || txt == nullptr) return 0;
    lv_point_t p;
    lv_text_get_size(&p, txt, f, lv_obj_get_style_text_letter_space(lbl, LV_PART_MAIN), 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    return p.x;
}

void peindre_puces() {
    const LecteurUI& u = ui();
    // Noms montrés : celui de HA, sinon « Lecteur n ».
    std::string noms[kLecteurPuces];
    int32_t largeurs[kLecteurPuces] = {};
    for (int i = 0; i < s_n && i < kLecteurPuces; i++) {
        if (s_lecteurs[i].nom[0] != '\0') {
            noms[i] = s_lecteurs[i].nom;
        } else {
            char n[4];
            std::snprintf(n, sizeof(n), "%d", i + 1);
            noms[i] = tr_fill("Lecteur {n}", {{"n", n}});
        }
        const int32_t naturelle = kPuceNomX + largeur_texte(u.puce_nom[i], noms[i].c_str()) + kPuceMargeD;
        largeurs[i] = naturelle < kPuceMinL ? kPuceMinL : naturelle;
    }
    // Partage de la rangée : une pastille qui tient dans sa part garde sa largeur, la place
    // qu'elle laisse revient aux autres ; celles qui dépassent encore se partagent le reste.
    if (s_n > 0) {
        bool fixe[kLecteurPuces] = {};
        int32_t reste = kPucesL - (s_n - 1) * kPuceEcart;
        int libres = s_n;
        bool change = true;
        while (change && libres > 0) {
            change = false;
            const int32_t part = reste / libres;
            for (int i = 0; i < s_n; i++) {
                if (fixe[i] || largeurs[i] > part) continue;
                fixe[i] = true;
                reste -= largeurs[i];
                libres--;
                change = true;
            }
        }
        if (libres > 0) {
            const int32_t part = reste / libres;
            for (int i = 0; i < s_n; i++)
                if (!fixe[i]) largeurs[i] = part;
        }
    }
    int32_t largeur = s_n > 0 ? (s_n - 1) * kPuceEcart : 0;
    for (int i = 0; i < s_n; i++) largeur += largeurs[i];
    int32_t x = kPucesCentreX - largeur / 2;
    for (int i = 0; i < kLecteurPuces; i++) {
        const bool montre = i < s_n;
        ui_hidden(u.puce[i], !montre);
        if (!montre) continue;
        ui_x(u.puce[i], x);
        lv_obj_set_width(u.puce[i], largeurs[i]);
        x += largeurs[i] + kPuceEcart;
        ui_text(u.puce_icone[i], glyphe_genre(s_lecteurs[i].genre));
        const int32_t nom_l = largeurs[i] - kPuceNomX - kPuceMargeD;
        lv_obj_set_width(u.puce_nom[i], nom_l);
        texte_ha_coupe(u.puce_nom[i], noms[i].c_str(), nom_l);
        const bool actif = i == s_e.lu.actif;
        // Active : bord accent de 3 px ; les autres : le liseré du verre (GLASS_RIM).
        highlight_button_border(u.puce[i], actif, UIColor.ACCENT, 3);
        const uint32_t c = actif ? UIColor.ACCENT : UIColor.TEXT_SOFT;
        ui_text_color(u.puce_icone[i], c);
        ui_text_color(u.puce_nom[i], c);
    }
}

void peindre_popup() {
    const LecteurUI& u = ui();
    if (u.popup == nullptr) return;
    // En-tête : le lecteur, et l'application qu'il montre.
    const bool app = s_e.app[0] != '\0' && allume();
    ui_hidden(u.app, !app);
    if (app) texte_ha_coupe(u.app_texte, s_e.app, kAppL);
    const char* nom = s_e.nom[0] != '\0' ? s_e.nom : tr("Musique");
    texte_ha_coupe(u.nom, nom, app ? kNomL : kNomSeulL);

    const char* msg = message();
    const bool morceau = msg[0] == '\0';
    ui_hidden(u.message, morceau);
    if (!morceau) ui_text(u.message, msg);
    // Conseil : où choisir les lecteurs, ou comment allumer.
    const char* conseil = "";
    if (s_e.lu.etat == LecteurEtat::AUCUN && s_recu)
        conseil = tr("Choisissez vos lecteurs dans Home Assistant : « Tab5 · lecteurs de musique ».");
    ui_hidden(u.conseil, conseil[0] == '\0');
    if (conseil[0] != '\0') ui_text(u.conseil, conseil);
    ui_hidden(u.btn[LECTEUR_BTN_ALLUMER], allume() || !a(LECTEUR_F_ALLUMER));

    ui_hidden(u.titre, !morceau);
    ui_hidden(u.artiste, !morceau);
    ui_hidden(u.album, !morceau);
    if (morceau) {
        texte_ha_coupe(u.titre, s_e.titre[0] != '\0' ? s_e.titre : s_e.app, kTexteL);
        texte_ha_coupe(u.artiste, s_e.artiste, kTexteL);
        texte_ha_coupe(u.album, s_e.album, kTexteL);
    }
    // Position, commandes et volume : lecteur allumé seulement.
    ui_hidden(u.lecture, !allume());
    if (allume()) {
        peindre_position();
        peindre_commandes();
        peindre_volume();
    }
    peindre_puces();
}

// Mini-barre : en lecture, ou dans les 5 min qui suivent une pause.
bool mini_visible() {
    if (s_e.lu.etat == LecteurEtat::PAUSE) return s_en_pause && esphome::millis() - s_pause_ms < kPauseVisibleMs;
    return en_lecture();
}

void peindre_mini() {
    const LecteurUI& u = ui();
    if (u.mini == nullptr) return;
    // Le lecteur compact de la zone gauche montre déjà le morceau : pas de mini-barre
    // (choix d'Axel, 10/10/2026) ; elle revient quand la zone change.
    const bool montre = mini_visible() && !s_zone_montre;
    ui_hidden(u.mini, !montre);
    nabu_masquer(montre);
    if (!montre) return;
    const char* l1 = s_e.titre[0] != '\0' ? s_e.titre : (s_e.app[0] != '\0' ? s_e.app : s_e.nom);
    const char* l2 = s_e.artiste[0] != '\0' ? s_e.artiste : s_e.nom;
    texte_ha_coupe(u.mini_titre, l1, kMiniTexteL);
    texte_ha_coupe(u.mini_artiste, l2 == l1 ? "" : l2, kMiniTexteL);
    ui_text(u.mini_ico, glyphe_lecture(en_lecture()));
    // Le bouton rond qui porte l'icône : masqué si le lecteur ne sait pas lire / pause.
    if (u.mini_ico != nullptr) ui_hidden(lv_obj_get_parent(u.mini_ico), !a(LECTEUR_F_LECTURE));
}

// ─── Lecteur compact de la zone à gauche de l'horloge ───

// Ce que le coin arrondi de la carte (rayon intérieur ri, hauteur h) mange au bord droit
// d'une ligne de y0 à y1 : 0 au milieu de la carte, quelques pixels dans un cadre très
// arrondi (thème Capsule).
int32_t retrait_coin(int32_t ri, int32_t h, int32_t y0, int32_t y1) {
    if (ri <= 0) return 0;
    int32_t dy = 0;
    if (y0 < ri) dy = ri - y0;
    if (y1 > h - ri && y1 - (h - ri) > dy) dy = y1 - (h - ri);
    if (dy <= 0) return 0;
    if (dy > ri) dy = ri;
    return ri - static_cast<int32_t>(std::floor(std::sqrt(static_cast<float>(ri * ri - dy * dy))));
}

// Taille d'un widget sans toucher à son drapeau « masqué » (ui_poser le montrerait).
void taille(lv_obj_t* o, int32_t w, int32_t h) {
    if (o == nullptr) return;
    if (lv_obj_get_style_width(o, LV_PART_MAIN) != w) lv_obj_set_width(o, w);
    if (lv_obj_get_style_height(o, LV_PART_MAIN) != h) lv_obj_set_height(o, h);
}

bool zone_barre_visible() { return allume() && message()[0] == '\0' && !std::isnan(s_e.lu.duree); }

void peindre_zone_position() {
    const LecteurUI& u = ui();
    if (u.zone_barre == nullptr || !zone_barre_visible()) return;
    const float p = lecteur_position(s_e.lu, static_cast<float>(esphome::millis() - s_recu_ms) / 1000.0f);
    const int32_t v = std::isnan(p) ? 0 : tab5_float_vers_int(p * 1000.0f / s_e.lu.duree, 0, 1000, 0);
    if (lv_bar_get_value(u.zone_barre) != v) lv_bar_set_value(u.zone_barre, v, LV_ANIM_OFF);
}

void peindre_zone() {
    const LecteurUI& u = ui();
    if (u.zone == nullptr) return;
    if (!s_zone_montre) {
        s_zone_sale = true;  // caché : repeint à sa prochaine apparition
        return;
    }
    s_zone_sale = false;
    // Géométrie du thème : bordure et rayon de la carte (padding nul, lecteur_zone.yaml).
    const int32_t b = lv_obj_get_style_border_width(u.zone, LV_PART_MAIN);
    const int32_t carte_l = lv_obj_get_style_width(u.zone, LV_PART_MAIN);
    const int32_t carte_h = lv_obj_get_style_height(u.zone, LV_PART_MAIN);
    const int32_t w = carte_l - 2 * b, h = carte_h - 2 * b;
    int32_t r = lv_obj_get_style_radius(u.zone, LV_PART_MAIN);
    if (r > carte_h / 2) r = carte_h / 2;  // LV_RADIUS_CIRCLE compris
    const int32_t ri = r > b ? r - b : 0;
    const int32_t m = kZoneMarge, c = h - 2 * m;
    // Pochette : son rayon suit celui de la carte, à la marge près (coins concentriques).
    int32_t rc = ri - m;
    if (rc < kZoneRayonMin) rc = kZoneRayonMin;
    if (rc > c / 2) rc = c / 2;
    ui_x(u.zone_cadre, m);
    ui_y(u.zone_cadre, m);
    taille(u.zone_cadre, c, c);
    ui_style_num(u.zone_cadre, LV_STYLE_RADIUS, rc);
    taille(u.zone_pochette, c, c);
    ui_style_num(u.zone_pochette, LV_STYLE_RADIUS, rc);
    // Textes : le morceau, sinon la phrase du popup (« Rien en lecture »…) et le lecteur.
    const char* msg = message();
    const bool morceau = msg[0] == '\0';
    const char* l1 = msg;
    const char* l2 = s_e.lu.etat == LecteurEtat::AUCUN ? "" : s_e.nom;
    if (morceau) {
        l1 = s_e.titre[0] != '\0' ? s_e.titre : s_e.app;
        l2 = s_e.artiste[0] != '\0' ? s_e.artiste : s_e.nom;
    }
    const bool cmd[3] = {allume() && a(LECTEUR_F_PRECEDENT), allume() && a(LECTEUR_F_LECTURE),
                         allume() && a(LECTEUR_F_SUIVANT)};
    const bool commandes = cmd[0] || cmd[1] || cmd[2];
    const bool barre = zone_barre_visible();
    const lv_font_t* f = u.zone_titre != nullptr ? lv_obj_get_style_text_font(u.zone_titre, LV_PART_MAIN) : nullptr;
    const int32_t lh = f != nullptr ? lv_font_get_line_height(f) : 26;
    const int32_t x0 = m + c + m, cw = w - m - x0;
    // Sans commande ni barre, les deux lignes au milieu de la hauteur.
    int32_t y1 = m + 2;
    if (!commandes && !barre) y1 = (h - (l2[0] != '\0' ? kZoneLigne + lh : lh)) / 2;
    const int32_t y2 = y1 + kZoneLigne;
    ui_x(u.zone_titre, x0);
    ui_y(u.zone_titre, y1);
    texte_ha_coupe(u.zone_titre, l1, cw - retrait_coin(ri, h, y1, y1 + lh));
    ui_hidden(u.zone_artiste, l2[0] == '\0');
    ui_x(u.zone_artiste, x0);
    ui_y(u.zone_artiste, y2);
    texte_ha_coupe(u.zone_artiste, l2, cw - retrait_coin(ri, h, y2, y2 + lh));
    // Barre de lecture fine sous l'artiste.
    ui_hidden(u.zone_barre, !barre);
    if (barre) {
        const int32_t yb = y2 + lh + kZoneBarreEcart;
        ui_x(u.zone_barre, x0);
        ui_y(u.zone_barre, yb);
        taille(u.zone_barre, cw - retrait_coin(ri, h, yb, yb + kZoneBarreH), kZoneBarreH);
        peindre_zone_position();
    }
    // Précédent, lecture / pause, suivant : centrés sous la colonne, en bas ; chacun masqué
    // à sa place quand le lecteur ne l'offre pas.
    const int32_t larg[3] = {kZoneBtnL, kZoneLectureL, kZoneBtnL};
    int32_t x = x0 + (cw - (2 * kZoneBtnL + kZoneLectureL + 2 * kZoneBtnEcart)) / 2;
    const int32_t y_bas = h - m;
    for (int i = 0; i < 3; i++) {
        ui_hidden(u.zone_btn[i], !cmd[i]);
        ui_x(u.zone_btn[i], x);
        ui_y(u.zone_btn[i], y_bas - kZoneLectureL + (kZoneLectureL - larg[i]) / 2);
        x += larg[i] + kZoneBtnEcart;
    }
    ui_text(u.zone_ico, glyphe_lecture(en_lecture()));
}

void peindre() {
    peindre_popup();
    peindre_zone();
    peindre_mini();
    peindre_pochette();
}

// Pochette : téléchargée seulement si son adresse change ; aucune → image libérée.
void demander_image() {
    if (std::strcmp(s_url_demandee, s_e.image) == 0) return;
    std::snprintf(s_url_demandee, sizeof(s_url_demandee), "%s", s_e.image);
    s_image_prete = false;
    if (ui().image != nullptr) ui().image(s_e.image);
}

}  // namespace

void lecteur_recu(const std::string& lecteurs, const std::string& etat) {
    if (payload_trop_long(kTag, lecteurs.size() + etat.size())) return;
    LecteurListeLu l[kLecteursMax];
    s_n = lecteurs_lire(Champ{lecteurs.data(), lecteurs.size()}, l);
    for (int i = 0; i < s_n; i++) {
        copier(s_lecteurs[i].nom, sizeof(s_lecteurs[i].nom), l[i].nom);
        s_lecteurs[i].genre = l[i].genre;
    }
    const LecteurEtat avant = s_e.lu.etat;
    const bool zone_avant = lecteur_zone_disponible();
    s_recu = true;
    LecteurEtatLu lu;
    if (!lecteur_etat_lire(Champ{etat.data(), etat.size()}, lu) && !etat.empty())
        payload_refuse(kTag, "état illisible", etat.size());
    copier(s_e.nom, sizeof(s_e.nom), lu.nom);
    copier(s_e.titre, sizeof(s_e.titre), lu.titre);
    copier(s_e.artiste, sizeof(s_e.artiste), lu.artiste);
    copier(s_e.album, sizeof(s_e.album), lu.album);
    copier(s_e.app, sizeof(s_e.app), lu.app);
    if (!ha_image_url(lu.image, s_base, s_e.image, sizeof(s_e.image)) && lu.image.n > 0)
        payload_refuse(kTag, "pochette illisible", lu.image.n);
    lu.nom = lu.titre = lu.artiste = lu.album = lu.app = lu.image = Champ{nullptr, 0};
    s_e.lu = lu;
    s_recu_ms = esphome::millis();
    if (s_e.lu.etat == LecteurEtat::PAUSE) {
        if (avant != LecteurEtat::PAUSE || !s_en_pause) {
            s_pause_ms = esphome::millis();
            s_en_pause = true;
        }
    } else {
        s_en_pause = false;
    }
    demander_image();
    peindre();
    // Liste vidée ou remplie dans HA : la zone gauche saute le lecteur compact, ou le
    // retrouve s'il y était.
    if (lecteur_zone_disponible() != zone_avant) zone_gauche_appliquer();
}

void lecteur_ouvrir(const std::string& cle) {
    peindre();
    if (ui().envoyer != nullptr) ui().envoyer("ouvrir", cle.c_str(), "");
}

void lecteur_tic() {
    if (ui().mini != nullptr && !lv_obj_has_flag(ui().mini, LV_OBJ_FLAG_HIDDEN) && !mini_visible()) peindre_mini();
    if (popup_ouvert() && allume() && en_lecture()) peindre_position();
    if (s_zone_montre && en_lecture()) peindre_zone_position();
}

void lecteur_image_prete(const lv_image_dsc_t* dsc) {
    const LecteurUI& u = ui();
    if (dsc == nullptr || dsc->data == nullptr || dsc->header.w == 0 || dsc->header.h == 0) {
        lecteur_image_erreur();
        return;
    }
    // Même descripteur, pixels neufs : LVGL ne doit rien garder de l'image d'avant. COVER :
    // l'image garde ses proportions et remplit le cadre (360 × 360, vignette de 56 × 56,
    // pochette du lecteur compact),
    // une pochette qui n'est pas carrée est rognée au lieu d'être bordée de noir.
    lv_image_cache_drop(dsc);
    for (lv_obj_t* img : {u.pochette, u.mini_pochette, u.zone_pochette}) {
        if (img == nullptr) continue;
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_COVER);
        lv_image_set_src(img, dsc);
    }
    s_image_prete = true;
    peindre_pochette();
}

void lecteur_image_erreur() {
    s_image_prete = false;
    peindre_pochette();
}

void lecteur_hote_ha(const std::string& adresse) { ha_base_depuis_hote(adresse.c_str(), s_base, sizeof(s_base)); }

void lecteur_appui(int bouton) {
    if (ui_appui_glisse()) return;
    char v[8] = "";
    switch (bouton) {
        case LECTEUR_BTN_LECTURE:
            if (!a(LECTEUR_F_LECTURE)) return;
            envoyer("lecture");
            // L'effet attendu tout de suite ; la poussée de HA le confirme.
            s_e.lu.etat = en_lecture() ? LecteurEtat::PAUSE : LecteurEtat::LECTURE;
            s_recu_ms = esphome::millis();
            s_en_pause = s_e.lu.etat == LecteurEtat::PAUSE;
            s_pause_ms = esphome::millis();
            break;
        case LECTEUR_BTN_PRECEDENT:
            if (a(LECTEUR_F_PRECEDENT)) envoyer("precedent");
            return;
        case LECTEUR_BTN_SUIVANT:
            if (a(LECTEUR_F_SUIVANT)) envoyer("suivant");
            return;
        case LECTEUR_BTN_ALEATOIRE:
            if (!a(LECTEUR_F_ALEATOIRE)) return;
            s_e.lu.aleatoire = s_e.lu.aleatoire == 1 ? 0 : 1;
            envoyer("aleatoire", s_e.lu.aleatoire == 1 ? "1" : "0");
            break;
        case LECTEUR_BTN_REPETITION: {
            if (!a(LECTEUR_F_REPETITION)) return;
            // off → all → one → off
            const LecteurRepetition r = s_e.lu.repetition == LecteurRepetition::TOUT  ? LecteurRepetition::UNE
                                        : s_e.lu.repetition == LecteurRepetition::UNE ? LecteurRepetition::NON
                                                                                      : LecteurRepetition::TOUT;
            s_e.lu.repetition = r;
            const char* code = "off";
            if (r == LecteurRepetition::TOUT) code = "all";
            else if (r == LecteurRepetition::UNE) code = "one";
            envoyer("repetition", code);
            break;
        }
        case LECTEUR_BTN_MUET:
            if (!a(LECTEUR_F_MUET)) return;
            s_e.lu.muet = s_e.lu.muet == 1 ? 0 : 1;
            std::snprintf(v, sizeof(v), "%d", s_e.lu.muet);
            envoyer("muet", v);
            break;
        case LECTEUR_BTN_ALLUMER:
            if (a(LECTEUR_F_ALLUMER)) envoyer("allumer");
            return;
        default: return;
    }
    peindre();
}

void lecteur_curseur(int curseur, bool relache) {
    const LecteurUI& u = ui();
    if (curseur == LECTEUR_CURSEUR_POSITION) {
        s_glisse[curseur] = !relache;
        if (u.position == nullptr || std::isnan(s_e.lu.duree)) return;
        const float p = static_cast<float>(lv_slider_get_value(u.position)) * s_e.lu.duree / 1000.0f;
        char t[16];
        lecteur_temps_texte(p, t, sizeof(t));
        ui_text(u.ecoule, t);
        if (!relache || !a(LECTEUR_F_POSITION)) return;
        char v[16];
        std::snprintf(v, sizeof(v), "%.0f", p);
        envoyer("position", v);
        s_e.lu.position = p;
        s_recu_ms = esphome::millis();
    } else if (curseur == LECTEUR_CURSEUR_VOLUME) {
        s_glisse[curseur] = !relache;
        if (u.volume == nullptr) return;
        const int vol = tab5_float_vers_int(static_cast<float>(lv_slider_get_value(u.volume)), 0, 100, 0);
        char t[8];
        std::snprintf(t, sizeof(t), "%d %%", vol);
        ui_text(u.volume_texte, t);
        if (!relache || !a(LECTEUR_F_VOLUME)) return;
        std::snprintf(t, sizeof(t), "%d", vol);
        envoyer("volume", t);
        s_e.lu.volume = vol;
        if (vol > 0 && s_e.lu.muet == 1) s_e.lu.muet = 0;  // HA rétablit le son en réglant le volume
        peindre_volume();
    }
}

void lecteur_puce(int i) {
    if (ui_appui_glisse() || i < 0 || i >= s_n || i == s_e.lu.actif) return;
    char lecteur[8];
    std::snprintf(lecteur, sizeof(lecteur), "%d", i);
    if (ui().envoyer != nullptr) ui().envoyer("choisir", lecteur, "");
    // La pastille touchée tout de suite ; le reste à la poussée de HA.
    s_e.lu.actif = i;
    peindre_puces();
}

void lecteur_rejouer_theme() {
    if (ui().popup == nullptr && ui().mini == nullptr && ui().zone == nullptr) return;
    peindre();
}

void lecteur_zone_montrer(bool montre) {
    if (montre == s_zone_montre && !(montre && s_zone_sale)) return;
    s_zone_montre = montre;
    peindre_zone();
    peindre_mini();
}

bool lecteur_zone_disponible() { return !(s_recu && s_e.lu.etat == LecteurEtat::AUCUN); }
