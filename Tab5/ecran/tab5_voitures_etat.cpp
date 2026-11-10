/**
 * [AI-CONTEXT]
 * @file tab5_voitures_etat.cpp
 * @role Page « État » du popup Voitures (ADR-0081, lot 7 « C4 » du plan des voitures). Par voiture
 *       reçue, deux cartes de verre l'une sous l'autre : en haut le nom, le verdict des pneus
 *       (« Pneus corrects » en vert, « Pression basse » en orange, rien sans les quatre pressions)
 *       et la voiture vue de dessus avec la pression de chaque pneu à côté de lui ; en bas cinq
 *       lignes (Cabine, Extérieur, Portes, Sentinelle, Logiciel), une icône, un libellé, une valeur.
 *       Les données sont les onze derniers champs de tab5_maj_voiture (voiture_lire,
 *       Tab5/socle/tab5_parse.h) ; une valeur que Home Assistant ne donne pas s'écrit « -- ».
 *       Deux voitures : une colonne chacune ; une seule : les cartes pleine largeur, contenu dans
 *       la colonne d'une demi-carte, centré (comme l'Aperçu, ADR-0079).
 * @architecture_constraint Le popup (tab5_voitures.cpp) garde ses propres voitures et appelle
 *       voitures_etat_recu() / voitures_etat_a_donnees() / voitures_etat_peindre() par sa table de
 *       pages ; ce fichier ne sait rien de ses onglets. Push-only et events-only (ADR-0001,
 *       ADR-0025) : aucune entité nommée, aucune action. Popup fermé ou autre page affichée :
 *       une poussée garde la voiture et ne repeint RIEN. Libellés, icônes et cartes sont posés
 *       par voitures_etat_haut.yaml / voitures_etat_bas.yaml / voitures_etat_ligne.yaml (ordre des
 *       enfants : voir kEnfants*), le dessin de la voiture (pneus, carrosserie, vitres) est créé
 *       ici à la première peinture. Couleurs : rôles de la palette (ADR-0029), réécrites à chaque
 *       peinture (comparées d'abord : ui_style_couleur), donc suivies par un changement de thème
 *       (voitures_rejouer_theme() → le popup repeint la page affichée).
 * @ai_warning La voiture est vue de dessus, avant à DROITE : le côté gauche de la voiture est donc
 *       en HAUT. Haut-gauche = pneu arrière gauche, haut-droite = avant gauche, bas-gauche =
 *       arrière droit, bas-droite = avant droit (kPlaces). Les pneus sont créés AVANT la
 *       carrosserie : sa moitié intérieure les cache, seule la moitié extérieure se voit. Un pneu
 *       est « bas » sous kVoiturePneuBasBar (2,2 bar : un choix, pas une règle de Tesla). Les
 *       ids YAML et l'ordre des enfants sont liés à ce fichier : un widget de plus s'ajoute en
 *       FIN de son gabarit et dans kEnfants*.
 * @ai_instruction Une ligne de plus = son entrée dans `Ligne`, kLibelles, la table du texte
 *       (valeur_ligne) et la fonction glyphe_ligne, un voitures_etat_ligne.yaml de plus dans le
 *       gabarit bas, et son champ en fin de payload (tab5_parse.h). Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <strings.h>

namespace {

constexpr const char* kTag = "tab5.voitures_etat";
constexpr size_t kUniteMax = 8;
constexpr int kPneus = kVoiturePneus;

// Les pneus dans VoitureLue : avant gauche, avant droit, arrière gauche, arrière droit.
enum Pneu { AV_G = 0,
            AV_D = 1,
            AR_G = 2,
            AR_D = 3 };
// Où un pneu se dessine : voiture vue de dessus, avant à droite (le côté gauche est en haut).
struct Place {
    bool haut;    // le côté gauche de la voiture
    bool avant;   // l'essieu avant
};
constexpr Place kPlaces[kPneus] = {{true, true}, {false, true}, {true, false}, {false, false}};

// Les cinq lignes de la carte du bas, dans l'ordre d'affichage.
enum Ligne { L_CABINE = 0,
             L_EXTERIEUR = 1,
             L_PORTES = 2,
             L_SENTINELLE = 3,
             L_LOGICIEL = 4,
             kLignes = 5 };
const char* const kLibelles[kLignes] = {tr_noop("Cabine"), tr_noop("Extérieur"), tr_noop("Portes"),
                                        tr_noop("Sentinelle"), tr_noop("Logiciel")};

// Ordre des enfants posé par les gabarits YAML : la page porte [haut 0, bas 0, haut 1, bas 1] ; une
// carte du haut [nom, icône du verdict, texte du verdict, pression AV_G, AV_D, AR_G, AR_D] ; une
// carte du bas cinq lignes, chacune [icône, libellé, valeur].
constexpr int kEnfantsHaut = 3 + kPneus;
constexpr int kEnfantsLigne = 3;

// L'état d'une voiture : ce que voiture_lire() a lu, les textes copiés (le payload ne vit pas plus loin).
// Le nom n'y est pas : il est dans voiture_recue() (tab5_voitures.h), la copie du popup.
struct Voit {
    bool presente = false;
    char unite_pression[kUniteMax] = {};
    char unite_temperature[kUniteMax] = {};
    float pneu[kPneus] = {NAN, NAN, NAN, NAN};
    float pneu_bar[kPneus] = {NAN, NAN, NAN, NAN};
    float temp_int = NAN, temp_ext = NAN;
    VoitureOuiNon verrouillee = VoitureOuiNon::INCONNU;
    VoitureOuiNon sentinelle = VoitureOuiNon::INCONNU;
    VoitureOuiNon logiciel = VoitureOuiNon::INCONNU;
};
Voit s_v[kVoituresMax];

// Les widgets d'une voiture : ceux des gabarits YAML, puis le dessin créé ici.
struct Carte {
    lv_obj_t* haut = nullptr;
    lv_obj_t* bas = nullptr;
    lv_obj_t* nom = nullptr;
    lv_obj_t* puce_icone = nullptr;
    lv_obj_t* puce = nullptr;
    lv_obj_t* pression[kPneus] = {};
    lv_obj_t* ligne[kLignes] = {};
    lv_obj_t* icone[kLignes] = {};
    lv_obj_t* libelle[kLignes] = {};
    lv_obj_t* valeur[kLignes] = {};
    lv_obj_t* roue[kPneus] = {};
    lv_obj_t* carrosserie = nullptr;
    lv_obj_t* toit = nullptr;
    lv_obj_t* pare_brise = nullptr;
    lv_obj_t* lunette = nullptr;
};
Carte s_c[kVoituresMax];
lv_obj_t* s_page = nullptr;
bool s_lie = false;
bool s_construit = false;

// --- Géométrie ----------------------------------------------------------------------------
// Les nombres sont ceux du Tab5 (corps de 1202 × 598), ramenés à la carte (la 7B : 946 × 478,
// hauteur_carte) ; les polices ne changent pas.
constexpr int32_t kPad = 26;                                   // marge intérieure (côtés)
constexpr int32_t kHautCarte = 18;                             // marge du haut et du bas d'une carte
constexpr int32_t kEcartTitre = 12;                            // du titre au dessin
constexpr int32_t kEcartLibelle = 14;                          // d'un pneu à sa pression ; icône à libellé
constexpr int32_t kEcartPuce = 10;                             // de l'icône du verdict à son texte
constexpr int32_t kPasLigne = hauteur_carte(44);               // une ligne de la carte du bas
constexpr int32_t kCarteSeuleL = kCorpsW;                      // une seule voiture : pleine largeur
constexpr int32_t kCarteDeuxL = (kCorpsW - kCartesEcart) / 2;  // deux voitures : côte à côte
constexpr int32_t kColonneMax = kCarteDeuxL - 2 * kPad;        // le contenu ne dépasse pas une demi-carte
constexpr int32_t kRoueEp = 24;                                // épaisseur d'un pneu (sa moitié dépasse)
constexpr int32_t kVoitureLMin = 150;                          // la voiture, au plus petit

int32_t haut(lv_obj_t* l) {
    const lv_font_t* f = l != nullptr ? lv_obj_get_style_text_font(l, LV_PART_MAIN) : nullptr;
    return f != nullptr ? lv_font_get_line_height(f) : 0;
}

// Un label posé à (x, y) sur `l` px de large (texte coupé par « … » au-delà).
void poser_texte(lv_obj_t* lbl, const char* txt, int32_t x, int32_t y, int32_t l) {
    texte_ha_coupe(lbl, txt, l);
    ui_poser(lbl, x, y, l, LV_SIZE_CONTENT);
}

lv_obj_t* enfant(lv_obj_t* parent, int i) { return lv_obj_get_child(parent, i); }

// --- Données ------------------------------------------------------------------------------

bool a_une_donnee(const Voit& v) {
    for (int i = 0; i < kPneus; i++)
        if (std::isfinite(v.pneu[i])) return true;
    return std::isfinite(v.temp_int) || std::isfinite(v.temp_ext) || v.verrouillee != VoitureOuiNon::INCONNU ||
           v.sentinelle != VoitureOuiNon::INCONNU || v.logiciel != VoitureOuiNon::INCONNU;
}

// « 2.9 bar », « 42 psi », « 290 kPa » : le bar avec une décimale, les autres unités entières ; la
// valeur dans l'unité reçue, jamais convertie à l'écran. « -- » quand le pneu n'est pas connu.
void texte_pression(char* out, size_t n, const Voit& v, int pneu) {
    if (!std::isfinite(v.pneu[pneu])) {
        snprintf(out, n, "--");
        return;
    }
    const char* u = v.unite_pression[0] != '\0' ? v.unite_pression : "bar";
    const bool bar = strcasecmp(u, "bar") == 0;
    snprintf(out, n, bar ? "%.1f %s" : "%.0f %s", static_cast<double>(v.pneu[pneu]), u);
}

// Le « °C » par défaut, quand Home Assistant n'envoie pas d'unité.
const char* const kDegreC = "°C";

// « 19 °C » : la température arrondie, l'unité reçue (« °C » sans unité) ; « -- » sans valeur.
void texte_temperature(char* out, size_t n, float t, const char* unite) {
    if (!std::isfinite(t)) {
        snprintf(out, n, "--");
        return;
    }
    snprintf(out, n, "%ld %s", std::lround(t), unite[0] != '\0' ? unite : kDegreC);
}

// --- Icônes (mdi_font_32 ; MDI_CODE_TARGETS dans tools/check_tab5_code_rules.py) ------------------

// L'icône du verdict des pneus.
const char* glyphe_puce(PneusVerdict d) {
    return d == PneusVerdict::BAS ? "\U000F05D6" /* alert-circle-outline */ : "\U000F05E1" /* check-circle-outline */;
}

