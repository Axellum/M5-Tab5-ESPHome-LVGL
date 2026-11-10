/**
 * Harnais libFuzzer de la lecture des payloads de Home Assistant (Tab5/socle/tab5_parse.h,
 * lot F de l'audit du 30/09/2026). Le premier octet choisit le parseur
 * ((octet - '0') modulo le nombre de parseurs : « 0… » = le premier, « : » le onzième,
 * le caractère qui suit « 9 », « ; » le douzième, « < » le treizième, « = » le quatorzième,
 * « > » le quinzième, « ? » le seizième), le reste est le
 * payload, passé comme le firmware le passe (std::string, puis c_str() ou data()/size()).
 * Les graines sont les payloads du fuzz de la tablette virtuelle
 * (tools/sanitizers/fuzz_services.py), écrites par tools/fuzz/graines.py.
 *
 * Build & run (CI, job `fuzz-parseurs` de .github/workflows/sanitizers.yml) :
 *   clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
 *       -I Tab5/socle tools/fuzz/fuzz_parse.cpp Tab5/socle/tab5_parse.cpp Tab5/socle/tab5_champs.cpp \
 *       Tab5/socle/tab5_core.cpp Tab5/socle/tab5_i18n.cpp -o fuzz_parse
 *   ./fuzz_parse -max_total_time=180 corpus/
 * Témoin positif : -DTAB5_FUZZ_TEMOIN ajoute un comportement indéfini connu (un float hors
 * des bornes de int, comme tools/sanitizers/temoin.cpp) sur un payload « TEMOIN » ; le job
 * le rejoue sur tools/fuzz/temoin.txt et exige un rapport, sinon « 0 rapport » ne prouve rien.
 */
#include "tab5_core.h"
#include "tab5_flux_mjpeg.h"
#include "tab5_parse.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Globaux que le firmware définit dans tab5_custom.cpp (tab5_core.cpp s'en sert).
DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];
int32_t cal_jours_anchor_day = -1;

