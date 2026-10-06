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
 *       montée » ; une prise quand la tension dit qu'il n'y a pas de batterie
 *       (batterie_tension_ui, discussion #278, 05/10/2026). Le même état peint la ligne
 *       « Batterie » de la console système (update_console_batterie_ui, 06/10/2026).
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
    bool montee = false;  // interrupteur « Tab5 Batterie montée »
    float niveau = NAN;
    bool en_charge = false;
    float tension = NAN;  // dernière lecture de l'INA226 (V), ligne « Batterie » de la console
    DetectionBatterie detection;  // d'après la tension (batterie_lecture, tab5_core.h)
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
    return s_batterie.detection.presence == PresenceBatterie::ABSENTE ? UIColor.TEXT_SOFT
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
    const PresenceBatterie presence = s_batterie.detection.presence;
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
    if (visibilite) {
        bandeau_apply_ui();
        ui_hidden(g_zones_ui.mini_solaire, std::isnan(s_solaire));
    }
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
        // Tuile − / + au choix (ADR-0033) : « rN|état|valeur » (tab5_reglables.cpp).
        if (p1 != std::string::npos && p1 < fin &&
            reglables_etat_recu(payload.data() + debut, p1 - debut, payload.data() + p1 + 1, fin - p1 - 1)) {
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

bool batterie_tension_ui(float tension, uint32_t maintenant_ms) {
    s_batterie.tension = tension;  // NAN si l'INA226 ne répond pas : la console n'affiche pas une vieille tension
    const PresenceBatterie avant = s_batterie.detection.presence;
    const PresenceBatterie apres = batterie_lecture(s_batterie.detection, tension, maintenant_ms);
    if (apres == avant) return false;
    ESP_LOGI("TAB5", "Batterie detectee : %s (%.2f V)",
             apres == PresenceBatterie::PRESENTE ? "oui" : "non", tension);
    batterie_peindre();
    return true;
}

bool batterie_presente() { return s_batterie.detection.presence == PresenceBatterie::PRESENTE; }

// Console système, ligne « Batterie » (discussion #278, 06/10/2026) : le texte vient de
// batterie_texte_console() (tab5_core.cpp), l'icône est celle du bandeau (même glyphe,
// même couleur), juste à gauche de la valeur. Les deux sont alignés à droite : la largeur
// du texte est mesurée avec la police du label, sans attendre la mise en page.
void update_console_batterie_ui(lv_obj_t* icone, lv_obj_t* valeur) {
    if (valeur == nullptr) return;
    const PresenceBatterie presence = s_batterie.detection.presence;
    char buf[48];
    batterie_texte_console(buf, sizeof(buf), s_batterie.montee, presence, s_batterie.niveau,
                           s_batterie.tension);
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
    // même sans TV. Leur mini icône dit que l'appui long a de quoi ouvrir : la
    // télécommande sur la manette, le popup Énergie sur « HA » (production solaire reçue).
    ui_hidden(u.mini_tv, zone_absente(Zone::TV));
    ui_hidden(u.mini_solaire, !solaire_present());

    // Tuiles (épaules, boutons) et calque « HA » : ce sont les tuiles de la pièce de la
    // page (ADR-0023) — tuiles_appliquer_ui(), en fin de fonction ; en mode héritage,
    // elles suivent ces zones (zone_tuile_absente).

    // Popup lumière : son sélecteur liste les lumières de la pièce, à l'ouverture
    // (tab5_tuiles.cpp) ; en mode héritage, les lampes présentes.

    // Carte clim : − / consigne / + (le popup s'ouvre depuis la consigne). Depuis
    // l'ADR-0033, ses − / + règlent l'appareil choisi (clim, appareils du blueprint,
    // tablette) : masquée sans clim ni appareil, comme avant sans clim (tab5_reglables.cpp).
    reglables_appliquer_ui();

    // Températures : salon masqué ; sans serre, l'icône devient une manette, la zone
    // tactile de l'arcade (btn_serre_games) reste à la même place.
    ui_hidden(u.icon_salon, zone_absente(Zone::SALON));
    ui_hidden(u.val_salon, zone_absente(Zone::SALON));
    const bool sans_serre = zone_absente(Zone::SERRE);
    ui_hidden(u.val_serre, sans_serre);
    ui_text(u.icon_serre, sans_serre ? "\U000F0297" : "\U000F002D");

    // Pots : ligne des plantes de la rangée sous l'horloge (sans pot, elle sort de la
    // rotation, ADR-0031 ; la rangée disparaît s'il n'y a rien d'autre) et popup « Mes
    // Plantes ».
    const int n_pots = zones_pots_presents();
    moisture_slots_refresh();
    rangee_appliquer_ui();
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