// L'icône d'une ligne, selon sa valeur.
const char* glyphe_ligne(Ligne l, const Voit& v) {
    switch (l) {
        case L_CABINE:
        case L_EXTERIEUR: return "\U000F050F";   // thermometer
        case L_PORTES: return v.verrouillee == VoitureOuiNon::NON ? "\U000F033F" /* lock-open */ : "\U000F033E" /* lock */;
        case L_SENTINELLE:
            return v.sentinelle == VoitureOuiNon::OUI ? "\U000F06D0" /* eye-outline */ : "\U000F06D1" /* eye-off-outline */;
        default:
            return v.logiciel == VoitureOuiNon::NON ? "\U000F06B0" /* update */ : "\U000F05E1" /* check-circle-outline */;
    }
}

// Le texte et la couleur de la valeur d'une ligne, et la couleur de son icône. Rien de connu : « -- »
// et le retrait (TEXT_DIM). Texte : un statique ou un tampon de l'appelant (`buf`).
struct Valeur {
    const char* texte = "--";
    uint32_t couleur = 0;
    uint32_t icone = 0;
};
Valeur valeur_ligne(Ligne l, const Voit& v, char* buf, size_t n) {
    Valeur r;
    r.couleur = UIColor.TEXT_DIM;
    r.icone = UIColor.TEXT_DIM;
    switch (l) {
        case L_CABINE:
        case L_EXTERIEUR: {
            const float t = l == L_CABINE ? v.temp_int : v.temp_ext;
            texte_temperature(buf, n, t, v.unite_temperature);
            r.texte = buf;
            r.icone = l == L_CABINE ? UIColor.TEMP_MAX : UIColor.TEMP_MIN;
            r.couleur = std::isfinite(t) ? UIColor.TEXT_PRIMARY : UIColor.TEXT_DIM;
            break;
        }
        case L_PORTES:
            if (v.verrouillee == VoitureOuiNon::OUI) {
                r.texte = tr("Verrouillées");
                r.couleur = r.icone = UIColor.SUCCESS;
            } else if (v.verrouillee == VoitureOuiNon::NON) {
                r.texte = tr("Déverrouillées");
                r.couleur = r.icone = UIColor.WARNING;
            }
            break;
        case L_SENTINELLE:
            if (v.sentinelle == VoitureOuiNon::OUI) {
                r.texte = tr("Activée");
                r.couleur = r.icone = UIColor.INFO;
            } else if (v.sentinelle == VoitureOuiNon::NON) {
                r.texte = tr("Désactivée");
                r.couleur = UIColor.TEXT_SOFT;
            }
            break;
        default:
            if (v.logiciel == VoitureOuiNon::OUI) {
                r.texte = tr("À jour");
                r.couleur = r.icone = UIColor.SUCCESS;
            } else if (v.logiciel == VoitureOuiNon::NON) {
                r.texte = tr("À mettre à jour");
                r.couleur = r.icone = UIColor.INFO;
            }
            break;
    }
    return r;
}

