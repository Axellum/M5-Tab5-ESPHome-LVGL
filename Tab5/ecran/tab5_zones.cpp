/**
 * [AI-CONTEXT]
 * @file tab5_zones.cpp
 * @role Zones optionnelles (lot 5 de l'audit « ouverture », 27/09/2026) : quelles zones
 *       de l'écran masquer, et le masquage lui-même. Et, depuis le lot 6a (ADR-0019),
 *       emplacements_appliquer() : les valeurs des emplacements poussées par HA (les tuiles
 *       « tRT », les réglages de la clim « climr », ADR-0026, et les clims des tuiles
 *       « crRT » / « ceRT », ADR-0027, d'abord ; découpage « clé|reste;… » et nombres lus par
 *       Tab5/socle/tab5_parse.cpp, lot F, testés sur PC). Contrat et raisons dans
 *       tab5_custom.h (« Zones optionnelles ») ; échanges avec HA dans tab5-zones.yaml.
 *       Et le bandeau d'état du haut gauche (bandeau_apply_ui : une table d'icônes,
 *       BandeauIcone dans tab5_custom.h), dont l'icône de la batterie de la tablette
 *       (04/10/2026), qui ne dépend pas de HA mais de l'interrupteur « Tab5 Batterie
 *       montée » ; une prise quand il n'y a pas de batterie (présence décidée chargeur coupé
 *       par tab5_batterie.h, 08/10/2026 ; discussion #278). Le même état peint la ligne
 *       « Batterie » de la console système (update_console_batterie_ui, 06/10/2026).
 *       Et les gestes de l'accueil au choix (09/10/2026, lot A, ADR-0039) : les trois zones
 *       de l'horloge (heures, minutes, date) et les trois boutons du haut, tap court et
 *       appui long, choisis dans le blueprint (clés « gestes », puis l'ancienne « appuis ») ;
 *       geste_cible() dit ce que fait un geste, le script tab5_geste
 *       (tab5-navigation.yaml) le fait ; boutons_haut_apply_ui() peint les icônes.
 *       Et la clé « defil » (09/10/2026, lot 3, ADR-0041 : défilement auto / fixe de la
 *       rangée, du panneau Ok Nabu et de la tuile − / +), routée vers tab5_rangee.cpp, et la
 *       clé « gauche » (10/10/2026, ADR-0051 : contenu de la zone à gauche de l'horloge),
 *       routée vers tab5_zone_gauche.cpp.
 * @architecture_constraint Rien ne disparaît sans réponse de HA : la tablette seule ne
 *       sait pas distinguer une entité absente d'une entité pas encore transmise. Une
 *       donnée reçue fait toujours réapparaître sa zone (zone_vue), même si HA l'a
 *       déclarée absente.
 * @ai_instruction Une zone de plus : valeur À LA FIN de l'enum Zone (tab5_custom.h : les
 *       bits des zones absentes sont gardés en NVS dans cet ordre), clé à la fin de kCles
 *       ci-dessous (même ordre), puis ses widgets dans zones_apply_ui(). Les clés
 *       sont lues par HA (package tab5_push.yaml) : ne jamais les traduire ni les renommer
 *       sans le package. tests/test_zones.py vérifie qu'elles concordent.
 *       Un code de geste de plus (écran ou action) : à la FIN de kCodesGestes (la NVS garde
 *       son index), son glyphe dans code_glyphe() (mdi_font_26 et mdi_font_70, règle 9),
 *       son cas dans le script tab5_geste s'il est une action, et dans le blueprint
 *       (codes_gestes, options &codes_gestes) : tests/test_gestes.py compare.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

ZonesUI g_zones_ui;

namespace {

constexpr int kNbZones = static_cast<int>(Zone::COUNT);

// Clés échangées avec HA (événement esphome.tab5_zones, action tab5_maj_zones), dans
// l'ordre de l'enum Zone.
constexpr const char* kCles[kNbZones] = {
    "lumiere_1", "lumiere_2", "lumiere_3", "pc", "tv", "telephone", "salon", "serre",
    "pot_1", "pot_2", "pot_3", "pot_4", "pot_5", "clim", "volet", "planning", "discussion",
};

// Réglages de la clim dans tab5_maj_emplacements (ADR-0026) : « climr|min|max|pas|unité|
// capacités|nom ». Lue par le blueprint : ne pas la renommer sans lui (tests/test_clim.py).
constexpr char kCleClimReglages[] = "climr";

// Production solaire du bandeau d'état (04/10/2026) dans tab5_maj_emplacements :
// « solaire|pourcentage » (0 à 100 de la puissance crête ; « nan » ou vide = aucune
// valeur, l'icône disparaît). Calculée et poussée par le blueprint (section « Énergie »,
// puissance crête) ; une clé et pas une action, comme climr (ADR-0026) : un firmware
// plus ancien l'ignore. Ne pas la renommer sans le blueprint (tests/test_solaire.py).
constexpr char kCleSolaire[] = "solaire";

// Gestes de l'accueil (09/10/2026, lot A, ADR-0039) dans tab5_maj_emplacements :
// « gestes|c1|…|c12 », un code par geste dans l'ordre de l'enum Geste (heures court, heures
// long, minutes court, minutes long, date court, date long, maison court, maison long,
// engrenage court, engrenage long, manette court, manette long), « auto » ou un code de
// kCodesGestes. Poussée par le blueprint (section « Horloge et boutons du haut ») avec tous
// les états. Une clé et pas une variable de service : un firmware plus ancien l'ignore.
// Codes lus par le blueprint : ni traduits ni renommés sans lui (tests/test_gestes.py).
constexpr char kCleGestes[] = "gestes";
// L'ancienne clé (07/10/2026) : « appuis|maison|engrenage|manette », les appuis longs des
// trois boutons. Toujours lue (blueprint d'avant le lot A) ; un payload qui porte aussi
// « gestes » la laisse de côté (gestes_fin_payload).
constexpr char kCleAppuis[] = "appuis";
// Zone à gauche de l'horloge (10/10/2026, ADR-0051) : « gauche|départ|c1|c2|… », lue par
// zone_gauche_lire() (Tab5/socle/tab5_parse.cpp) et gardée par tab5_zone_gauche.cpp.
// Poussée par le blueprint avec les gestes ; un firmware plus ancien l'ignore. Codes lus par
// le blueprint : ni traduits ni renommés sans lui (tests/test_zone_gauche.py).
constexpr char kCleZoneGauche[] = "gauche";
// Défilement au choix (09/10/2026, lot 3, ADR-0041) : « defil|rangée|nabu|clim|secondes »,
// lu par defilement_recu() (tab5_rangee.cpp). Poussée par le blueprint avec les gestes ; un
// payload de gestes sans elle (blueprint d'avant le lot 3) remet les défauts
// (gestes_fin_payload). Valeurs lues par le blueprint : ni traduites ni renommées sans lui
// (tests/test_nabu.py).
constexpr char kCleDefilement[] = "defil";
// Télécommandes du popup (10/10/2026, ADR-0056) : « telecommandes|écran|nom|… », lue par
// telecommandes_lire() (Tab5/socle/tab5_parse.cpp) et gardée par tab5_telecommande.cpp.
// Poussée par le blueprint avec les gestes ; un firmware plus ancien l'ignore. Codes lus par
// le blueprint : ni traduits ni renommés sans lui (tests/test_telecommandes.py).
constexpr char kCleTelecommandes[] = "telecommandes";
struct CodeGeste {
    const char* code;
    Ecran ecran;  // l'écran ouvert (action ECRAN), AUCUN sinon
    GesteAction action;
};
// L'INDEX d'un code dans cette table est ce que garde la NVS (SauvegardeAppuis,
// SauvegardeGestes) : un code de plus s'ajoute à la FIN, aucun ne se retire ni ne se
// déplace.
constexpr CodeGeste kCodesGestes[] = {
    {"rien", Ecran::AUCUN, GesteAction::RIEN},
    {"assistant", Ecran::ASSISTANT, GesteAction::ECRAN},
    {"calendrier", Ecran::CALENDRIER, GesteAction::ECRAN},
    {"reveil", Ecran::REVEIL, GesteAction::ECRAN},
    {"clim", Ecran::CLIM, GesteAction::ECRAN},
    {"plantes", Ecran::PLANTES, GesteAction::ECRAN},
    {"tv", Ecran::TV, GesteAction::ECRAN},
    {"console", Ecran::CONSOLE, GesteAction::ECRAN},
    {"energie", Ecran::ENERGIE, GesteAction::ECRAN},
    {"reglages", Ecran::REGLAGES, GesteAction::ECRAN},
    {"alertes", Ecran::ALERTES, GesteAction::ECRAN},
    {"arcade", Ecran::ARCADE, GesteAction::ECRAN},
    // Popup Maison (ADR-0037), 07/10/2026 : ajouté à la fin (NVS).
    {"maison", Ecran::MAISON, GesteAction::ECRAN},
    // Actions de l'accueil (09/10/2026, lot A) : ajoutées à la fin (NVS).
    {"mode_domo", Ecran::AUCUN, GesteAction::MODE_DOMO},
    {"appareil_suivant", Ecran::AUCUN, GesteAction::APPAREIL_SUIVANT},
    {"rangee_suivante", Ecran::AUCUN, GesteAction::RANGEE_SUIVANTE},
    {"ecoute", Ecran::AUCUN, GesteAction::ECOUTE},
    // Panneau Ok Nabu à lignes (09/10/2026, lot 3, ADR-0041) : ajouté à la fin (NVS, index 17).
    {"nabu_suivant", Ecran::AUCUN, GesteAction::NABU_SUIVANTE},
    // Roue de navigation (09/10/2026, ADR-0042) : ses trois écrans nouveaux et la roue
    // elle-même, ajoutés à la fin (NVS).
    {"lumieres", Ecran::LUMIERES, GesteAction::ECRAN},
    {"volet", Ecran::VOLET, GesteAction::ECRAN},
    {"temperature", Ecran::TEMPERATURE, GesteAction::ECRAN},
    {"roue", Ecran::AUCUN, GesteAction::ROUE},
    // Popup Météo (09/10/2026, ADR-0043) : ajouté à la fin (NVS, index 22).
    {"meteo", Ecran::METEO, GesteAction::ECRAN},
    // Zone à gauche de l'horloge au choix (10/10/2026, ADR-0051) : ajouté à la fin (NVS, index 23).
    {"zone_gauche_suivante", Ecran::AUCUN, GesteAction::ZONE_GAUCHE_SUIVANTE},
    // Lecteur de musique (10/10/2026, ADR-0050) : ajouté à la fin (NVS, index 24).
    {"musique", Ecran::MUSIQUE, GesteAction::ECRAN},
    // Popup Caméras (09/10/2026, ADR-0049) : ajouté à la fin (NVS, index 25).
    {"cameras", Ecran::CAMERAS, GesteAction::ECRAN},
    // Capteurs suivis (10/10/2026, ADR-0054) : ajouté à la fin (NVS, index 26).
    {"suivi", Ecran::SUIVI, GesteAction::ECRAN},
    // Froid : réfrigérateurs et congélateurs (10/10/2026, ADR-0055) : ajouté à la fin (NVS, index 27).
    {"froid", Ecran::FROID, GesteAction::ECRAN},
    // Serveur IA : un serveur de LLM local (10/10/2026, ADR-0059) : ajouté à la fin (NVS, index 28).
    {"serveur_ia", Ecran::SERVEUR_IA, GesteAction::ECRAN},
};
constexpr int kNbCodes = static_cast<int>(sizeof(kCodesGestes) / sizeof(kCodesGestes[0]));
constexpr int8_t kAuto = -1;
// « auto » d'un appui long de bouton sans choix dans la clé appuis : l'écran d'avant le
// choix (06/10/2026), par bouton (ordre de BoutonHaut).
constexpr Ecran kEcranAuto[BOUTON_HAUT_NB] = {Ecran::ENERGIE, Ecran::CONSOLE, Ecran::TV};
// « auto » de chaque geste (ordre de Geste) : le comportement d'avant le choix, un code de
// kCodesGestes. nullptr : l'appui long d'un bouton, qui suit la clé appuis (puis kEcranAuto).
// Tap des heures : la ligne suivante du panneau Ok Nabu depuis le lot 3 (« rien » avant) ;
// avec le panneau d'origine (l'écoute seule, aucune ligne du blueprint), il ne fait rien.
constexpr const char* kGestesAuto[GESTE_NB] = {
    "nabu_suivant",
    "reveil",  // heures
    "appareil_suivant",
    "reveil",  // minutes
    "rangee_suivante",
    "calendrier",  // date
    "mode_domo",
    nullptr,  // maison
    "reglages",
    nullptr,  // engrenage (Réglages : page Écran, tab5_modal_registry_init)
    "arcade",
    nullptr,  // manette
};
constexpr uint32_t kMagic = 0x5A4F4E31;    // « ZON1 »
constexpr uint32_t kPrefKey = 0x7A6F6E65;  // « zone »
constexpr uint32_t kMagicAppuis = 0x41505031;    // « APP1 »
constexpr uint32_t kPrefKeyAppuis = 0x61707569;  // « apui »
constexpr uint32_t kMagicGestes = 0x47535431;    // « GST1 »
constexpr uint32_t kPrefKeyGestes = 0x67657374;  // « gest »

struct Sauvegarde {
    uint32_t magic;
    uint32_t absentes;
};

// Choix des appuis longs (clé appuis), gardés en NVS : la mini icône est juste dès le
// démarrage, avant que HA les repousse. Par bouton : l'index du code dans kCodesGestes
// (stable même si l'enum Ecran gagne une valeur avant ARCADE), -1 pour « auto ». Taille
// inchangée depuis le 07/10/2026 (magic APP1).
struct SauvegardeAppuis {
    uint32_t magic;
    int8_t code[BOUTON_HAUT_NB];
};
// Choix des 12 gestes (clé gestes), même principe : index dans kCodesGestes, -1 « auto ».
// Une taille de plus (un geste ajouté) = un nouveau magic.
struct SauvegardeGestes {
    uint32_t magic;
    int8_t code[GESTE_NB];
};

uint32_t s_absentes = 0;  // zones masquées, gardées en NVS
uint32_t s_vues = 0;      // entités suivies entendues depuis le démarrage
bool s_charge = false;
// Vrai au démarrage : la première poussée des prévisions demande l'état des zones,
// même au mode démo (qui n'est pas « Home Assistant »). Réarmé à chaque connexion de HA.
bool s_demande = true;
esphome::ESPPreferenceObject s_pref;
// Appui long de chaque bouton (BoutonHaut) d'après la clé appuis : kAuto, ou un index de
// kCodesGestes.
int8_t s_appuis[BOUTON_HAUT_NB] = {kAuto, kAuto, kAuto};
esphome::ESPPreferenceObject s_pref_appuis;
// Chaque geste (Geste) d'après la clé gestes : kAuto, ou un index de kCodesGestes.
static_assert(GESTE_NB == 12, "un geste de plus : son « auto » (kGestesAuto), le blueprint, un nouveau magic");
int8_t s_gestes[GESTE_NB] = {kAuto, kAuto, kAuto, kAuto, kAuto, kAuto,
                             kAuto, kAuto, kAuto, kAuto, kAuto, kAuto};
esphome::ESPPreferenceObject s_pref_gestes;
// Clés vues dans le payload en cours (emplacements_appliquer, gestes_fin_payload).
bool s_appuis_vue = false;
bool s_gestes_vue = false;
bool s_defil_vue = false;

constexpr uint32_t bit_de(Zone z) { return 1u << static_cast<int>(z); }

Zone zone_pot(int i) { return static_cast<Zone>(static_cast<int>(Zone::POT_1) + i); }
Zone zone_lumiere(int i) { return static_cast<Zone>(static_cast<int>(Zone::LUMIERE_1) + i); }

// Batterie de la tablette : dernier état reçu (tab5_custom.h, batterie_*_ui).
// Présence, tension et consommation : tab5_batterie.cpp (batterie_presence()…).
struct EtatBatterie {
    bool montee = false;  // interrupteur « Tab5 Batterie montée »
    float niveau = NAN;
    bool en_charge = false;
};
EtatBatterie s_batterie;

// Production solaire, en % de la puissance crête (clé solaire) ; NAN = aucune valeur
// reçue, ou pas de solaire chez l'utilisateur : l'icône est masquée (ADR-0018).
float s_solaire = NAN;

// Une icône du bandeau est-elle masquée ? Une icône de plus qui peut disparaître :
// son cas ici (les autres restent toujours affichées).
bool bandeau_masquee(BandeauIcone i) {
    switch (i) {
        case BANDEAU_PC: return zone_absente(Zone::PC);
        case BANDEAU_TELEPHONE: return zone_absente(Zone::TELEPHONE);
        case BANDEAU_SOLAIRE: return std::isnan(s_solaire);
        case BANDEAU_BATTERIE: return !s_batterie.montee;
        default: return false;
    }
}

// Bandeau d'état (haut gauche) : les icônes visibles se suivent au pas de 35 px.
void bandeau_apply_ui() {
    int32_t x = 10;
    for (int i = 0; i < BANDEAU_NB; i++) {
        lv_obj_t* const icone = g_zones_ui.bandeau[i];
        if (icone == nullptr) continue;  // avant tab5_zones_apply (setup)
        const bool masquee = bandeau_masquee(static_cast<BandeauIcone>(i));
        ui_hidden(icone, masquee);
        if (masquee) continue;
        ui_x(icone, x);
        x += 35;
    }
}

// Glyphe de la batterie : une prise quand la tension dit qu'il n'y a pas de batterie
// (discussion #278, 05/10/2026 : la tablette vit sur l'USB ; power-plug plein, plus
// lisible à 26 px en bpp 1 que le trident usb ou la prise usb-c), sinon quatre paliers
// alignés sur les seuils de couleur de get_battery_color() (> 80, > 40, ≥ 20, en
// dessous), un éclair pendant la charge (le niveau, estimé d'après la tension, lit trop
// haut pendant la charge : un seul glyphe plutôt que des paliers trompeurs), un point
// d'interrogation sans mesure.
const char* batterie_glyphe(PresenceBatterie presence, float niveau, bool en_charge) {
    if (presence == PresenceBatterie::ABSENTE) return "\U000F06A5";  // power-plug
    if (std::isnan(niveau)) return "\U000F0091";   // battery-unknown
    if (en_charge) return "\U000F0084";            // battery-charging
    if (niveau > 80.0f) return "\U000F0079";       // battery
    if (niveau > 40.0f) return "\U000F12A2";       // battery-medium
    if (niveau >= 20.0f) return "\U000F12A1";      // battery-low
    return "\U000F0083";                           // battery-alert
}

// Couleur de l'icône batterie, une seule source pour le bandeau et la console. La prise
// n'est ni une alerte ni un niveau : couleur du texte du thème. Relue dans la palette
// active à chaque peinture (zones_rejouer_theme, ADR-0029).
uint32_t batterie_couleur() {
    return batterie_presence() == PresenceBatterie::ABSENTE ? UIColor.TEXT_SOFT
                                                             : get_battery_color(s_batterie.niveau);
}

// Icône de la ligne « Batterie » de la console système, retenue par
// update_console_batterie_ui() pour que zones_rejouer_theme() la repeigne : sa couleur est
// posée en style local, un changement de thème ne la touche pas sinon (rendu « clair »,
// bascule à chaud contre démarrage à froid, 06/10/2026). Le widget vit autant que l'écran.
lv_obj_t* s_console_batterie_icone = nullptr;

void batterie_peindre() {
    lv_obj_t* const icone = g_zones_ui.bandeau[BANDEAU_BATTERIE];
    if (icone == nullptr) return;
    const PresenceBatterie presence = batterie_presence();
    ui_text(icone, batterie_glyphe(presence, s_batterie.niveau, s_batterie.en_charge));
    ui_text_color(icone, batterie_couleur());
}

// Production solaire : le panneau seul à tous les paliers, c'est la couleur qui donne la
// production. Le soleil sur le panneau (solar-power, puis solar-power-variant) se lisait en
// morceaux à 26 px en bpp 1 (captures comparées, choix d'Axel du 04/10/2026). Couleur :
// l'échelle de la batterie (get_battery_color, une seule source : > 80 vert, > 40 bleu,
// ≥ 20 ambre, en dessous rouge), sauf 0 % : gris éteint, pas une alerte.
const char* solaire_glyphe(float /*pourcent*/) {
    return "\U000F0D9B";  // solar-panel
}

