/**
 * [AI-CONTEXT]
 * @file tab5_piece_climat.cpp
 * @role Climat de la pièce en mode HA (ADR-0040, 09/10/2026, demande d'Axel) : la zone des
 *       températures de la carte clim de l'accueil (climate_card.yaml : icon_salon /
 *       val_temp_salon à gauche, icon_serre / current_serre à droite) montre, en mode HA,
 *       la température et l'humidité de la pièce affichée quand le blueprint en a déclaré
 *       une ; le salon et la serre sinon. Aussi la clé « pR » de tab5_maj_emplacements (lue
 *       par piece_climat_lire, Tab5/socle/tab5_parse.h), la clé du popup Température d'un appui long, et la pièce dont la tuile − / + règle la
 *       clim (piece_climat_clim, lue par tab5_reglables.cpp).
 * @architecture_constraint Seul écrivain de ces quatre widgets depuis l'ADR-0040 : les
 *       capteurs du salon et de la serre (tab5-sensors-domotique.yaml) passent par
 *       accueil_salon_* / accueil_serre_*, qui gardent la valeur et ne peignent que quand la
 *       zone montre le salon et la serre ; le masquage des zones SALON / SERRE (ancien bloc
 *       de zones_apply_ui) est ici aussi. Revenir au salon remet exactement l'écran d'avant :
 *       glyphes du canapé et de la serre (manette sans serre), dernières valeurs reçues, ou
 *       « -- ° » et la couleur des styles si rien n'est encore arrivé.
 * @ai_instruction Couleurs : UIColor et les dégradés de tab5_forecast (règle 1) ; une
 *       couleur locale ôtée rend la main aux styles du label (style_text_dim,
 *       style_text_temp_max). Glyphes de icon_salon et icon_serre : dans mdi_font_45 et
 *       posés seulement par accueil_temperatures_ui (MDI_CODE_TARGETS de
 *       tools/check_tab5_code_rules.py). Rien de traduit ici : « % » et « ° » seulement.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

// Ce que le blueprint a déclaré pour la pièce R (clé « pR », ADR-0040). Une température
// déclarée suffit pour que la zone montre la pièce ; l'humidité et la clim s'y ajoutent.
struct PieceClimat {
    bool temperature = false;
    bool humidite = false;
    bool clim = false;
    float t = NAN;
    float h = NAN;
};
PieceClimat s_pieces[kPieces];

// Clés du popup Température pour la pièce R (kCles de tab5_historique.cpp, et ce que le
// blueprint accepte ; tests/test_piece_climat.py compare).
constexpr const char* kClesHistorique[kPieces] = {"p0", "p1", "p2", "p3", "p4"};

// Dernières valeurs du salon et de la serre (capteurs temp_salon, hum_salon, temp_serre).
// `*_recu` : une valeur est arrivée (même NaN) ; sinon le label garde « -- ° » et la
// couleur de ses styles, comme au démarrage. salon_hum_couleur : la dernière humidité
// finie (le capteur ne recolorait pas l'icône sur NaN).
struct Accueil {
    float salon = NAN;
    float serre = NAN;
    float salon_hum_couleur = NAN;
    bool salon_recu = false;
    bool serre_recu = false;
};
Accueil s_accueil;

// Pièce que la zone montre : celle du mode HA, si elle a une température déclarée.
int piece_montree() {
    const int r = tuiles_piece_mode_ha();
    return (r >= 0 && r < kPieces && s_pieces[r].temperature) ? r : -1;
}

bool zone_prete() {
    const ZonesUI& u = g_zones_ui;
    return u.icon_salon != nullptr && u.val_salon != nullptr && u.icon_serre != nullptr && u.val_serre != nullptr;
}

// Couleur locale du texte ôtée : le label reprend celle de ses styles (et du thème).
void couleur_des_styles(lv_obj_t* obj) {
    if (obj != nullptr) lv_obj_remove_local_style_prop(obj, LV_STYLE_TEXT_COLOR, LV_PART_MAIN);
}

// Icône teintée par une humidité : la couleur de son dégradé si elle est connue, celle des
// styles sinon.
void icone_humidite(lv_obj_t* icone, float h) {
    if (std::isfinite(h)) ui_text_color(icone, get_humidity_color(h));
    else couleur_des_styles(icone);
}

// Température comme update_temp_ui() (« 21.4 ° » en dégradé, « -- ° » grisé), ou, jamais
// reçue, le texte du YAML et la couleur des styles.
void temperature(lv_obj_t* label, bool recue, float x) {
    if (recue) {
        update_temp_ui(label, x);
        return;
    }
    ui_text(label, "-- \xC2\xB0");
    couleur_des_styles(label);
}

// Humidité de la pièce : « 48 % » dans la couleur de son dégradé, « -- % » grisé inconnue.
void humidite(lv_obj_t* label, float h) {
    if (!std::isfinite(h)) {
        ui_text(label, "-- %");
        ui_text_color(label, UIColor.TEXT_DIM);
        return;
    }
    char buf[12];
    snprintf(buf, sizeof(buf), "%.0f %%", h);
    ui_text(label, buf);
    ui_text_color(label, get_humidity_color(h));
}

}  // namespace

bool piece_climat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    // Lecture : piece_climat_lire (Tab5/socle/tab5_parse.h, testée sur PC et fuzzée).
    PieceClimatLu lu;
    if (!piece_climat_lire(Champ{cle, n_cle}, Champ{reste, n_reste}, lu)) return false;
    const int r = lu.piece;
    PieceClimat& p = s_pieces[r];
    const bool avait_clim = p.clim;
    p.temperature = lu.temperature;
    p.t = lu.t;
    p.humidite = lu.humidite;
    p.h = lu.h;
    p.clim = lu.clim;
    // Clim retirée de la pièce : ses réglages et son état sont oubliés (le popup se ferme
    // s'il la montrait, une consigne en attente ne part pas).
    if (avait_clim && !p.clim) clim_piece_oublier(r);
    if (r == tuiles_piece_mode_ha()) {
        accueil_temperatures_ui();
        reglables_clim_changee();  // la tuile − / + suit la clim de la pièce
    }
    return true;
}

void accueil_salon_temperature(float x) {
    s_accueil.salon = x;
    s_accueil.salon_recu = true;
    if (piece_montree() < 0) update_temp_ui(g_zones_ui.val_salon, x);
}

void accueil_salon_humidite(float x) {
    if (!std::isfinite(x)) return;  // comme avant : NaN ne recolore pas l'icône
    s_accueil.salon_hum_couleur = x;
    if (piece_montree() < 0) set_icon_color_ui(g_zones_ui.icon_salon, get_humidity_color(x));
}

void accueil_serre_temperature(float x) {
    s_accueil.serre = x;
    s_accueil.serre_recu = true;
    if (piece_montree() < 0) update_temp_ui(g_zones_ui.val_serre, x);
}

void accueil_temperatures_ui() {
    if (!zone_prete()) return;  // avant tab5_zones_apply (setup) : il rappellera
    const ZonesUI& u = g_zones_ui;
    const int r = piece_montree();
    if (r >= 0) {
        // La pièce : thermomètre teinté par son humidité, sa température ; à droite, une
        // goutte et son humidité, ou rien.
        const PieceClimat& p = s_pieces[r];
        ui_hidden(u.icon_salon, false);
        ui_hidden(u.val_salon, false);
        ui_text(u.icon_salon, "\U000F050F");
        icone_humidite(u.icon_salon, p.humidite ? p.h : NAN);
        update_temp_ui(u.val_salon, p.t);
        ui_hidden(u.icon_serre, !p.humidite);
        ui_hidden(u.val_serre, !p.humidite);
        if (!p.humidite) return;
        ui_text(u.icon_serre, "\U000F058E");
        icone_humidite(u.icon_serre, p.h);
        humidite(u.val_serre, p.h);
        return;
    }
    // Le salon et la serre, comme avant l'ADR-0040. Salon masqué avec sa zone ; sans
    // serre, l'icône dit ce que fait le tap de sa zone tactile (btn_serre_games, restée à la
    // même place) : le contenu suivant de la zone à gauche de l'horloge (view-carousel,
    // l'icône du geste « zone_gauche_suivante », ADR-0051 ; une manette avant, pour l'Arcade).
    const bool sans_salon = zone_absente(Zone::SALON);
    ui_hidden(u.icon_salon, sans_salon);
    ui_hidden(u.val_salon, sans_salon);
    ui_text(u.icon_salon, "\U000F04B9");
    icone_humidite(u.icon_salon, s_accueil.salon_hum_couleur);
    temperature(u.val_salon, s_accueil.salon_recu, s_accueil.salon);
    const bool sans_serre = zone_absente(Zone::SERRE);
    ui_hidden(u.icon_serre, false);
    ui_hidden(u.val_serre, sans_serre);
    ui_text(u.icon_serre, sans_serre ? "\U000F056C" : "\U000F002D");
    couleur_des_styles(u.icon_serre);
    temperature(u.val_serre, s_accueil.serre_recu, s_accueil.serre);
}

const char* accueil_historique_cle(bool droite) {
    const int r = piece_montree();
    if (r >= 0) {
        // À droite, l'humidité de la pièce : le même popup, qui la trace avec la
        // température (ADR-0047) ; rien sans humidité déclarée.
        return droite && !s_pieces[r].humidite ? nullptr : kClesHistorique[r];
    }
    if (droite) return zone_absente(Zone::SERRE) ? nullptr : "serre";
    return zone_absente(Zone::SALON) ? nullptr : "salon";
}

bool piece_climat_a_temperature(int r) { return r >= 0 && r < kPieces && s_pieces[r].temperature; }

int piece_climat_clim() {
    const int r = piece_montree();
    return (r >= 0 && s_pieces[r].clim && clim_piece_connue(r)) ? r : -1;
}