// --- Construction ---------------------------------------------------------------------------

// Un rectangle de dessin (ui_rectangle : sans style, non cliquable, masqué) plein ou en contour.
lv_obj_t* forme(lv_obj_t* parent, lv_opa_t opa, int32_t rayon) {
    lv_obj_t* o = ui_rectangle(parent, opa, rayon);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

// Les widgets des gabarits YAML, par leur place d'enfant ; faux (et journalisé) si le compte ne
// tombe pas : un gabarit changé sans ce fichier ne plante pas, la page ne s'affiche pas.
bool lier_carte(Carte& k, lv_obj_t* haut_obj, lv_obj_t* bas_obj) {
    if (haut_obj == nullptr || bas_obj == nullptr) return false;
    if (static_cast<int>(lv_obj_get_child_count(haut_obj)) != kEnfantsHaut ||
        static_cast<int>(lv_obj_get_child_count(bas_obj)) != kLignes) {
        ESP_LOGW(kTag, "gabarit de la page État inattendu (%d + %d enfants)",
                 static_cast<int>(lv_obj_get_child_count(haut_obj)), static_cast<int>(lv_obj_get_child_count(bas_obj)));
        return false;
    }
    k.haut = haut_obj;
    k.bas = bas_obj;
    k.nom = enfant(haut_obj, 0);
    k.puce_icone = enfant(haut_obj, 1);
    k.puce = enfant(haut_obj, 2);
    for (int p = 0; p < kPneus; p++) k.pression[p] = enfant(haut_obj, 3 + p);
    for (int l = 0; l < kLignes; l++) {
        k.ligne[l] = enfant(bas_obj, l);
        if (static_cast<int>(lv_obj_get_child_count(k.ligne[l])) != kEnfantsLigne) {
            ESP_LOGW(kTag, "ligne %d de la page État inattendue", l);
            return false;
        }
        k.icone[l] = enfant(k.ligne[l], 0);
        k.libelle[l] = enfant(k.ligne[l], 1);
        k.valeur[l] = enfant(k.ligne[l], 2);
    }
    return true;
}

// Le dessin de la voiture vue de dessus : quatre pneus d'abord (la carrosserie en cache la moitié
// intérieure), la carrosserie, trois vitres (lunette, toit, pare-brise : l'avant est à droite).
void construire() {
    if (s_construit || !s_lie) return;
    for (int c = 0; c < kVoituresMax; c++) {
        Carte& k = s_c[c];
        for (int p = 0; p < kPneus; p++) k.roue[p] = forme(k.haut, LV_OPA_COVER, 8);
        k.carrosserie = forme(k.haut, LV_OPA_COVER, 40);
        ui_style_num(k.carrosserie, LV_STYLE_BORDER_WIDTH, 3);
        ui_style_num(k.carrosserie, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
        ui_style_num(k.carrosserie, LV_STYLE_BORDER_SIDE, LV_BORDER_SIDE_FULL);
        k.lunette = forme(k.haut, LV_OPA_COVER, 10);
        k.toit = forme(k.haut, LV_OPA_COVER, 10);
        k.pare_brise = forme(k.haut, LV_OPA_COVER, 10);
    }
    s_construit = true;
}

// --- Peinture -------------------------------------------------------------------------------

// La carte du haut : le nom, le verdict, la voiture et ses quatre pressions, dans la carte de
// largeur `w` et de hauteur `h`.
void peindre_haut(int c, const Voit& v, int rang, int32_t w, int32_t h) {
    const Carte& k = s_c[c];
    const int32_t col = std::min<int32_t>(w - 2 * kPad, kColonneMax);
    const int32_t x = (w - col) / 2;

    // --- En-tête : le nom à gauche, le verdict des pneus à droite --------------------------
    char nom[96];
    const char* nom_recu = voiture_recue(rang - 1).nom;
    if (nom_recu[0] != '\0') snprintf(nom, sizeof(nom), "%s", nom_recu);
    else snprintf(nom, sizeof(nom), tr("Voiture %d"), rang);
    const int32_t h_nom = haut(k.nom);
    const PneusVerdict verdict = voiture_pneus_verdict(v.pneu_bar);
    int32_t reserve = 0;   // la place que prend le verdict, à droite
    if (verdict == PneusVerdict::INCONNU) {
        ui_hidden(k.puce_icone, true);
        ui_hidden(k.puce, true);
    } else {
        const char* mot = verdict == PneusVerdict::BAS ? tr("Pression basse") : tr("Pneus corrects");
        const uint32_t couleur = verdict == PneusVerdict::BAS ? UIColor.WARNING : UIColor.SUCCESS;
        const char* ico = glyphe_puce(verdict);
        const int32_t w_ico = ui_largeur_texte(k.puce_icone, ico);
        const int32_t w_txt = std::min<int32_t>(ui_largeur_texte(k.puce, mot), col * 6 / 10);
        const int32_t h_ico = haut(k.puce_icone);
        const int32_t h_txt = haut(k.puce);
        ui_text(k.puce_icone, ico);
        ui_text_color(k.puce_icone, couleur);
        ui_text_color(k.puce, couleur);
        poser_texte(k.puce, mot, x + col - w_txt, kHautCarte + (h_nom - h_txt) / 2, w_txt);
        ui_poser(k.puce_icone, x + col - w_txt - kEcartPuce - w_ico, kHautCarte + (h_nom - h_ico) / 2, LV_SIZE_CONTENT,
                 LV_SIZE_CONTENT);
        reserve = w_txt + kEcartPuce + w_ico + 16;
    }
    ui_text_color(k.nom, rang == 1 ? UIColor.ACCENT : UIColor.ACCENT_ALT);
    poser_texte(k.nom, nom, x, kHautCarte, std::max<int32_t>(col - reserve, 40));

    // --- Pressions : le texte de chacune, la plus large donne la marge de chaque côté --------
    char txt[kPneus][24];
    int32_t lw = 0;
    for (int p = 0; p < kPneus; p++) {
        texte_pression(txt[p], sizeof(txt[p]), v, p);
        lw = std::max<int32_t>(lw, ui_largeur_texte(k.pression[p], txt[p]));
    }
    // --- La voiture : centrée dans ce qui reste, longueur d'abord bornée par les pressions ---
    const int32_t y0 = kHautCarte + h_nom + kEcartTitre;
    const int32_t dh = h - y0 - kHautCarte;                       // la hauteur du dessin, pneus compris
    const int32_t cote = lw + kEcartLibelle;                      // de chaque côté de la voiture
    int32_t vl = std::max<int32_t>(col - 2 * cote, kVoitureLMin);          // longueur de la voiture
    int32_t vh = std::min<int32_t>(dh - kRoueEp, vl * 10 / 24);            // sa largeur : un peu plus de deux fois moins
    vh = std::max<int32_t>(vh, 40);
    vl = std::min<int32_t>(vl, vh * 24 / 10 + 40);                         // sur un corps très bas, pas plus de 2,4 + marge
    const int32_t vx = x + (col - vl) / 2;
    const int32_t vy = y0 + (dh - vh) / 2;
    const int32_t roue_l = vl * 22 / 100;

    ui_style_couleur(k.carrosserie, LV_STYLE_BG_COLOR, UIColor.ARC_TRACK);
    ui_style_couleur(k.carrosserie, LV_STYLE_BORDER_COLOR, UIColor.TEXT_SOFT);
    ui_poser(k.carrosserie, vx, vy, vl, vh);
    // Vitres : la lunette (arrière), le toit, le pare-brise (avant) ; les trois dans la carrosserie.
    const int32_t vit_h = vh * 66 / 100;
    const int32_t vit_y = vy + (vh - vit_h) / 2;
    ui_style_couleur(k.lunette, LV_STYLE_BG_COLOR, UIColor.BG);
    ui_style_couleur(k.toit, LV_STYLE_BG_COLOR, UIColor.GLASS_LO);
    ui_style_couleur(k.pare_brise, LV_STYLE_BG_COLOR, UIColor.BG);
    ui_poser(k.lunette, vx + vl * 12 / 100, vit_y, vl * 17 / 100, vit_h);
    ui_poser(k.toit, vx + vl * 31 / 100, vit_y, vl * 29 / 100, vit_h);
    ui_poser(k.pare_brise, vx + vl * 62 / 100, vit_y, vl * 18 / 100, vit_h);

    for (int p = 0; p < kPneus; p++) {
        const Place& pl = kPlaces[p];
        const bool connu = std::isfinite(v.pneu[p]);
        const bool bas = connu && std::isfinite(v.pneu_bar[p]) && v.pneu_bar[p] < kVoiturePneuBasBar;
        // Le pneu : sur le bord haut ou bas de la carrosserie, à l'essieu avant ou arrière.
        const int32_t cx = vx + vl * (pl.avant ? 78 : 22) / 100;
        const int32_t cy = pl.haut ? vy : vy + vh;
        ui_style_couleur(k.roue[p], LV_STYLE_BG_COLOR, bas ? UIColor.WARNING : (connu ? UIColor.TEXT_SOFT : UIColor.BAR_INACTIVE));
        ui_poser(k.roue[p], cx - roue_l / 2, cy - kRoueEp / 2, roue_l, kRoueEp);
        // Sa pression : sur la ligne de son bord, au bout de la voiture de son côté (arrière à gauche,
        // avant à droite) ; alignée vers la voiture.
        const int32_t h_p = haut(k.pression[p]);
        const int32_t px = pl.avant ? vx + vl + kEcartLibelle : vx - kEcartLibelle - lw;
        ui_style_num(k.pression[p], LV_STYLE_TEXT_ALIGN, pl.avant ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_RIGHT);
        ui_text_color(k.pression[p], bas ? UIColor.WARNING : (connu ? UIColor.TEXT_PRIMARY : UIColor.TEXT_DIM));
        poser_texte(k.pression[p], txt[p], px, cy - h_p / 2, lw);
    }
}

// La carte du bas : cinq lignes (icône, libellé, valeur), centrées en hauteur.
void peindre_bas(int c, const Voit& v, int32_t w, int32_t h) {
    const Carte& k = s_c[c];
    const int32_t col = std::min<int32_t>(w - 2 * kPad, kColonneMax);
    const int32_t x = (w - col) / 2;
    int32_t pas = kPasLigne;
    for (int l = 0; l < kLignes; l++) pas = std::max<int32_t>(pas, haut(k.valeur[l]) + 4);
    const int32_t y0 = std::max<int32_t>(0, (h - pas * kLignes) / 2);
    char buf[kLignes][40];
    for (int l = 0; l < kLignes; l++) {
        const Ligne lg = static_cast<Ligne>(l);
        const Valeur val = valeur_ligne(lg, v, buf[l], sizeof(buf[l]));
        ui_poser(k.ligne[l], x, y0 + l * pas, col, pas);
        // L'icône à gauche, centrée en hauteur dans la ligne.
        const char* ico = glyphe_ligne(lg, v);
        ui_text(k.icone[l], ico);
        ui_text_color(k.icone[l], val.icone);
        const int32_t w_ico = std::max<int32_t>(ui_largeur_texte(k.icone[l], ico), 32);
        ui_poser(k.icone[l], 0, (pas - haut(k.icone[l])) / 2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        // Le libellé à côté, la valeur calée à droite sur le reste de la ligne.
        const char* lib = tr(kLibelles[l]);
        const int32_t w_lib = ui_largeur_texte(k.libelle[l], lib);
        ui_text_color(k.libelle[l], UIColor.TEXT_DIM);
        poser_texte(k.libelle[l], lib, w_ico + kEcartLibelle, (pas - haut(k.libelle[l])) / 2, w_lib);
        const int32_t vx = w_ico + kEcartLibelle + w_lib + 16;
        ui_text_color(k.valeur[l], val.couleur);
        ui_style_num(k.valeur[l], LV_STYLE_TEXT_ALIGN, LV_TEXT_ALIGN_RIGHT);
        poser_texte(k.valeur[l], val.texte, vx, (pas - haut(k.valeur[l])) / 2, std::max<int32_t>(col - vx, 40));
    }
}

}  // namespace

void voitures_etat_lier(lv_obj_t* page) {
    if (s_lie) return;
    // Le conteneur est un enfant du corps de la carte modale, comme les cartes de l'Aperçu.
    if (page == nullptr || lv_obj_get_parent(page) != voitures_corps() ||
        static_cast<int>(lv_obj_get_child_count(page)) != 2 * kVoituresMax) {
        ESP_LOGW(kTag, "page État inattendue");
        return;
    }
    for (int c = 0; c < kVoituresMax; c++)
        if (!lier_carte(s_c[c], enfant(page, 2 * c), enfant(page, 2 * c + 1))) return;
    s_page = page;
    s_lie = true;
}

void voitures_etat_recu(int k, const VoitureLue* lu) {
    if (k < 0 || k >= kVoituresMax) return;
    Voit v;
    if (lu != nullptr) {
        v.presente = true;
        texte_ha_copier(v.unite_pression, sizeof(v.unite_pression), lu->unite_pression.p, lu->unite_pression.n);
        texte_ha_copier(v.unite_temperature, sizeof(v.unite_temperature), lu->unite_temperature.p,
                        lu->unite_temperature.n);
        for (int i = 0; i < kPneus; i++) {
            v.pneu[i] = lu->pneu[i];
            v.pneu_bar[i] = lu->pneu_bar[i];
        }
        v.temp_int = lu->temp_int;
        v.temp_ext = lu->temp_ext;
        v.verrouillee = lu->verrouillee;
        v.sentinelle = lu->sentinelle;
        v.logiciel = lu->logiciel_a_jour;
    }
    s_v[k] = v;
}

bool voitures_etat_a_donnees() {
    if (!s_lie) return false;   // gabarit refusé : pas d'onglet devant une page vide
    for (const Voit& v : s_v)
        if (v.presente && a_une_donnee(v)) return true;
    return false;
}

void voitures_etat_montrer(bool visible) {
    if (s_lie) ui_hidden(s_page, !visible);
}

void voitures_etat_peindre() {
    if (!s_lie) return;
    construire();
    if (!s_construit) return;
    // Les voitures reçues, dans l'ordre des rangs : la seconde seule prend la première colonne. Une
    // voiture sans aucune donnée d'état garde sa colonne (« -- » partout) tant qu'une autre en a.
    int rang[kVoituresMax] = {};
    int n = 0;
    for (int r = 0; r < kVoituresMax; r++)
        if (s_v[r].presente) rang[n++] = r;
    const int32_t w = n == 1 ? kCarteSeuleL : kCarteDeuxL;
    // Carte du bas : le nombre de lignes à la hauteur de la ligne ; carte du haut : le reste de la hauteur.
    int32_t pas = kPasLigne;
    for (int l = 0; l < kLignes; l++) pas = std::max<int32_t>(pas, haut(s_c[0].valeur[l]) + 4);
    const int32_t h_bas = std::min<int32_t>(pas * kLignes + 2 * kHautCarte, kCorpsH / 2);
    const int32_t h_haut = kCorpsH - kCartesEcart - h_bas;
    for (int c = 0; c < kVoituresMax; c++) {
        Carte& k = s_c[c];
        if (c >= n) {
            ui_hidden(k.haut, true);
            ui_hidden(k.bas, true);
            continue;
        }
        const int32_t x0 = c * (kCarteDeuxL + kCartesEcart);
        ui_poser(k.haut, x0, 0, w, h_haut);
        ui_poser(k.bas, x0, h_haut + kCartesEcart, w, h_bas);
        peindre_haut(c, s_v[rang[c]], rang[c] + 1, w, h_haut);
        peindre_bas(c, s_v[rang[c]], w, h_bas);
    }
}