uint32_t solaire_couleur(float pourcent) {
    return pourcent > 0.0f ? get_battery_color(pourcent) : UIColor.INACTIVE;
}

void solaire_peindre() {
    lv_obj_t* const icone = g_zones_ui.bandeau[BANDEAU_SOLAIRE];
    if (icone == nullptr || std::isnan(s_solaire)) return;
    ui_text(icone, solaire_glyphe(s_solaire));
    ui_text_color(icone, solaire_couleur(s_solaire));
}

// « solaire|pourcentage » : borné à 0-100, illisible = aucune valeur. Montrer ou cacher
// l'icône resserre le bandeau ; sinon, seul son glyphe et sa couleur changent.
void solaire_recu(const char* valeur, size_t n) {
    const float v = solaire_pourcent(valeur, n);  // Tab5/socle/tab5_parse.cpp (lot F)
    const bool visibilite = std::isnan(v) != std::isnan(s_solaire);
    s_solaire = v;
    solaire_peindre();
    if (visibilite) {
        bandeau_apply_ui();
        boutons_haut_apply_ui();  // Énergie disponible ou non pour un appui long
    }
}

void charger() {
    if (s_charge) return;
    s_charge = true;
    s_pref = esphome::global_preferences->make_preference<Sauvegarde>(kPrefKey);
    Sauvegarde s{};
    if (s_pref.load(&s) && s.magic == kMagic) s_absentes = s.absentes;
    s_pref_appuis = esphome::global_preferences->make_preference<SauvegardeAppuis>(kPrefKeyAppuis);
    SauvegardeAppuis a{};
    if (s_pref_appuis.load(&a) && a.magic == kMagicAppuis) {
        for (int b = 0; b < BOUTON_HAUT_NB; b++) {
            const int c = a.code[b];
            s_appuis[b] = (c >= 0 && c < kNbCodes) ? static_cast<int8_t>(c) : kAuto;
        }
    }
    s_pref_gestes = esphome::global_preferences->make_preference<SauvegardeGestes>(kPrefKeyGestes);
    SauvegardeGestes g{};
    if (s_pref_gestes.load(&g) && g.magic == kMagicGestes) {
        for (int i = 0; i < GESTE_NB; i++) {
            const int c = g.code[i];
            s_gestes[i] = (c >= 0 && c < kNbCodes) ? static_cast<int8_t>(c) : kAuto;
        }
    }
}

