/**
 * [AI-CONTEXT]
 * @file tab5_zones.cpp
 * @role Zones optionnelles (lot 5 de l'audit « ouverture », 27/09/2026) : quelles zones
 *       de l'écran masquer, et le masquage lui-même. Et, depuis le lot 6a (ADR-0019),
 *       emplacements_appliquer() : les valeurs des emplacements poussées par HA (les tuiles
 *       « tRT », les réglages de la clim « climr », ADR-0026, et les clims des tuiles
 *       « crRT » / « ceRT », ADR-0027, d'abord). Contrat et raisons dans
 *       tab5_custom.h (« Zones optionnelles ») ; échanges avec HA dans tab5-zones.yaml.
 *       Et le bandeau d'état du haut gauche (bandeau_apply_ui : une table d'icônes,
 *       BandeauIcone dans tab5_custom.h), dont l'icône de la batterie de la tablette
 *       (04/10/2026), qui ne dépend pas de HA mais de l'interrupteur « Tab5 Batterie
 *       montée ».
 * @architecture_constraint Rien ne disparaît sans réponse de HA : la tablette seule ne
 *       sait pas distinguer une entité absente d'une entité pas encore transmise. Une
 *       donnée reçue fait toujours réapparaître sa zone (zone_vue), même si HA l'a
 *       déclarée absente.
 * @ai_instruction Une zone de plus : valeur À LA FIN de l'enum Zone (tab5_custom.h : les
 *       bits des zones absentes sont gardés en NVS dans cet ordre), clé à la fin de kCles
 *       ci-dessous (même ordre), puis ses widgets dans zones_apply_ui(). Les clés
 *       sont lues par HA (package tab5_push.yaml) : ne jamais les traduire ni les renommer
 *       sans le package. tests/test_zones.py vérifie qu'elles concordent.
 */
#include "tab5_internal.h"
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

constexpr uint32_t kMagic = 0x5A4F4E31;    // « ZON1 »
constexpr uint32_t kPrefKey = 0x7A6F6E65;  // « zone »

struct Sauvegarde {
    uint32_t magic;
    uint32_t absentes;
};

uint32_t s_absentes = 0;  // zones masquées, gardées en NVS
uint32_t s_vues = 0;      // entités suivies entendues depuis le démarrage
bool s_charge = false;
// Vrai au démarrage : la première poussée des prévisions demande l'état des zones,
// même au mode démo (qui n'est pas « Home Assistant »). Réarmé à chaque connexion de HA.
bool s_demande = true;
esphome::ESPPreferenceObject s_pref;

constexpr uint32_t bit_de(Zone z) { return 1u << static_cast<int>(z); }

Zone zone_pot(int i) { return static_cast<Zone>(static_cast<int>(Zone::POT_1) + i); }
Zone zone_lumiere(int i) { return static_cast<Zone>(static_cast<int>(Zone::LUMIERE_1) + i); }

