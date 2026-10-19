/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_priv.h
 * @role Ce que les fichiers des tuiles partagent (découpe du 08/10/2026, lot L7 de
 *       l'audit du 07/10/2026) : tab5_tuiles.cpp (modèle, NVS, dessin, gestes),
 *       tab5_tuiles_popups.cpp (popups lumière, volet et appareil), tab5_tuiles_roue.cpp
 *       (roue d'actions rapides d'une tuile, teintes des lampes) et tab5_tuiles_cases.cpp
 *       (cases des tuiles coupées en deux ou en quatre, ADR-0066). Les types du modèle,
 *       l'état partagé et les fonctions appelées d'un fichier à l'autre, dans le namespace
 *       `tuiles`. Le reste de chaque fichier lui reste propre.
 * @architecture_constraint Privé aux tuiles : aucun autre fichier ne l'inclut (le reste du
 *       firmware passe par tab5_custom.h et tab5_internal.h). Il est dans `includes:`
 *       (tab5-ha-hmi.yaml, tab5-rendu-host.yaml) pour être copié à côté des .cpp, donc
 *       inclus aussi dans main.cpp : rien que des types, des constantes et des
 *       déclarations, tout dans `tuiles`, aucun `using namespace`.
 *       Def, Modele et Etat sont gardés tels quels en NVS (magie du modèle = « TU » + sa
 *       taille depuis l'ADR-0066, « RAN1 ») : un champ de plus ou un tableau plus grand
 *       change la magie, les définitions gardées sont oubliées une fois (HA les renvoie à
 *       chaque connexion).
 * @ai_instruction Une fonction d'un fichier des tuiles appelée par un autre : sa
 *       déclaration ici, sous le fichier qui la définit. Une fonction pour le reste du
 *       firmware : tab5_custom.h (lambdas YAML) ou tab5_internal.h (C++).
 */
#pragma once

#include "tab5_internal.h"
#include "tab5_disposition.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
#include "lvgl.h"
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace tuiles {

// ─── Modèle ─────────────────────────────────────────────────────────────────────────

// Types de tuile (ADR-0023), dans l'ordre de kTypes (tab5_tuiles.cpp).
enum class Type : uint8_t { VIDE, LUM, INT, VOL, MED, ACT, CAP, BIN, CLI };

// Options : une lettre chacune, bit i = lettre i de kLettresOptions.
//   d graduable, c couleur, o allumer seulement, k confirmer, r lecture seule,
//   t télécommande TV du blueprint, m climatisation du blueprint (sans m, une tuile cli a
//   sa propre clim dans le popup dès que HA en a envoyé les réglages, ADR-0027),
//   e capteur de la section « Énergie » du blueprint (un cap qui ouvre le popup Énergie,
//   ADR-0028). Un firmware plus ancien ignore une lettre qu'il ne connaît pas.
constexpr char kLettresOptions[] = "dcokrtme";
enum : uint8_t { OPT_D = 1, OPT_C = 2, OPT_O = 4, OPT_K = 8, OPT_R = 16, OPT_T = 32, OPT_M = 64, OPT_E = 128 };

// Taille des champs gardés (octets, zéro final compris) ; kNom, kIcone et kEtat :
// tab5_modele_ha.h.
constexpr size_t kComplement = 16;   // unité (cap) ou classe d'appareil (bin)

struct Def {
    uint8_t type;       // Type
    uint8_t options;    // OPT_*
    char icone[modele_ha::kIcone];
    char complement[kComplement];
    char nom[modele_ha::kNom];
};

// Exactement ce qui part en NVS : tout en octets, sans bourrage (memcmp fiable). Dimensionné
// sur kPiecesMax pièces (ADR-0066, lot A) de kTuilesMax éléments (lot B), 10 × 20 Def, ~12 Ko :
// sa magie suit sa taille (kMagic, tab5_tuiles.cpp), un modèle d'une autre mise en page n'est
// jamais relu. Son bourrage final est un champ (`fin`) : aucun octet implicite, que
// memcmp lirait sans qu'une copie le garantisse.
constexpr size_t kModeleUtile = 8 + kPiecesMax * modele_ha::kNom + kPiecesMax * kTuilesMax * sizeof(Def);
struct Modele {
    uint32_t magic;
    uint8_t recues;       // 1 dès la première tab5_maj_tuiles : fin du mode héritage
    uint8_t pieces_n;     // nombre de pièces (« pn|N », 1 à kPiecesMax ; kPiecesDefaut sans pn)
    uint8_t reserve[2];
    char pieces[kPiecesMax][modele_ha::kNom];
    Def tuiles[kPiecesMax][kTuilesMax];
    uint8_t fin[4 - kModeleUtile % 4];  // jusqu'au multiple de 4 (alignement de magic)
};
static_assert(sizeof(Modele) == kModeleUtile + sizeof(Modele::fin), "Modele sans bourrage implicite");

struct Etat {
    char brut[modele_ha::kEtat];  // état HA ("" tant que rien n'est reçu)
    float valeur;         // luminosité, position, mesure, température ; NaN sinon
    uint32_t couleur;     // couleur propre d'une lumière (rgb_color)
    bool a_couleur;
    bool recu;
    uint8_t sens;         // volet : SENS_INCONNU, SENS_OUVRIR, SENS_FERMER (toucher du titre)
};

// Mode héritage : les emplacements 3.x forment la pièce 0.
struct Heritage {
    bool pc = false;
    bool pc_recu = false;
    bool tv = false;
    bool lum[3] = {};
    bool lum_recu[3] = {};
    float lum_val[3] = {NAN, NAN, NAN};  // luminosité 0-255 (arc du popup)
    char volet[modele_ha::kEtat] = "";  // dernier etat_physique (En_mouvement, Ouvert, Ferme…)
    int8_t volet_ouvert = -1;    // dernier état connu hors mouvement : 1 ouvert, 0 fermé
};

// Le mode héritage a les cinq emplacements de la 3.x (PC/TV, volet, trois lumières), jamais
// plus : ses tableaux (lum[3], kHeritageLumieres) s'arrêtent à la tuile 4 (ADR-0066).
constexpr int kTuilesHeritage = 5;
// Commandes 3.x des trois lumières (tuiles 2 à 4), comme les boutons de la 3.1.
constexpr const char* kHeritageLumieres[3] = {"lumiere_1", "lumiere_2", "lumiere_3"};

// Ce qu'une tuile montre, selon son type et son état (vue_def).
struct Vue {
    const char* icone = nullptr;        // épaule gauche (32 px) ; nullptr : inchangée
    uint32_t couleur = UIColor.INACTIVE;
    const char* icone_carte = nullptr;  // carte du mode HA (70 px) ; nullptr : inchangée
    uint32_t couleur_carte = UIColor.INACTIVE;
    const char* droite = nullptr;       // épaule droite ; nullptr : masquée
    uint32_t couleur_droite = UIColor.TEXT_DIM;
    const char* nom = "";
    char ligne[40] = "";                // ligne d'état de la carte
    uint32_t couleur_ligne = UIColor.INACTIVE;
    bool agit = false;                  // un appui fait quelque chose
    bool actif = false;                 // allumé, ouvert, en lecture… (icône « on »)
};

// Minuteries d'une tuile : confirmation (option k, 3 s) et « OK » après « lancer » (1 s).
struct Minuterie {
    int r = -1;
    int t = -1;
    lv_timer_t* timer = nullptr;
};

// Fenêtre qu'ouvre un geste.
enum class Fenetre : uint8_t {
    AUCUNE,
    LUMIERE,
    VOLET,
    APPAREIL,
    TELECOMMANDE,
    CLIM,
    ENERGIE,
    LECTEUR,  // le popup Musique (ADR-0050) sur le media_player de la tuile
};

// Les gestes d'une tuile, ses options appliquées (gestes(), table kGestes).
struct Gestes {
    bool agit = false;
    const char* commande = nullptr;     // appui court ; nullptr : `fenetre`, ou le sens d'un volet
    bool roue = false;                  // appui long : la roue d'abord
    Fenetre fenetre = Fenetre::AUCUNE;  // appui long sans roue ; AUCUNE : un volet envoie l'autre sens
};

// La clim d'une tuile cli (clim_cible()) : -1, -1 et « clim » pour celle du blueprint.
struct ClimCible {
    int r = -1;
    int t = -1;
    modele_ha::CleTuile cle;
    const char* emplacement() const { return r < 0 ? "clim" : cle.s; }
};

// ─── tab5_tuiles.cpp ────────────────────────────────────────────────────────────────

extern Modele s_m;
extern Etat s_etats[kPiecesMax][kTuilesMax];
extern Heritage s_h;
extern Minuterie s_confirmation;
extern Minuterie s_ok;

void charger();
bool heritage();
// Nombre de pièces à l'écran (ADR-0066) : « pn » reçu, kPiecesDefaut en mode héritage.
int pieces_nombre();
bool tuile_presente(int r, int t);
int piece_courante();
// Les cartes des appareils sont à l'écran : mode HA, ou page de pièce de la météo (ADR-0066).
bool cartes_affichees();
bool minuterie_sur(const Minuterie& m, int r, int t);
bool vol_mouvement(const char* s);
Gestes gestes(const Def& d, bool clim_connue);
ClimCible clim_cible(const Def& d, int r, int t);
void vue_def(const Def& d, const Etat& e, int r, int t, Vue& v);
void vue(int r, int t, Vue& v);
void ui_texte_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur, const char* suffixe = nullptr);
void ui_fond(lv_obj_t* obj, uint32_t hex);
void widgets_meteo(int place, lv_obj_t*& gauche, lv_obj_t*& droite, lv_obj_t*& bouton);
// Disposition de la pièce R sur les cinq places (Tab5/socle/tab5_disposition.h, ADR-0066) :
// ses éléments présents ; `meteo` garde l'élément T sur la place T jusqu'à cinq éléments
// d'index < 5 (positions fixes), le mode HA les centre.
disposition::Disposition disposition_piece(int r, bool meteo);
void envoyer(const char* emplacement, const char* action);
void envoyer_tuile(int r, int t, const char* action);
void tuile_appui_piece(int r, int t, bool long_appui);

// ─── tab5_tuiles_popups.cpp ─────────────────────────────────────────────────────────

bool est_lumiere(int r, int t);
// Un volet du popup Volets (vol sans l'option r ni k, jamais en mode héritage).
bool est_volet(int r, int t);
bool vol_position_connue(const Etat& e);
void popup_lumiere_ouvrir(int r, int t);
void popup_volet_ouvrir(int r, int t);
void popup_appareil_ouvrir(int r, int t);
// Un état a changé : chaque popup ouvert sur cette tuile la repeint (peindre_tuile).
void popup_lumiere_etat(int r, int t);
void popup_volet_etat(int r, int t);
void popup_appareil_etat(int r, int t);
void popup_volet_etat_pousse(int r, int t);
void popups_revalider();
void popups_rejouer_theme();

// ─── tab5_tuiles_cases.cpp ──────────────────────────────────────────────────────────

// Mode HA : les cases de la place `p` (calque du mode HA, x = `x`), coupée comme `pl`, aux
// éléments de la pièce `r` ; créées à la première coupe de cette place. Les autres places
// (entières, vides) : cases_masquer.
void cases_peindre_place(int p, const disposition::Place& pl, int32_t x, int r);
void cases_masquer(int p);
// Une case `k` de la place `p` montre l'élément tRT : repeinte (un état a changé).
void case_peindre(int p, int k, int r, int t);
// Ancre de la roue d'actions rapides d'une case (son icône) ; nullptr si elle n'existe pas.
lv_obj_t* case_ancre(int p, int k);

// ─── tab5_tuiles_roue.cpp ───────────────────────────────────────────────────────────

bool roue_de_la_tuile(int r, int t);
// Roue d'actions rapides ouverte sur cette tuile : repeinte (boutons courants, moyeu, jauge).
void roue_tuile_etat(int r, int t);

}  // namespace tuiles