// Un code (champ des clés appuis et gestes) : son index dans kCodesGestes, ou kAuto pour
// « auto », un champ vide ou un code inconnu (blueprint plus récent que ce firmware).
int8_t code_lu(const Champ& f) {
    for (int c = 0; c < kNbCodes; c++) {
        if (champ_est(f, kCodesGestes[c].code)) return static_cast<int8_t>(c);
    }
    return kAuto;
}

const char* code_nom(int8_t c) { return (c >= 0 && c < kNbCodes) ? kCodesGestes[c].code : "auto"; }

// Index d'un code écrit en toutes lettres (kGestesAuto) ; « rien » s'il n'existe pas.
int8_t code_index(const char* nom) {
    for (int c = 0; c < kNbCodes; c++) {
        if (std::strcmp(kCodesGestes[c].code, nom) == 0) return static_cast<int8_t>(c);
    }
    return 0;
}

// Champs « a|b|c… » dans `choix` (`nb` au plus) : un champ manquant ou inconnu vaut
// « auto », les champs en trop sont ignorés (blueprint plus récent : gestes ajoutés).
void codes_lire(const char* valeur, size_t n, int8_t* choix, int nb) {
    Champ f[GESTE_NB];
    const int k = champs_decouper(valeur, n, '|', f, nb);
    for (int i = 0; i < nb; i++) choix[i] = i < k ? code_lu(f[i]) : kAuto;
}