// Batterie de la tablette : dernier état reçu (tab5_custom.h, batterie_*_ui).
struct EtatBatterie {
    bool montee = false;
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

// Glyphe de la batterie : quatre paliers alignés sur les seuils de couleur de
// get_battery_color() (> 80, > 40, ≥ 20, en dessous), un éclair pendant la charge
// (le niveau, estimé d'après la tension, lit trop haut pendant la charge : un seul
// glyphe plutôt que des paliers trompeurs), un point d'interrogation sans mesure.
const char* batterie_glyphe(float niveau, bool en_charge) {
    if (std::isnan(niveau)) return "\U000F0091";   // battery-unknown
    if (en_charge) return "\U000F0084";            // battery-charging
    if (niveau > 80.0f) return "\U000F0079";       // battery
    if (niveau > 40.0f) return "\U000F12A2";       // battery-medium
    if (niveau >= 20.0f) return "\U000F12A1";      // battery-low
    return "\U000F0083";                           // battery-alert
}

void batterie_peindre() {
    lv_obj_t* const icone = g_zones_ui.bandeau[BANDEAU_BATTERIE];
    if (icone == nullptr) return;
    ui_text(icone, batterie_glyphe(s_batterie.niveau, s_batterie.en_charge));
    ui_text_color(icone, get_battery_color(s_batterie.niveau));
}

// Production solaire : le panneau seul la nuit (0 %), le panneau au soleil dès qu'il
// produit. Couleur : l'échelle de la batterie (get_battery_color, une seule source :
// > 80 vert, > 40 bleu, ≥ 20 ambre, en dessous rouge), sauf 0 % : éteint, pas une alerte.
const char* solaire_glyphe(float pourcent) {
    return pourcent > 0.0f ? "\U000F0A72"   // solar-power
                           : "\U000F0D9B";  // solar-panel
}

uint32_t solaire_couleur(float pourcent) {
    return pourcent > 0.0f ? get_battery_color(pourcent) : UIColor::INACTIVE;
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
    char tampon[16];
    const size_t l = n < sizeof(tampon) - 1 ? n : sizeof(tampon) - 1;
    std::memcpy(tampon, valeur, l);
    tampon[l] = '\0';
    char* bout = nullptr;
    float v = std::strtof(tampon, &bout);
    if (l == 0 || bout == tampon) v = NAN;
    v = tab5_fini_ou_nan(v);
    if (!std::isnan(v)) v = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    const bool visibilite = std::isnan(v) != std::isnan(s_solaire);
    s_solaire = v;
    solaire_peindre();
    if (visibilite) bandeau_apply_ui();
}

void charger() {
    if (s_charge) return;
    s_charge = true;
    s_pref = esphome::global_preferences->make_preference<Sauvegarde>(kPrefKey);
    Sauvegarde s{};
    if (s_pref.load(&s) && s.magic == kMagic) s_absentes = s.absentes;
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
    ESP_LOGI("TAB5", "Zone %s de retour (donnee recue)", kCles[static_cast<int>(z)]);
    return true;
}

bool zones_reponse_ha(const std::string& absentes) {
    charger();
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
    ESP_LOGI("TAB5", "Zones masquees : %s", zones_texte_masquees().c_str());
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
    int appliquees = 0;
    size_t debut = 0;
    while (debut < payload.size()) {
        size_t fin = payload.find(';', debut);
        if (fin == std::string::npos) fin = payload.size();
        const size_t p1 = payload.find('|', debut);
        // Tuiles de pièce (ADR-0023) : « tRT|état|valeur|couleur », quatre champs, avant
        // la table des emplacements 3.x (tab5_tuiles.cpp).
        if (p1 != std::string::npos && p1 < fin &&
            tuiles_etat_recu(payload.data() + debut, p1 - debut, payload.data() + p1 + 1, fin - p1 - 1)) {
            appliquees++;
            debut = fin + 1;
            continue;
        }
        // Réglages de la clim (ADR-0026), eux aussi avant la table 3.x, qui les ignorerait.
        if (p1 != std::string::npos && p1 < fin && p1 - debut == sizeof(kCleClimReglages) - 1 &&
            payload.compare(debut, p1 - debut, kCleClimReglages) == 0) {
            clim_reglages_recu(payload.data() + p1 + 1, fin - p1 - 1);
            appliquees++;
            debut = fin + 1;
            continue;
        }
        // Production solaire du bandeau d'état : « solaire|pourcentage ».
        if (p1 != std::string::npos && p1 < fin && p1 - debut == sizeof(kCleSolaire) - 1 &&
            payload.compare(debut, p1 - debut, kCleSolaire) == 0) {
            solaire_recu(payload.data() + p1 + 1, fin - p1 - 1);
            appliquees++;
            debut = fin + 1;
            continue;
        }
        // Clims des tuiles (ADR-0027) : « crRT|réglages » et « ceRT|état » (tab5_cards.cpp).
        if (p1 != std::string::npos && p1 < fin &&
            clim_tuile_recu(payload.data() + debut, p1 - debut, payload.data() + p1 + 1, fin - p1 - 1)) {
            appliquees++;
            debut = fin + 1;
            continue;
        }
        if (p1 != std::string::npos && p1 < fin) {
            const size_t p2 = payload.find('|', p1 + 1);
            const bool trois = (p2 != std::string::npos && p2 < fin);
            const std::string cle = payload.substr(debut, p1 - debut);
            const std::string etat = payload.substr(p1 + 1, (trois ? p2 : fin) - p1 - 1);
            const std::string valeur = trois ? payload.substr(p2 + 1, fin - p2 - 1) : std::string();
            for (size_t i = 0; i < n; i++) {
                if (cle != cibles[i].cle) continue;
                // Valeur d'abord : le on_value de l'état (lumières) lit la luminosité.
                if (cibles[i].valeur != nullptr) {
                    char* bout = nullptr;
                    float v = strtof(valeur.c_str(), &bout);
                    if (valeur.empty() || bout == valeur.c_str()) v = NAN;  // « unavailable »…
                    v = tab5_fini_ou_nan(v);  // « inf » : inconnue aussi (lot A, audit du 30/09)
                    cibles[i].valeur->publish_state(v);
                }
                if (cibles[i].texte != nullptr) cibles[i].texte->publish_state(etat);
                appliquees++;
                break;
            }
        }
        debut = fin + 1;
    }
    return appliquees;
}

void batterie_montee_ui(bool montee) {
    if (montee == s_batterie.montee) return;
    s_batterie.montee = montee;
    ESP_LOGI("TAB5", "Batterie montee : %s (icone du bandeau)", montee ? "oui" : "non");
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

    // Rangée HA / Sys / TV (haut droite) : sans TV, HA et Sys glissent d'une colonne.
    const bool sans_tv = zone_absente(Zone::TV);
    ui_hidden(u.btn_tv, sans_tv);
    ui_x(u.btn_ha, sans_tv ? 999 : 855);
    ui_x(u.btn_sys, sans_tv ? 1143 : 999);

    // Tuiles (épaules, boutons) et calque « HA » : ce sont les tuiles de la pièce de la
    // page (ADR-0023) — tuiles_appliquer_ui(), en fin de fonction ; en mode héritage,
    // elles suivent ces zones (zone_tuile_absente).

    // Popup lumière : son sélecteur liste les lumières de la pièce, à l'ouverture
    // (tab5_tuiles.cpp) ; en mode héritage, les lampes présentes.

    // Carte clim : − / consigne / + (le popup s'ouvre depuis la consigne).
    ui_hidden(u.clim_zone, zone_absente(Zone::CLIM));

    // Températures : salon masqué ; sans serre, l'icône devient une manette, la zone
    // tactile de l'arcade (btn_serre_games) reste à la même place.
    ui_hidden(u.icon_salon, zone_absente(Zone::SALON));
    ui_hidden(u.val_salon, zone_absente(Zone::SALON));
    const bool sans_serre = zone_absente(Zone::SERRE);
    ui_hidden(u.val_serre, sans_serre);
    ui_text(u.icon_serre, sans_serre ? "\U000F0297" : "\U000F002D");

    // Pots : rangée de l'accueil et popup « Mes Plantes ».
    const int n_pots = zones_pots_presents();
    ui_hidden(u.pots_row, n_pots == 0);
    ui_hidden(u.pots_zone, n_pots == 0);
    moisture_slots_refresh();
    {
        int32_t largeur = 1250;  // modal_card_w (tab5-ui-tokens.yaml)
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
