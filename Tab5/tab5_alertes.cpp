/**
 * [AI-CONTEXT]
 * @file tab5_alertes.cpp
 * @role Popup « Alertes » (lot 4 du plan des alertes de la carte centrale, 06/10/2026) :
 *       l'historique des 20 dernières alertes, une ligne chacune (pastille de la gravité,
 *       libellé, puis « apparue 14 h 02 · lue 14 h 10 · terminée 15 h 30 »), et le bouton
 *       « Tout marquer comme lu ». Ouvert par un appui long sur la carte centrale de
 *       l'accueil (tab5-lvgl.yaml) ou par « Aller à l'écran → Alertes ».
 *       À l'ouverture, la tablette demande l'historique à Home Assistant (événement
 *       esphome.tab5_alertes_historique), qui répond par l'action
 *       tab5_maj_alertes_historique, puis la repousse tant que le popup est ouvert et que
 *       l'historique change (packages/tab5_push.yaml).
 * @architecture_constraint Push-only, events-only (ADR-0001, ADR-0025) : la tablette ne
 *       nomme aucune entité. Elle garde la dernière liste reçue, rien d'autre : ce qui est
 *       lu, terminé ou abonné est décidé par HA (custom_templates/tab5_alertes.jinja).
 *       Lignes créées une fois, à la première ouverture, dans alertes_liste (colonne
 *       défilante, alertes_popup.yaml), masquées au-delà de la liste reçue. Couleurs de la
 *       palette, repeintes par alertes_rejouer_theme() (thèmes, ADR-0029).
 * @ai_instruction Le format de l'action est un contrat avec
 *       custom_templates/tab5_alertes.jinja (tab5_alertes_historique_payload), le rendu
 *       hors tablette (tools/rendu/ecrans.py) et la graine du fuzz
 *       (tools/sanitizers/fuzz_services.py) ; tests/test_alertes_ha.py les lit.
 *       Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

AlertesUI g_alertes_ui;

namespace {

constexpr int kLignesMax = 20;
constexpr int32_t kLigneH = 56;
// Libellé venu de HA : coupé à 96 octets (UTF-8 valide), « … » fait le reste à l'écran.
constexpr size_t kTexteMax = 96;
// Au-delà de 2100, une heure lue est fausse (« -1 » donne 4294967295) : comme absente.
constexpr uint32_t kEpochMax = 4102444800u;
// Avant 2020, l'heure de la tablette n'est pas encore réglée : pas de « aujourd'hui ».
constexpr time_t kHeureValide = 1577836800;

struct Ligne {
    uint32_t apparue = 0;
    uint32_t lue = 0;        // 0 : pas encore lue
    uint32_t terminee = 0;   // 0 : en cours
    char gravite = 'O';      // R, O ou J (Rouge, Orange, Jaune)
    std::string texte;       // libellé déjà composé dans la langue de l'écran
};

Ligne s_lignes[kLignesMax];
int s_nb = 0;
bool s_recu = false;

struct LigneUI {
    lv_obj_t* ligne = nullptr;
    lv_obj_t* pastille = nullptr;
    lv_obj_t* libelle = nullptr;
    lv_obj_t* heures = nullptr;
};
LigneUI s_ui[kLignesMax];

uint32_t lire_epoch(const char* d, size_t n) {
    char tmp[16];
    if (n == 0 || n >= sizeof(tmp)) return 0;
    memcpy(tmp, d, n);
    tmp[n] = '\0';
    const unsigned long v = strtoul(tmp, nullptr, 10);
    return v > kEpochMax ? 0 : static_cast<uint32_t>(v);
}

// « 14 h 02 » aujourd'hui, « Lun 5 14 h 02 » un autre jour (heure locale de la tablette).
void heure_txt(char* out, size_t n, uint32_t epoch) {
    const time_t t = static_cast<time_t>(epoch);
    struct tm lt = {};
    localtime_r(&t, &lt);
    char hm[24];
    snprintf(hm, sizeof(hm), tr("%d h %02d"), lt.tm_hour, lt.tm_min);
    const time_t maintenant = tab5_time_source(nullptr);
    struct tm auj = {};
    localtime_r(&maintenant, &auj);
    if (maintenant < kHeureValide || (lt.tm_year == auj.tm_year && lt.tm_yday == auj.tm_yday)) {
        snprintf(out, n, "%s", hm);
        return;
    }
    snprintf(out, n, "%s %d %s", day_short_utf8(lt.tm_wday), lt.tm_mday, hm);
}

uint32_t couleur_gravite(char g) {
    switch (g) {
        case 'R': return UIColor.ALERT_RED;
        case 'J': return UIColor.ALERT_YELLOW;
        default: return UIColor.ALERT_ORANGE;
    }
}

lv_obj_t* libelle(lv_obj_t* parent, const esphome::font::Font* police) {
    lv_obj_t* l = lv_label_create(parent);
    if (police != nullptr) esphome::lvgl::lv_obj_set_style_text_font(l, police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    return l;
}

// Les 20 lignes : une fois, à la première ouverture. Rangée flex : pastille, libellé qui
// prend la place restante (coupé par « … »), heures à droite.
void construire() {
    AlertesUI& u = g_alertes_ui;
    if (s_ui[0].ligne != nullptr || u.liste == nullptr) return;
    // Colonne posée ici : ESPHome refuse un `layout:` sur un obj déclaré sans `widgets:`.
    lv_obj_set_flex_flow(u.liste, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(u.liste, 0, LV_PART_MAIN);
    for (int k = 0; k < kLignesMax; k++) {
        LigneUI& r = s_ui[k];
        r.ligne = lv_obj_create(u.liste);
        lv_obj_remove_style_all(r.ligne);
        lv_obj_set_size(r.ligne, lv_pct(100), kLigneH);
        lv_obj_set_flex_flow(r.ligne, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r.ligne, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_hor(r.ligne, 14, LV_PART_MAIN);
        lv_obj_set_style_pad_column(r.ligne, 18, LV_PART_MAIN);
        lv_obj_set_style_border_side(r.ligne, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
        lv_obj_set_style_border_width(r.ligne, 1, LV_PART_MAIN);
        lv_obj_set_style_border_opa(r.ligne, LV_OPA_30, LV_PART_MAIN);
        lv_obj_remove_flag(r.ligne, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(r.ligne, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(r.ligne, LV_OBJ_FLAG_HIDDEN);

        r.pastille = lv_obj_create(r.ligne);
        lv_obj_remove_style_all(r.pastille);
        lv_obj_set_size(r.pastille, 16, 16);
        lv_obj_set_style_radius(r.pastille, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(r.pastille, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_remove_flag(r.pastille, LV_OBJ_FLAG_CLICKABLE);

        r.libelle = libelle(r.ligne, u.police);
        lv_obj_set_flex_grow(r.libelle, 1);
        // « … » ne se pose qu'à hauteur fixe : sans elle, le libellé passe à la ligne.
        lv_obj_set_height(r.libelle, lv_font_get_line_height(lv_obj_get_style_text_font(r.libelle, LV_PART_MAIN)));
        lv_label_set_long_mode(r.libelle, LV_LABEL_LONG_MODE_DOTS);

        r.heures = libelle(r.ligne, u.police_heures);
    }
}

// Lignes, bouton « Tout marquer comme lu » et texte d'attente, d'après la dernière liste.
void peindre() {
    AlertesUI& u = g_alertes_ui;
    if (s_ui[0].ligne == nullptr) return;
    bool a_lire = false;
    for (int k = 0; k < kLignesMax; k++) {
        LigneUI& r = s_ui[k];
        if (k >= s_nb) {
            ui_hidden(r.ligne, true);
            continue;
        }
        const Ligne& e = s_lignes[k];
        const bool en_cours = e.terminee == 0;
        const bool non_lue = en_cours && e.lue == 0;
        a_lire = a_lire || non_lue;
        lv_obj_set_style_border_color(r.ligne, lv_color_hex(UIColor.GLASS_RIM), LV_PART_MAIN);
        lv_obj_set_style_bg_color(r.pastille, lv_color_hex(couleur_gravite(e.gravite)), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(r.pastille, non_lue ? LV_OPA_COVER : (en_cours ? LV_OPA_60 : LV_OPA_30),
                                LV_PART_MAIN);
        ui_text(r.libelle, e.texte.c_str());
        ui_text_color(r.libelle, en_cours ? UIColor.TEXT_SOFT : UIColor.TEXT_DIM);

        char h[40];
        char morceau[64];
        char tout[200];
        heure_txt(h, sizeof(h), e.apparue);
        snprintf(tout, sizeof(tout), tr("apparue %s"), h);
        if (e.lue != 0) {
            heure_txt(h, sizeof(h), e.lue);
            snprintf(morceau, sizeof(morceau), tr("lue %s"), h);
            strncat(tout, " · ", sizeof(tout) - strlen(tout) - 1);
            strncat(tout, morceau, sizeof(tout) - strlen(tout) - 1);
        }
        if (e.terminee != 0) {
            heure_txt(h, sizeof(h), e.terminee);
            snprintf(morceau, sizeof(morceau), tr("terminée %s"), h);
            strncat(tout, " · ", sizeof(tout) - strlen(tout) - 1);
            strncat(tout, morceau, sizeof(tout) - strlen(tout) - 1);
        }
        ui_text(r.heures, tout);
        ui_text_color(r.heures, UIColor.TEXT_DIM);
        ui_hidden(r.ligne, false);
    }
    ui_hidden(u.tout_lu, !a_lire);
    if (u.attente != nullptr) {
        ui_text(u.attente, s_recu ? tr("Aucune alerte") : tr("En attente de Home Assistant"));
        ui_text_color(u.attente, UIColor.TEXT_DIM);
        ui_hidden(u.attente, s_nb > 0);
    }
}

}  // namespace

void alertes_historique_recu(const std::string& payload) {
    s_nb = 0;
    s_recu = true;
    const char* p = payload.c_str();
    while (*p != '\0' && s_nb < kLignesMax) {
        const char* fin = strchr(p, ';');
        const size_t n = fin ? static_cast<size_t>(fin - p) : strlen(p);
        // « apparue|lue|terminée|gravité|libellé » : le libellé est le reste (il ne contient
        // ni « | » ni « ; », HA les remplace) ; une entrée sans ses cinq champs est ignorée.
        const char* champ[5] = {};
        size_t taille[5] = {};
        int nb = 0;
        const char* d = p;
        for (const char* c = p; c <= p + n && nb < 5; c++) {
            if (c == p + n || (*c == '|' && nb < 4)) {
                champ[nb] = d;
                taille[nb] = static_cast<size_t>(c - d);
                nb++;
                d = c + 1;
            }
        }
        if (nb == 5) {
            Ligne& e = s_lignes[s_nb];
            e.apparue = lire_epoch(champ[0], taille[0]);
            e.lue = lire_epoch(champ[1], taille[1]);
            e.terminee = lire_epoch(champ[2], taille[2]);
            e.gravite = taille[3] > 0 ? champ[3][0] : 'O';
            if (e.gravite != 'R' && e.gravite != 'J') e.gravite = 'O';
            char brut[kTexteMax];
            texte_ha_copier(brut, sizeof(brut), champ[4], taille[4]);
            e.texte = ha_alerte_texte(brut);
            if (e.apparue != 0) s_nb++;
        }
        if (fin == nullptr) break;
        p = fin + 1;
    }
    peindre();
}

void alertes_ouvrir() {
    AlertesUI& u = g_alertes_ui;
    if (u.popup == nullptr) return;
    // Appui long au bout d'un glissement (swipe des prévisions, doigt gardé 400 ms) : rien.
    // LVGL remet ces deux marques à zéro à chaque appui ; hors appui (« Aller à l'écran »),
    // aucun périphérique n'est actif.
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr && (lv_indev_get_press_moved(indev) || lv_indev_get_gesture_dir(indev) != LV_DIR_NONE))
        return;
    construire();
    if (u.liste != nullptr) lv_obj_scroll_to_y(u.liste, 0, LV_ANIM_OFF);
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre();
    if (u.demander != nullptr) u.demander();
}

// « Tout marquer comme lu » : la liste montre tout de suite les alertes lues ; HA renvoie
// la vraie liste juste après (l'historique change pendant que le popup est ouvert).
void alertes_tout_lu_local() {
    const time_t maintenant = tab5_time_source(nullptr);
    for (int k = 0; k < s_nb; k++) {
        Ligne& e = s_lignes[k];
        if (e.terminee == 0 && e.lue == 0) e.lue = static_cast<uint32_t>(maintenant);
    }
    peindre();
}

// Thèmes (ADR-0029) : couleurs de la palette active, si les lignes existent.
void alertes_rejouer_theme() { peindre(); }