// « appuis|maison|engrenage|manette ». Gardé en NVS et repeint seulement s'il change
// (poussé à chaque connexion).
void appuis_recu(const char* valeur, size_t n) {
    charger();
    s_appuis_vue = true;
    int8_t choix[BOUTON_HAUT_NB];
    codes_lire(valeur, n, choix, BOUTON_HAUT_NB);
    if (std::memcmp(choix, s_appuis, sizeof(choix)) == 0) return;
    std::memcpy(s_appuis, choix, sizeof(choix));
    SauvegardeAppuis a;
    std::memset(&a, 0, sizeof(a));  // octet de bourrage compris : rien d'indéterminé en NVS
    a.magic = kMagicAppuis;
    std::memcpy(a.code, s_appuis, sizeof(a.code));
    s_pref_appuis.save(&a);
    ESP_LOGI("tab5.zones", "Appuis longs : maison %s, engrenage %s, manette %s", code_nom(s_appuis[BOUTON_MAISON]),
             code_nom(s_appuis[BOUTON_ENGRENAGE]), code_nom(s_appuis[BOUTON_MANETTE]));
    boutons_haut_apply_ui();
}

void gestes_garder(const int8_t* choix) {
    if (std::memcmp(choix, s_gestes, sizeof(s_gestes)) == 0) return;
    std::memcpy(s_gestes, choix, sizeof(s_gestes));
    SauvegardeGestes g;
    std::memset(&g, 0, sizeof(g));  // bourrage compris : rien d'indéterminé en NVS
    g.magic = kMagicGestes;
    std::memcpy(g.code, s_gestes, sizeof(g.code));
    s_pref_gestes.save(&g);
    ESP_LOGI("tab5.zones",
             "Gestes : heures %s/%s, minutes %s/%s, date %s/%s, maison %s/%s, engrenage %s/%s, "
             "manette %s/%s",
             code_nom(s_gestes[0]), code_nom(s_gestes[1]), code_nom(s_gestes[2]),
             code_nom(s_gestes[3]), code_nom(s_gestes[4]), code_nom(s_gestes[5]), code_nom(s_gestes[6]),
             code_nom(s_gestes[7]), code_nom(s_gestes[8]), code_nom(s_gestes[9]), code_nom(s_gestes[10]),
             code_nom(s_gestes[11]));
    boutons_haut_apply_ui();
}

