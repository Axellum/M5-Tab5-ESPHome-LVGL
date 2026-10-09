/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_roue.cpp
 * @role Roue d'actions rapides d'une tuile (ADR-0036 ; sortie de tab5_tuiles.cpp le
 *       08/10/2026, lot L7 de l'audit du 07/10/2026) : ce que la roue propose pour une tuile
 *       (roue_composer : commandes, familles, liens « Maison » et « Détails »), les choix
 *       d'une famille (roue_choix : luminosités, blancs et couleurs, positions, modes,
 *       consignes et bascules de la clim), ce que fait un toucher (mêmes commandes que la
 *       tuile et ses popups) et son repeint quand HA pousse un état. Le dessin, le dépliage
 *       et la fermeture : tab5_roue.cpp, qui ne sait rien des appareils. Avec les teintes
 *       des lampes à couleur (lampe_teinte, UI-8), seule liste du popup lumière et de la roue.
 * @architecture_constraint Lit le modèle des tuiles par tab5_tuiles_priv.h ; la clim de la
 *       tuile ou du blueprint par clim_cible() et les fonctions clim_* de tab5_clim.cpp.
 *       Ouverte par tuile_roue_ouvrir (n'importe quelle ancre : carte du mode HA, tuile
 *       météo, ligne du popup Maison) ; roue_tuile_etat la suit (peindre_tuile).
 *       Roue d'une clim quelconque (ADR-0048, 09/10/2026) : ouverte par le toucher de la
 *       température de la pièce (clim_roue_ouvrir, sans tuile : rt.r = -1), ses boutons
 *       sont ceux de la clim d'une tuile cli — le même code, composer_clim dans
 *       roue_composer —, « Clims ▸ » à la place de « Maison » quand la tablette en connaît
 *       plusieurs, « Détails » = le carrousel des clims sur elle (ADR-0038). Son moyeu
 *       n'est pas sur la température (trop haute) mais sur une ancre basse
 *       (clim_ancre_basse) : l'éventail s'ouvre au-dessus, entier.
 * @ai_instruction Une famille de plus : sa RoueAction, sa ligne dans roue_composer et dans
 *       roue_choix, son glyphe dans glyphe_roue (tab5_roue.cpp, règle 9). Les commandes
 *       envoyées restent celles du tableau de l'ADR-0023 (tests/test_roue.py compare).
 *       Une clim est désignée par un ClimRef (r < 0 : celle du blueprint ; t < 0 : celle
 *       de la pièce r ; sinon la tuile tRT), comme tab5_clim.cpp qui la lit.
 */
#include "tab5_tuiles_priv.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
#include "tab5_tuiles_icones.h"
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

// est(), tuile_cle() : tab5_modele_ha.h.
using namespace modele_ha;

// ─── Teintes des lampes à couleur (UI-8, audit du 07/10/2026) ───────────────────────
//
// Le nom envoyé à HA (commande couleur, color_name) et la couleur montrée : pastilles du
// popup lumière (light_white_btn.yaml, light_color_preset_btn.yaml) et de la roue. Une
// seule liste : avant le 08/10/2026, la roue recopiait les teintes du popup. Une teinte
// peut s'écarter du CSS de HA pour rester lisible sur fond sombre (purple → 0xA855F7).
// Couleurs fixes de l'objet montré (une lampe rouge reste rouge), hors thème (ADR-0029).

namespace {
struct LampeTeinte {
    const char* nom;
    uint32_t couleur;
};
constexpr LampeTeinte kLampeTeintes[] = {
    // Blancs nommés
    {"warmwhite", 0xFFC864},  {"navajowhite", 0xFFDEAD}, {"white", 0xFFFFFF},
    // Couleurs, des chaudes aux froides
    {"gold", 0xFFD700},       {"orange", 0xFFA500},      {"orangered", 0xFF4500},
    {"red", 0xFF2020},        {"deeppink", 0xFF1493},    {"magenta", 0xFF00FF},
    {"blueviolet", 0x8A2BE2}, {"purple", 0xA855F7},      {"blue", 0x3B82F6},
    {"cyan", 0x00E5FF},       {"springgreen", 0x00FF7F}, {"green", 0x22C55E},
};
}  // namespace

uint32_t lampe_teinte(const char* nom) {
    if (nom != nullptr)
        for (const LampeTeinte& l : kLampeTeintes)
            if (std::strcmp(l.nom, nom) == 0) return l.couleur;
    return UIColor.TEXT_DIM;  // nom inconnu (aucun : tests/test_roue.py)
}

