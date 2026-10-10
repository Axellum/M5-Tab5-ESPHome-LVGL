/**
 * [AI-CONTEXT]
 * @file tab5_serveur_ia.cpp
 * @role Serveur IA (ADR-0059, 10/10/2026, lot 2 de l'audit des LLM locaux) : peint, d'après
 *       ce que pousse tab5_maj_serveur_ia (lu par serveur_ia_lire(), Tab5/socle/
 *       tab5_parse.h, section 14), le popup « Serveur IA » (serveur_ia_popup.yaml) :
 *         - un bandeau : pastille en ligne / hors ligne / inconnu, état en clair, nom du
 *           serveur (nom de son appareil dans HA), modèle chargé coupé par « … » ;
 *         - à gauche, les tokens par seconde : grand chiffre (roboto_55_b), courbe lissée
 *           des kServeurIaPoints dernières poussées, requêtes en cours et en file ;
 *         - à droite, quatre cartes (serveur_ia_carte.yaml) : VRAM utilisée sur totale et
 *           sa barre, température du GPU colorée par niveau, RAM en % et sa barre,
 *           puissance en W.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : rien n'est
 *       demandé à HA ; aucune entité nommée ; AUCUN seuil ici (HA pousse le niveau de la
 *       température). Une valeur inconnue s'écrit « — » (serveur_ia_nombre_texte), jamais
 *       un zéro inventé. Couleurs par les rôles de la palette (ADR-0029) : pastille
 *       SUCCESS en ligne, INACTIVE hors ligne, BAR_INACTIVE inconnu ; température 0
 *       SUCCESS, 1 WARNING, 2 ERROR, inconnue TEXT_DIM ; courbe et barres ACCENT sur
 *       ARC_TRACK ; repeintes par serveur_ia_rejouer_theme(). Popup fermé : une poussée ne
 *       repeint RIEN (elle garde les valeurs et ajoute le point de la courbe) ; peint à
 *       l'ouverture. Pas de minuteur : la courbe avance au rythme des poussées de HA.
 *       Courbe et barres : lv_line et objets simples (lv_chart et lv_canvas ne sont pas
 *       compilés), créés à la première peinture ; écritures comparées d'abord.
 * @ai_warning lv_line_set_points() ne COPIE PAS les points : s_pts et s_base vivent au
 *       niveau du fichier. Les icônes des cartes sont posées par le YAML (mdi_font_32) :
 *       aucun glyphe n'est écrit d'ici.
 * @ai_instruction Un widget de plus = son champ dans ServeurIaUI (tab5_serveur_ia.h) et sa
 *       ligne dans le script tab5_serveur_ia_lier (tab5-serveur-ia.yaml). Un texte affiché
 *       passe par tr(). Les boutons d'action (lot 3) iront dans le bandeau, à droite du
 *       modèle : le modèle y est coupé à la place qui reste (texte_ha_coupe).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <cmath>
#include <cstdio>
#include <cstring>

ServeurIaUI g_serveur_ia_ui;

namespace {

constexpr const char* kTag = "tab5.serveur_ia";
constexpr size_t kNomMax = 48;
constexpr size_t kModeleMax = 96;
constexpr int kLisse = 4;  // points de courbe entre deux poussées
constexpr int kCourbeMax = (kServeurIaPoints - 1) * kLisse + 1;
static_assert(kServeurIaPoints <= kCourbeLissePoints, "ui_courbe_lisse ne trace pas plus de points");

// Corps de la carte modale : x kCorpsX..kCorpsX + kCorpsW, y kCorpsY..kCorpsBas (comme
// Froid). Bandeau en haut, puis la carte des tokens/s à gauche et la grille 2 × 2 à droite.
constexpr int32_t kCorpsBas = kCarteH - 20;
constexpr int32_t kBandeauH = 76;
constexpr int32_t kBasY = kCorpsY + kBandeauH + kCartesEcart;
constexpr int32_t kBasH = kCorpsBas - kBasY;
constexpr int32_t kGaucheL = (kCorpsW - kCartesEcart) / 2;
constexpr int32_t kDroiteX = kCorpsX + kGaucheL + kCartesEcart;
constexpr int32_t kDroiteL = kCorpsX + kCorpsW - kDroiteX;
constexpr int32_t kPetiteL = (kDroiteL - kCartesEcart) / 2;
constexpr int32_t kPetiteH = (kBasH - kCartesEcart) / 2;
// Dans une carte.
constexpr int32_t kMarge = 22;
constexpr int32_t kHautY = 12;
constexpr int32_t kIconeEcart = 10;   // entre une icône et son titre
constexpr int32_t kEcart = 6;         // entre deux rangées de texte
constexpr int32_t kCourbeEcart = 18;  // au-dessus et au-dessous de la courbe
constexpr int32_t kTrait = 4;
constexpr int32_t kTraitBase = 2;
constexpr int32_t kPoint = 12;        // dernier point de la courbe
constexpr int32_t kAnneau = 3;        // son anneau, à la couleur du fond
constexpr int32_t kPastille = 20;
constexpr int32_t kBarreH = 12;

// Les quatre petites cartes (index de ServeurIaUI::carte).
constexpr int VRAM = 0;
constexpr int GPU = 1;
constexpr int RAM = 2;
constexpr int PUISSANCE = 3;

// Ce qui est reçu : nom et modèle copiés à part (le payload ne vit pas ; leurs Champ vidés).
ServeurIaLu s_lu;
char s_nom[kNomMax + 1] = {};
char s_modele[kModeleMax + 1] = {};
bool s_recu = false;     // une poussée de HA est arrivée depuis le démarrage
bool s_present = false;  // la dernière poussée décrit un serveur
ServeurIaCourbe s_courbe;

// Objets créés à la première peinture.
lv_obj_t* s_pastille = nullptr;
lv_obj_t* s_ligne = nullptr;
lv_obj_t* s_point = nullptr;
lv_obj_t* s_base = nullptr;
lv_obj_t* s_piste[kServeurIaCartes] = {};
lv_obj_t* s_jauge[kServeurIaCartes] = {};
lv_point_precise_t s_pts[kCourbeMax];
lv_point_precise_t s_base_pts[2];

const ServeurIaUI& ui() { return g_serveur_ia_ui; }

bool popup_ouvert() { return ui().popup != nullptr && !lv_obj_has_flag(ui().popup, LV_OBJ_FLAG_HIDDEN); }

int32_t largeur_texte(lv_obj_t* l, const char* t) {
    if (l == nullptr || t == nullptr || t[0] == '\0') return 0;
    lv_point_t sz;
    lv_text_get_size(&sz, t, lv_obj_get_style_text_font(l, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

int32_t hauteur_ligne(lv_obj_t* l) {
    const lv_font_t* f = l != nullptr ? lv_obj_get_style_text_font(l, LV_PART_MAIN) : nullptr;
    return f != nullptr ? lv_font_get_line_height(f) : 26;
}

void poser(lv_obj_t* o, int32_t x, int32_t y) {
    ui_x(o, x);
    ui_y(o, y);
    ui_hidden(o, false);
}

// Taille utile d'une carte : sa largeur ou sa hauteur moins bordure et padding (LVGL 9
// place les enfants dans la bordure et le padding).
int32_t utile(lv_obj_t* c, bool largeur) {
    const int32_t b = lv_obj_get_style_border_width(c, LV_PART_MAIN);
    if (largeur) {
        return lv_obj_get_style_width(c, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_left(c, LV_PART_MAIN) -
               lv_obj_get_style_pad_right(c, LV_PART_MAIN);
    }
    return lv_obj_get_style_height(c, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_top(c, LV_PART_MAIN) -
           lv_obj_get_style_pad_bottom(c, LV_PART_MAIN);
}

float requetes_float(int32_t r) { return r < 0 ? NAN : static_cast<float>(r); }

// Inconnu : TEXT_DIM, pas BAR_INACTIVE (égal à INACTIVE dans la palette sombre, la pastille
// « hors ligne » et « inconnu » s'y confondaient) ; TEXT_DIM ≠ INACTIVE dans les 41 palettes
// de tab5_themes_data.h (vérifié le 10/10/2026).
uint32_t couleur_pastille() {
    if (s_lu.en_ligne == 1) return UIColor.SUCCESS;
    if (s_lu.en_ligne == 0) return UIColor.INACTIVE;
    return UIColor.TEXT_DIM;
}

// Niveau inconnu (kServeurIaNiveauInconnu) : le texte secondaire, jamais le vert de
// « normale » pour une température que HA n'a pas classée.
uint32_t couleur_temperature() {
    if (!std::isfinite(s_lu.temperature)) return UIColor.TEXT_DIM;
    switch (s_lu.niveau) {
        case 0: return UIColor.SUCCESS;
        case 1: return UIColor.WARNING;
        case 2: return UIColor.ERROR;
        default: return UIColor.TEXT_DIM;
    }
}

// « 42.7 tok/s »… : le nombre et son unité (texte traduit, « %s Go »), ou « — » seul.
void valeur_unite(char* out, size_t n, float v, int decimales, const char* format) {
    char t[24];
    serveur_ia_nombre_texte(v, decimales, t, sizeof(t));
    if (std::isfinite(v)) snprintf(out, n, format, t);
    else snprintf(out, n, "%s", t);
}

// ─── Bandeau ───

void peindre_bandeau() {
    const ServeurIaUI& u = ui();
    lv_obj_t* b = u.bandeau;
    if (b == nullptr) return;
    ui_poser(b, kCorpsX, kCorpsY, kCorpsW, kBandeauH);
    const int32_t cw = utile(b, true), ch = utile(b, false);
    if (s_pastille == nullptr) s_pastille = ui_rectangle(b, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    const uint32_t c = couleur_pastille();
    ui_style_couleur(s_pastille, LV_STYLE_BG_COLOR, c);
    ui_poser(s_pastille, kMarge, (ch - kPastille) / 2, kPastille, kPastille);
    // État en clair, de la couleur de la pastille (inconnu : le texte secondaire).
    const char* etat = tr("État inconnu");
    if (s_lu.en_ligne == 1) etat = tr("En ligne");
    if (s_lu.en_ligne == 0) etat = tr("Hors ligne");
    ui_text(u.etat, etat);
    ui_text_color(u.etat, s_lu.en_ligne == 1 ? UIColor.SUCCESS : UIColor.TEXT_DIM);
    int32_t x = kMarge + kPastille + 14;
    poser(u.etat, x, (ch - hauteur_ligne(u.etat)) / 2);
    x += largeur_texte(u.etat, etat) + 28;
    // Le modèle à droite (aucun : « Aucun modèle chargé »), le nom entre les deux.
    const char* modele = s_modele[0] != '\0' ? s_modele : tr("Aucun modèle chargé");
    ui_text_color(u.modele, s_modele[0] != '\0' ? UIColor.TEXT_SOFT : UIColor.TEXT_DIM);
    const int32_t modele_max = (cw - x) / 2;
    texte_ha_coupe(u.modele, modele, modele_max);
    const int32_t lm = largeur_texte(u.modele, lv_label_get_text(u.modele));
    poser(u.modele, cw - kMarge - lm, (ch - hauteur_ligne(u.modele)) / 2);
    ui_hidden(u.nom, s_nom[0] == '\0');
    if (s_nom[0] != '\0') {
        poser(u.nom, x, (ch - hauteur_ligne(u.nom)) / 2);
        texte_ha_coupe(u.nom, s_nom, cw - kMarge - lm - 28 - x);
    }
}

// ─── Tokens par seconde ───

void creer_courbe(lv_obj_t* parent) {
    if (s_ligne != nullptr || parent == nullptr) return;
    // Ordre de création = ordre de dessin : la ligne de base sous la courbe.
    s_base = ui_ligne(parent, kTraitBase);
    s_ligne = ui_ligne(parent, kTrait);
    s_point = ui_rectangle(parent, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    ui_style_num(s_point, LV_STYLE_BORDER_WIDTH, kAnneau);
    ui_style_num(s_point, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
}

// La courbe entre x0 et x1, de y_haut à y_bas (0 tok/s en bas). Les kServeurIaPoints
// places sont fixes : les premières poussées entrent par la droite. Une valeur inconnue
// laisse un trou que la courbe enjambe ; moins de deux valeurs connues : seule la ligne de
// base reste.
void tracer(int32_t x0, int32_t x1, int32_t y_haut, int32_t y_bas) {
    // La ligne de base ne bouge qu'avec la géométrie : lv_line_set_points() invalide
    // toujours la ligne, il n'est appelé que si un point change (première peinture
    // comprise : s_base_pts part de zéro). La couleur (thème) passe par ui_style_couleur(),
    // qui compare d'abord.
    const lv_point_precise_t a = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(y_bas)};
    const lv_point_precise_t b = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(y_bas)};
    if (s_base_pts[0].x != a.x || s_base_pts[0].y != a.y || s_base_pts[1].x != b.x || s_base_pts[1].y != b.y) {
        s_base_pts[0] = a;
        s_base_pts[1] = b;
        lv_line_set_points(s_base, s_base_pts, 2);
    }
    ui_style_couleur(s_base, LV_STYLE_LINE_COLOR, UIColor.ARC_TRACK);
    ui_hidden(s_base, false);
    float haut = 0;
    if (!s_courbe.haut(haut)) {
        ui_hidden(s_ligne, true);
        ui_hidden(s_point, true);
        return;
    }
    float xs[kServeurIaPoints], ys[kServeurIaPoints];
    int m = 0;
    const int decalage = kServeurIaPoints - s_courbe.n;
    for (int k = 0; k < s_courbe.n; k++) {
        const float v = s_courbe.v[k];
        if (!std::isfinite(v)) continue;
        xs[m] = static_cast<float>(x0) +
                static_cast<float>(x1 - x0) * static_cast<float>(k + decalage) / static_cast<float>(kServeurIaPoints - 1);
        ys[m] = static_cast<float>(y_bas) - v / haut * static_cast<float>(y_bas - y_haut);
        m++;
    }
    const int np = ui_courbe_lisse(xs, ys, m, kLisse, s_pts);
    if (np < 2) {
        ui_hidden(s_ligne, true);
        ui_hidden(s_point, true);
        return;
    }
    ui_style_couleur(s_ligne, LV_STYLE_LINE_COLOR, UIColor.ACCENT);
    lv_line_set_points(s_ligne, s_pts, static_cast<uint32_t>(np));
    ui_hidden(s_ligne, false);
    ui_style_couleur(s_point, LV_STYLE_BG_COLOR, UIColor.ACCENT);
    ui_style_couleur(s_point, LV_STYLE_BORDER_COLOR, UIColor.BG);
    const int32_t px = static_cast<int32_t>(lroundf(xs[m - 1])), py = static_cast<int32_t>(lroundf(ys[m - 1]));
    ui_poser(s_point, px - kPoint / 2, py - kPoint / 2, kPoint, kPoint);
}

void peindre_tps() {
    const ServeurIaUI& u = ui();
    lv_obj_t* c = u.tps_carte;
    if (c == nullptr) return;
    ui_poser(c, kCorpsX, kBasY, kGaucheL, kBasH);
    const int32_t cw = utile(c, true), ch = utile(c, false);
    // En haut : l'icône et le titre (textes du YAML).
    const int32_t lh_icone = hauteur_ligne(u.tps_icone);
    poser(u.tps_icone, kMarge, kHautY);
    poser(u.tps_titre, kMarge + largeur_texte(u.tps_icone, lv_label_get_text(u.tps_icone)) + kIconeEcart,
          kHautY + (lh_icone - hauteur_ligne(u.tps_titre)) / 2);
    // Le grand chiffre et son unité, sur la même ligne de pied.
    char t[24];
    serveur_ia_nombre_texte(s_lu.tps, 1, t, sizeof(t));
    ui_text(u.tps_valeur, t);
    const int32_t y_valeur = kHautY + lh_icone + kEcart;
    const int32_t lh_valeur = hauteur_ligne(u.tps_valeur);
    poser(u.tps_valeur, kMarge, y_valeur);
    poser(u.tps_unite, kMarge + largeur_texte(u.tps_valeur, t) + 12,
          y_valeur + lh_valeur - hauteur_ligne(u.tps_unite) - 8);
    // En bas : les requêtes.
    char cours[16], file[16], req[96];
    serveur_ia_nombre_texte(requetes_float(s_lu.en_cours), 0, cours, sizeof(cours));
    serveur_ia_nombre_texte(requetes_float(s_lu.file), 0, file, sizeof(file));
    snprintf(req, sizeof(req), tr("Requêtes en cours : %s · en file : %s"), cours, file);
    const int32_t lh_req = hauteur_ligne(u.requetes);
    const int32_t y_req = ch - kMarge / 2 - lh_req;
    poser(u.requetes, kMarge, y_req);
    texte_ha_coupe(u.requetes, req, cw - 2 * kMarge);
    // La courbe entre les deux.
    creer_courbe(c);
    const int32_t y_haut = y_valeur + lh_valeur + kCourbeEcart;
    const int32_t y_bas = y_req - kCourbeEcart;
    if (y_bas - y_haut < 24) {
        for (lv_obj_t* o : {s_ligne, s_point, s_base}) ui_hidden(o, true);
        return;
    }
    tracer(kMarge, cw - kMarge, y_haut, y_bas);
}

// ─── Petites cartes ───

// Barre d'une carte (VRAM, RAM) : piste ARC_TRACK, jauge ACCENT ; `pct` inconnu : la piste
// seule.
void barre(int i, int32_t cw, int32_t ch, float pct) {
    lv_obj_t* carte = ui().carte[i];
    if (s_piste[i] == nullptr) {
        s_piste[i] = ui_rectangle(carte, LV_OPA_COVER, LV_RADIUS_CIRCLE);
        s_jauge[i] = ui_rectangle(carte, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    }
    const int32_t w = cw - 2 * kMarge, y = ch - kMarge - kBarreH;
    ui_style_couleur(s_piste[i], LV_STYLE_BG_COLOR, UIColor.ARC_TRACK);
    ui_poser(s_piste[i], kMarge, y, w, kBarreH);
    if (!std::isfinite(pct)) {
        ui_hidden(s_jauge[i], true);
        return;
    }
    // Au moins la hauteur de la barre : 1 % reste un point visible, pas un trait.
    int32_t wj = static_cast<int32_t>(lroundf(static_cast<float>(w) * pct / 100.0f));
    if (wj < kBarreH) wj = kBarreH;
    ui_style_couleur(s_jauge[i], LV_STYLE_BG_COLOR, UIColor.ACCENT);
    ui_poser(s_jauge[i], kMarge, y, wj, kBarreH);
}

void peindre_carte(int i, const char* valeur, const char* detail, uint32_t couleur, bool colore, bool avec_barre,
                   float pct) {
    const ServeurIaUI& u = ui();
    lv_obj_t* carte = u.carte[i];
    if (carte == nullptr) return;
    const int32_t x = kDroiteX + (i % 2) * (kPetiteL + kCartesEcart);
    const int32_t y = kBasY + (i / 2) * (kPetiteH + kCartesEcart);
    ui_poser(carte, x, y, kPetiteL, kPetiteH);
    const int32_t cw = utile(carte, true), ch = utile(carte, false);
    const int32_t lh_icone = hauteur_ligne(u.icone[i]);
    poser(u.icone[i], kMarge, kHautY);
    const int32_t x_titre = kMarge + largeur_texte(u.icone[i], lv_label_get_text(u.icone[i])) + kIconeEcart;
    poser(u.titre[i], x_titre, kHautY + (lh_icone - hauteur_ligne(u.titre[i])) / 2);
    const int32_t y_valeur = kHautY + lh_icone + 2 * kEcart;
    // Coupée à la carte (« … ») comme le détail : une valeur longue ne déborde jamais.
    texte_ha_coupe(u.valeur[i], valeur, cw - 2 * kMarge);
    if (colore) ui_text_color(u.valeur[i], couleur);
    poser(u.valeur[i], kMarge, y_valeur);
    ui_hidden(u.detail[i], detail[0] == '\0');
    if (detail[0] != '\0') {
        if (colore) ui_text_color(u.detail[i], couleur);
        poser(u.detail[i], kMarge, y_valeur + hauteur_ligne(u.valeur[i]) + kEcart);
        texte_ha_coupe(u.detail[i], detail, cw - 2 * kMarge);
    }
    if (avec_barre) barre(i, cw, ch, pct);
}

void peindre_cartes() {
    char valeur[32], detail[96], t[24], p[24];
    // VRAM : utilisée en Go (sinon le %), puis « sur 16.0 Go · 70 % ».
    if (std::isfinite(s_lu.vram)) valeur_unite(valeur, sizeof(valeur), s_lu.vram, 1, tr("%s Go"));
    else valeur_unite(valeur, sizeof(valeur), s_lu.vram_pct, 0, "%s %%");
    detail[0] = '\0';
    serveur_ia_nombre_texte(s_lu.vram_pct, 0, p, sizeof(p));
    if (std::isfinite(s_lu.vram_total)) {
        serveur_ia_nombre_texte(s_lu.vram_total, 1, t, sizeof(t));
        if (std::isfinite(s_lu.vram_pct)) snprintf(detail, sizeof(detail), tr("sur %s Go · %s %%"), t, p);
        else snprintf(detail, sizeof(detail), tr("sur %s Go"), t);
    } else if (std::isfinite(s_lu.vram) && std::isfinite(s_lu.vram_pct)) {
        snprintf(detail, sizeof(detail), "%s %%", p);
    }
    peindre_carte(VRAM, valeur, detail, 0, false, true, s_lu.vram_pct);
    // GPU : la température, colorée par le niveau que HA calcule.
    valeur_unite(valeur, sizeof(valeur), s_lu.temperature, 0, "%s °C");
    detail[0] = '\0';
    if (std::isfinite(s_lu.temperature) && s_lu.niveau <= 2) {
        snprintf(detail, sizeof(detail), "%s",
                 s_lu.niveau == 0   ? tr("Température normale")
                 : s_lu.niveau == 1 ? tr("Température élevée")
                                    : tr("Température critique"));
    }
    peindre_carte(GPU, valeur, detail, couleur_temperature(), true, false, NAN);
    // RAM : le % et sa barre.
    valeur_unite(valeur, sizeof(valeur), s_lu.ram, 0, "%s %%");
    peindre_carte(RAM, valeur, "", 0, false, true, s_lu.ram);
    // Puissance : en W.
    valeur_unite(valeur, sizeof(valeur), s_lu.puissance, 0, "%s W");
    peindre_carte(PUISSANCE, valeur, "", 0, false, false, NAN);
}

void peindre_popup() {
    const ServeurIaUI& u = ui();
    if (u.popup == nullptr) return;
    // Sans serveur : la phrase (en attente, ou aucun capteur choisi et où les choisir).
    const bool vide = !s_present;
    ui_hidden(u.attente, !vide);
    ui_hidden(u.conseil, !(s_recu && !s_present));
    if (vide) ui_text(u.attente, s_recu ? tr("Aucun serveur choisi") : tr("En attente de Home Assistant"));
    if (s_recu && !s_present) {
        ui_text(u.conseil, tr("Choisissez ses capteurs dans Home Assistant : listes « Tab5 · serveur IA »."));
    }
    ui_hidden(u.bandeau, vide);
    ui_hidden(u.tps_carte, vide);
    for (lv_obj_t* c : u.carte) ui_hidden(c, vide);
    if (vide) return;
    peindre_bandeau();
    peindre_tps();
    peindre_cartes();
}

}  // namespace

// ─── API (tab5_serveur_ia.h, tab5_internal.h) ──────────────────────────────────────────

void serveur_ia_recu(const std::string& payload) {
    if (payload_trop_long(kTag, payload.size(), kServeurIaMax)) return;
    // Tout champ est facultatif : un payload non vide est un serveur (ses valeurs
    // illisibles, inconnues) ; vide ou « ; » seuls : aucun capteur choisi.
    ServeurIaLu lu;
    s_present = serveur_ia_lire(Champ{payload.data(), payload.size()}, lu);
    if (s_present) {
        texte_ha_copier(s_nom, sizeof(s_nom), lu.nom.p, lu.nom.n);
        texte_ha_copier(s_modele, sizeof(s_modele), lu.modele.p, lu.modele.n);
        lu.nom = Champ{nullptr, 0};
        lu.modele = Champ{nullptr, 0};
        s_lu = lu;
        s_courbe.ajouter(lu.tps);
    } else {
        s_lu = ServeurIaLu{};
        s_nom[0] = '\0';
        s_modele[0] = '\0';
        s_courbe = ServeurIaCourbe{};
    }
    s_recu = true;
    if (popup_ouvert()) peindre_popup();
}

void serveur_ia_ouvrir() { peindre_popup(); }

void serveur_ia_rejouer_theme() {
    if (popup_ouvert()) peindre_popup();
}

bool serveur_ia_disponible() { return s_present; }