// « gestes|c1|…|c12 » (ordre de Geste).
void gestes_recu(const char* valeur, size_t n) {
    charger();
    s_gestes_vue = true;
    int8_t choix[GESTE_NB];
    codes_lire(valeur, n, choix, GESTE_NB);
    gestes_garder(choix);
}

// Fin d'un payload : la clé appuis sans la clé gestes vient d'un blueprint d'avant le lot A,
// qui ne sait rien des gestes. Ses appuis longs comptent alors seuls : les gestes gardés
// d'un blueprint plus récent repartent en « auto » (« gestes remplace appuis quand elle
// est présente »).
// Même règle pour le défilement (lot 3) : un payload qui porte les gestes (ou les appuis)
// sans la clé defil vient d'un blueprint qui ne la connaît pas ; ses zones reprennent leurs
// défauts (rangée auto, Ok Nabu et tuile − / + fixes).
void gestes_fin_payload() {
    if (s_appuis_vue && !s_gestes_vue) {
        int8_t aucun[GESTE_NB];
        std::memset(aucun, kAuto, sizeof(aucun));
        gestes_garder(aucun);
    }
    if ((s_appuis_vue || s_gestes_vue) && !s_defil_vue) defilement_defaut();
    s_appuis_vue = s_gestes_vue = s_defil_vue = false;
}

bool est_bouton(int g) { return g >= GESTE_MAISON_COURT && g < GESTE_NB; }
bool est_long(int g) { return (g % 2) == 1; }
BoutonHaut bouton_de(int g) { return static_cast<BoutonHaut>((g - GESTE_MAISON_COURT) / 2); }

// Code effectif du geste g : son choix dans la clé gestes ; « auto » : pour l'appui long
// d'un bouton, son choix dans la clé appuis (kAuto s'il n'en a pas : kEcranAuto), sinon
// l'« auto » du geste (kGestesAuto).
int8_t code_effectif(int g) {
    if (s_gestes[g] != kAuto) return s_gestes[g];
    if (est_bouton(g) && est_long(g)) return s_appuis[bouton_de(g)];
    return code_index(kGestesAuto[g]);
}

// Glyphe d'un code (index de kCodesGestes), nullptr pour « rien ». Le même dans les deux
// polices où il s'affiche : mini icône d'un bouton (mdi_font_26, ce que fait l'appui long)
// et icône centrale (mdi_font_70, ce que fait le tap quand il n'est pas « auto »). Un
// écran : le glyphe de l'en-tête de son popup (tests/test_appuis.py), sauf la console
// (son en-tête gardait le flocon de l'ancien bouton, qui se lirait « clim ») et l'Arcade
// (sans en-tête : la manette). Une action : son icône (tests/test_gestes.py).
const char* code_glyphe(int8_t c) {
    if (c < 0 || c >= kNbCodes) return nullptr;
    const CodeGeste& k = kCodesGestes[c];
    switch (k.action) {
        case GesteAction::MODE_DOMO: return "\U000F07D0";         // home-assistant (le bouton maison)
        case GesteAction::APPAREIL_SUIVANT: return "\U000F14C9";  // plus-minus-variant (tuile − / +)
        case GesteAction::RANGEE_SUIVANTE: return "\U000F0729";   // view-sequential (rangée sous l'horloge)
        case GesteAction::ECOUTE: return "\U000F07C5";            // ear-hearing (Ok Nabu)
        case GesteAction::NABU_SUIVANTE: return "\U000F050A";     // microphone-message (panneau Ok Nabu)
        case GesteAction::ROUE: return "\U000F1382";              // compass-rose (moyeu de la roue)
        case GesteAction::ZONE_GAUCHE_SUIVANTE: return "\U000F056C";  // view-carousel (contenus de la zone gauche)
        case GesteAction::ECRAN: break;
        default: return nullptr;
    }
    switch (k.ecran) {
        case Ecran::ASSISTANT: return "\U000F036C";   // microphone
        case Ecran::CALENDRIER: return "\U000F0E17";  // calendar-month
        case Ecran::REVEIL: return "\U000F0020";      // alarm
        case Ecran::CLIM: return "\U000F0717";        // snowflake
        case Ecran::PLANTES: return "\U000F024A";     // flower
        case Ecran::TV: return "\U000F07C0";          // desktop-classic
        case Ecran::CONSOLE: return "\U000F018D";     // console
        case Ecran::ENERGIE: return "\U000F0A72";     // solar-power
        case Ecran::REGLAGES: return "\U000F0493";    // cog
        case Ecran::ALERTES: return "\U000F0E81";     // bell-alert-outline
        case Ecran::MAISON: return "\U000F02DC";      // home
        case Ecran::LUMIERES: return "\U000F0335";    // lightbulb
        case Ecran::VOLET: return "\U000F111E";       // window-shutter-open
        case Ecran::TEMPERATURE: return "\U000F050F"; // thermometer
        case Ecran::METEO: return "\U000F0595";       // weather-partly-cloudy (titre du popup Météo)
        case Ecran::MUSIQUE: return "\U000F075A";     // music (titre du popup Musique)
        case Ecran::CAMERAS: return "\U000F07AE";     // cctv (titre du popup Caméras)
        case Ecran::SUIVI: return "\U000F012A";       // chart-line (titre du popup Suivi)
        case Ecran::FROID: return "\U000F0290";       // fridge (titre du popup Froid)
        case Ecran::SERVEUR_IA: return "\U000F048B";  // server (titre du popup Serveur IA)
        case Ecran::ARCADE: return "\U000F0297";      // gamepad-variant
        default: return nullptr;
    }
}