// ─── Roue d'actions rapides (ADR-0036, 07/10/2026, discussion #278) ─────────────────
//
// L'appui long d'une lum, d'un vol ou d'une cli ouvre la roue (tab5_roue.cpp). Premier
// anneau : « Maison » (le popup de toutes les pièces, sauf quand la roue s'ouvre depuis
// lui), les commandes de la tuile et ses familles de réglages, puis « Détails » (le popup
// complet de son appui long d'avant, tuile_ouvrir_popup). Toucher une famille déplie ses
// choix sur le second anneau : luminosités, blancs et couleurs d'une lampe, positions d'un
// volet, modes, consignes et options d'une clim. Aucune commande nouvelle (mêmes
// événements esphome.tab5_action, mêmes valeurs que la tuile et ses popups : ADR-0023,
// ADR-0026, ADR-0027), aucune mise à jour optimiste nouvelle. Un choix ferme la roue.
//
// Roue d'une clim (ADR-0048) : le toucher de la température de la pièce ouvre la roue de
// la clim qu'ouvrait le carrousel (celle de la pièce affichée en mode HA, sinon celle du
// blueprint), sans tuile. Mêmes boutons clim qu'une tuile cli ; « Clims ▸ » (au moins deux
// clims) déplie les autres sur le second anneau, en toucher une rouvre la roue sur elle ;
// « Détails » ouvre le carrousel sur elle.

