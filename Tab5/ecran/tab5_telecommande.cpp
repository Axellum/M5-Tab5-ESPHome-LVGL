/**
 * [AI-CONTEXT]
 * @file tab5_telecommande.cpp
 * @role Popup télécommande à plusieurs télécommandes (ADR-0056, 10/10/2026, demande
 *       d'Axel : TV Samsung, Apple TV et Freebox Player) : une page par télécommande que le
 *       blueprint pousse (clé « telecommandes » de tab5_maj_emplacements, lue par
 *       telecommandes_lire, tab5_parse.h section 13), leurs noms en haut et le glissement
 *       gauche / droite (la brique commune des popups à pages, tab5_pages.cpp, ADR-0046).
 *       La page montrée décide :
 *         - l'emplacement des événements esphome.tab5_action de toutes les touches
 *           (telecommande_cle : « tv », « tv1 »…), que le blueprint traduit pour la bonne
 *           télécommande ;
 *         - la disposition : tv (Source, Muet, applications) ou boitier (Stop, icône du
 *           volume, rangée de lecture). Tout le reste (pavé, OK, lecture, retour,
 *           accueil, menu, volume) est le même widget pour toutes.
 * @architecture_constraint Push-only, events-only (ADR-0001, ADR-0025) : rien n'est demandé
 *       à HA, aucune entité nommée. Changement de page INSTANTANÉ (préférence d'Axel) : on
 *       cache et on montre, rien ne glisse. Couleurs : styles de rôle du YAML, et l'onglet
 *       de la page montrée par choix_peindre() (palette active, UIColor.ACCENT). Rien n'est
 *       gardé en NVS : le blueprint repousse la liste à chaque connexion ; d'ici là, la page
 *       unique d'avant (aucune commande ne part sans HA, de toute façon).
 * @ai_warning [AI-WARNING] La première télécommande garde l'emplacement « tv » : c'est celui
 *       qu'un blueprint d'avant l'ADR-0056 commande (la télécommande de la TV), et celui
 *       qu'un firmware d'avant envoie à un blueprint récent. Ne pas la renommer « tv0 ».
 * @ai_instruction Un texte affiché passe par tr(). Une disposition de plus = une valeur de
 *       TelecommandeEcran (tab5_parse.h), son code dans le blueprint et ses widgets ici.
 */
#include "tab5_internal.h"
#include "tab5_parse.h"
#include "lvgl.h"

#include <algorithm>
#include <cstdio>

TelecommandeUI g_telecommande_ui;

static_assert(kTelecommandeOnglets == kTelecommandesMax, "un onglet par télécommande");

namespace {

constexpr size_t kNomMax = 48;  // octets, zéro final compris : l'onglet coupe avec « … »

struct Telecommande {
    TelecommandeEcran ecran = TelecommandeEcran::TV;
    char nom[kNomMax] = {};
};

Telecommande s_t[kTelecommandesMax];
int s_n = 0;     // télécommandes connues de HA ; 0 = la page unique d'avant l'ADR-0056
int s_page = 0;  // page montrée

int nombre_pages() { return s_n; }
int page_courante() { return s_page; }

TelecommandeEcran ecran_montre() { return s_n > 0 ? s_t[s_page].ecran : TelecommandeEcran::TV; }

void peindre() {
    const TelecommandeUI& u = g_telecommande_ui;
    if (u.popup == nullptr) return;
    const bool tv = ecran_montre() == TelecommandeEcran::TV;
    ui_hidden(u.source, !tv);
    ui_hidden(u.stop, tv);
    ui_hidden(u.muet, !tv);
    ui_hidden(u.volume_icone, tv);
    ui_hidden(u.applis, !tv);
    ui_hidden(u.lecture, tv);
    // Le titre perd « TV » quand les onglets sont là : plusieurs appareils, et la place
    // des noms (quatre onglets commencent à x = 318).
    ui_text(u.titre, s_n >= 2 ? tr("Télécommande") : tr("Télécommande TV"));
    static char defauts[kTelecommandesMax][kNomMax];
    const char* noms[kTelecommandesMax] = {};
    for (int i = 0; i < s_n; i++) {
        if (s_t[i].nom[0] != '\0') {
            noms[i] = s_t[i].nom;
        } else {
            snprintf(defauts[i], sizeof(defauts[i]), "%s %d", tr("Télécommande"), i + 1);
            noms[i] = defauts[i];
        }
    }
    pages_onglets(u.onglet, kTelecommandeOnglets, noms, s_n, s_page);
}

void afficher_page(int page) {
    if (page < 0 || page >= std::max(s_n, 1)) page = 0;
    s_page = page;
    peindre();
    ui_mark_activity();
}

// Geste gauche / droite du popup (tab5_pages.cpp, ADR-0046) ; les onglets suivent.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_page};

}  // namespace

void telecommande_brancher() {
    TelecommandeUI& u = g_telecommande_ui;
    if (u.popup == nullptr) return;
    if (s_pages.popup == nullptr) {
        s_pages.popup = u.popup;
        pages_brancher(&s_pages);
    }
    peindre();
}

void telecommandes_recu(const char* valeur, size_t n) {
    TelecommandeLue lues[kTelecommandesMax];
    const int nb = telecommandes_lire(valeur, n, lues);
    const bool connues_avant = s_n > 0;
    for (int i = 0; i < nb; i++) {
        Telecommande t;
        t.ecran = lues[i].ecran;
        texte_ha_copier(t.nom, sizeof(t.nom), lues[i].nom.p, lues[i].nom.n);
        s_t[i] = t;
    }
    for (int i = nb; i < kTelecommandesMax; i++) s_t[i] = Telecommande{};
    s_n = nb;
    if (s_page >= std::max(s_n, 1)) s_page = 0;
    ESP_LOGI("tab5.telecommande", "%d telecommande(s) connue(s)", s_n);
    peindre();
    // L'écran Télécommande peut apparaître ou disparaître (sans la TV du blueprint) : la
    // petite icône des boutons du haut suit (ecran_disponible).
    if (connues_avant != (s_n > 0)) boutons_haut_apply_ui();
}

void telecommande_ouvrir(int page) {
    const TelecommandeUI& u = g_telecommande_ui;
    if (u.popup == nullptr) return;
    if (page >= 0) s_page = (page < std::max(s_n, 1)) ? page : 0;
    peindre();
    animate_popup_open(u.popup);
}

void telecommande_page(int page) {
    if (page < 0 || page >= s_n) return;
    afficher_page(page);
}

std::string telecommande_cle() { return telecommande_emplacement(s_n > 0 ? s_page : 0); }

bool telecommandes_connues() { return s_n > 0; }

void telecommande_rejouer_theme() { peindre(); }
