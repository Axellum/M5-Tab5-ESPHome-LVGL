/**
 * [AI-CONTEXT]
 * @file tab5_roue_navigation.cpp
 * @role Roue de navigation (ADR-0042, demande d'Axel du 09/10/2026 : « une roue à choix
 *       multiples sur le touch long de la barre centrale pour choisir les pages où
 *       aller ») : l'appui long de la carte centrale ouvre, centrée sur elle, la roue
 *       d'actions rapides des tuiles (tab5_roue.cpp, ADR-0036) sur les écrans de la
 *       tablette. Premier anneau, de gauche à droite : Alertes, Pièces ▸, Appareils ▸,
 *       Agenda ▸, Tablette ▸, Assistant ; une famille (▸, un point) déplie ses écrans sur
 *       le second anneau (Agenda ▸ : Météo, Calendrier, Réveil, Caméras, Froid, ADR-0055, et
 *       Serveur IA, ADR-0059), chacun avec son mot. Le moyeu dit « Aller à », puis le nom de la
 *       famille dépliée. Ne sont proposés que les écrans qui ont quelque chose à montrer
 *       dans cette maison (ecran_disponible, pièces qui ont des appareils) ; une famille
 *       vide disparaît, les autres boutons se resserrent. Appareils ▸ : Températures,
 *       Clims, Lumières, Volets, Énergie, Plantes, Musique, TV (les deux dernières le
 *       10/10/2026, demande d'Axel : « ajoute musique et tv dans appareils »).
 *       Pièces ▸ : Maison puis chaque pièce occupée ; plus de sept (dix au plus, ADR-0066),
 *       deux pages de la même famille : Maison, six pièces et « Suite » (»), puis « Retour »
 *       («) et les autres ; la roue se rouvre sur l'autre page, son aspect ne change pas.
 * @architecture_constraint Aucun moteur de roue ici : ce fichier compose les boutons et dit
 *       ce que fait un toucher (RoueRappels). Un écran s'ouvre par la routine unique
 *       tab5_ecran_ouvrir (RoueUI::ouvrir_ecran, ADR-0013) ; une pièce par le mode HA
 *       (tuiles_aller_piece). Un toucher ferme la roue d'abord (tab5_roue.cpp). Rien n'est
 *       gardé en NVS ; aucun événement envoyé à HA. Textes par tr() (tr_noop dans les
 *       tables), noms des pièces tels que HA les donne.
 * @ai_instruction Un écran de plus dans une famille : sa ligne dans kAppareils, kAgenda ou
 *       kTablette (kRoueChoix au plus par famille : 8, la géométrie de la carte centrale
 *       vérifiée par tests/test_roue_navigation.py), son icône dans RoueIcone et
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
// est au bout de l'éventail, sans voisin d'un côté sur sa rangée. Musique (popup Musique,
// ADR-0050) et TV (télécommande) au bout (demande d'Axel du 10/10/2026) : 8 choix, le
// plafond kRoueChoix. La TV n'est proposée qu'avec sa zone (ecran_disponible), Musique
// tant que HA n'a pas dit qu'aucun lecteur n'est choisi (propose()).
constexpr Destination kAppareils[] = {
    {Ecran::TEMPERATURE, RoueIcone::TEMPERATURE, tr_noop("Températures")},
    {Ecran::CLIM, RoueIcone::CLIMS, tr_noop("Clims")},
    {Ecran::LUMIERES, RoueIcone::LUMIERES, tr_noop("Lumières")},
    {Ecran::VOLET, RoueIcone::VOLET, tr_noop("Volets")},
    {Ecran::ENERGIE, RoueIcone::ENERGIE, tr_noop("Énergie")},
    {Ecran::PLANTES, RoueIcone::PLANTES, tr_noop("Plantes")},
    {Ecran::MUSIQUE, RoueIcone::MUSIQUE, tr_noop("Musique")},
    {Ecran::TV, RoueIcone::TV, tr_noop("TV")},
};
// Agenda : le temps qui vient, la Météo en tête (popup Météo, ADR-0043). Caméras à la fin
// (ADR-0049, demande d'Axel du 09/10/2026) : Appareils et le premier anneau sont pleins
// (kRoueChoix, kRoueBoutons). Toujours proposé, comme la Météo : la tablette ne connaît
// les caméras qu'en les demandant ; sans caméra, le popup dit « Aucune caméra choisie ».
// Froid après elles (ADR-0055, demande d'Axel du 10/10/2026) : la surveillance de la
// maison, comme les caméras ; Appareils et le premier anneau sont toujours pleins. Proposé
// tant que HA n'a pas dit qu'aucun appareil n'est déclaré (propose()). Son mot est
// « Frigos », pas « Froid » : la clé « Froid » se traduit déjà « Cool » (mode d'une clim,
// blanc d'une lampe).
// Serveur IA en dernier (ADR-0059) : la surveillance d'une machine de la maison, comme
// les deux précédents ; proposé seulement quand HA a poussé un serveur
// (serveur_ia_disponible) : une maison sans serveur de LLM ne le voit pas.
constexpr Destination kAgenda[] = {
    {Ecran::METEO, RoueIcone::METEO, tr_noop("Météo")},
    {Ecran::CALENDRIER, RoueIcone::CALENDRIER, tr_noop("Calendrier")},
    {Ecran::REVEIL, RoueIcone::REVEIL, tr_noop("Réveil")},
    {Ecran::CAMERAS, RoueIcone::CAMERAS, tr_noop("Caméras")},
    {Ecran::FROID, RoueIcone::FROID, tr_noop("Frigos")},
    {Ecran::SERVEUR_IA, RoueIcone::SERVEUR_IA, tr_noop("Serveur IA")},
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
// Pièces : Maison et chaque pièce occupée ; plus de kRoueChoix − 1 pièces (dix au plus,
// ADR-0066) : deux pages de la même famille, la première finit par « Suite » (»), la
// seconde commence par « Retour » («) — l'aspect de la roue ne change pas.
constexpr int kPiecesPage1 = kRoueChoix - 2;  // Maison, six pièces, « Suite »
static_assert(1 + (kPiecesMax - kPiecesPage1) <= kRoueChoix, "Pièces : la seconde page tient dans l'anneau");

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
char s_noms[kPiecesMax][40];
int s_page_pieces = 0;  // page de la famille Pièces (plus de sept pièces, ADR-0066)

// Un écran d'une famille est-il proposé ? Ce que la maison a (ecran_disponible) ; Froid
// (ADR-0055), pas quand HA a dit qu'aucun appareil n'est déclaré (froid_disponible) ; Serveur
// IA (ADR-0059), seulement quand HA a poussé un serveur (serveur_ia_disponible : le paquet
// HA est facultatif, une maison qui ne l'a pas ne pousse jamais rien) ; Musique,
// en plus, comme la zone à gauche de l'horloge (ADR-0051) : pas quand HA a dit qu'aucun
// lecteur n'est choisi (lecteur_zone_disponible ; avant sa première réponse, proposée). Le
// geste « musique » et « Aller à l'écran », eux, l'ouvrent toujours : choisis exprès, ils
// montrent le popup, qui dit où choisir les lecteurs.
bool propose(Ecran e) {
    if (!ecran_disponible(e)) return false;
    if (e == Ecran::FROID) return froid_disponible();
    if (e == Ecran::SERVEUR_IA) return serveur_ia_disponible();
    return e != Ecran::MUSIQUE || lecteur_zone_disponible();
}

// Cible d'un choix : un écran (sa valeur, > 0), une pièce R (-1 - R, R de 0 à 9) ou l'autre
// page de la famille Pièces (ADR-0066).
constexpr int cible_piece(int r) { return -1 - r; }
constexpr int kCibleSuite = -100;
constexpr int kCibleRetour = -101;

// Pièces occupées, dans l'ordre du blueprint (leurs noms dans s_noms) ; leur nombre.
int pieces_occupees(int* out) {
    int n = 0;
    for (int r = 0; r < tuiles_pieces_nombre() && r < kPiecesMax; r++)
        if (tuiles_piece_titre(r, s_noms[r], sizeof(s_noms[r]))) out[n++] = r;
    return n;
}

// Choix de la famille `p`, dans `c` (kRoueChoix au plus), et leurs cibles ; 0 : famille
// vide. Recalculés à chaque dépliage, repeint et toucher : ils suivent la maison (zones,
// pièces reçues).
int choix_de(const Premier& p, RoueChoix* c, int* cible) {
    int m = 0;
    if (p.genre == Genre::ECRAN) return 0;
    if (p.genre == Genre::PIECES) {
        // Maison (ADR-0037) d'abord, puis les pièces dans l'ordre du blueprint, leur numéro
        // dans le bouton (leur rang dans le blueprint) et leur nom dessous ; celle que le
        // mode HA montre est l'état courant. Plus de sept pièces : deux pages (« Suite »).
        int pieces[kPiecesMax];
        const int n = pieces_occupees(pieces);
        if (n == 0) return 0;  // Maison seule : aucune pièce, aucune famille
        const bool deux_pages = 1 + n > kRoueChoix;
        if (!deux_pages) s_page_pieces = 0;
        auto lien = [&](const char* texte, const char* legende, int vers) {
            c[m] = RoueChoix{};
            snprintf(c[m].texte, sizeof(c[m].texte), "%s", texte);
            c[m].legende = legende;
            cible[m++] = vers;
        };
        int debut = 0, fin = n;
        if (s_page_pieces == 0) {
            c[m] = RoueChoix{};
            c[m].icone = RoueIcone::MAISON;
            c[m].legende = tr("Maison");
            cible[m++] = static_cast<int>(Ecran::MAISON);
            if (deux_pages) fin = kPiecesPage1;
        } else {
            lien("\xC2\xAB", tr("Retour"), kCibleRetour);  // « (Latin-1, roboto_22)
            debut = kPiecesPage1;
        }
        const int montree = tuiles_piece_montree();
        for (int i = debut; i < fin && m < kRoueChoix; i++) {
            const int r = pieces[i];
            c[m] = RoueChoix{};
            snprintf(c[m].texte, sizeof(c[m].texte), "%d", r + 1);
            c[m].legende = s_noms[r];
            c[m].courant = r == montree;
            cible[m++] = cible_piece(r);
        }
        if (deux_pages && s_page_pieces == 0) lien("\xC2\xBB", tr("Suite"), kCibleSuite);  // »
        return m;
    }
    for (int k = 0; k < p.n && m < kRoueChoix; k++) {
        const Destination& d = p.liste[k];
        if (!propose(d.ecran)) continue;
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

bool peindre(bool garder);

// L'autre page de la famille Pièces : la roue (fermée par le toucher) se rouvre, la famille
// du bouton `i` dépliée sur cette page — rien n'est dessiné entre les deux.
void pieces_page(int i, int page) {
    s_page_pieces = page;
    if (peindre(false)) roue_actions_choisir(i);
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
    if (j < 0 || j >= m) return;
    if (cible[j] == kCibleSuite || cible[j] == kCibleRetour) pieces_page(i, cible[j] == kCibleSuite ? 1 : 0);
    else ouvrir_cible(cible[j]);
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
    // Famille Pièces sur deux pages (ADR-0066) : celle de la pièce montrée, sinon la première.
    int pieces[kPiecesMax];
    const int n = pieces_occupees(pieces);
    const int montree = tuiles_piece_montree();
    s_page_pieces = 0;
    for (int i = kPiecesPage1; i < n && 1 + n > kRoueChoix; i++)
        if (pieces[i] == montree) s_page_pieces = 1;
    peindre(false);
}