namespace tuiles {

// Liste courte, plus lisible serrée.
// clang-format off
enum class RoueAction : uint8_t {
    MAISON, REGLAGES, ALLUMER, ETEINDRE, OUVRIR, ARRETER, FERMER, CLIM_ARRET,
    LUMINOSITE, BLANCS, COULEURS, POSITION, MODE, CONSIGNE, OPTIONS, CLIMS,
};
// clang-format on

// Modes d'une clim offerts par la roue, dans cet ordre : lettre de capacité (ADR-0026),
// mode envoyé (commande « mode », comme les boutons du popup), icône. « auto » et
// « heat_cool » n'ont ni lettre ni commande (ADR-0026) : la roue ne les offre pas.
struct RoueModeClim {
    char lettre;
    const char* mode;
    RoueIcone icone;
};
constexpr RoueModeClim kRoueModesClim[] = {
    {'h', "heat", RoueIcone::CHAUFFER},
    {'c', "cool", RoueIcone::REFROIDIR},
    {'d', "dry", RoueIcone::SECHER},
    {'f', "fan_only", RoueIcone::VENTILER},
};
// Luminosités offertes (commande luminosite_pct, comme les raccourcis du popup lumière).
constexpr uint8_t kRoueLuminosites[] = {10, 25, 50, 75, 100};
// Positions offertes à un volet qui donne la sienne (commande position, comme le volet
// dessiné du popup).
constexpr uint8_t kRouePositions[] = {25, 50, 75};
// Pastilles d'une lampe à couleur (option c, commande couleur) : le nom envoyé à HA
// (color_name) et, pour un blanc, son mot. Leur couleur : lampe_teinte(), la même que
// les pastilles du popup lumière (UI-8).
struct RouePastille {
    const char* nom;
    const char* legende;
};
constexpr RouePastille kRoueBlancs[] = {
    {"warmwhite", tr_noop("Chaud")},
    {"navajowhite", tr_noop("Crème")},
    {"white", tr_noop("Froid")},
};
constexpr RouePastille kRoueCouleurs[] = {
    {"red", nullptr},   {"orange", nullptr}, {"gold", nullptr},
    {"green", nullptr}, {"blue", nullptr},   {"purple", nullptr},
};

// La roue ouverte : sa tuile (r = -1 : la roue d'une clim, sans tuile), la clim qu'elle
// vise (celle d'une tuile cli, ou celle de la roue d'une clim ; r = -2 : aucune), son
// ancre, sa vue d'origine et ce que fait chaque bouton.
struct RoueTuile {
    int r = -1;
    int t = -1;
    ClimRef clim{-2, -1, -1};
    lv_obj_t* ancre = nullptr;
    bool depuis_maison = false;
    int n = 0;
    RoueAction action[kRoueBoutons] = {};
};
RoueTuile s_rt;

// La roue d'une clim, sans tuile (ADR-0048) ; sinon celle de la tuile tRT.
bool roue_de_clim(const RoueTuile& rt) { return rt.r < 0; }
bool vise_une_clim(const RoueTuile& rt) { return rt.clim.r >= -1; }

// Emplacement des commandes d'une clim (celui que vise son popup) : « clim » pour celle du
// blueprint, « tRT » pour une tuile, « cpR » pour une pièce (ADR-0040).
struct ClimEmplacement {
    char s[8] = "clim";
};
ClimEmplacement clim_emplacement(const ClimRef& c) {
    ClimEmplacement e;
    if (c.r >= 0) {
        const CleTuile k = c.t < 0 ? clim_piece_cle(c.r) : tuile_cle(c.r, c.t);
        snprintf(e.s, sizeof(e.s), "%s", k.s);
    }
    return e;
}

// Mode de la clim visée, pour marquer l'état courant : l'état poussé de la tuile cli
// (comme avant l'ADR-0048), celui de la clim elle-même pour la roue d'une clim.
const char* roue_clim_mode(const RoueTuile& rt) {
    if (!roue_de_clim(rt)) return s_etats[rt.r][rt.t].brut;
    return clim_mode_connu(rt.clim.r, rt.clim.t);
}

// « Clims ▸ » : les clims de clims_enumerer, au plus kRoueChoix, celle de la roue comprise
// (au-delà de kRoueChoix, la fenêtre qui finit sur elle). Renvoie leur nombre, `courant` le
// rang de celle de la roue (-1 : aucune).
int roue_clims(const RoueTuile& rt, ClimRef out[kRoueChoix], int& courant) {
    ClimRef l[kClimPastilles];
    const int n = clims_enumerer(l, kClimPastilles);
    int ici = -1;
    for (int k = 0; k < n && ici < 0; k++)
        if (l[k].r == rt.clim.r && l[k].t == rt.clim.t) ici = k;
    const int debut = ici >= kRoueChoix ? ici - kRoueChoix + 1 : 0;
    const int m = std::min(n - debut, kRoueChoix);
    for (int k = 0; k < m; k++) out[k] = l[debut + k];
    courant = ici >= 0 ? ici - debut : -1;
    return m;
}

// Ce qu'envoie un choix du second anneau (à l'emplacement de la tuile, « clim » pour la
// clim du blueprint).
struct RoueEnvoi {
    const char* commande = nullptr;
    char valeur[16] = "";
};

// Boutons du premier anneau de la tuile tRT, ou de la clim de la roue d'une clim (r < 0,
// ADR-0048), dans `b` (leurs actions dans `rt`) : « Maison » d'abord (sauf depuis lui ;
// « Clims ▸ » à sa place sur la roue d'une clim quand la tablette en connaît plusieurs),
// « Détails » en dernier. 0 sans roue : type sans roue, option r, option k (une lampe ou
// un volet à confirmer garde son appui long d'avant), clim sans capacité reçue, aucune
// commande. Le bouton de l'état courant (lampe allumée ou éteinte, volet ouvert ou fermé,
// clim arrêtée) est marqué.
int roue_composer(int r, int t, bool depuis_maison, RoueBouton b[kRoueBoutons], RoueTuile& rt) {
    const bool de_clim = r < 0;
    if (!de_clim && (heritage() || !tuile_presente(r, t))) return 0;
    int n = 0;
    auto ajouter = [&](RoueAction a, RoueIcone i, RoueGenre g, bool courant, const char* legende) {
        if (n >= kRoueBoutons) return;
        b[n] = RoueBouton{i, g, courant, legende};
        rt.action[n] = a;
        n++;
    };
    auto commande = [&](RoueAction a, RoueIcone i, bool courant) {
        ajouter(a, i, RoueGenre::ACTION, courant, nullptr);
    };
    auto famille = [&](RoueAction a, RoueIcone i) { ajouter(a, i, RoueGenre::FAMILLE, false, nullptr); };
    // Les boutons d'une clim (ADR-0036) : ce que HA a poussé pour elle, rien d'autre. Ceux
    // de la tuile cli et ceux de la roue d'une clim (ADR-0048) : ce code seul.
    auto composer_clim = [&](const ClimRef& c, const char* mode) -> bool {
        const char* capacites = clim_capacites_connues(c.r, c.t);
        if (capacites == nullptr) return false;
        commande(RoueAction::CLIM_ARRET, RoueIcone::ETEINDRE, est(mode, "off"));
        bool modes = false;
        for (const RoueModeClim& m : kRoueModesClim) modes = modes || std::strchr(capacites, m.lettre) != nullptr;
        if (modes) famille(RoueAction::MODE, RoueIcone::MODE);
        float valeurs[5];
        char textes[5][10];
        int courant = -1;
        if (clim_roue_consignes(c.r, c.t, valeurs, textes, courant) > 0) famille(RoueAction::CONSIGNE, RoueIcone::CONSIGNE);
        ClimBascule bascules[5];
        if (clim_roue_bascules(c.r, c.t, bascules) > 0) famille(RoueAction::OPTIONS, RoueIcone::OPTIONS);
        return true;
    };
    if (de_clim) {
        // Roue d'une clim : « Clims ▸ » quand la tablette en connaît plusieurs (la place de
        // « Maison » : six boutons au plus), sinon « Maison ».
        ClimRef l[kRoueChoix];
        int ici = -1;
        if (roue_clims(rt, l, ici) >= 2) famille(RoueAction::CLIMS, RoueIcone::CLIMS);
        else ajouter(RoueAction::MAISON, RoueIcone::MAISON, RoueGenre::LIEN, false, tr("Maison"));
        const int premiere = n;
        if (!composer_clim(rt.clim, clim_mode_connu(rt.clim.r, rt.clim.t)) || n == premiere) return 0;
        ajouter(RoueAction::REGLAGES, RoueIcone::REGLAGES, RoueGenre::LIEN, false, tr("Détails"));
        return n;
    }
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    if (d.options & OPT_R) return 0;
    if (!depuis_maison) ajouter(RoueAction::MAISON, RoueIcone::MAISON, RoueGenre::LIEN, false, tr("Maison"));
    const int premiere = n;
    switch (static_cast<Type>(d.type)) {
        case Type::LUM: {
            if (d.options & OPT_K) return 0;
            const bool allumee = est(e.brut, "on");
            // Option o : jamais éteinte depuis l'écran, pas d'Éteindre.
            if (d.options & OPT_D) {
                // Variateur : la commande qui change l'état, puis les luminosités.
                if (allumee && !(d.options & OPT_O)) commande(RoueAction::ETEINDRE, RoueIcone::ETEINDRE, false);
                else commande(RoueAction::ALLUMER, RoueIcone::ALLUMER, allumee);
                famille(RoueAction::LUMINOSITE, RoueIcone::LUMINOSITE);
            } else {
                commande(RoueAction::ALLUMER, RoueIcone::ALLUMER, allumee);
                if (!(d.options & OPT_O)) commande(RoueAction::ETEINDRE, RoueIcone::ETEINDRE, est(e.brut, "off"));
            }
            if (d.options & OPT_C) {
                famille(RoueAction::BLANCS, RoueIcone::BLANCS);
                famille(RoueAction::COULEURS, RoueIcone::COULEURS);
            }
            break;
        }
        case Type::VOL: {
            if (d.options & OPT_K) return 0;
            // « Ouvert » et « Fermé » comme les mots du popup (vol_etat_mots) : ouvert mais
            // arrêté en route, c'est « Partiel », pas Ouvrir.
            const bool partiel = e.valeur < 0.0f || (e.valeur > 0.0f && e.valeur < 100.0f);
            commande(RoueAction::OUVRIR, RoueIcone::OUVRIR, est(e.brut, "open") && !partiel);
            commande(RoueAction::ARRETER, RoueIcone::STOP, false);
            commande(RoueAction::FERMER, RoueIcone::FERMER, est(e.brut, "closed"));
            if (vol_position_connue(e)) famille(RoueAction::POSITION, RoueIcone::POSITION);
            break;
        }
        case Type::CLI: {
            // La clim du blueprint (option m) ou celle de la tuile.
            const ClimCible c = clim_cible(d, r, t);
            rt.clim = ClimRef{static_cast<int8_t>(c.r), static_cast<int8_t>(c.t), static_cast<int8_t>(r)};
            if (!composer_clim(rt.clim, e.brut)) return 0;
            break;
        }
        default:
            return 0;
    }
    if (n == premiere) return 0;
    // « Détails » (UI-13, décision d'Axel du 07/10/2026) : « Réglages » désignait aussi les
    // Réglages de la tablette (engrenage).
    ajouter(RoueAction::REGLAGES, RoueIcone::REGLAGES, RoueGenre::LIEN, false, tr("Détails"));
    return n;
}

const char* roue_legende_mode(char lettre) {
    switch (lettre) {
        case 'h': return tr_ctx("clim", "Chaud");
        case 'c': return tr("Froid");
        case 'd': return tr("Sec");
        default: return tr("Ventilation");
    }
}

RoueIcone roue_icone_bascule(char lettre) {
    switch (lettre) {
        case 'e': return RoueIcone::ECO;
        case 'b': return RoueIcone::BOOST;
        case 'q': return RoueIcone::SILENCE;
        case 's': return RoueIcone::OSCILLATION;
        default: return RoueIcone::BRISE;
    }
}

const char* roue_legende_bascule(char lettre) {
    switch (lettre) {
        case 'e': return tr("Éco");
        case 'b': return tr("Boost");
        case 'q': return tr("Silence");
        case 's': return tr("Oscillation");
        default: return tr("Brise");
    }
}

// Noms des clims de « Clims ▸ » (leurs mots sous les choix) : lus par la roue au repeint
// qui suit, juste après roue_choix.
char s_noms_clims[kRoueChoix][49];

// Choix du second anneau de la famille i de `rt` dans `c`, ce qu'ils envoient dans `env` ;
// renvoie leur nombre (0 : rien à déplier). Le choix de l'état courant (luminosité,
// position, mode, consigne, option active) est marqué ; une couleur ne l'est jamais (l'état
// poussé n'en dit que la teinte affichée).
int roue_choix(const RoueTuile& rt, int i, RoueChoix c[kRoueChoix], RoueEnvoi env[kRoueChoix]) {
    if (i < 0 || i >= rt.n) return 0;
    if (!roue_de_clim(rt) && (heritage() || !tuile_presente(rt.r, rt.t))) return 0;
    // Lampe, volet : l'état de la tuile ; une clim (tuile cli ou roue d'une clim) : rt.clim.
    static const Etat kSansTuile{};
    const Etat& e = roue_de_clim(rt) ? kSansTuile : s_etats[rt.r][rt.t];
    const ClimRef& clim = rt.clim;
    int m = 0;
    RoueChoix rebut;
    auto choix = [&](const char* commande, const char* valeur, bool courant) -> RoueChoix& {
        if (m >= kRoueChoix) return rebut;
        c[m] = RoueChoix{};
        c[m].courant = courant;
        env[m].commande = commande;
        snprintf(env[m].valeur, sizeof(env[m].valeur), "%s", valeur != nullptr ? valeur : "");
        return c[m++];
    };
    auto pastilles = [&](const RouePastille* p, size_t nb) {
        for (size_t k = 0; k < nb; k++) {
            RoueChoix& x = choix("couleur", p[k].nom, false);
            x.a_pastille = true;
            x.pastille = lampe_teinte(p[k].nom);
            if (p[k].legende != nullptr) x.legende = tr(p[k].legende);
        }
    };
    char nombre[8];
    switch (rt.action[i]) {
        case RoueAction::LUMINOSITE: {
            // Luminosité 0-255 de l'état, en % (128 → 50) ; éteinte ou inconnue : -1.
            const int pct = est(e.brut, "on") ? lum_pct(e.valeur) : -1;
            for (uint8_t p : kRoueLuminosites) {
                snprintf(nombre, sizeof(nombre), "%u", static_cast<unsigned>(p));
                RoueChoix& x = choix("luminosite_pct", nombre, pct == p);
                snprintf(x.texte, sizeof(x.texte), "%u %%", static_cast<unsigned>(p));
            }
            break;
        }
        case RoueAction::BLANCS:
            pastilles(kRoueBlancs, sizeof(kRoueBlancs) / sizeof(kRoueBlancs[0]));
            break;
        case RoueAction::COULEURS:
            pastilles(kRoueCouleurs, sizeof(kRoueCouleurs) / sizeof(kRoueCouleurs[0]));
            break;
        case RoueAction::POSITION: {
            // Jamais sans position connue (le volet a pu la perdre roue ouverte).
            if (!vol_position_connue(e)) break;
            const long position = std::lround(e.valeur);
            for (uint8_t p : kRouePositions) {
                snprintf(nombre, sizeof(nombre), "%u", static_cast<unsigned>(p));
                RoueChoix& x = choix("position", nombre, position == p);
                snprintf(x.texte, sizeof(x.texte), "%u %%", static_cast<unsigned>(p));
            }
            break;
        }
        case RoueAction::MODE: {
            const char* capacites = clim_capacites_connues(clim.r, clim.t);
            if (capacites == nullptr) break;
            const char* mode = roue_clim_mode(rt);
            for (const RoueModeClim& md : kRoueModesClim) {
                if (std::strchr(capacites, md.lettre) == nullptr) continue;
                RoueChoix& x = choix("mode", md.mode, est(mode, md.mode));
                x.icone = md.icone;
                x.legende = roue_legende_mode(md.lettre);
            }
            break;
        }
        case RoueAction::CONSIGNE: {
            // La consigne et deux pas de chaque côté, dans les bornes de la clim ; envoyée
            // comme celle du popup (clim_consigne_texte : « 21.5 »).
            float valeurs[5];
            char textes[5][10];
            int courant = -1;
            const int nb = clim_roue_consignes(clim.r, clim.t, valeurs, textes, courant);
            for (int k = 0; k < nb; k++) {
                RoueChoix& x = choix("consigne", clim_consigne_texte(valeurs[k]).c_str(), k == courant);
                snprintf(x.texte, sizeof(x.texte), "%.9s", textes[k]);  // une ligne de textes[5][10]
            }
            break;
        }
        case RoueAction::OPTIONS: {
            // Bascules du popup (Éco, Boost, Silence, Oscillation, Brise) : même commande,
            // même valeur que leur bouton (clim_popup_preset, _silence, _oscillation, _brise).
            ClimBascule bascules[5];
            const int nb = clim_roue_bascules(clim.r, clim.t, bascules);
            for (int k = 0; k < nb; k++) {
                RoueChoix& x = choix(bascules[k].commande, bascules[k].valeur, bascules[k].actif);
                x.icone = roue_icone_bascule(bascules[k].lettre);
                x.legende = roue_legende_bascule(bascules[k].lettre);
            }
            break;
        }
        case RoueAction::CLIMS: {
            // Les autres clims (ADR-0048) : leur consigne (éteinte : l'icône Éteindre), leur
            // nom dessous ; celle de la roue marquée. Rien n'est envoyé : un toucher rouvre
            // la roue sur elle (roue_tuile_choisir_choix).
            ClimRef l[kRoueChoix];
            int ici = -1;
            const int nb = roue_clims(rt, l, ici);
            for (int k = 0; k < nb; k++) {
                ClimTete tete;
                clim_tete(l[k].r, l[k].t, tete);
                RoueChoix& x = choix(nullptr, "", k == ici);
                if (est(clim_mode_connu(l[k].r, l[k].t), "off")) x.icone = RoueIcone::ETEINDRE;
                else snprintf(x.texte, sizeof(x.texte), "%s", tete.consigne);
                snprintf(s_noms_clims[k], sizeof(s_noms_clims[k]), "%s", tete.nom);
                x.legende = s_noms_clims[k];
            }
            break;
        }
        default:
            break;
    }
    return m;
}

// Jauge du moyeu : luminosité (0 éteinte), position du volet, consigne de la clim dans ses
// bornes ; -1 sans valeur (lampe sans variateur, position ou consigne inconnue).
int roue_jauge(int r, int t) {
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    switch (static_cast<Type>(d.type)) {
        case Type::LUM:
            if (!(d.options & OPT_D)) return -1;
            if (!est(e.brut, "on")) return 0;
            return lum_pct(e.valeur);
        case Type::VOL:
            return vol_position_connue(e) ? std::clamp(static_cast<int>(std::lround(e.valeur)), 0, 100) : -1;
        case Type::CLI: {
            const ClimCible c = clim_cible(d, r, t);
            return clim_roue_jauge(c.r, c.t);
        }
        default:
            return -1;
    }
}

void roue_tuile_rejouer();

// Toucher du bouton i du premier anneau (roue déjà fermée ; une famille, tab5_roue.cpp la
// déplie sans passer par ici) : la commande, par les chemins de la tuile, ou un lien.
void roue_tuile_choisir(int i) {
    charger();
    const RoueTuile rt = s_rt;
    if (i < 0 || i >= rt.n) return;
    if (!roue_de_clim(rt) && (heritage() || !tuile_presente(rt.r, rt.t))) return;
    const TuilesUI& u = g_tuiles_ui;
    // Une clim : à l'emplacement que vise son popup (« clim » pour celle du blueprint).
    const ClimEmplacement clim = clim_emplacement(rt.clim);
    switch (rt.action[i]) {
        case RoueAction::MAISON:
            if (g_roue_ui.ouvrir_ecran != nullptr) g_roue_ui.ouvrir_ecran(static_cast<int>(Ecran::MAISON));
            return;
        case RoueAction::REGLAGES:
            // Roue d'une clim : le carrousel des clims sur elle (ADR-0038, ADR-0048).
            if (roue_de_clim(rt)) clim_carrousel_ouvrir_sur(rt.clim);
            else tuile_ouvrir_popup(rt.r, rt.t);
            return;
        case RoueAction::ALLUMER:
            envoyer_tuile(rt.r, rt.t, "allumer");
            return;
        case RoueAction::ETEINDRE:
            envoyer_tuile(rt.r, rt.t, "eteindre");
            return;
        case RoueAction::OUVRIR:
            envoyer_tuile(rt.r, rt.t, "ouvrir");
            return;
        case RoueAction::ARRETER:
            envoyer_tuile(rt.r, rt.t, "arreter");
            return;
        case RoueAction::FERMER:
            envoyer_tuile(rt.r, rt.t, "fermer");
            return;
        case RoueAction::CLIM_ARRET:
            if (u.envoyer != nullptr) u.envoyer(clim.s, "eteindre", "");
            return;
        default:
            return;
    }
}

// Choix de la famille dépliée (rappel `famille` de tab5_roue.cpp), recalculés à chaque
// dépliage et à chaque repeint : ils suivent l'état poussé.
int roue_tuile_famille(int i, RoueChoix* c) {
    charger();
    RoueEnvoi env[kRoueChoix];
    return roue_choix(s_rt, i, c, env);
}

// Toucher du choix j de la famille i (roue déjà fermée) : sa commande, recalculée sur
// l'état d'aujourd'hui.
void roue_tuile_choisir_choix(int i, int j) {
    charger();
    const RoueTuile rt = s_rt;
    RoueChoix c[kRoueChoix];
    RoueEnvoi env[kRoueChoix];
    const int m = roue_choix(rt, i, c, env);
    if (j < 0 || j >= m) return;
    // « Clims ▸ » (ADR-0048) : la roue rouverte sur la clim touchée, à la même place ; sans
    // réglages reçus pour elle, le carrousel sur elle.
    if (rt.action[i] == RoueAction::CLIMS) {
        ClimRef l[kRoueChoix];
        int ici = -1;
        if (j >= roue_clims(rt, l, ici)) return;
        if (!clim_roue_ouvrir(l[j], rt.ancre)) clim_carrousel_ouvrir_sur(l[j]);
        return;
    }
    const TuilesUI& u = g_tuiles_ui;
    if (env[j].commande == nullptr || u.envoyer == nullptr) return;
    // Une clim : à l'emplacement que vise son popup ; une lampe, un volet : la tuile.
    const ClimEmplacement clim = clim_emplacement(rt.clim);
    const CleTuile cle = tuile_cle(rt.r, rt.t);
    const bool est_clim = vise_une_clim(rt);
    u.envoyer(est_clim ? clim.s : cle.s, env[j].commande, env[j].valeur);
}

// Boutons, moyeu (icône, ligne d'état, nom, jauge) et couleur d'état (celle de la pastille
// de la carte, peindre_carte) de la tuile de `rt`, puis la roue autour de son ancre.
// `garder` : un repeint (thème, état poussé) garde la famille dépliée. Faux sans roue.
bool roue_tuile_peindre(RoueTuile& rt, bool garder) {
    RoueBouton b[kRoueBoutons];
    if (!roue_de_clim(rt)) rt.clim = ClimRef{-2, -1, -1};  // une tuile cli la pose (roue_composer)
    rt.n = roue_composer(rt.r, rt.t, rt.depuis_maison, b, rt);
    if (rt.n == 0) return false;
    RoueTete tete;
    Vue v;
    ClimTete clim;
    if (roue_de_clim(rt)) {
        // Le moyeu d'une clim : l'icône, la ligne d'état et la couleur d'une tuile cli, son
        // nom, la jauge de sa consigne (ADR-0048).
        clim_tete(rt.clim.r, rt.clim.t, clim);
        tete.icone = tuile_icone("clim", clim.actif, nullptr);
        tete.valeur = clim.ligne;
        tete.nom = clim.nom;
        tete.couleur = clim.couleur;
        tete.jauge = clim_roue_jauge(rt.clim.r, rt.clim.t);
    } else {
        vue(rt.r, rt.t, v);
        tete.icone = v.icone_carte != nullptr ? v.icone_carte : "";
        tete.valeur = v.ligne;
        tete.nom = v.nom;
        tete.couleur = v.couleur_carte;
        tete.jauge = roue_jauge(rt.r, rt.t);
    }
    RoueRappels rappels;
    rappels.choisir = roue_tuile_choisir;
    rappels.famille = roue_tuile_famille;
    rappels.choisir_choix = roue_tuile_choisir_choix;
    rappels.rejouer = roue_tuile_rejouer;
    s_rt = rt;
    return roue_ouvrir(rt.ancre, tete, b, rt.n, rappels, garder);
}

// Changement de thème, roue ouverte : la même roue dans la nouvelle palette ; si la tuile
// n'en a plus (état ou définition changés entre-temps), elle se ferme.
void roue_tuile_rejouer() {
    charger();
    RoueTuile rt = s_rt;
    if (!roue_tuile_peindre(rt, true)) roue_actions_fermer();
}

// HA a poussé un état de la tuile tRT (peindre_tuile) : la roue ouverte sur elle suit
// (bouton et choix de l'état courant, moyeu, jauge) ; elle se ferme si la tuile n'en a plus.
void roue_tuile_etat(int r, int t) {
    if (roue_actions_ouverte() && s_rt.r == r && s_rt.t == t) roue_tuile_rejouer();
}

}  // namespace tuiles

