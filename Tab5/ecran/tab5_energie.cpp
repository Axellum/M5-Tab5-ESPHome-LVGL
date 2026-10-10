/**
 * [AI-CONTEXT]
 * @file tab5_energie.cpp
 * @role Popup « Énergie » (ADR-0028, 04/10/2026, discussion #278) : l'instantané d'une
 *       installation solaire et l'historique de sa production.
 *         - Instantané : action tab5_maj_energie (tab5-api-logic.yaml), quatre cartes
 *           (solaire, maison, réseau, batterie). Une carte dont aucun capteur n'est choisi
 *           disparaît, les autres se partagent la largeur.
 *         - Historique : action tab5_maj_energie_historique, une série par vue (24 heures
 *           du jour, 30 derniers jours, 12 derniers mois), en barres construites ici.
 *           Sans capteur d'énergie choisi (jour vide), la carte du graphique disparaît.
 *       Home Assistant n'envoie rien tant que le popup est fermé : l'ouverture et chaque
 *       changement de vue émettent esphome.tab5_energie (vue), auquel le blueprint répond
 *       en lançant script.tab5_energie (packages/tab5_energie.yaml), qui pousse
 *       l'instantané tant que « Écran courant » vaut « Énergie ».
 * @architecture_constraint Aucune donnée gardée en NVS : tout repart de HA à l'ouverture.
 *       Écritures comparées d'abord (ui_text, ui_hidden, ui_x, ui_y : LVGL 9.5 invalide
 *       même à valeur égale). Les barres et les libellés de l'axe sont créés une fois, à
 *       la première ouverture, dans energie_zone (energie_popup.yaml).
 * @ai_instruction Le format des deux actions est un contrat avec packages/tab5_energie.yaml,
 *       la démo (tools/demo/) et le rendu (tools/rendu/) : tests/test_energie.py les lit
 *       tous. Un texte affiché passe par tr(). Une icône de plus = son glyphe dans
 *       mdi_font_45 (tab5-styles.yaml) ; glyphe_carte() est rattachée aux labels
 *       energie_icone_* par MDI_CODE_TARGETS (règle 9).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

EnergieUI g_energie_ui;

namespace {

enum Carte : int { SOLAIRE = 0, MAISON = 1, RESEAU = 2, BATTERIE = 3, NB_CARTES = 4 };
enum Vue : int { HEURES = 0, JOURS = 1, MOIS = 2, NB_VUES = 3 };
constexpr const char* kVues[NB_VUES] = {"heures", "jours", "mois"};
constexpr int kSlots[NB_VUES] = {24, 30, 12};
constexpr int kSlotsMax = 30;
constexpr int kAxeMax = 12;

// Géométrie (energie_popup.yaml) : corps du popup x 24..1226, y 72..670 (kCorpsX, kCorpsW,
// kCartesEcart : tab5_geometrie.h).
constexpr int32_t kCartesY = 72;            // avec le graphique
constexpr int32_t kCartesSeulesY = 266;     // sans : centrées dans le corps (598 − 210) / 2
constexpr int32_t kMargeTexte = 44;         // 22 px de chaque côté
// Zone des barres (energie_zone, kGraphiqueL × 286) : maximum en haut, repère, barres, axe ;
// libellés de l'axe de kAxeLibelleL px, texte centré (tab5_geometrie.h).
constexpr int32_t kBordX = 20;              // marge : le premier libellé de l'axe tient entier
constexpr int32_t kBarresHaut = 34;
constexpr int32_t kBarresBas = 252;
constexpr int32_t kAxeY = 258;
// Sous 10 W, le réseau et la batterie sont « au repos ».
constexpr float kRepos = 10.0f;

struct Mesure {
    bool choisi = false;
    float v = NAN;
};

struct Instant {
    bool recu = false;
    Mesure solaire, maison, reseau, batterie, batterie_puissance, batterie_temperature, jour;
    char unite_temperature[8] = {};
};

struct Serie {
    bool recue = false;
    int annee = 0, mois = 0, jour = 0;
    int n = 0;
    float v[kSlotsMax] = {};
};

Instant s_i;
Serie s_series[NB_VUES];
int s_vue = HEURES;
lv_obj_t* s_barres[kSlotsMax] = {};
lv_obj_t* s_axe[kAxeMax] = {};
lv_obj_t* s_repere = nullptr;
lv_obj_t* s_maximum = nullptr;
lv_obj_t* s_vide = nullptr;                 // « Aucun historique » au milieu de la zone

// --- Lecture des payloads ---------------------------------------------------------------

// Champs et nombres : champ_suivant() et champ_nombre() (tab5_champs.h ; NAN s'il ne se
// lit pas : « nan », « unknown », vide).
Mesure lire_mesure(const Champ& c) {
    Mesure m;
    m.choisi = c.n > 0;
    m.v = champ_nombre(c, NAN);
    return m;
}

int vue_de(const std::string& nom) {
    for (int v = 0; v < NB_VUES; v++)
        if (nom == kVues[v]) return v;
    return -1;
}

// Dates de l'axe des jours : jours_du_mois() (tab5_core.h).

// --- Dessin ------------------------------------------------------------------------------

bool visible() {
    const EnergieUI& u = g_energie_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

// Icône d'une carte (mdi_font_45). etat : réseau 1 achat, -1 vente, 0 rien ; batterie
// 1 charge, -1 décharge, 0 repos.
const char* glyphe_carte(int carte, int etat) {
    switch (carte) {
        case SOLAIRE: return "\U000F0A72";                                   // solar-power
        case MAISON: return "\U000F1903";                                    // home-lightning-bolt
        case RESEAU:
            if (etat > 0) return "\U000F192D";                               // transmission-tower-import
            if (etat < 0) return "\U000F192C";                               // transmission-tower-export
            return "\U000F0D3E";                                             // transmission-tower
        default:
            if (etat > 0) return "\U000F17E0";                               // battery-arrow-up
            if (etat < 0) return "\U000F17DE";                               // battery-arrow-down
            return "\U000F0079";                                             // battery
    }
}

void puissance(char* out, size_t n, float w) {
    if (std::isnan(w)) snprintf(out, n, "--");
    else energie_formater(out, n, w, "W");
}

void peindre_carte(int c, const char* valeur, uint32_t couleur_valeur, int etat, uint32_t couleur_icone,
                   const char* l1, const char* l2, int32_t largeur) {
    const EnergieUI& u = g_energie_ui;
    ui_text(u.valeur[c], valeur);
    ui_text_color(u.valeur[c], couleur_valeur);
    ui_text(u.icone[c], glyphe_carte(c, etat));
    ui_text_color(u.icone[c], couleur_icone);
    texte_ha_coupe(u.ligne1[c], l1, largeur - kMargeTexte);
    texte_ha_coupe(u.ligne2[c], l2, largeur - kMargeTexte);
}

void peindre_instant() {
    EnergieUI& u = g_energie_ui;
    const Instant& i = s_i;
    bool montre[NB_CARTES];
    montre[SOLAIRE] = i.solaire.choisi || i.jour.choisi;
    montre[MAISON] = i.maison.choisi;
    montre[RESEAU] = i.reseau.choisi;
    montre[BATTERIE] = i.batterie.choisi || i.batterie_puissance.choisi;
    int n = 0;
    for (bool m : montre) n += m ? 1 : 0;
    const bool graphique = i.jour.choisi;
    ui_hidden(u.graphique, !graphique);
    // Message central : avant le premier instantané, ou HA a répondu sans aucun capteur
    // (section « Énergie » du blueprint vide).
    ui_hidden(u.attente, i.recu && n > 0);
    ui_text(u.attente, i.recu ? tr("Aucun capteur d'énergie choisi") : tr("En attente de Home Assistant"));
    if (n == 0) {
        for (int c = 0; c < NB_CARTES; c++) ui_hidden(u.carte[c], true);
        return;
    }
    // Cartes visibles réparties sur toute la largeur du corps, dans l'ordre.
    const int32_t largeur = (kCorpsW - (n - 1) * kCartesEcart) / n;
    const int32_t y = graphique ? kCartesY : kCartesSeulesY;
    int k = 0;
    for (int c = 0; c < NB_CARTES; c++) {
        ui_hidden(u.carte[c], !montre[c]);
        if (!montre[c] || u.carte[c] == nullptr) continue;
        if (lv_obj_get_style_width(u.carte[c], LV_PART_MAIN) != largeur) lv_obj_set_width(u.carte[c], largeur);
        ui_x(u.carte[c], kCorpsX + k * (largeur + kCartesEcart));
        ui_y(u.carte[c], y);
        k++;
    }

    char v[24], l1[48], l2[48], x[24];
    // Solaire : sa puissance (sinon la production du jour), et la production du jour.
    if (montre[SOLAIRE]) {
        if (i.solaire.choisi) puissance(v, sizeof(v), i.solaire.v);
        else if (!std::isnan(i.jour.v)) energie_formater(v, sizeof(v), i.jour.v, "kWh");
        else snprintf(v, sizeof(v), "--");
        l1[0] = '\0';
        if (i.jour.choisi && i.solaire.choisi) {
            if (std::isnan(i.jour.v)) snprintf(x, sizeof(x), "--");
            else energie_formater(x, sizeof(x), i.jour.v, "kWh");
            snprintf(l1, sizeof(l1), "%s %s", tr("Aujourd'hui"), x);
        } else if (i.jour.choisi) {
            snprintf(l1, sizeof(l1), "%s", tr("Produit aujourd'hui"));
        }
        const bool produit = !std::isnan(i.solaire.v) && i.solaire.v >= kRepos;
        peindre_carte(SOLAIRE, v, UIColor.TEXT_PRIMARY, 0, produit ? UIColor.GOLD : UIColor.TEXT_DIM, l1, "",
                      largeur);
    }
    if (montre[MAISON]) {
        puissance(v, sizeof(v), i.maison.v);
        peindre_carte(MAISON, v, UIColor.TEXT_PRIMARY, 0, UIColor.INFO, tr("Consommation"), "", largeur);
    }
    // Réseau : + achat (import), − vente (export) ; la valeur sans son signe.
    if (montre[RESEAU]) {
        const float r = i.reseau.v;
        const int etat = std::isnan(r) || std::fabs(r) < kRepos ? 0 : (r > 0 ? 1 : -1);
        puissance(v, sizeof(v), std::isnan(r) ? NAN : std::fabs(r));
        const char* sens = etat > 0 ? tr("Depuis le réseau") : etat < 0 ? tr("Vers le réseau") : tr("Aucun échange");
        const uint32_t c = etat > 0 ? UIColor.WARNING : etat < 0 ? UIColor.SUCCESS : UIColor.TEXT_DIM;
        peindre_carte(RESEAU, v, UIColor.TEXT_PRIMARY, etat, c, std::isnan(r) ? "" : sens, "", largeur);
    }
    // Batterie : son niveau (sinon sa puissance), charge / décharge, température.
    if (montre[BATTERIE]) {
        const float p = i.batterie_puissance.v;
        const int etat = std::isnan(p) || std::fabs(p) < kRepos ? 0 : (p > 0 ? 1 : -1);
        uint32_t couleur = UIColor.TEXT_PRIMARY;
        if (i.batterie.choisi) {
            if (std::isnan(i.batterie.v)) snprintf(v, sizeof(v), "--");
            else snprintf(v, sizeof(v), "%.0f %%", i.batterie.v);
            couleur = get_battery_color(i.batterie.v);
        } else {
            puissance(v, sizeof(v), std::isnan(p) ? NAN : std::fabs(p));
        }
        l1[0] = '\0';
        if (i.batterie_puissance.choisi && !std::isnan(p)) {
            if (etat == 0) {
                snprintf(l1, sizeof(l1), "%s", tr("Au repos"));
            } else {
                puissance(x, sizeof(x), std::fabs(p));
                snprintf(l1, sizeof(l1), "%s %s", etat > 0 ? tr("Charge") : tr("Décharge"), x);
            }
        }
        l2[0] = '\0';
        if (i.batterie_temperature.choisi) {
            if (std::isnan(i.batterie_temperature.v)) snprintf(x, sizeof(x), "--");
            else snprintf(x, sizeof(x), "%.1f %s", i.batterie_temperature.v, i.unite_temperature);
            snprintf(l2, sizeof(l2), "%s %s", tr("Température"), x);
        }
        const uint32_t icone = etat > 0 ? UIColor.SUCCESS : etat < 0 ? UIColor.WARNING : get_battery_color(i.batterie.v);
        peindre_carte(BATTERIE, v, couleur, etat, icone, l1, l2, largeur);
    }
}

// Titre du graphique : la période et son total.
void peindre_titre(const Serie& s) {
    char total[24], buf[64];
    float somme = 0.0f;
    bool une = false;
    for (int k = 0; k < s.n; k++)
        if (!std::isnan(s.v[k])) {
            somme += s.v[k];
            une = true;
        }
    if (s_vue == HEURES && !std::isnan(s_i.jour.v)) {
        somme = s_i.jour.v;   // le total du jour, au plus près (le dernier partiel compris)
        une = true;
    }
    if (une) energie_formater(total, sizeof(total), somme, "kWh");
    else snprintf(total, sizeof(total), "--");
    static const char* const kPeriodes[NB_VUES] = {tr_noop("Aujourd'hui"), tr_noop("30 derniers jours"),
                                                   tr_noop("12 derniers mois")};
    snprintf(buf, sizeof(buf), "%s \xC2\xB7 %s", tr(kPeriodes[s_vue]), total);
    ui_text(g_energie_ui.titre, buf);
}

// Libellé de l'axe sous la barre k : heures 00:00, 03:00… ; jours : le quantième tous les
// cinq jours en finissant par aujourd'hui ; mois : leur nom court.
bool libelle_axe(const Serie& s, int k, char* out, size_t n) {
    switch (s_vue) {
        case HEURES:
            if (k % 3 != 0) return false;
            snprintf(out, n, "%02d:00", k);
            return true;
        case JOURS: {
            if ((s.n - 1 - k) % 5 != 0) return false;
            int a = s.annee, m = s.mois, j = s.jour + k;
            while (m >= 1 && m <= 12 && j > jours_du_mois(a, m)) {
                j -= jours_du_mois(a, m);
                if (++m > 12) {
                    m = 1;
                    a++;
                }
            }
            snprintf(out, n, "%d", j);
            return true;
        }
        default: {
            const int m = ((s.mois - 1 + k) % 12 + 12) % 12 + 1;
            snprintf(out, n, "%s", month_short_utf8(m));
            return true;
        }
    }
}

void peindre_graphique() {
    EnergieUI& u = g_energie_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_barres[0] == nullptr) return;
    const Serie& s = s_series[s_vue];
    peindre_titre(s);
    // La dernière barre qui a une valeur : l'heure, le jour ou le mois en cours.
    int courante = -1;
    float maximum = 0.0f;
    for (int k = 0; k < s.n; k++)
        if (!std::isnan(s.v[k])) {
            courante = k;
            if (s.v[k] > maximum) maximum = s.v[k];
        }
    const bool vide = !s.recue || courante < 0;
    ui_hidden(s_repere, vide);
    ui_hidden(s_maximum, vide);
    // Pas de barres : le message dit pourquoi.
    ui_hidden(s_vide, !vide);
    if (vide) ui_text(s_vide, s.recue ? tr("Aucun historique") : tr("En attente de Home Assistant"));
    char buf[24];
    if (!vide) {
        energie_formater(buf, sizeof(buf), maximum, "kWh");
        ui_text(s_maximum, buf);
    }
    const int nb = s.n > 0 ? s.n : kSlots[s_vue];
    const int32_t pas = (kGraphiqueL - 2 * kBordX) / nb;
    const int32_t largeur = pas * 7 / 10;
    const int32_t hauteur_max = kBarresBas - kBarresHaut;
    int axe = 0;
    for (int k = 0; k < kSlotsMax; k++) {
        lv_obj_t* b = s_barres[k];
        const bool montre = !vide && k < s.n && !std::isnan(s.v[k]);
        ui_hidden(b, !montre);
        if (montre) {
            int32_t h = maximum > 0.0f ? static_cast<int32_t>(std::lround(s.v[k] / maximum * hauteur_max)) : 0;
            if (h < 2) h = 2;   // une barre nulle reste visible : la donnée existe
            if (lv_obj_get_style_width(b, LV_PART_MAIN) != largeur) lv_obj_set_width(b, largeur);
            if (lv_obj_get_style_height(b, LV_PART_MAIN) != h) lv_obj_set_height(b, h);
            ui_x(b, kBordX + k * pas + (pas - largeur) / 2);
            ui_y(b, kBarresBas - h);
            const uint32_t c = k == courante ? UIColor.ACCENT : UIColor.GOLD;
            lv_style_value_t cur;
            if (lv_obj_get_local_style_prop(b, LV_STYLE_BG_COLOR, &cur, LV_PART_MAIN) != LV_STYLE_RES_FOUND ||
                !lv_color_eq(cur.color, lv_color_hex(c)))
                lv_obj_set_style_bg_color(b, lv_color_hex(c), LV_PART_MAIN);
        }
        if (k < nb && axe < kAxeMax && s.recue && libelle_axe(s, k, buf, sizeof(buf))) {
            lv_obj_t* l = s_axe[axe++];
            ui_text(l, buf);
            ui_hidden(l, false);
            // Centré sous sa barre (largeur fixe kAxeLibelleL, texte centré dedans).
            ui_x(l, kBordX + k * pas + pas / 2 - kAxeLibelleL / 2);
        }
    }
    for (; axe < kAxeMax; axe++) ui_hidden(s_axe[axe], true);
}

void peindre() {
    if (!visible()) return;
    peindre_instant();
    peindre_graphique();
}

// Noms des cartes, barres, repère et libellés de l'axe : une fois, à la première ouverture.
void construire() {
    EnergieUI& u = g_energie_ui;
    if (s_barres[0] != nullptr || u.zone == nullptr) return;
    // « RÉSEAU » est aussi le réseau Wi-Fi de la console système (« NETWORK ») : contexte.
    ui_text(u.nom[SOLAIRE], tr("SOLAIRE"));
    ui_text(u.nom[MAISON], tr("MAISON"));
    ui_text(u.nom[RESEAU], tr_ctx("energie", "RÉSEAU"));
    ui_text(u.nom[BATTERIE], tr("BATTERIE"));
    s_repere = lv_obj_create(u.zone);
    lv_obj_remove_style_all(s_repere);
    lv_obj_set_pos(s_repere, kBordX, kBarresHaut);
    lv_obj_set_size(s_repere, kGraphiqueL - 2 * kBordX, 1);
    lv_obj_set_style_bg_color(s_repere, lv_color_hex(UIColor.GLASS_RIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_repere, LV_OPA_40, LV_PART_MAIN);
    lv_obj_remove_flag(s_repere, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_repere, LV_OBJ_FLAG_HIDDEN);
    auto libelle = [&u](lv_obj_t* parent) {
        lv_obj_t* l = lv_label_create(parent);
        if (u.police != nullptr) esphome::lvgl::lv_obj_set_style_text_font(l, u.police, LV_PART_MAIN);
        lv_obj_set_style_text_color(l, lv_color_hex(UIColor.TEXT_DIM), LV_PART_MAIN);
        lv_label_set_text(l, "");
        lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
        return l;
    };
    s_maximum = libelle(u.zone);
    lv_obj_set_pos(s_maximum, kBordX, 0);
    s_vide = libelle(u.zone);
    lv_obj_align(s_vide, LV_ALIGN_CENTER, 0, 0);
    for (int k = 0; k < kSlotsMax; k++) {
        lv_obj_t* b = lv_obj_create(u.zone);
        lv_obj_remove_style_all(b);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(b, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
        lv_obj_set_style_radius(b, 4, LV_PART_MAIN);
        lv_obj_set_size(b, 10, 2);
        lv_obj_remove_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);
        s_barres[k] = b;
    }
    // Libellés de l'axe, texte centré dans kAxeLibelleL : un tous les trois pas en heures (138 px),
    // cinq en jours (187 px) ; en mois un pas fait 93 px et le nom court (« Janv ») tient.
    for (int k = 0; k < kAxeMax; k++) {
        lv_obj_t* l = libelle(u.zone);
        lv_obj_set_y(l, kAxeY);
        lv_obj_set_width(l, kAxeLibelleL);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        s_axe[k] = l;
    }
}

void demander() {
    if (g_energie_ui.demander != nullptr) g_energie_ui.demander(kVues[s_vue]);
}

}  // namespace

bool energie_formater(char* out, size_t n, float v, const char* unite) {
    if (out == nullptr || n == 0 || unite == nullptr) return false;
    float k = 0.0f;
    bool energie = false;
    if (std::strcmp(unite, "W") == 0) k = 1.0f;
    else if (std::strcmp(unite, "kW") == 0) k = 1000.0f;
    else if (std::strcmp(unite, "MW") == 0) k = 1e6f;
    else if (std::strcmp(unite, "Wh") == 0) { k = 0.001f; energie = true; }
    else if (std::strcmp(unite, "kWh") == 0) { k = 1.0f; energie = true; }
    else if (std::strcmp(unite, "MWh") == 0) { k = 1000.0f; energie = true; }
    else return false;
    const float x = v * k;   // W, ou kWh
    const float a = std::fabs(x);
    if (energie) {
        if (a < 10.0f) snprintf(out, n, "%.2f kWh", x);
        else if (a < 100.0f) snprintf(out, n, "%.1f kWh", x);
        else if (a < 1000.0f) snprintf(out, n, "%.0f kWh", x);
        else snprintf(out, n, "%.2f MWh", x / 1000.0f);
    } else {
        if (a < 1000.0f) snprintf(out, n, "%.0f W", x);
        else if (a < 10000.0f) snprintf(out, n, "%.2f kW", x / 1000.0f);
        else if (a < 1e6f) snprintf(out, n, "%.1f kW", x / 1000.0f);
        else snprintf(out, n, "%.2f MW", x / 1e6f);
    }
    return true;
}

void energie_instantane(const std::string& payload) {
    if (payload_trop_long("tab5.energie", payload.size())) return;
    const char* p = payload.data();
    const char* fin = p + payload.size();
    Instant i;
    i.recu = true;
    Mesure* champs[] = {&i.solaire, &i.maison, &i.reseau, &i.batterie, &i.batterie_puissance,
                        &i.batterie_temperature};
    for (Mesure* m : champs) *m = lire_mesure(champ_suivant(p, fin, '|'));
    const Champ unite = champ_suivant(p, fin, '|');
    texte_ha_copier(i.unite_temperature, sizeof(i.unite_temperature), unite.p, unite.n);
    i.jour = lire_mesure(champ_suivant(p, fin, '|'));
    s_i = i;
    peindre();
}

void energie_historique(const std::string& vue, const std::string& debut, const std::string& valeurs) {
    const int v = vue_de(vue);
    if (v < 0) {
        payload_refuse("tab5.energie", "historique : vue inconnue", vue.size());
        return;
    }
    if (payload_trop_long("tab5.energie", valeurs.size())) return;
    Serie s;
    s.recue = true;
    // Date illisible : 1er janvier (seuls les libellés de l'axe s'en servent). Année bornée
    // comme dans l'historique (1970..2200, DO-2, audit du 07/10/2026) : libelle_axe fait
    // `a++`, qui déborderait sur une année proche de INT_MAX.
    if (std::sscanf(debut.c_str(), "%d-%d-%d", &s.annee, &s.mois, &s.jour) != 3 || s.annee < 1970 ||
        s.annee > 2200 || s.mois < 1 || s.mois > 12 || s.jour < 1 || s.jour > 31) {
        s.annee = 2000;
        s.mois = 1;
        s.jour = 1;
    }
    const char* p = valeurs.data();
    const char* fin = p + valeurs.size();
    // Un champ vide compte (pas de donnée : heure à venir…), le dernier aussi : « 3.1;; »
    // donne trois valeurs. Avant (DO-11, audit du 07/10/2026), le champ vide après le
    // dernier « ; » était perdu : 23 barres au lieu de 24 l'après-midi, espacement changé.
    bool apres_sep = false;
    while ((p < fin || apres_sep) && s.n < kSlots[v]) {
        const Champ c = champ_suivant(p, fin, ';');
        apres_sep = p > c.p + c.n;   // le champ s'est terminé sur un « ; »
        s.v[s.n++] = champ_nombre(c, NAN);
    }
    // Une valeur négative (compteur remis à zéro mal compté) n'a pas de barre.
    for (int k = 0; k < s.n; k++)
        if (s.v[k] < 0.0f) s.v[k] = NAN;
    s_series[v] = s;
    if (v == s_vue) peindre();
}

// ADR-0058 : à écrire (lot firmware).
void energie_soleil(const std::string& payload) { (void) payload; }
void energie_bilan(const std::string& vue, const std::string& debut, const std::string& payload) {
    (void) vue;
    (void) debut;
    (void) payload;
}

void energie_ouvrir() {
    EnergieUI& u = g_energie_ui;
    if (u.popup == nullptr) return;
    construire();
    s_vue = HEURES;
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre();
    demander();
}

void energie_choisir_vue(int vue) {
    if (vue < 0 || vue >= NB_VUES) return;
    if (vue != s_vue) {
        s_vue = vue;
        peindre();
    }
    demander();
}

// Thèmes (ADR-0029) : ce que construire() a peint une fois (repère, libellés), puis le
// popup s'il est ouvert ; fermé, sa prochaine ouverture repeint cartes et barres.
void energie_rejouer_theme() {
    if (s_barres[0] == nullptr) return;
    lv_obj_set_style_bg_color(s_repere, lv_color_hex(UIColor.GLASS_RIM), LV_PART_MAIN);
    ui_text_color(s_maximum, UIColor.TEXT_DIM);
    ui_text_color(s_vide, UIColor.TEXT_DIM);
    for (lv_obj_t* l : s_axe) ui_text_color(l, UIColor.TEXT_DIM);
    peindre();
}