// Le code fait-il quelque chose dans cette maison ? Un écran absent (ecran_disponible) ou
// « rien » : non.
bool code_actif(int8_t c) {
    if (c < 0 || c >= kNbCodes) return false;
    const CodeGeste& k = kCodesGestes[c];
    if (k.action == GesteAction::RIEN) return false;
    return k.action != GesteAction::ECRAN || ecran_disponible(k.ecran);
}

// Glyphe de la mini icône du bouton b (mdi_font_26) : ce que fait son appui long,
// nullptr = masquée (rien, ou un écran absent). « auto » sans choix : celles du 06/10/2026
// (panneau solaire, écran de la télécommande, rien sur l'engrenage).
const char* mini_glyphe(BoutonHaut b) {
    const int8_t c = code_effectif(geste_bouton(b, true));
    if (c == kAuto) {
        if (!ecran_disponible(kEcranAuto[b])) return nullptr;
        switch (b) {
            case BOUTON_MAISON: return "\U000F0D9B";   // solar-panel (icône du bandeau d'état)
            case BOUTON_MANETTE: return "\U000F07C0";  // desktop-classic (télécommande TV)
            default: return nullptr;
        }
    }
    return code_actif(c) ? code_glyphe(c) : nullptr;
}

// Glyphe de l'icône centrale du bouton b (mdi_font_70) : celle d'origine (tab5-lvgl.yaml)
// tant que son tap est « auto » ou ne fait rien ; sinon celle de ce que fait le tap.
const char* tap_glyphe(BoutonHaut b) {
    const int8_t c = s_gestes[geste_bouton(b, false)];
    const char* g = (c != kAuto && code_actif(c)) ? code_glyphe(c) : nullptr;
    if (g != nullptr) return g;
    switch (b) {
        case BOUTON_MAISON: return "\U000F07D0";     // home-assistant
        case BOUTON_ENGRENAGE: return "\U000F0493";  // cog
        default: return "\U000F0297";                // gamepad-variant
    }
}

void sauver() {
    Sauvegarde s{kMagic, s_absentes};
    s_pref.save(&s);
}

}  // namespace

bool zone_absente(Zone z) {
    charger();
    return (s_absentes & bit_de(z)) != 0;
}

bool zone_vue(Zone z) {
    charger();
    const uint32_t b = bit_de(z);
    s_vues |= b;
    if ((s_absentes & b) == 0) return false;
    s_absentes &= ~b;
    sauver();
    ESP_LOGI("tab5.zones", "Zone %s de retour (donnee recue)", kCles[static_cast<int>(z)]);
    return true;
}

bool zones_reponse_ha(const std::string& absentes) {
    charger();
    if (payload_trop_long("tab5.zones", absentes.size())) return false;
    uint32_t nouv = 0;
    size_t debut = 0;
    while (debut <= absentes.size()) {
        size_t fin = absentes.find(',', debut);
        if (fin == std::string::npos) fin = absentes.size();
        size_t a = debut, b = fin;
        while (a < b && absentes[a] == ' ') a++;
        while (b > a && absentes[b - 1] == ' ') b--;
        for (int i = 0; i < kNbZones && a < b; i++) {
            if (strlen(kCles[i]) == b - a && absentes.compare(a, b - a, kCles[i]) == 0) {
                nouv |= 1u << i;
                break;
            }
        }
        debut = fin + 1;
    }
    // Une entité entendue existe : HA ne peut pas l'avoir vue absente, sauf à être
    // un autre client (mode démo) ; la donnée l'emporte.
    nouv &= ~s_vues;
    if (nouv == s_absentes) return false;
    s_absentes = nouv;
    sauver();
    ESP_LOGI("tab5.zones", "Zones masquees : %s", zones_texte_masquees().c_str());
    return true;
}

bool zone_tuile_absente(int tuile) {
    switch (tuile) {
        case 0: return zone_absente(Zone::PC) && zone_absente(Zone::TV);
        case 1: return zone_absente(Zone::VOLET);
        case 2: case 3: case 4: return zone_absente(zone_lumiere(tuile - 2));
        default: return false;
    }
}

int zones_pots_presents() {
    int n = 0;
    for (int i = 0; i < 5; i++)
        if (!zone_absente(zone_pot(i))) n++;
    return n;
}

std::string zones_texte_masquees() {
    charger();
    std::string t;
    for (int i = 0; i < kNbZones; i++) {
        if ((s_absentes & (1u << i)) == 0) continue;
        if (!t.empty()) t += ", ";
        t += kCles[i];
    }
    return t.empty() ? std::string("aucune") : t;
}


