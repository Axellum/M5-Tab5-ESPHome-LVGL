/**
 * [AI-CONTEXT]
 * @file tab5_roue_navigation.cpp
 * @role Roue de navigation (ADR-0042, demande d'Axel du 09/10/2026 : « une roue à choix
 *       multiples sur le touch long de la barre centrale pour choisir les pages où
 *       aller ») : l'appui long de la carte centrale ouvre, centrée sur elle, la roue
 *       d'actions rapides des tuiles (tab5_roue.cpp, ADR-0036) sur les écrans de la
 *       tablette. Premier anneau, de gauche à droite : Alertes, Pièces ▸, Appareils ▸,
 *       Agenda ▸, Tablette ▸, Assistant ; une famille (▸, un point) déplie ses écrans sur
 *       le second anneau, chacun avec son mot. Le moyeu dit « Aller à », puis le nom de la
 *       famille dépliée. Ne sont proposés que les écrans qui ont quelque chose à montrer
 *       dans cette maison (ecran_disponible, pièces qui ont des appareils) ; une famille
 *       vide disparaît, les autres boutons se resserrent.
 * @architecture_constraint Aucun moteur de roue ici : ce fichier compose les boutons et dit
 *       ce que fait un toucher (RoueRappels). Un écran s'ouvre par la routine unique
 *       tab5_ecran_ouvrir (RoueUI::ouvrir_ecran, ADR-0013) ; une pièce par le mode HA
 *       (tuiles_aller_piece). Un toucher ferme la roue d'abord (tab5_roue.cpp). Rien n'est
 *       gardé en NVS ; aucun événement envoyé à HA. Textes par tr() (tr_noop dans les
 *       tables), noms des pièces tels que HA les donne.
 * @ai_instruction Un écran de plus dans une famille : sa ligne dans kAppareils, kAgenda ou
 *       kTablette (kRoueChoix au plus par famille), son icône dans RoueIcone et
 *       glyphe_roue (tab5_roue.cpp, mdi_font_36, règle 9). Un bouton de plus au
 *       premier anneau : kPremier (kRoueBoutons au plus). tests/test_roue_navigation.py
 *       relit ces tables.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <cstdio>

namespace {

// Un écran d'une famille : ce qu'il ouvre, son icône, son mot (traduit à l'affichage).
struct Destination {
    Ecran ecran;
    RoueIcone icone;
    const char* mot;
};

// Appareils : les fenêtres de la maison. Températures en tête : son mot, le plus long,
// est au bout de l'éventail, sans voisin d'un côté sur sa rangée.
constexpr Destination kAppareils[] = {
    {Ecran::TEMPERATURE, RoueIcone::TEMPERATURE, tr_noop("Températures")},
    {Ecran::CLIM, RoueIcone::CLIMS, tr_noop("Clims")},
    {Ecran::LUMIERES, RoueIcone::LUMIERES, tr_noop("Lumières")},
    {Ecran::VOLET, RoueIcone::VOLET, tr_noop("Volets")},
    {Ecran::ENERGIE, RoueIcone::ENERGIE, tr_noop("Énergie")},
    {Ecran::PLANTES, RoueIcone::PLANTES, tr_noop("Plantes")},
};
// Agenda : le temps qui vient, la Météo en tête (popup Météo, ADR-0043). Caméras à la fin
// (ADR-0049, demande d'Axel du 09/10/2026) : Appareils et le premier anneau sont pleins
// (kRoueChoix, kRoueBoutons). Toujours proposé, comme la Météo : la tablette ne connaît
// les caméras qu'en les demandant ; sans caméra, le popup dit « Aucune caméra choisie ».
constexpr Destination kAgenda[] = {
    {Ecran::METEO, RoueIcone::METEO, tr_noop("Météo")},
    {Ecran::CALENDRIER, RoueIcone::CALENDRIER, tr_noop("Calendrier")},
    {Ecran::REVEIL, RoueIcone::REVEIL, tr_noop("Réveil")},
    {Ecran::CAMERAS, RoueIcone::CAMERAS, tr_noop("Caméras")},
};
// Tablette : ce qui est à elle, pas à la maison.
constexpr Destination kTablette[] = {
    {Ecran::ARCADE, RoueIcone::JEUX, tr_noop("Jeux")},
    {Ecran::REGLAGES, RoueIcone::ENGRENAGE, tr_noop("Réglages")},
    {Ecran::CONSOLE, RoueIcone::SYSTEME, tr_noop("Système")},
};

// Un bouton du premier anneau : un écran, une famille d'écrans, ou les pièces (Maison
// puis chaque pièce qui a des appareils : le mode HA sur elle).
enum class Genre : uint8_t {
    ECRAN,
    FAMILLE,
    PIECES,
};
struct Premier {
    Genre genre;
    RoueIcone icone;
    const char* mot;
    Ecran ecran;                // Genre::ECRAN
    const Destination* liste;  // Genre::FAMILLE
    int n;
};
template <size_t N>
constexpr Premier famille(RoueIcone icone, const char* mot, const Destination (&liste)[N]) {
    return Premier{Genre::FAMILLE, icone, mot, Ecran::AUCUN, liste, static_cast<int>(N)};
}
constexpr Premier kPremier[] = {
    {Genre::ECRAN, RoueIcone::ALERTES, tr_noop("Alertes"), Ecran::ALERTES, nullptr, 0},
    {Genre::PIECES, RoueIcone::PIECES, tr_noop("Pièces"), Ecran::AUCUN, nullptr, 0},
    famille(RoueIcone::APPAREILS, tr_noop("Appareils"), kAppareils),
    famille(RoueIcone::AGENDA, tr_noop("Agenda"), kAgenda),
    famille(RoueIcone::TABLETTE, tr_noop("Tablette"), kTablette),
    {Genre::ECRAN, RoueIcone::ASSISTANT, tr_noop("Assistant"), Ecran::ASSISTANT, nullptr, 0},
};
constexpr int kNbPremier = static_cast<int>(sizeof(kPremier) / sizeof(kPremier[0]));
static_assert(kNbPremier <= kRoueBoutons, "premier anneau : kRoueBoutons boutons au plus");
static_assert(sizeof(kAppareils) / sizeof(kAppareils[0]) <= kRoueChoix, "une famille : kRoueChoix choix au plus");
static_assert(sizeof(kAgenda) / sizeof(kAgenda[0]) <= kRoueChoix, "une famille : kRoueChoix choix au plus");
static_assert(sizeof(kTablette) / sizeof(kTablette[0]) <= kRoueChoix, "une famille : kRoueChoix choix au plus");
static_assert(1 + kPieces <= kRoueChoix, "Pièces : Maison et chaque pièce");

// Rose des vents du moyeu (mdi_font_45, règle 9 : MDI_CODE_TARGETS de
// check_tab5_code_rules.py la rattache à roue_moyeu_icone).
const char* glyphe_navigation() {
    return "\U000F1382";  // compass-rose
}

// Roue ouverte : la ligne de kPremier de chaque bouton montré. Noms des pièces (mots du
// second anneau, lus à chaque repeint).
struct Navigation {
    int n = 0;
    int premier[kRoueBoutons] = {};
};
Navigation s_nav;
char s_noms[kPieces][40];

// Cible d'un choix : un écran (sa valeur, > 0) ou une pièce R (-1 - R).
constexpr int cible_piece(int r) { return -1 - r; }

// Choix de la famille `p`, dans `c` (kRoueChoix au plus), et leurs cibles ; 0 : famille
// vide. Recalculés à chaque dépliage, repeint et toucher : ils suivent la maison (zones,
// pièces reçues).
int choix_de(const Premier& p, RoueChoix* c, int* cible) {
    int m = 0;
    if (p.genre == Genre::ECRAN) return 0;
    if (p.genre == Genre::PIECES) {
        // Maison (ADR-0037) d'abord, puis les pièces dans l'ordre du blueprint, leur numéro
        // dans le bouton (celui du titre « Pièce n/5 ») et leur nom dessous ; celle que le
        // mode HA montre est l'état courant.
        c[m] = RoueChoix{};
        c[m].icone = RoueIcone::MAISON;
        c[m].legende = tr("Maison");
        cible[m++] = static_cast<int>(Ecran::MAISON);
        const int montree = tuiles_piece_mode_ha();
        for (int r = 0; r < kPieces && m < kRoueChoix; r++) {
            if (!tuiles_piece_titre(r, s_noms[r], sizeof(s_noms[r]))) continue;
            c[m] = RoueChoix{};
            snprintf(c[m].texte, sizeof(c[m].texte), "%d", r + 1);
            c[m].legende = s_noms[r];
            c[m].courant = r == montree;
            cible[m++] = cible_piece(r);
        }
        return m > 1 ? m : 0;  // Maison seule : aucune pièce, aucune famille
    }
    for (int k = 0; k < p.n && m < kRoueChoix; k++) {
        const Destination& d = p.liste[k];
        if (!ecran_disponible(d.ecran)) continue;
        c[m] = RoueChoix{};
        c[m].icone = d.icone;
        c[m].legende = tr(d.mot);
        cible[m++] = static_cast<int>(d.ecran);
    }
    return m;
}

// Choix de la famille du bouton i de la roue ouverte.
int choix(int i, RoueChoix* c, int* cible) {
    return (i >= 0 && i < s_nav.n) ? choix_de(kPremier[s_nav.premier[i]], c, cible) : 0;
}

void ouvrir_cible(int cible) {
    if (cible < 0) tuiles_aller_piece(-1 - cible);
    else if (g_roue_ui.ouvrir_ecran != nullptr) g_roue_ui.ouvrir_ecran(cible);
}

// Rappels de la roue (tab5_roue.cpp ferme la roue avant choisir et choisir_choix).
void choisir(int i) {
    if (i < 0 || i >= s_nav.n) return;
    const Premier& p = kPremier[s_nav.premier[i]];
    if (p.genre == Genre::ECRAN) ouvrir_cible(static_cast<int>(p.ecran));
}

int famille_choix(int i, RoueChoix* c) {
    int cible[kRoueChoix];
    return choix(i, c, cible);
}

void choisir_choix(int i, int j) {
    RoueChoix c[kRoueChoix];
    int cible[kRoueChoix];
    const int m = choix(i, c, cible);
    if (j >= 0 && j < m) ouvrir_cible(cible[j]);
}

// Une alerte est-elle à l'écran (vigilance, bandeau d'alerte de HA) ? Le bouton Alertes
// prend alors l'aspect de l'état courant.
bool alerte_en_cours() {
    const CentralPanelCtx& ctx = g_central_ctx;
    if (ctx.has_mf_alerts) return true;
    for (bool a : ctx.has_ha)
        if (a) return true;
    return false;
}

void rejouer();

// Compose le premier anneau (les familles vides en moins) et ouvre la roue sur la carte
// centrale. `garder` : un repeint (thème) garde la famille dépliée.
bool peindre(bool garder) {
    lv_obj_t* ancre = g_roue_ui.carte_centrale;
    if (ancre == nullptr) return false;
    RoueBouton b[kRoueBoutons];
    s_nav.n = 0;
    for (int k = 0; k < kNbPremier; k++) {
        const Premier& p = kPremier[k];
        if (p.genre == Genre::ECRAN && !ecran_disponible(p.ecran)) continue;
        if (p.genre != Genre::ECRAN) {
            RoueChoix c[kRoueChoix];
            int cible[kRoueChoix];
            if (choix_de(p, c, cible) == 0) continue;
        }
        const int i = s_nav.n;
        s_nav.premier[i] = k;
        b[i] = RoueBouton{};
        b[i].icone = p.icone;
        b[i].genre = p.genre == Genre::ECRAN ? RoueGenre::ACTION : RoueGenre::FAMILLE;
        b[i].courant = p.ecran == Ecran::ALERTES && alerte_en_cours();
        b[i].legende = tr(p.mot);
        s_nav.n = i + 1;
    }
    if (s_nav.n == 0) return false;
    RoueTete tete;
    tete.icone = glyphe_navigation();
    tete.valeur = tr("Aller à");
    tete.nom = "";
    tete.jauge = -1;
    tete.couleur = UIColor.ACCENT;
    tete.mots = true;
    RoueRappels rappels;
    rappels.choisir = choisir;
    rappels.famille = famille_choix;
    rappels.choisir_choix = choisir_choix;
    rappels.rejouer = rejouer;
    return roue_ouvrir(ancre, tete, b, s_nav.n, rappels, garder);
}

// Changement de thème, roue ouverte : la même roue dans la nouvelle palette.
void rejouer() {
    if (!peindre(true)) roue_actions_fermer();
}

}  // namespace

void roue_navigation_ouvrir() {
    // Un appui long au bout d'un glissement (swipe des prévisions ou des pièces) : rien.
    if (ui_appui_glisse()) return;
    peindre(false);
}