namespace {

void heures(const std::string& p) {
    previsions_premier_creneau(p.c_str());
    previsions_heures_lire(p.c_str(), cal_heures_data);
}

void jours(const std::string& p) { previsions_jours_lire(p.c_str(), cal_jours_data, cal_jours_anchor_day); }

void vigilance(const std::string& p) {
    VigilanceLue v;
    vigilance_lire(p.c_str(), v);
    // Le premier champ est la phrase pluie (update_rain_phrase_ui, tab5_central.cpp).
    pluie_phrase_lire(std::string(v.champs[0]));
    vigilance_niveau(v.champs[1]);
    VigilanceActive a[kVigilanceActivesMax];
    const int n = vigilance_actives(v, a);
    for (int i = 0; i < n; i++) vigilance_niveau(a[i].niveau);
}

void alertes_ha(const std::string& p) {
    LecteurAlertesHa l(p.c_str());
    // Borné comme l'écran (4 bandeaux) puis au-delà : le lecteur doit finir seul.
    for (AlerteHaJeton j = l.suivant(); j.type != AlerteHaType::FIN; j = l.suivant()) {
        if (j.type == AlerteHaType::ALERTE) alerte_texte_lire(j.texte);
    }
}

void historique(const std::string& p) {
    AlerteHistoriqueLue e[20];
    int illisibles = 0;
    const int n = alertes_historique_lire(p.c_str(), e, 20, illisibles);
    for (int i = 0; i < n; i++) {
        const std::string texte(e[i].texte.p, e[i].texte.n);
        alerte_texte_lire(texte.c_str());
    }
}

void pluie(const std::string& p) {
    PluieBarre b[kPluieBarresMax];
    pluie_barres_lire(p.c_str(), b);
    pluie_niveau(p);
}

// Mois : le payload sert de codes, d'heures (« | ») et de détails (« ~ »), comme les trois
// variables du service ; tous les jours sont lus, comme le rendu de la grille.
void calendrier_mois(const std::string& p) {
    int y = 0, m = 0;
    calendrier_mois_lire(p, p, y, m);
    for (int day = 1; day <= 31; day++) {
        calendrier_code_jour(p, day);
        calendrier_champ(p, day - 1, '|');
        calendrier_champ(p, day - 1, '~');
    }
}

void calendrier_jour(const std::string& p) {
    int y = 0, m = 0, d = 0;
    calendrier_date_lire(p.c_str(), y, m, d);
    LecteurJourCalendrier l(p.c_str());
    int lignes = 0;
    for (CalJourLigne c = l.suivante(); c.type != CalJourType::FIN; c = l.suivante()) {
        if (c.type == CalJourType::LIGNE) lignes += (std::strcmp(c.genre, "travail") == 0) ? 1 : 0;
    }
    (void) lignes;
}

// Emplacements : chaque reste est lu comme la table 3.x (état|valeur, nombre), comme la
// production solaire et comme les clés de la clim (climr/crRT, ceRT), quelle que soit sa
// clé : le firmware les reçoit toutes par ce service. Le climat d'une pièce (« pR ») est
// lu avec sa vraie clé ; la zone à gauche de l'horloge (« gauche », ADR-0051) sur tout reste.
void emplacements(const std::string& p) {
    size_t debut = 0;
    EmplacementLu e;
    while (emplacement_suivant(p, debut, e)) {
        if (!e.a_cle) continue;
        solaire_pourcent(e.reste.p, e.reste.n);
        Champ etat, valeur;
        emplacement_etat_valeur(e.reste, etat, valeur);
        emplacement_nombre(std::string(valeur.p, valeur.n));
        ClimReglages r;
        Champ nom;
        if (clim_reglages_lire(e.reste.p, e.reste.n, r, nom) == 6) {
            const std::string copie(nom.p, nom.n);  // ce que texte_ha_copier recevrait
            (void) copie;
        }
        ClimEtat c;
        clim_etat_lire(e.reste.p, e.reste.n, c);
        PieceClimatLu pc;
        piece_climat_lire(e.cle, e.reste, pc);
        zone_gauche_lire(e.reste.p, e.reste.n);  // « gauche|… » (ADR-0051)
        TelecommandeLue t[kTelecommandesMax];  // « telecommandes|… » (ADR-0056)
        const int nt = telecommandes_lire(e.reste.p, e.reste.n, t);
        for (int i = 0; i < nt; i++) {
            const std::string nom(t[i].nom.p, t[i].nom.n);  // ce que texte_ha_copier recevrait
            (void) nom;
            telecommande_emplacement(i);
        }
        Champ pl[kPlaylistsMax];  // « playlists|… » (ADR-0068)
        const int np = playlists_lire(e.reste.p, e.reste.n, pl);
        for (int i = 0; i < np; i++) {
            const std::string nom(pl[i].p, pl[i].n);  // ce que texte_ha_copier recevrait
            (void) nom;
        }
    }
}

// Popup Température : les trois variables du service (entete, mesures, previsions)
// séparées par un saut de ligne dans le payload (tools/fuzz/graines.py) ; une variable
// absente vaut "". Chaque champ est aussi lu comme une humidité.
void temperature(const std::string& p) {
    static HistoriqueSerie s;  // ~2 Ko : hors de la pile, comme le bloc PSRAM de l'écran
    std::string v[3];
    size_t debut = 0;
    for (int i = 0; i < 3; i++) {
        const size_t saut = i < 2 ? p.find('\n', debut) : std::string::npos;
        v[i] = p.substr(debut, saut == std::string::npos ? std::string::npos : saut - debut);
        if (saut == std::string::npos) break;
        debut = saut + 1;  // saut < p.size() : debut <= p.size()
    }
    const Champ nom = historique_lire(Champ{v[0].data(), v[0].size()}, Champ{v[1].data(), v[1].size()},
                                      Champ{v[2].data(), v[2].size()}, s);
    const std::string copie(nom.p, nom.n);  // ce que texte_ha_copier recevrait
    (void) copie;
    humidite_lire(Champ{p.data(), p.size()});
}

// Lecteur de musique (ADR-0050) : les deux variables du service (lecteurs, etat) séparées
// par un saut de ligne, comme temperature() ; l'image lue comme l'écran la lit (base tirée
// du payload lui-même, comme une adresse de client API), la position avancée et écrite.
void lecteur(const std::string& p) {
    const size_t saut = p.find('\n');
    const std::string lecteurs = p.substr(0, saut);
    const std::string etat = saut == std::string::npos ? std::string() : p.substr(saut + 1);
    LecteurListeLu l[kLecteursMax];
    const int n = lecteurs_lire(Champ{lecteurs.data(), lecteurs.size()}, l);
    for (int i = 0; i < n; i++) {
        const std::string nom(l[i].nom.p, l[i].nom.n);  // ce que texte_ha_copier recevrait
        (void) nom;
    }
    LecteurEtatLu e;
    if (lecteur_etat_lire(Champ{etat.data(), etat.size()}, e)) {
        char base[64];
        ha_base_depuis_hote(lecteurs.c_str(), base, sizeof(base));
        char url[kLecteurUrlMax];
        ha_image_url(e.image, base, url, sizeof(url));
        ha_image_url(e.image, lecteurs.c_str(), url, sizeof(url));
        char t[16];
        lecteur_temps_texte(lecteur_position(e, 1.5f), t, sizeof(t));
        lecteur_temps_texte(e.duree, t, sizeof(t));
    }
    lecteur_genre_lire(Champ{p.data(), p.size()});
    lecteur_etat_code(Champ{p.data(), p.size()});
    lecteur_fonctions_lire(Champ{p.data(), p.size()});
}

// Musique de toute la maison (ADR-0067) : les deux variables de chaque service séparées
// par un saut de ligne, comme lecteur() ; ce que l'écran lit ensuite (groupes, jouer ou
// ouvrir, URL de la pochette avec la base tirée du payload).
void musique_pieces(const std::string& p) {
    const size_t saut = p.find('\n');
    const std::string pieces = p.substr(0, saut);
    const std::string contexte = saut == std::string::npos ? std::string() : p.substr(saut + 1);
    MusiquePiecesLues lu;
    const int n = musique_pieces_lire(Champ{pieces.data(), pieces.size()}, Champ{contexte.data(), contexte.size()}, lu);
    for (int i = 0; i < n; i++) {
        const std::string nom(lu.pieces[i].nom.p, lu.pieces[i].nom.n);  // ce que texte_ha_copier recevrait
        const std::string titre(lu.pieces[i].titre.p, lu.pieces[i].titre.n);
        (void) nom;
        (void) titre;
        musique_membres(lu, lu.pieces[i].index);
        for (int j = 0; j < n; j++) musique_meme_groupe(lu, lu.pieces[i].index, lu.pieces[j].index);
    }
    musique_membres(lu, lu.montre);
    const std::string suivant(lu.suivant.p, lu.suivant.n);
    (void) suivant;
    musique_fonctions_lire(Champ{p.data(), p.size()});
}

void musique_liste(const std::string& p) {
    const size_t saut = p.find('\n');
    const std::string contexte = p.substr(0, saut);
    const std::string elements = saut == std::string::npos ? std::string() : p.substr(saut + 1);
    MusiqueListeLue lu;
    const int n = musique_liste_lire(Champ{contexte.data(), contexte.size()}, Champ{elements.data(), elements.size()}, lu);
    char base[64];
    ha_base_depuis_hote(contexte.c_str(), base, sizeof(base));
    for (int i = 0; i < n; i++) {
        const MusiqueElementLu& e = lu.elements[i];
        const std::string nom(e.nom.p, e.nom.n);
        const std::string sous(e.sous.p, e.sous.n);
        const std::string ref(e.ref.p, e.ref.n);  // ce que l'événement renverrait
        (void) nom;
        (void) sous;
        (void) ref;
        musique_element_jouable(e);
        musique_element_conteneur(e);
        musique_ref_valide(e.ref, e.genre);
        char url[kLecteurUrlMax];
        ha_image_url(e.image, base, url, sizeof(url));
    }
    musique_vue_code(lu.vue);
    musique_vue_categorie(lu.vue);
    musique_vue_lire(Champ{p.data(), p.size()});
}

// Flux MJPEG de Home Assistant (ADR-0073, Tab5/socle/tab5_flux_mjpeg.h) : des octets du RÉSEAU,
// analysés comme le chargeur le fait (extraire, retirer ce qui est consommé, recommencer).
// Aucune fonction de tab5_parse.h : appelée depuis cameras() pour garder la table des parseurs.
void flux_mjpeg(const std::string& p) {
    const uint8_t* d = reinterpret_cast<const uint8_t*>(p.data());
    size_t n = p.size();
    for (int tour = 0; tour < 64 && n > 0; tour++) {
        flux::Image img;
        size_t consomme = 0;
        const flux::Etat e = flux::extraire(d, n, 4096, &img, &consomme);
        if (consomme > n) __builtin_trap();
        if (e == flux::Etat::IMAGE && (img.debut > n || img.n > n - img.debut)) __builtin_trap();
        if (e == flux::Etat::IMAGE && consomme != img.debut + img.n) __builtin_trap();
        if (consomme == 0) break;   // rien à retirer : le chargeur attendrait des octets de plus
        d += consomme;
        n -= consomme;
    }
}

// Popup Caméras : les deux variables du service (adresse, cameras) séparées par un saut de
// ligne, comme temperature(). L'URL de chaque caméra est construite avec l'adresse donnée,
// puis avec la base tirée du payload pris comme adresse de client (on_client_connected).
void cameras(const std::string& p) {
    flux_mjpeg(p);
    const size_t saut = p.find('\n');
    const std::string adresse = p.substr(0, saut);
    const std::string liste = saut == std::string::npos ? std::string() : p.substr(saut + 1);
    char base[64];
    ha_base_depuis_hote(adresse.c_str(), base, sizeof(base));
    CameraLue c[kCamerasMax];
    int pieces = 0;
    const int n = cameras_lire(Champ{liste.data(), liste.size()}, c, &pieces);
    // Invariants dont l'écran dépend (tab5_cameras.cpp indexe ses puces par piece_i) :
    // jamais plus de pièces que de caméras, chaque rang dans [0, pieces).
    if (pieces < 0 || pieces > n || (n > 0 && pieces == 0)) __builtin_trap();
    char url[kCameraUrlMax];
    for (int i = 0; i < n; i++) {
        if (c[i].piece_i < 0 || c[i].piece_i >= pieces) __builtin_trap();
        camera_url(c[i].image, adresse.c_str(), 960, 540, url, sizeof(url));
        camera_url(c[i].image, base, 480, 270, url, sizeof(url));
        camera_flux_url(c[i].image, adresse.c_str(), url, sizeof(url));
        camera_flux_url(c[i].image, base, url, sizeof(url));
        const std::string copie(c[i].nom.p, c[i].nom.n);  // ce que texte_ha_copier recevrait
        const std::string piece(c[i].piece.p, c[i].piece.n);
        (void) copie;
        (void) piece;
    }
}

// Suivi de capteurs (ADR-0054) : la variable payload seule.
void suivi(const std::string& p) {
    SuiviLu s[kSuivisMax];
    const int n = suivis_lire(Champ{p.data(), p.size()}, s);
    for (int i = 0; i < n; i++) {
        char t[48];
        suivi_nombre_texte(s[i].valeur, s[i].decimales, t, sizeof(t));
        suivi_variation_texte(s[i], t, sizeof(t));
        suivi_variation_texte(s[i], t, 4);
        suivi_sens(s[i]);
        const std::string nom(s[i].nom.p, s[i].nom.n);  // ce que texte_ha_copier recevrait
        (void) nom;
    }
    suivi_decimales(Champ{p.data(), p.size()});
}

// Froid (ADR-0055) : la variable payload seule, et le libellé codé de ses alertes.
void froid(const std::string& p) {
    FroidLu a[kFroidMax];
    const int n = froid_lire(Champ{p.data(), p.size()}, a);
    for (int i = 0; i < n; i++) {
        char t[16];
        froid_temperature_texte(a[i].valeur, t, sizeof(t));
        froid_temperature_texte(a[i].dernier.max, t, 4);
        float bas = 0, haut = 0;
        froid_echelle(a[i], bas, haut);
        const std::string nom(a[i].nom.p, a[i].nom.n);  // ce que texte_ha_copier recevrait
        (void) nom;
    }
    froid_niveau_max(a, n);
    froid_cause(Champ{p.data(), p.size()});
    FroidAlerteLu lu;
    froid_alerte_lire(p.c_str(), lu);
}

// Serveur IA (ADR-0059) : la variable payload seule, ses textes et la courbe des tokens/s.
void serveur_ia(const std::string& p) {
    ServeurIaLu s;
    if (!serveur_ia_lire(Champ{p.data(), p.size()}, s)) return;
    char t[16];
    for (float v : {s.tps, s.vram, s.vram_total, s.vram_pct, s.temperature, s.ram, s.puissance}) {
        serveur_ia_nombre_texte(v, 1, t, sizeof(t));
        serveur_ia_nombre_texte(v, 0, t, 4);
    }
    const std::string nom(s.nom.p, s.nom.n);  // ce que texte_ha_copier recevrait
    const std::string modele(s.modele.p, s.modele.n);
    (void) nom;
    (void) modele;
    ServeurIaCourbe c;
    for (int i = 0; i < kServeurIaPoints + 2; i++) c.ajouter(i % 3 ? s.tps : NAN);
    float h = 0;
    c.haut(h);
    // Actions (ADR-0060) : un bouton actif est toujours montré, aucun bit hors des trois.
    if ((s.actives & ~s.actions) != 0 || (s.actions >> kServeurIaActions) != 0) __builtin_trap();
}

// Énergie, page « Aujourd'hui » (ADR-0058) : la variable payload seule ; ses nombres
// passent aussi par le partage des flux de l'instantané (même lecture en W).
void energie_soleil(const std::string& p) {
    EnergieSoleilLu s;
    energie_soleil_lire(Champ{p.data(), p.size()}, s);
    energie_flux_calculer(s.prevu_jour, s.prevu_demain, s.n_prevu > 0 ? -s.prevu[0] : NAN,
                          s.n_clair > 0 ? -s.clair[0] : NAN);
    float v[kEnergieHeures];
    energie_kwh_liste(Champ{p.data(), p.size()}, v, kEnergieHeures);
}

// Énergie, page « Bilan » (ADR-0058) : vue, début, payload séparés par un saut de ligne,
// comme temperature() ; la production de la vue tirée de la variable debut (même liste).
void energie_bilan(const std::string& p) {
    std::string v[3];
    size_t debut = 0;
    for (int i = 0; i < 3; i++) {
        const size_t saut = i < 2 ? p.find('\n', debut) : std::string::npos;
        v[i] = p.substr(debut, saut == std::string::npos ? std::string::npos : saut - debut);
        if (saut == std::string::npos) break;
        debut = saut + 1;
    }
    const bool heures = v[0] == "heures";
    const bool mois = v[0] == "mois" || v[0] == "annee_civile";
    const int slots = heures ? 24 : (mois ? 12 : kEnergieSlotsMax);
    EnergieBilanLu b;
    energie_bilan_lire(Champ{v[2].data(), v[2].size()}, slots, b);
    float produit[kEnergieSlotsMax];
    int n = energie_kwh_liste(Champ{v[1].data(), v[1].size()}, produit, slots);
    // ADR-0062 : la production du bilan l'emporte sur la série de l'historique.
    if (b.prod_choisie) {
        n = b.n_prod;
        for (int k = 0; k < n; k++) produit[k] = b.prod[k];
    }
    EnergieBilanCreneau c[kEnergieSlotsMax];
    float net[kEnergieSlotsMax], cumul[kEnergieSlotsMax];
    for (int k = 0; k < n; k++) {
        c[k] = energie_bilan_creneau(produit[k], b, k);
        net[k] = c[k].net;
    }
    energie_bilan_totaux(produit, n, b);
    // Date tirée de la vue : bornée comme energie_bilan() (tab5_energie.cpp).
    const int jour = 1 + static_cast<int>(v[1].size() % 31), m = 1 + static_cast<int>(v[2].size() % 12);
    const EnergieVue vue = heures ? EnergieVue::HEURES : (mois ? EnergieVue::MOIS : EnergieVue::JOURS);
    const int dernier = energie_cumul(net, n, vue, 2026, m, jour, cumul);
    const EnergieMois em = energie_mois(c, n, 2026, m, jour);
    energie_echelle(net, cumul, n, em.fin);
    energie_rond(dernier >= 0 ? cumul[dernier] : em.net);
    energie_pas(em.rythme);
    int a, mm, j;
    energie_jour_plus(2026, m, jour, n, a, mm, j);
    const std::string devise(b.devise.p != nullptr ? b.devise.p : "", b.devise.n);  // texte_ha_copier
    (void) devise;
}

// Voitures du Flux (tab5_maj_voiture) : rang, payload séparés par un saut de ligne. La
// puissance lue passe aussi par le partage des flux (deux voitures, l'une tirée du rang,
// l'autre du payload), comme tab5_energie.cpp.
void voiture(const std::string& p) {
    const size_t saut = p.find('\n');
    const std::string rang = p.substr(0, saut);
    const std::string payload = saut == std::string::npos ? std::string() : p.substr(saut + 1);
    VoitureLue v;
    voiture_lire(Champ{payload.data(), payload.size()}, v);
    const std::string nom(v.nom.p != nullptr ? v.nom.p : "", v.nom.n);  // ce que texte_ha_copier recevrait
    const std::string unite(v.unite_distance.p != nullptr ? v.unite_distance.p : "", v.unite_distance.n);
    (void) nom;
    (void) unite;
    // Page « État » (ADR-0081) : les deux unités copiées, le verdict des pneus, une pression reconvertie.
    const std::string unite_p(v.unite_pression.p != nullptr ? v.unite_pression.p : "", v.unite_pression.n);
    const std::string unite_t(v.unite_temperature.p != nullptr ? v.unite_temperature.p : "", v.unite_temperature.n);
    (void) unite_p;
    (void) unite_t;
    (void) voiture_pneus_verdict(v.pneu_bar);
    for (int i = 0; i < kVoiturePneus; i++) (void) pression_en_bar(v.pneu[i], v.unite_pression);
    float charges[kVoituresMax] = {std::isfinite(v.puissance) ? v.puissance : 0.0f,
                                   rang.empty() ? 0.0f : static_cast<float>(rang.size()) * 1000.0f};
    energie_flux_calculer(6400.0f, 8100.0f, 1700.0f, NAN, charges, kVoituresMax, !rang.empty() && rang[0] == '1');
    energie_flux_calculer(NAN, NAN, -static_cast<float>(payload.size()), -charges[0], charges, kVoituresMax, true);
}

// Charges des voitures (tab5_maj_voiture_charges) : la liste lue, puis son total, comme tab5_energie.cpp.
void charges(const std::string& p) {
    ChargeLue c[kChargesMax];
    const int n = charges_lire(Champ{p.data(), p.size()}, c);
    const ChargesTotal t = charges_total(c, n);
    (void) t;
    // La charge en cours : à un « maintenant » tiré de la taille du payload (négatif = horloge non réglée).
    const int32_t maintenant = static_cast<int32_t>(p.size() % 3 == 0 ? -1 : 29000000 + 1440 * static_cast<int32_t>(p.size()));
    for (int i = 0; i < n; i++) (void) charge_en_cours(c[i], maintenant);
}

// Consommation du jour des voitures (tab5_maj_voiture_jour) : rang, début et valeurs séparés par des
// sauts de ligne ; le début est lu par l'écran (tab5_energie.cpp), pas par le socle. La série tirée est lue
// pour son rang, puis pour les trois (une graine ne fait qu'un appel), avec les piles et les totaux que
// l'écran en tire, la production prise d'une taille du payload.
void voiture_jour(const std::string& p) {
    const size_t s1 = p.find('\n');
    const std::string rang = p.substr(0, s1);
    const std::string reste = s1 == std::string::npos ? std::string() : p.substr(s1 + 1);
    const size_t s2 = reste.find('\n');
    const std::string valeurs = s2 == std::string::npos ? std::string() : reste.substr(s2 + 1);
    const Champ serie{valeurs.data(), valeurs.size()};
    ConsoJour c;
    conso_jour_lire(Champ{rang.data(), rang.size()}, serie, c);
    conso_jour_lire(Champ{"0", 1}, serie, c);
    conso_jour_lire(Champ{"1", 1}, serie, c);
    conso_jour_lire(Champ{"2", 1}, serie, c);
    (void) conso_jour_voitures(c);
    for (int h = -1; h <= kEnergieHeures; h++) (void) conso_jour_pile(c, h);
    float production[kEnergieHeures];
    for (int h = 0; h < kEnergieHeures; h++) production[h] = h % 5 == 0 ? NAN : static_cast<float>(valeurs.size() % 7);
    (void) conso_jour_totaux(c, production, kEnergieHeures);
    (void) conso_jour_totaux(c, production, static_cast<int>(valeurs.size() % 40));
    (void) conso_jour_totaux(c, nullptr, 0);
}

// Trajets des voitures (tab5_maj_voiture_trajets) : rang et payload séparés par un saut de ligne ; le rang est
// lu par l'écran (tab5_voitures_trajets.cpp), pas par le socle. Le texte lu, puis les jours qu'il porte (la
// date, ses jours de la semaine, y compris hors de la fenêtre) et ce que l'écran en tire.
void voiture_trajets(const std::string& p) {
    const size_t s = p.find('\n');
    const std::string payload = s == std::string::npos ? std::string() : p.substr(s + 1);
    TrajetsLu t;
    trajets_lire(Champ{payload.data(), payload.size()}, t);
    int jour = 0, semaine = 0;
    for (int i = -1; i <= kTrajetsJours; i++) (void) trajets_jour(t, i, jour, semaine);
    float total = 0.0f;
    for (int i = 0; i < kTrajetsJours; i++) {
        if (std::isfinite(t.km[i])) total += t.km[i];
        if (std::isfinite(t.kwh[i])) total += t.kwh[i];
    }
    (void) total;
}

void info(const std::string& p) {
    // L'écran ne lit que le texte après « @ha| » (compose_info_code, tab5_central.cpp).
    InfoCodeLu lu;
    info_code_lire(p.rfind("@ha|", 0) == 0 ? p.c_str() + 4 : p.c_str(), lu);
}

// Un parseur par entrée ; l'ordre fixe le premier octet des graines (tools/fuzz/graines.py).
using Parseur = void (*)(const std::string&);
constexpr Parseur kParseurs[] = {
    heures,  // '0' tab5_maj_previsions_heures_bulk
    jours,      // '1' tab5_maj_previsions_jours_bulk
    vigilance,   // '2' tab5_maj_alerte_meteo_france
    alertes_ha,  // '3' tab5_maj_alertes_ha_bulk
    historique,  // '4' tab5_maj_alertes_historique
    info,        // '5' tab5_maj_info_texte
    pluie,            // '6' tab5_maj_pluie_1h_bulk
    calendrier_mois,  // '7' tab5_maj_calendrier_mois
    calendrier_jour,  // '8' tab5_maj_calendrier_jour
    emplacements,     // '9' tab5_maj_emplacements
    temperature,      // ':' tab5_maj_historique
    lecteur,          // ';' tab5_maj_lecteur
    cameras,          // '<' tab5_maj_cameras
    suivi,            // '=' tab5_maj_suivi
    froid,            // '>' tab5_maj_froid
    serveur_ia,       // '?' tab5_maj_serveur_ia
    energie_soleil,   // '@' tab5_maj_energie_soleil
    energie_bilan,    // 'A' tab5_maj_energie_bilan
    musique_pieces,   // 'B' tab5_maj_musique_pieces
    musique_liste,    // 'C' tab5_maj_musique_liste
    voiture,          // 'D' tab5_maj_voiture
    charges,          // 'E' tab5_maj_voiture_charges
    voiture_jour,     // 'F' tab5_maj_voiture_jour
    voiture_trajets,  // 'G' tab5_maj_voiture_trajets
};
constexpr size_t kNbParseurs = sizeof(kParseurs) / sizeof(kParseurs[0]);

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    const size_t choix = static_cast<uint8_t>(data[0] - '0') % kNbParseurs;
    const std::string payload(reinterpret_cast<const char*>(data + 1), size - 1);
#ifdef TAB5_FUZZ_TEMOIN
    if (payload.compare(0, 6, "TEMOIN") == 0) {
        volatile float f = 1e30f;
        volatile int i = static_cast<int>(f);
        (void) i;
    }
#endif
    kParseurs[choix](payload);
    return 0;
}