int emplacements_appliquer(const std::string& payload, const EmplacementCible* cibles, size_t n) {
    if (payload_trop_long("tab5.zones", payload.size())) return 0;
    int appliquees = 0;
    size_t debut = 0;
    // Découpage « clé|reste;… » par emplacement_suivant() (Tab5/socle/tab5_parse.cpp, lot F).
    EmplacementLu e;
    while (emplacement_suivant(payload, debut, e)) {
        if (!e.a_cle) continue;
        // Tuiles de pièce (ADR-0023) : « tRT|état|valeur|couleur », quatre champs, avant
        // la table des emplacements 3.x (tab5_tuiles.cpp).
        if (tuiles_etat_recu(e.cle.p, e.cle.n, e.reste.p, e.reste.n)) {
            appliquees++;
            continue;
        }
        // Réglages de la clim (ADR-0026), eux aussi avant la table 3.x, qui les ignorerait.
        if (champ_est(e.cle, kCleClimReglages)) {
            clim_reglages_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Production solaire du bandeau d'état : « solaire|pourcentage ».
        if (champ_est(e.cle, kCleSolaire)) {
            solaire_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Appuis longs des boutons du haut : « appuis|maison|engrenage|manette ».
        if (champ_est(e.cle, kCleAppuis)) {
            appuis_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Gestes de l'horloge et des boutons du haut (lot A) : « gestes|c1|…|c12 ».
        if (champ_est(e.cle, kCleGestes)) {
            gestes_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Défilement des zones (lot 3) : « defil|rangée|nabu|clim|secondes ».
        if (champ_est(e.cle, kCleDefilement)) {
            s_defil_vue = true;
            defilement_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Zone à gauche de l'horloge (ADR-0051) : « gauche|départ|c1|… ».
        if (champ_est(e.cle, kCleZoneGauche)) {
            zone_gauche_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Télécommandes du popup (ADR-0056) : « telecommandes|écran|nom|… ».
        if (champ_est(e.cle, kCleTelecommandes)) {
            telecommandes_recu(e.reste.p, e.reste.n);
            appliquees++;
            continue;
        }
        // Clims des tuiles (ADR-0027) : « crRT|réglages » et « ceRT|état » (tab5_clim.cpp).
        if (clim_tuile_recu(e.cle.p, e.cle.n, e.reste.p, e.reste.n)) {
            appliquees++;
            continue;
        }
        // Tuile − / + au choix (ADR-0033) : « rN|état|valeur » (tab5_reglables.cpp).
        if (reglables_etat_recu(e.cle.p, e.cle.n, e.reste.p, e.reste.n)) {
            appliquees++;
            continue;
        }
        // Climat de la pièce (ADR-0040) : « pR|température|humidité|clim »
        // (tab5_piece_climat.cpp, lecture par piece_climat_lire, tab5_parse.h).
        if (piece_climat_recu(e.cle.p, e.cle.n, e.reste.p, e.reste.n)) {
            appliquees++;
            continue;
        }
        Champ c_etat, c_valeur;
        emplacement_etat_valeur(e.reste, c_etat, c_valeur);
        const std::string cle(e.cle.p, e.cle.n);
        const std::string etat(c_etat.p, c_etat.n);
        const std::string valeur(c_valeur.p, c_valeur.n);
        for (size_t i = 0; i < n; i++) {
            if (cle != cibles[i].cle) continue;
            // Valeur d'abord : le on_value de l'état (lumières) lit la luminosité.
            // « unavailable » ou « inf » : inconnue (NAN, lot A de l'audit du 30/09).
            if (cibles[i].valeur != nullptr) cibles[i].valeur->publish_state(emplacement_nombre(valeur));
            if (cibles[i].texte != nullptr) cibles[i].texte->publish_state(etat);
            appliquees++;
            break;
        }
    }
    gestes_fin_payload();
    return appliquees;
}

void batterie_montee_ui(bool montee) {
    if (montee == s_batterie.montee) return;
    s_batterie.montee = montee;
    ESP_LOGI("tab5.zones", "Batterie montee : %s (icone du bandeau)", montee ? "oui" : "non");
    batterie_peindre();
    bandeau_apply_ui();
}

void batterie_niveau_ui(float niveau) {
    s_batterie.niveau = tab5_fini_ou_nan(niveau);
    batterie_peindre();
}

void batterie_charge_ui(bool en_charge) {
    s_batterie.en_charge = en_charge;
    batterie_peindre();
}

float batterie_niveau_lu() { return s_batterie.niveau; }

bool batterie_en_charge_lue() { return s_batterie.en_charge; }

bool batterie_tension_ui(float tension, uint32_t maintenant_ms) {
    if (!chargeur_tension(tension, maintenant_ms)) return false;
    ESP_LOGI("tab5.zones", "Batterie detectee : %s (%.2f V)", batterie_presente() ? "oui" : "non", tension);
    batterie_peindre();
    return true;
}

// Console système, ligne « Batterie » (discussion #278, 06/10/2026) : le texte vient de
// batterie_texte_console() (tab5_core.cpp), l'icône est celle du bandeau (même glyphe,
// même couleur), juste à gauche de la valeur. Les deux sont alignés à droite : la largeur
// du texte est mesurée avec la police du label, sans attendre la mise en page.
void update_console_batterie_ui(lv_obj_t* icone, lv_obj_t* valeur) {
    if (valeur == nullptr) return;
    const PresenceBatterie presence = batterie_presence();
    char buf[48];
    batterie_texte_console(buf, sizeof(buf), s_batterie.montee, presence, s_batterie.niveau,
                           batterie_derniere_tension(), batterie_derniere_consommation());
    ui_text(valeur, buf);
    if (icone == nullptr) return;
    s_console_batterie_icone = icone;
    ui_hidden(icone, !s_batterie.montee);
    if (!s_batterie.montee) return;
    ui_text(icone, batterie_glyphe(presence, s_batterie.niveau, s_batterie.en_charge));
    ui_text_color(icone, batterie_couleur());
    lv_point_t taille;
    lv_text_get_size(&taille, buf, lv_obj_get_style_text_font(valeur, LV_PART_MAIN), 0, 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    // Valeur en TOP_RIGHT à x = -22 (console_sys.yaml), 10 px entre l'icône et le texte.
    ui_x(icone, -22 - taille.x - 10);
}

bool solaire_present() { return !std::isnan(s_solaire); }

bool ecran_sans_zone(Ecran e) {
    switch (e) {
        case Ecran::CLIM: return zone_absente(Zone::CLIM);
        case Ecran::PLANTES: return zones_pots_presents() == 0;
        // Sans la TV du blueprint, une autre télécommande suffit (ADR-0056).
        case Ecran::TV: return zone_absente(Zone::TV) && !telecommandes_connues();
        // ADR-0042 : aucune tuile dont le popup s'ouvre ; aucune température à l'accueil
        // (salon, serre, ou celle de la pièce affichée en mode HA, ADR-0040).
        case Ecran::LUMIERES:
        case Ecran::VOLET: return !tuiles_ecran_disponible(e);
        case Ecran::TEMPERATURE:
            return accueil_historique_cle(false) == nullptr && accueil_historique_cle(true) == nullptr;
        default: return false;
    }
}

bool ecran_disponible(Ecran e) {
    if (e == Ecran::AUCUN || e == Ecran::ACCUEIL || e >= Ecran::NB) return false;
    if (e == Ecran::ENERGIE && !solaire_present()) return false;
    return !ecran_sans_zone(e);
}

GesteCible geste_cible(int geste) {
    constexpr GesteCible kRien{GesteAction::RIEN, 0};
    if (geste < 0 || geste >= GESTE_NB) return kRien;
    charger();
    const int8_t c = code_effectif(geste);
    if (c == kAuto) {  // appui long d'un bouton, sans choix nulle part
        const Ecran e = kEcranAuto[bouton_de(geste)];
        return ecran_disponible(e) ? GesteCible{GesteAction::ECRAN, static_cast<int>(e)} : kRien;
    }
    if (!code_actif(c)) return kRien;
    const CodeGeste& k = kCodesGestes[c];
    return GesteCible{k.action, k.action == GesteAction::ECRAN ? static_cast<int>(k.ecran) : 0};
}

void boutons_haut_apply_ui() {
    charger();
    for (int b = 0; b < BOUTON_HAUT_NB; b++) {
        const BoutonHaut bouton = static_cast<BoutonHaut>(b);
        lv_obj_t* const mini = g_zones_ui.mini[b];
        if (mini != nullptr) {  // nullptr avant tab5_zones_apply (setup)
            const char* glyphe = mini_glyphe(bouton);
            if (glyphe != nullptr) ui_text(mini, glyphe);
            ui_hidden(mini, glyphe == nullptr);
        }
        if (g_zones_ui.icone[b] != nullptr) ui_text(g_zones_ui.icone[b], tap_glyphe(bouton));
    }
}

// Thèmes (ADR-0029) : icônes de la batterie (montée ; bandeau et console système) et du
// solaire (valeur reçue).
void zones_rejouer_theme() {
    if (s_batterie.montee) {
        batterie_peindre();
        if (s_console_batterie_icone != nullptr) ui_text_color(s_console_batterie_icone, batterie_couleur());
    }
    solaire_peindre();
}

void zones_nouvelle_connexion() { s_demande = true; }

bool zones_demande_a_envoyer() {
    if (!s_demande) return false;
    s_demande = false;
    return true;
}

void zones_apply_ui() {
    charger();
    const ZonesUI& u = g_zones_ui;

    // Bandeau d'état (haut gauche) : les icônes restantes se resserrent, pas de 35 px.
    // La batterie et le solaire sont peints ici aussi : leur état a pu arriver avant les
    // pointeurs.
    bandeau_apply_ui();
    batterie_peindre();
    solaire_peindre();

    // Boutons du haut (06/10/2026) : ils restent à leur place, la manette ouvre l'Arcade
    // même sans TV. Leur mini icône dit ce que fait l'appui long, quand il fait quelque
    // chose (choix du blueprint depuis le 07/10/2026 ; « auto » : la télécommande sur la
    // manette, le popup Énergie sur « HA » quand la production solaire est reçue) ; leur
    // icône centrale, ce que fait le tap quand le blueprint l'a changé (09/10/2026).
    boutons_haut_apply_ui();

    // Tuiles (épaules, boutons) et calque « HA » : ce sont les tuiles de la pièce de la
    // page (ADR-0023) — tuiles_appliquer_ui(), en fin de fonction ; en mode héritage,
    // elles suivent ces zones (zone_tuile_absente).

    // Popup lumière : son sélecteur liste les lumières de la pièce, à l'ouverture
    // (tab5_tuiles.cpp) ; en mode héritage, les lampes présentes.

    // Carte clim : − / consigne / + (le popup s'ouvre depuis la consigne). Depuis
    // l'ADR-0033, ses − / + règlent l'appareil choisi (clim, appareils du blueprint,
    // tablette) : masquée sans clim ni appareil, comme avant sans clim (tab5_reglables.cpp).
    reglables_appliquer_ui();

    // Températures : la pièce affichée en mode HA quand elle a une température déclarée,
    // sinon le salon (masqué avec sa zone) et la serre (sans elle, l'icône devient une
    // manette, la zone tactile de l'arcade reste à la même place) — tab5_piece_climat.cpp,
    // ADR-0040.
    accueil_temperatures_ui();

    // Pots : ligne des plantes de la rangée sous l'horloge (sans pot, elle sort de la
    // rotation, ADR-0031 ; la rangée disparaît s'il n'y a rien d'autre) et popup « Mes
    // Plantes ».
    const int n_pots = zones_pots_presents();
    moisture_slots_refresh();
    rangee_appliquer_ui();
    {
        int32_t largeur = kCarteL;  // modal_card_w (tab5_geometrie.h)
        for (lv_obj_t* c : u.pot_card) {
            if (c == nullptr) continue;
            const int32_t w = lv_obj_get_style_width(lv_obj_get_parent(c), LV_PART_MAIN);
            if (w > 0) largeur = w;
            break;
        }
        int32_t x = (largeur - (n_pots * 244 - 18)) / 2;
        for (int i = 0; i < 5; i++) {
            const bool absente = zone_absente(zone_pot(i));
            ui_hidden(u.pot_card[i], absente);
            if (absente || u.pot_card[i] == nullptr) continue;
            ui_x(u.pot_card[i], x);
            x += 244;
        }
    }

    // Planning : hors du rotateur de la carte centrale sans agenda de travail.
    central_planning_set_off(zone_absente(Zone::PLANNING));

    // Mode vocal : sans pipeline de discussion (« Aucun » dans HA), plus de choix entre
    // Domotique et Discussion — les deux boutons de l'accueil et ceux du popup assistant,
    // avec leur titre. Le retour au mode Domotique est fait par tab5_zones_apply.
    const bool sans_discussion = zone_absente(Zone::DISCUSSION);
    for (lv_obj_t* o : {u.btn_domo, u.btn_discu, u.assist_domo, u.assist_discu, u.assist_cerveau})
        ui_hidden(o, sans_discussion);

    // Pièces et tuiles (ADR-0023) : en mode héritage, leurs tuiles suivent ces zones ;
    // bouton « HA », cartes et titre de la pièce.
    tuiles_appliquer_ui();
}