using namespace tuiles;

// HA a poussé les réglages ou l'état d'une clim, ou elle est oubliée (tab5_clim.cpp) : la
// roue ouverte sur une clim (tuile cli, roue d'une clim) suit, ou se ferme.
void roue_clim_changee() {
    if (roue_actions_ouverte() && vise_une_clim(s_rt)) roue_tuile_rejouer();
}

// Ancre de la roue d'une clim (ADR-0048). La température de la pièce est trop haute (centre
// en y 164) : autour d'elle, la roue passerait sous l'ancre, en éventail serré et pivoté
// sur la tuile − / + et la carte centrale (rendu du 09/10/2026). Le moyeu montre la clim, il
// n'a pas à être sur la température : il se pose sur un point bas, à la verticale de la zone
// touchée, ramenée dans [kClimAncreXMin, kClimAncreXMax], où les deux anneaux et leurs mots
// tiennent au-dessus sans pivot, six familles de six choix comprises (mesuré par
// disposer() et mot_recul() de tab5_roue.cpp, mots de 150 px : y de 365 à 477 et x de 479
// à 801 ; tests/test_roue_clim.py refait la mesure).
// Un objet de 1 px sans style ni toucher, créé une fois sur le calque du haut : roue_ouvrir()
// garde sa signature (une ancre lv_obj_t*) et sa géométrie.
namespace {
constexpr int32_t kClimAncreY = 460;
constexpr int32_t kClimAncreXMin = 480;
constexpr int32_t kClimAncreXMax = 800;
lv_obj_t* s_clim_ancre = nullptr;

lv_obj_t* clim_ancre_basse(lv_obj_t* zone) {
    if (zone == s_clim_ancre) return zone;  // « Clims ▸ » : la roue rouvre au même endroit
    if (s_clim_ancre == nullptr) {
        s_clim_ancre = lv_obj_create(lv_layer_top());
        lv_obj_remove_style_all(s_clim_ancre);
        lv_obj_remove_flag(s_clim_ancre, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(s_clim_ancre, 1, 1);
    }
    lv_obj_update_layout(zone);
    lv_area_t a;
    lv_obj_get_coords(zone, &a);
    ui_x(s_clim_ancre, std::clamp(a.x1 + lv_area_get_width(&a) / 2, kClimAncreXMin, kClimAncreXMax));
    ui_y(s_clim_ancre, kClimAncreY);
    return s_clim_ancre;
}
}  // namespace

bool clim_roue_ouvrir(const ClimRef& c, lv_obj_t* ancre) {
    charger();
    if (ancre == nullptr || c.r < -1 || c.r >= kPieces || c.t >= kTuiles) return false;
    RoueTuile rt;
    rt.clim = c;
    rt.ancre = clim_ancre_basse(ancre);
    return roue_tuile_peindre(rt, false);
}

bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre, bool depuis_maison) {
    charger();
    if (ancre == nullptr || r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return false;
    RoueTuile rt;
    rt.r = r;
    rt.t = t;
    rt.ancre = ancre;
    rt.depuis_maison = depuis_maison;
    return roue_tuile_peindre(rt, false);
}

// Ancre de la tuile T de la page courante : la pastille de sa carte (mode HA), le bouton
// au centre de sa tuile météo sinon. Appui long d'une tuile (tuile_appui_piece).
bool tuiles::roue_de_la_tuile(int r, int t) {
    if (g_central_ctx.ha_mode) return tuile_roue_ouvrir(r, t, g_tuiles_ui.carte_pastille[t]);
    lv_obj_t *g, *d, *b;
    widgets_meteo(t, g, d, b);
    return tuile_roue_ouvrir(r, t, b);
}
