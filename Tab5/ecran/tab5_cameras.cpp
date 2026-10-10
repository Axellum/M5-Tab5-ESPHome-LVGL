/**
 * [AI-CONTEXT]
 * @file tab5_cameras.cpp
 * @role Popup « Caméras » (ADR-0049, 09/10/2026, discussion #278 ; pièces et mémoire des
 *       images : ADR-0057, 10/10/2026) : l'image fixe des caméras de Home Assistant,
 *       rangées par pièce, rafraîchie tant que le popup est ouvert. Pas de vidéo : un JPEG
 *       à la fois, jamais deux.
 *         - Liste : action tab5_maj_cameras (adresse, cameras), poussée par le blueprint
 *           en réponse à esphome.tab5_cameras, émis à l'ouverture, toutes les 4 minutes
 *           tant que le popup reste ouvert (le jeton des URL de HA change toutes les
 *           5 minutes, l'ancien reste valable 5 minutes de plus) et après un échec
 *           (30 s au plus souvent : jeton périmé, caméra hors ligne). Chaque caméra a sa
 *           pièce (area_name) et, si HA la dit indisponible, l'heure depuis laquelle.
 *         - Pièces : à partir de deux, la colonne à gauche (« Toutes » + une puce par
 *           pièce, avec son nombre de caméras) filtre ; le balayage (pages_brancher,
 *           ADR-0046) et les pastilles ne parcourent que le filtre. Pièce, caméra et vue
 *           choisies gardées en NVS (« cams ») : retrouvées à l'ouverture suivante, même
 *           après un redémarrage.
 *         - Deux vues (lot 2) : la MOSAÏQUE (dès deux caméras dans le filtre : 2 × 2
 *           vignettes au plus par page, 2 ou 3 cases disposées pour remplir le cadre) et la
 *           caméra EN GRAND. Tap sur une vignette → en grand ; tap sur l'image → mosaïque.
 *           Vignettes demandées à 480 × 270 (kVignetteL/H), montrées « couvrantes » dans
 *           leur case de 476 × 266 : recadrées sans mise à l'échelle quand elles y tiennent
 *           presque, réduites sinon (echelle_couvrir()).
 *         - Image : téléchargée et décodée HORS de la boucle principale par le chargeur
 *           (tab5_cameras_charge.h : tâche FreeRTOS, décodeur JPEG matériel), une à la
 *           fois ; URL construite par camera_url() (tab5_parse.h, section 9). La boucle
 *           sonde le chargeur toutes les 100 ms (kSondeMs).
 *         - Ordonnanceur (prochaine()) : toujours UNE image à la fois. En grand : la caméra
 *           montrée toutes les 5 s ; entre deux, ses voisines du filtre qui n'ont pas encore
 *           d'image, une fois chacune (un balayage montre alors une image tout de suite).
 *           En mosaïque : les vignettes de la page à tour de rôle, d'abord celles sans
 *           image, puis chacune toutes les 10 s (kRafraichirMosaiqueMs). Une caméra que HA dit hors
 *           ligne n'est pas demandée ; une caméra en échec attend 10 s, 30 s, puis 60 s
 *           (kEchecsDelaisMs) sans retenir les autres. Deux échecs de suite : « Hors ligne
 *           depuis … », sa dernière image grisée.
 *         - Mémoire : la dernière image de chaque caméra (Vue), en grand et en vignette,
 *           reste en PSRAM tant que le popup est ouvert, kVuesOctetsMax au plus et jamais
 *           sous kPsramReserve libres (la plus anciennement vue part d'abord, jamais une
 *           image à l'écran).
 *         - Fermé (croix, retour automatique après 45 s sans toucher, autre écran) : le
 *           rappel suivant (cameras_tic), ou la fin du chargement en cours, rend toute la
 *           mémoire (images, JPEG, connexion), et plus rien n'est demandé.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : la tablette ne
 *       nomme aucune entité ; elle dit seulement qu'elle veut la liste. Couleurs : styles
 *       de rôle du YAML (cameras_popup.yaml, cameras_puce.yaml) et choix_peindre() (accent
 *       de la palette active, repeint au changement de thème par cameras_rejouer_theme()) ;
 *       ce fichier n'écrit que du texte, des positions, des
 *       affichages, une opacité d'image et la source de l'image. Le widget image est créé
 *       ici (dans cameras_cadre) : l'image: d'ESPHome exige une source fixe.
 * @ai_warning [AI-WARNING] Les widgets image (le grand, les quatre vignettes) montrent
 *       directement un tampon gardé (Vue::img). Avant de rendre un tampon
 *       (camera_image_rendre), aucun Affichage ne doit plus le montrer (oublier(), qui
 *       les parcourt tous) et le cache d'images de LVGL est vidé pour son
 *       descripteur. Un descripteur n'est jamais réécrit sous LVGL : deux par Affichage, à
 *       tour de rôle. La liste reçue réordonne les caméras (correspondance par le nom) :
 *       s_cam et s_en_cours suivent, les images aussi.
 * @ai_instruction Un texte affiché passe par tr(). Le format de tab5_maj_cameras est un
 *       contrat avec le blueprint, la démo (tools/demo/) et le rendu (tools/rendu/).
 */
#include "tab5_internal.h"
#include "tab5_cameras_charge.h"
#include "tab5_parse.h"
#include "lvgl.h"
#include "lvgl_private.h"  // lv_image_cache_drop() (cache d'images, hors de lvgl.h en 9.5)
#include <esp_attr.h>
#include <cstdio>
#include <cstring>
#include <ctime>

CamerasUI g_cameras_ui;

static_assert(kCamerasPastilles == kCamerasMax, "une pastille par caméra");
static_assert(kCamerasPuces == kCamerasMax + 1, "« Toutes » + une puce par pièce");

namespace {

// ─── Géométrie (cameras_popup.yaml) ─────────────────────────────────────────────────
// Carte de kCarteL : sans colonne, le cadre 964 × 544 est centré (143 px de chaque côté) ;
// avec la colonne des pièces (deux pièces ou plus), il passe à droite, à kMarge du bord,
// et la colonne prend le reste à gauche (238 px : un nom de pièce entier en roboto_22).
constexpr int kImageL = 960;               // demandée à HA (width / height du proxy)
constexpr int kImageH = 540;
constexpr int32_t kCadreL = kImageL + 4;   // bordure de 2 px du verre
constexpr int32_t kMarge = 16;
constexpr int32_t kCadreXCentre = (kCarteL - kCadreL) / 2;           // 143
constexpr int32_t kCadreXColonne = kCarteL - kMarge - kCadreL;        // 270
constexpr int kImageRayon = 18;            // coins de l'image (cadre style_glass_card)
constexpr lv_opa_t kOpaHorsLigne = LV_OPA_40;   // dernière image d'une caméra hors ligne

// Mosaïque (lot 2) : le contenu du cadre (960 × 540) en quatre cases de 476 × 266, 8 px
// d'écart (cameras_vignette.yaml). Une vignette est demandée à 480 × 270 : une caméra
// 1920 × 1080 arrive à cette taille (HA réduit par 1/8 … 7/8, jamais sous la demande),
// recadrée de 2 px par côté, sans transformation.
constexpr int kVignetteL = 480;
constexpr int kVignetteH = 270;
constexpr int32_t kCaseL = 476;
constexpr int32_t kCaseH = 266;
constexpr int32_t kCaseEcart = 8;
// Coins des cases (= radius de cameras_vignette.yaml) : petits exprès. LVGL 9.5 n'arrondit
// une image que sans mise à l'échelle, et sur SA surface (480 × 270, 2 px de plus que la
// case par côté) : avec 8 px, l'écart avec le bouton ne se voit pas ; avec 14, si.
constexpr int kCaseRayon = 8;
// Recadrer plutôt que réduire tant que la réduction serait légère (≥ 96 % : 246 / 256) :
// une image transformée coûte un rendu bien plus lent qu'une copie.
constexpr uint32_t kSansEchelleMin = 246;

// ─── Temps ──────────────────────────────────────────────────────────────────────────
constexpr uint32_t kRafraichirMs = 5000;        // la caméra montrée, après son image
constexpr uint32_t kRafraichirMosaiqueMs = 10000;   // chaque vignette de la page
constexpr uint32_t kApresErreurMs = 10000;      // URL illisible (adresse inconnue…)
constexpr uint32_t kRedemanderMs = 240000;      // liste (jetons) redemandée à HA
constexpr uint32_t kErreurRedemanderMs = 30000; // après un échec, et tant que rien n'est reçu
constexpr uint32_t kAttenteMaxMs = 5000;        // rien à charger : on revient voir
// Échecs de suite d'une caméra : 10 s, 30 s, puis 60 s avant le suivant (une caméra hors
// ligne occupe la tâche jusqu'à 12 s, et HA, à chaque essai). Remis à zéro par une image.
constexpr uint32_t kEchecsDelaisMs[] = {10000, 30000, 60000};
constexpr int kEchecsHorsLigne = 2;   // à partir de là : « Hors ligne depuis … »
constexpr int kSondeMs = 100;         // chargement en cours, popup ouvert
constexpr int kSondeFermeMs = 1000;   // chargement en cours, popup fermé (libérer à sa fin)

// ─── Mémoire des images (ADR-0057) ──────────────────────────────────────────────────
// 8 Mio : huit images 960 × 540 (1 Mio), treize 640 × 480, ou les seize vignettes
// (16 × 255 Kio, 480 × 272 décodées) et quatre grandes. PSRAM libre au repos : 23 Mo
// (docs/performance.md) ; jamais moins de kPsramReserve laissés au reste du firmware.
constexpr size_t kVuesOctetsMax = 8u * 1024u * 1024u;
constexpr size_t kPsramReserve = 6u * 1024u * 1024u;

// ─── NVS : pièce, caméra et vue choisies ────────────────────────────────────────────
constexpr uint32_t kPrefKey = 0x63616D73;  // « cams »
constexpr uint32_t kMagic = 0x43414D32;    // « CAM2 » : un champ de plus = un nouveau magic
struct Memo {
    uint32_t magic;
    uint32_t piece;      // empreinte du nom de la pièce, 0 = « Toutes »
    uint32_t camera;     // empreinte du nom de la caméra, 0 = aucune
    uint32_t mosaique;   // 1 : mosaïque, 0 : la caméra en grand
};

// La dernière image d'une caméra, gardée en PSRAM tant que le popup est ouvert.
struct Vue {
    CameraImage img;     // pixels nuls : aucune
    time_t quand;        // heure de l'image
    uint32_t vue_ms;     // dernier affichage (la plus ancienne part d'abord)
};

struct Camera {
    char nom[kCameraNomMax];
    char image[kCameraImageMax];   // vide : trop longue ou hors ligne sans image
    char piece[kCameraPieceMax];
    uint32_t hors_ligne_ha;        // horodatage Unix donné par HA, 0 : en ligne
    int8_t piece_i;                // rang de sa pièce (cameras_lire)
    uint8_t echecs;                // échecs de suite
    bool url_ko;                   // URL impossible à construire (adresse inconnue…)
    time_t premier_echec;          // heure du premier de ces échecs
    uint32_t du_ms;                // millis() à partir duquel elle peut être rechargée
    bool du_pose;                  // du_ms a un sens (sinon : tout de suite)
    Vue vue;                       // en grand (960 × 540 demandée)
    Vue mini;                      // en vignette (480 × 270 demandée)
};

// Un widget image et ses deux descripteurs, à tour de rôle ([AI-WARNING]).
struct Affichage {
    lv_obj_t* obj = nullptr;
    lv_image_dsc_t dsc[2] = {};
    uint8_t i = 0;
    const uint8_t* pixels = nullptr;   // le tampon montré, nul : aucun
};

// Tables en PSRAM (≈ 7 Ko chacune) : rien en RAM interne. Remises à zéro au démarrage.
EXT_RAM_BSS_ATTR Camera s_cams[kCamerasMax];
EXT_RAM_BSS_ATTR Camera s_tmp[kCamerasMax];   // reconstruction de la liste (cameras_recues)
int s_n = 0;
int s_np = 0;              // pièces distinctes (la pièce vide comprise)
bool s_recue = false;
uint32_t s_recue_ms = 0;
char s_adresse[128] = {};  // donnée par le blueprint (vide : celle du client de l'API)
char s_hote[64] = {};      // tirée du client de l'API « Home Assistant »
int s_piece = -1;          // pièce du filtre, -1 « Toutes »
int s_cam = 0;             // caméra montrée (index dans s_cams)
bool s_charge = false;     // un chargement est en cours dans la tâche
int s_en_cours = -1;       // sa caméra, -1 : plus dans la liste (image jetée à la fin)
bool s_en_cours_mini = false;   // ce chargement est une vignette
bool s_sans_adresse = false;
uint32_t s_demande_ms = 0;
Affichage s_grand;         // l'image du cadre
Affichage s_mini[kCamerasVignettes];   // les images des cases de la mosaïque
bool s_mosaique = true;    // vue choisie (la mosaïque demande deux caméras dans le filtre)
int s_page = 0;            // page de la mosaïque (groupes de kCamerasVignettes)
bool s_memo_lu = false;
Memo s_memo{kMagic, 0, 0, 1};
esphome::ESPPreferenceObject s_pref;

uint32_t maintenant_ms() { return esphome::millis(); }
bool passe(uint32_t ms, uint32_t t) { return static_cast<int32_t>(t - ms) >= 0; }

// FNV-1a, bit fort levé : jamais 0 (= « aucune »).
uint32_t empreinte(const char* s) {
    uint32_t h = 2166136261u;
    for (; *s != '\0'; s++) h = (h ^ static_cast<uint8_t>(*s)) * 16777619u;
    return h | 0x80000000u;
}

bool visible() {
    const CamerasUI& u = g_cameras_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

bool colonne() { return s_np >= 2; }

bool dans_filtre(int i) { return i >= 0 && i < s_n && (s_piece < 0 || s_cams[i].piece_i == s_piece); }

// Caméras du filtre, dans l'ordre du blueprint. Renvoie leur nombre.
int filtre(int out[kCamerasMax]) {
    int n = 0;
    for (int i = 0; i < s_n; i++)
        if (dans_filtre(i)) out[n++] = i;
    return n;
}

// Rang de la caméra montrée dans le filtre, -1 si elle n'y est pas.
int rang_montree(const int* f, int nf) {
    for (int k = 0; k < nf; k++)
        if (f[k] == s_cam) return k;
    return -1;
}

int nombre_filtre() {
    int f[kCamerasMax];
    return filtre(f);
}

// La mosaïque est montrée : choisie, et deux caméras ou plus dans le filtre.
bool en_mosaique() { return s_mosaique && nombre_filtre() >= 2; }

int pages_mosaique(int nf) { return (nf + kCamerasVignettes - 1) / kCamerasVignettes; }

bool hors_ligne(const Camera& c) { return c.hors_ligne_ha != 0 || c.echecs >= kEchecsHorsLigne; }

uint32_t delai_apres_echec(int echecs) {
    const int n = static_cast<int>(sizeof(kEchecsDelaisMs) / sizeof(kEchecsDelaisMs[0]));
    const int i = echecs < 1 ? 0 : (echecs > n ? n - 1 : echecs - 1);
    return kEchecsDelaisMs[i];
}

// ─── NVS ────────────────────────────────────────────────────────────────────────────

void memo_charger() {
    if (s_memo_lu) return;
    s_memo_lu = true;
    s_pref = esphome::global_preferences->make_preference<Memo>(kPrefKey);
    Memo m{};
    if (s_pref.load(&m) && m.magic == kMagic) s_memo = m;
    s_mosaique = s_memo.mosaique != 0;
}

// Pièce, caméra et vue choisies, écrites seulement si elles changent (ESPHome regroupe
// les écritures en flash : un balayage par seconde n'use rien).
void memo_sauver() {
    if (s_n == 0) return;
    Memo m{kMagic, 0, 0, s_mosaique ? 1u : 0u};
    if (s_piece >= 0) {
        for (int i = 0; i < s_n; i++)
            if (s_cams[i].piece_i == s_piece) {
                m.piece = empreinte(s_cams[i].piece);
                break;
            }
    }
    if (s_cam >= 0 && s_cam < s_n) m.camera = empreinte(s_cams[s_cam].nom);
    if (m.piece == s_memo.piece && m.camera == s_memo.camera && m.mosaique == s_memo.mosaique) return;
    s_memo = m;
    if (s_memo_lu) s_pref.save(&s_memo);
}

// ─── Affichage des images ───────────────────────────────────────────────────────────

// Échelle LVGL (256 = taille réelle) pour qu'une image tienne dans l x h : réduite si
// elle dépasse (HA envoie 1152 × 648 pour une caméra 2304 × 1296), jamais agrandie (une
// caméra 640 × 480 est montrée à sa taille).
uint32_t echelle_image(int l, int h, int cadre_l, int cadre_h) {
    if (l <= 0 || h <= 0 || (l <= cadre_l && h <= cadre_h)) return LV_SCALE_NONE;
    const uint32_t sl = static_cast<uint32_t>(cadre_l) * LV_SCALE_NONE / static_cast<uint32_t>(l);
    const uint32_t sh = static_cast<uint32_t>(cadre_h) * LV_SCALE_NONE / static_cast<uint32_t>(h);
    return sl < sh ? sl : sh;
}

// Échelle pour qu'une image COUVRE une case l x h (vignettes) : la plus grande des deux
// réductions, arrondie au-dessus (aucun bord vide) ; aucune quand elle serait légère
// (kSansEchelleMin : l'image est recadrée au centre) ou qu'il faudrait agrandir.
uint32_t echelle_couvrir(int l, int h, int case_l, int case_h) {
    if (l <= 0 || h <= 0) return LV_SCALE_NONE;
    const uint32_t sl = (static_cast<uint32_t>(case_l) * LV_SCALE_NONE + static_cast<uint32_t>(l) - 1) /
                        static_cast<uint32_t>(l);
    const uint32_t sh = (static_cast<uint32_t>(case_h) * LV_SCALE_NONE + static_cast<uint32_t>(h) - 1) /
                        static_cast<uint32_t>(h);
    const uint32_t s = sl > sh ? sl : sh;
    return s >= kSansEchelleMin ? LV_SCALE_NONE : s;
}

// `a` montre `img` (nul : rien), contenue dans l x h (le grand cadre) ou la couvrant (une
// case de la mosaïque). Un nouveau tampon passe par l'autre descripteur.
void montrer(Affichage& a, const CameraImage* img, int l, int h, bool couvrir = false) {
    if (a.obj == nullptr) return;
    if (img == nullptr || img->pixels == nullptr) {
        ui_hidden(a.obj, true);
        if (a.pixels != nullptr) {
            lv_image_set_src(a.obj, nullptr);
            lv_image_cache_drop(&a.dsc[a.i]);
            a.pixels = nullptr;
        }
        return;
    }
    if (img->pixels != a.pixels) {
        a.i = static_cast<uint8_t>(1 - a.i);
        lv_image_dsc_t& d = a.dsc[a.i];
        lv_image_cache_drop(&d);
        d = {};
        d.header.magic = LV_IMAGE_HEADER_MAGIC;
        d.header.cf = LV_COLOR_FORMAT_RGB565;
        d.header.w = static_cast<uint32_t>(img->largeur);
        d.header.h = static_cast<uint32_t>(img->hauteur);
        d.header.stride = static_cast<uint32_t>(img->pas);
        d.data = img->pixels;
        d.data_size = static_cast<uint32_t>(img->pas) * static_cast<uint32_t>(img->hauteur);
        lv_image_set_src(a.obj, &d);
        lv_image_set_scale(a.obj, couvrir ? echelle_couvrir(img->largeur, img->hauteur, l, h)
                                          : echelle_image(img->largeur, img->hauteur, l, h));
        a.pixels = img->pixels;
    }
    ui_hidden(a.obj, false);
}

// Un Affichage montre-t-il `pixels` ?
bool a_l_ecran(const uint8_t* pixels) {
    if (pixels == nullptr) return false;
    if (s_grand.pixels == pixels) return true;
    for (const Affichage& a : s_mini)
        if (a.pixels == pixels) return true;
    return false;
}

// Plus aucun Affichage ne montre `pixels` (avant de rendre son tampon).
void oublier(const uint8_t* pixels) {
    if (pixels == nullptr) return;
    if (s_grand.pixels == pixels) montrer(s_grand, nullptr, 0, 0);
    for (Affichage& a : s_mini)
        if (a.pixels == pixels) montrer(a, nullptr, 0, 0);
}

// garder = false : le tampon est libéré, pas gardé par le chargeur (vues_borner()).
void vue_rendre(Vue& v, bool garder = true) {
    oublier(v.img.pixels);
    camera_image_rendre(&v.img, garder);
    v = Vue{};
}

// Les deux images gardées d'une caméra : k < s_n la grande de s_cams[k], sinon la vignette
// de s_cams[k - s_n].
Vue& vue_k(int k) { return k < s_n ? s_cams[k].vue : s_cams[k - s_n].mini; }

size_t vues_octets() {
    size_t t = 0;
    for (int k = 0; k < 2 * s_n; k++) t += vue_k(k).img.octets;
    return t;
}

// Garde les images dans le budget : la plus anciennement vue part d'abord, jamais une
// image à l'écran. Libérée, pas gardée par le chargeur : sinon la PSRAM libre ne
// remonterait pas et tout partirait. Hors budget : le tampon de sortie du chargeur et
// le JPEG reçu (≈ 2 Mio au plus, kPsramReserve les couvre).
void vues_borner() {
    size_t total = vues_octets();
    while (total > kVuesOctetsMax || camera_psram_libre() < kPsramReserve) {
        int j = -1;
        for (int k = 0; k < 2 * s_n; k++) {
            const Vue& v = vue_k(k);
            if (v.img.pixels == nullptr || a_l_ecran(v.img.pixels)) continue;
            if (j < 0 || passe(v.vue_ms, vue_k(j).vue_ms)) j = k;
        }
        if (j < 0) return;
        total -= vue_k(j).img.octets;
        vue_rendre(vue_k(j), false);
    }
}

// Un widget image créé une fois, sous les autres enfants de `parent` (le message, le nom).
lv_obj_t* image_creer(lv_obj_t* parent, int rayon) {
    lv_obj_t* img = lv_image_create(parent);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(img, rayon, LV_PART_MAIN);
    lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_to_index(img, 0);
    return img;
}

// Les widgets image : celui du cadre et celui de chaque case. Une case garde la taille de
// la case et recadre l'image au centre (inner_align) : une vignette un peu plus grande que
// la case n'est pas transformée.
void creer_image() {
    CamerasUI& u = g_cameras_ui;
    if (u.image == nullptr && u.cadre != nullptr) {
        u.image = image_creer(u.cadre, kImageRayon);
        s_grand.obj = u.image;
    }
    for (int k = 0; k < kCamerasVignettes; k++) {
        if (u.vignette_image[k] != nullptr || u.vignette[k] == nullptr) continue;
        lv_obj_t* img = image_creer(u.vignette[k], kCaseRayon);
        lv_obj_set_size(img, kCaseL, kCaseH);
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_CENTER);
        u.vignette_image[k] = img;
        s_mini[k].obj = img;
    }
}

// ─── Textes ─────────────────────────────────────────────────────────────────────────

// « Hors ligne depuis 14:32 » (aujourd'hui), « … depuis lun. 14:32 » (la semaine),
// « … depuis le 3 oct. » (plus loin) ; « Hors ligne » sans heure connue.
void ecrire_hors_ligne(char* out, size_t n, time_t depuis) {
    const time_t maintenant = tab5_time_source(nullptr);
    if (!tab5_heure_valide(maintenant) || !tab5_heure_valide(depuis)) {
        snprintf(out, n, "%s", tr("Hors ligne"));
        return;
    }
    struct tm d{}, m{};
    localtime_r(&depuis, &d);
    localtime_r(&maintenant, &m);
    const bool meme_jour = d.tm_year == m.tm_year && d.tm_yday == m.tm_yday;
    if (meme_jour || depuis > maintenant)
        snprintf(out, n, tr("Hors ligne depuis %02d:%02d"), d.tm_hour, d.tm_min);
    else if (maintenant - depuis < 6 * 86400)
        snprintf(out, n, tr("Hors ligne depuis %s %02d:%02d"), day_short_utf8(d.tm_wday), d.tm_hour, d.tm_min);
    else
        snprintf(out, n, tr("Hors ligne depuis le %d %s"), d.tm_mday, month_short_utf8(d.tm_mon + 1));
}

// Nom d'une pièce pour sa puce : celui de sa première caméra ; la pièce vide = « Autres ».
const char* nom_piece(int p) {
    for (int i = 0; i < s_n; i++)
        if (s_cams[i].piece_i == p) return s_cams[i].piece[0] != '\0' ? s_cams[i].piece : tr("Autres");
    return "";
}

int compte_piece(int p) {
    int n = 0;
    for (int i = 0; i < s_n; i++)
        if (s_cams[i].piece_i == p) n++;
    return n;
}

// Colonne, cadre, textes dessous et pastilles : placés selon qu'il y a une colonne.
void disposer() {
    CamerasUI& u = g_cameras_ui;
    const bool col = colonne();
    const int32_t x = col ? kCadreXColonne : kCadreXCentre;
    ui_hidden(u.pieces, !col);
    ui_x(u.cadre, x);
    ui_x(u.nom, x);
    ui_x(u.heure, -(kCarteL - x - kCadreL));
    ui_x(u.pastilles, x + kCadreL / 2 - kCarteL / 2);
}

// Cases de la mosaïque pour `nc` caméras (0 : aucune) : deux côte à côte au milieu, trois
// (deux en haut, une centrée en bas) ou quatre (2 × 2) ; toujours le cadre rempli.
void disposer_cases(int nc) {
    CamerasUI& u = g_cameras_ui;
    constexpr int32_t kX2 = kCaseL + kCaseEcart;          // 484 : 2e colonne
    constexpr int32_t kY2 = kCaseH + kCaseEcart;          // 274 : 2e rangée
    constexpr int32_t kXMilieu = (kX2 + kCaseL) / 2 - kCaseL / 2;   // 242 : centrée
    constexpr int32_t kYMilieu = kY2 / 2;                 // 137 : deux au milieu
    for (int k = 0; k < kCamerasVignettes; k++) {
        lv_obj_t* v = u.vignette[k];
        if (v == nullptr) continue;
        ui_hidden(v, k >= nc);
        if (k >= nc) continue;
        int32_t x = (k % 2) * kX2, y = (k / 2) * kY2;
        if (nc == 2) y = kYMilieu;
        else if (nc == 3 && k == 2) x = kXMilieu;
        ui_x(v, x);
        ui_y(v, y);
    }
}

void peindre_puces() {
    CamerasUI& u = g_cameras_ui;
    if (!colonne()) return;
    char compte[8];
    for (int i = 0; i < kCamerasPuces; i++) {
        lv_obj_t* b = u.puce[i];
        if (b == nullptr) continue;
        const bool montree = i <= s_np;
        ui_hidden(b, !montree);
        if (!montree) continue;
        ui_text(lv_obj_get_child(b, 0), i == 0 ? tr("Toutes") : nom_piece(i - 1));
        snprintf(compte, sizeof(compte), "%d", i == 0 ? s_n : compte_piece(i - 1));
        ui_text(lv_obj_get_child(b, 1), compte);
    }
    choix_peindre(u.puce, s_np + 1, s_piece + 1);
}

// Ce que dit une caméra sans image à montrer (ou hors ligne, sur son image) ; vide si son
// image suffit.
void message_camera(const Camera& c, bool image, char* out, size_t n) {
    out[0] = '\0';
    if (hors_ligne(c))
        ecrire_hors_ligne(out, n, c.hors_ligne_ha != 0 ? static_cast<time_t>(c.hors_ligne_ha) : c.premier_echec);
    else if (image) return;
    else if (c.url_ko && s_sans_adresse) snprintf(out, n, "%s", tr("Adresse de Home Assistant inconnue"));
    else if (c.url_ko || c.echecs > 0 || c.image[0] == '\0')   // image vide : trop longue (jamais chargée)
        snprintf(out, n, "%s", tr("Image indisponible"));
    else snprintf(out, n, "%s", tr("Chargement..."));
}

// « Pièce · Caméra » (la pièce seule si la caméra n'a pas de nom ; le nom seul sans pièce).
void nom_complet(const Camera& c, char* out, size_t n) {
    if (c.piece[0] != '\0' && c.nom[0] != '\0') snprintf(out, n, "%s · %s", c.piece, c.nom);
    else snprintf(out, n, "%s", c.nom[0] != '\0' ? c.nom : c.piece);
}

// La caméra montrée en grand.
void peindre_grand(const int* f, int nf, char* message, size_t n) {
    CamerasUI& u = g_cameras_ui;
    disposer_cases(0);
    // Les vignettes ne sont plus « à l'écran » : vues_borner() peut les rendre.
    for (Affichage& a : s_mini) montrer(a, nullptr, 0, 0);
    const bool ok = s_n > 0 && s_cam >= 0 && s_cam < s_n;
    Camera* c = ok ? &s_cams[s_cam] : nullptr;
    const bool image = c != nullptr && c->vue.img.pixels != nullptr;
    const bool hl = c != nullptr && hors_ligne(*c);
    if (c != nullptr && s_recue) message_camera(*c, image, message, n);
    montrer(s_grand, image ? &c->vue.img : nullptr, kImageL, kImageH);
    if (image) {
        c->vue.vue_ms = maintenant_ms();
        ui_style_num(u.image, LV_STYLE_IMAGE_OPA, hl ? kOpaHorsLigne : static_cast<lv_opa_t>(LV_OPA_COVER));
    }
    char nom[kCameraNomMax + kCameraPieceMax + 8] = "";
    if (c != nullptr) nom_complet(*c, nom, sizeof(nom));
    ui_text(u.nom, nom);
    char heure[64] = "";
    if (image && tab5_heure_valide(c->vue.quand)) {
        struct tm t{};
        localtime_r(&c->vue.quand, &t);
        snprintf(heure, sizeof(heure),
                 (c->echecs > 0 && !hl) ? tr("Plus d'image depuis %02d:%02d:%02d") : tr("Image de %02d:%02d:%02d"), t.tm_hour,
                 t.tm_min, t.tm_sec);
    }
    ui_text(u.heure, heure);
    ui_hidden(u.pastilles, nf < 2);
    for (int i = 0; i < kCamerasPastilles; i++) ui_hidden(u.pastille[i], i >= nf);
    const int k = rang_montree(f, nf);
    pagination_afficher(u.pastille, nf, k < 0 ? 0 : k);
}

// La mosaïque : les caméras de la page s_page, une par case.
void peindre_mosaique(const int* f, int nf) {
    CamerasUI& u = g_cameras_ui;
    montrer(s_grand, nullptr, 0, 0);
    const int premiere = s_page * kCamerasVignettes;
    int nc = nf - premiere;
    if (nc > kCamerasVignettes) nc = kCamerasVignettes;
    if (nc < 0) nc = 0;
    disposer_cases(nc);
    const uint32_t t = maintenant_ms();
    // Le nom de la pièce avec « Toutes » quand il y a des pièces : on sait où est la caméra.
    const bool avec_piece = colonne() && s_piece < 0;
    for (int k = 0; k < nc; k++) {
        Camera& c = s_cams[f[premiere + k]];
        const bool image = c.mini.img.pixels != nullptr;
        montrer(s_mini[k], image ? &c.mini.img : nullptr, kCaseL, kCaseH, true);
        if (image) {
            c.mini.vue_ms = t;
            ui_style_num(u.vignette_image[k], LV_STYLE_IMAGE_OPA,
                         hors_ligne(c) ? kOpaHorsLigne : static_cast<lv_opa_t>(LV_OPA_COVER));
        }
        char nom[kCameraNomMax + kCameraPieceMax + 8];
        if (avec_piece) nom_complet(c, nom, sizeof(nom));
        else snprintf(nom, sizeof(nom), "%s", c.nom[0] != '\0' ? c.nom : c.piece);
        // Une ligne, « … » au-delà de la case (moins ses marges et celles de la pastille).
        texte_ha_coupe(u.vignette_nom[k], nom, kCaseL - 2 * 10 - 2 * 10);
        ui_hidden(u.vignette_nom[k], nom[0] == '\0');
        char message[96];
        message_camera(c, image, message, sizeof(message));
        ui_hidden(u.vignette_message[k], message[0] == '\0');
        if (message[0] != '\0') ui_text(u.vignette_message[k], message);
    }
    for (int k = nc; k < kCamerasVignettes; k++) montrer(s_mini[k], nullptr, 0, 0);
    // Sous le cadre : la pièce choisie (ou « Toutes les caméras ») et ce que fait un tap.
    const char* titre = s_piece >= 0 ? nom_piece(s_piece) : tr("Toutes les caméras");
    if (s_piece < 0 && s_np == 1 && s_cams[0].piece[0] != '\0') titre = s_cams[0].piece;
    ui_text(u.nom, titre);
    ui_text(u.heure, tr("Touchez une image pour l'agrandir"));
    const int pages = pages_mosaique(nf);
    ui_hidden(u.pastilles, pages < 2);
    for (int i = 0; i < kCamerasPastilles; i++) ui_hidden(u.pastille[i], i >= pages);
    pagination_afficher(u.pastille, pages, s_page);
}

void peindre() {
    if (!visible()) return;
    CamerasUI& u = g_cameras_ui;
    disposer();
    peindre_puces();
    int f[kCamerasMax];
    const int nf = filtre(f);
    char message[96] = "";
    if (!s_recue) snprintf(message, sizeof(message), "%s", tr("En attente de Home Assistant"));
    else if (s_n == 0) snprintf(message, sizeof(message), "%s", tr("Aucune caméra choisie"));
    if (s_recue && s_n > 0 && en_mosaique()) peindre_mosaique(f, nf);
    else peindre_grand(f, nf, message, sizeof(message));
    ui_hidden(u.message, message[0] == '\0');
    if (message[0] != '\0') ui_text(u.message, message);
}

// ─── Chargement ─────────────────────────────────────────────────────────────────────

void demander() {
    s_demande_ms = maintenant_ms();
    if (g_cameras_ui.demander != nullptr) g_cameras_ui.demander();
}

// Un seul rappel en attente (tab5_cameras_attente, mode restart) : pendant un chargement,
// jamais plus loin que la sonde suivante.
void attendre(int ms) {
    if (s_charge) {
        const int sonde = visible() ? kSondeMs : kSondeFermeMs;
        if (ms > sonde) ms = sonde;
    }
    if (ms < 0) ms = 0;
    if (g_cameras_ui.attendre != nullptr) g_cameras_ui.attendre(ms);
}

// Plus rien d'affiché : toutes les images gardées, le JPEG et la connexion à HA sont
// rendus. Pendant un chargement, le chargeur garde les siens : sa fin rappelle liberer().
void liberer() {
    montrer(s_grand, nullptr, 0, 0);
    for (Affichage& a : s_mini) montrer(a, nullptr, 0, 0);
    for (int k = 0; k < 2 * s_n; k++) vue_rendre(vue_k(k));
    camera_charge_liberer();
}

bool chargeable(const Camera& c) { return c.hors_ligne_ha == 0 && c.image[0] != '\0'; }

bool due(const Camera& c, uint32_t t) { return !c.du_pose || passe(c.du_ms, t); }

// Prochaine image à charger (*mini : une vignette), -1 : rien maintenant, *attente_ms dit
// dans combien de temps revenir voir.
//  - En grand : la caméra montrée quand son tour est venu, sinon une voisine du filtre qui
//    n'a pas encore d'image (suivante, puis précédente).
//  - En mosaïque : une vignette de la page sans image, sinon celle dont le tour est passé
//    depuis le plus longtemps.
int prochaine(uint32_t t, int* attente_ms, bool* mini) {
    int f[kCamerasMax];
    const int nf = filtre(f);
    uint32_t attente = kAttenteMaxMs;
    *mini = s_recue && s_n > 0 && en_mosaique();
    if (*mini) {
        const int premiere = s_page * kCamerasVignettes;
        int choisie = -1;
        bool choisie_sans = false;
        for (int k = premiere; k < nf && k < premiere + kCamerasVignettes; k++) {
            const Camera& c = s_cams[f[k]];
            if (!chargeable(c)) continue;
            if (!due(c, t)) {
                const uint32_t reste = c.du_ms - t;
                if (reste < attente) attente = reste;
                continue;
            }
            const bool sans = c.mini.img.pixels == nullptr;
            // Sans image d'abord ; puis le tour passé depuis le plus longtemps.
            if (choisie < 0 || (sans && !choisie_sans) ||
                (sans == choisie_sans && s_cams[choisie].du_pose && (!c.du_pose || passe(c.du_ms, s_cams[choisie].du_ms)))) {
                choisie = f[k];
                choisie_sans = sans;
            }
        }
        if (choisie >= 0) return choisie;
        *attente_ms = static_cast<int>(attente);
        return -1;
    }
    const int k = rang_montree(f, nf);
    int candidates[3];
    int nc = 0;
    if (k >= 0) {
        candidates[nc++] = s_cam;
        if (nf >= 2) candidates[nc++] = f[(k + 1) % nf];
        if (nf >= 3) candidates[nc++] = f[(k + nf - 1) % nf];
    }
    for (int j = 0; j < nc; j++) {
        const int i = candidates[j];
        const Camera& c = s_cams[i];
        if (!chargeable(c)) continue;
        if (j > 0 && c.vue.img.pixels != nullptr) continue;   // voisine déjà chargée
        if (due(c, t)) return i;
        const uint32_t reste = c.du_ms - t;
        if (reste < attente) attente = reste;
    }
    *attente_ms = static_cast<int>(attente);
    return -1;
}

void image_erreur(int cam);

// Lance le chargement de la caméra `i`, en grand ou en vignette.
void charger(int i, bool mini) {
    Camera& c = s_cams[i];
    const char* base = s_adresse[0] != '\0' ? s_adresse : s_hote;
    char url[kCameraUrlMax];
    const uint32_t t = maintenant_ms();
    if (!camera_url(Champ{c.image, std::strlen(c.image)}, base, mini ? kVignetteL : kImageL, mini ? kVignetteH : kImageH,
                    url, sizeof(url))) {
        s_sans_adresse = c.image[0] != '\0' && std::strncmp(c.image, "http", 4) != 0 && base[0] == '\0';
        // Sans l'URL : elle porte le jeton d'accès de HA.
        ESP_LOGW("tab5.cameras", "URL de la caméra %d illisible (%s)", i + 1,
                 s_sans_adresse ? "adresse de HA inconnue" : "image vide, trop longue ou refusée");
        c.url_ko = true;
        c.du_ms = t + kApresErreurMs;
        c.du_pose = true;
        peindre();
        attendre(0);
        return;
    }
    c.url_ko = false;
    s_sans_adresse = false;
    s_charge = true;
    s_en_cours = i;
    s_en_cours_mini = mini;
    if (!camera_charge_lancer(url)) {
        image_erreur(i);
        return;
    }
    peindre();
    attendre(kSondeMs);
}

void charger_suivante() {
    int attente = static_cast<int>(kAttenteMaxMs);
    bool mini = false;
    const int i = prochaine(maintenant_ms(), &attente, &mini);
    if (i >= 0) charger(i, mini);
    else attendre(attente);
}

// Fin d'un chargement réussi : l'image devient la Vue de sa caméra, grande ou vignette
// selon ce qui était demandé (l'ancienne rendue).
void image_prete(int cam) {
    s_charge = false;
    s_en_cours = -1;
    if (!visible()) {
        CameraImage img;
        if (camera_charge_prendre(&img)) camera_image_rendre(&img);
        liberer();
        return;
    }
    CameraImage img;
    if (!camera_charge_prendre(&img)) {
        camera_charge_acquitter();
        attendre(0);
        return;
    }
    if (cam < 0 || cam >= s_n) {   // retirée de la liste pendant le chargement
        camera_image_rendre(&img);
        attendre(0);
        return;
    }
    Camera& c = s_cams[cam];
    Vue& v = s_en_cours_mini ? c.mini : c.vue;
    Vue ancienne = v;
    v.img = img;
    v.quand = tab5_time_source(nullptr);
    v.vue_ms = maintenant_ms();
    c.echecs = 0;
    c.premier_echec = 0;
    // Le tour suivant, seulement si la vue n'a pas changé pendant le chargement : une
    // vignette finie après le tap sur sa case ne doit pas retarder la grande image.
    if (s_en_cours_mini == en_mosaique()) {
        c.du_ms = maintenant_ms() + (s_en_cours_mini ? kRafraichirMosaiqueMs : kRafraichirMs);
        c.du_pose = true;
    }
    peindre();                 // la nouvelle image d'abord (sans trou à l'écran)…
    vue_rendre(ancienne);      // … puis l'ancienne rendue
    vues_borner();
    attendre(0);
}

// Fin d'un chargement raté (HA injoignable, jeton périmé, caméra hors ligne, JPEG refusé)
// ou impossible à lancer. L'image gardée, s'il y en a une, reste valable.
void image_erreur(int cam) {
    s_charge = false;
    s_en_cours = -1;
    camera_charge_acquitter();
    if (!visible()) {
        liberer();
        return;
    }
    if (cam >= 0 && cam < s_n) {
        Camera& c = s_cams[cam];
        if (c.echecs < 255) c.echecs++;
        if (c.echecs == 1) c.premier_echec = tab5_time_source(nullptr);
        c.du_ms = maintenant_ms() + delai_apres_echec(c.echecs);
        c.du_pose = true;
        ESP_LOGW("tab5.cameras", "image de la caméra %d indisponible (%d échec(s) de suite)", cam + 1, c.echecs);
    }
    // Jeton périmé (401) ou caméra hors ligne : la liste redemandée, 30 s au plus souvent.
    if (maintenant_ms() - s_demande_ms >= kErreurRedemanderMs) demander();
    peindre();
    attendre(0);
}

// Les caméras de la page de la mosaïque sans vignette : chargées sans attendre.
void page_sans_attente() {
    int f[kCamerasMax];
    const int nf = filtre(f);
    const int premiere = s_page * kCamerasVignettes;
    for (int k = premiere; k < nf && k < premiere + kCamerasVignettes; k++) {
        Camera& c = s_cams[f[k]];
        if (c.mini.img.pixels == nullptr || c.echecs > 0) c.du_pose = false;
    }
}

// Après un changement de vue, de page ou de pièce : peindre, et charger tout de suite si
// la tâche est libre (sinon la fin du chargement en cours enchaîne).
void relancer() {
    ui_mark_activity();
    memo_sauver();
    peindre();
    if (!s_charge) attendre(0);
}

// ─── Pages (balayage, ADR-0046) ─────────────────────────────────────────────────────
// En mosaïque, une page = un groupe de kCamerasVignettes caméras ; en grand, une caméra.

int nombre_pages() {
    const int nf = nombre_filtre();
    return en_mosaique() ? pages_mosaique(nf) : nf;
}

int page_courante() {
    if (en_mosaique()) return s_page;
    int f[kCamerasMax];
    const int nf = filtre(f);
    return rang_montree(f, nf);
}

// Montre la caméra `i` (index dans s_cams) en grand : son image gardée tout de suite, la
// neuve dès que la tâche est libre.
void montrer_camera(int i) {
    if (i < 0 || i >= s_n || i == s_cam) return;
    s_cam = i;
    Camera& c = s_cams[i];
    // Choisie à la main : son essai suivant sans attendre, même après des échecs (son
    // compte reste : « Hors ligne » ne disparaît qu'avec une image).
    if (c.vue.img.pixels == nullptr || c.echecs > 0) c.du_pose = false;
    relancer();
}

void afficher_page(int page) {
    int f[kCamerasMax];
    const int nf = filtre(f);
    if (en_mosaique()) {
        if (page < 0 || page >= pages_mosaique(nf) || page == s_page) return;
        s_page = page;
        page_sans_attente();
        relancer();
        return;
    }
    if (page >= 0 && page < nf) montrer_camera(f[page]);
}

// Geste gauche / droite du popup (tab5_pages.cpp, ADR-0046) ; les pastilles suivent.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_page};

// Page de la mosaïque qui contient la caméra montrée (la première sinon).
void page_de_la_camera() {
    int f[kCamerasMax];
    const int nf = filtre(f);
    const int k = rang_montree(f, nf);
    s_page = k < 0 ? 0 : k / kCamerasVignettes;
}

// Pièce, caméra et vue gardées en NVS, appliquées à la première liste reçue.
bool s_memo_applique = false;
void memo_appliquer() {
    if (s_memo_applique || s_n == 0) return;
    s_memo_applique = true;
    if (s_memo.piece != 0 && colonne()) {
        for (int i = 0; i < s_n; i++)
            if (empreinte(s_cams[i].piece) == s_memo.piece) {
                s_piece = s_cams[i].piece_i;
                break;
            }
    }
    if (s_memo.camera != 0) {
        for (int i = 0; i < s_n; i++)
            if (dans_filtre(i) && empreinte(s_cams[i].nom) == s_memo.camera) {
                s_cam = i;
                break;
            }
    }
    page_de_la_camera();
}

// La caméra montrée reste dans le filtre ; sinon la première du filtre. La page de la
// mosaïque reste dans les pages.
void recaler() {
    if (s_piece >= s_np || !colonne()) s_piece = -1;
    int f[kCamerasMax];
    const int nf = filtre(f);
    if (!dans_filtre(s_cam)) s_cam = nf > 0 ? f[0] : 0;
    const int pages = pages_mosaique(nf);
    if (s_page >= pages) s_page = pages > 0 ? pages - 1 : 0;
    if (s_page < 0) s_page = 0;
}

}  // namespace

void cameras_recues(const std::string& adresse, const std::string& cameras) {
    if (payload_trop_long("tab5.cameras", cameras.size())) return;
    // Adresse : une base http(s):// qui tient, sinon ignorée (celle du client de l'API sert).
    const bool absolue = adresse.rfind("http://", 0) == 0 || adresse.rfind("https://", 0) == 0;
    if (!adresse.empty() && (adresse.size() >= sizeof(s_adresse) || !absolue)) {
        payload_refuse("tab5.cameras", "adresse illisible", adresse.size());
        s_adresse[0] = '\0';
    } else {
        snprintf(s_adresse, sizeof(s_adresse), "%s", adresse.c_str());
    }
    memo_charger();
    CameraLue lues[kCamerasMax];
    int np = 0;
    const int n = cameras_lire(Champ{cameras.data(), cameras.size()}, lues, &np);
    // Correspondance par le nom (la première d'avant de même nom, pas encore prise) : une
    // caméra garde son image, ses échecs et son tour quand la liste change d'ordre ou que
    // son jeton tourne.
    bool prise[kCamerasMax] = {};
    int ancien_de[kCamerasMax];
    for (int i = 0; i < n; i++) {
        Camera& c = s_tmp[i];
        c = Camera{};
        texte_ha_copier(c.nom, sizeof(c.nom), lues[i].nom.p, lues[i].nom.n);
        texte_ha_copier(c.piece, sizeof(c.piece), lues[i].piece.p, lues[i].piece.n);
        if (lues[i].image.n < sizeof(c.image)) std::memcpy(c.image, lues[i].image.p, lues[i].image.n);
        else payload_refuse("tab5.cameras", "image trop longue", lues[i].image.n);
        c.hors_ligne_ha = lues[i].hors_ligne;
        c.piece_i = lues[i].piece_i;
        ancien_de[i] = -1;
        for (int j = 0; j < s_n; j++) {
            if (prise[j] || std::strcmp(s_cams[j].nom, c.nom) != 0) continue;
            prise[j] = true;
            ancien_de[i] = j;
            const Camera& a = s_cams[j];
            c.vue = a.vue;
            c.mini = a.mini;
            c.echecs = a.echecs;
            c.premier_echec = a.premier_echec;
            c.du_ms = a.du_ms;
            c.du_pose = a.du_pose;
            break;
        }
        // HA la dit de nouveau en ligne : ses échecs ne comptent plus.
        if (ancien_de[i] >= 0 && s_cams[ancien_de[i]].hors_ligne_ha != 0 && c.hors_ligne_ha == 0) {
            c.echecs = 0;
            c.du_pose = false;
        }
    }
    // Les caméras retirées rendent leurs images.
    for (int j = 0; j < s_n; j++)
        if (!prise[j]) {
            vue_rendre(s_cams[j].vue);
            vue_rendre(s_cams[j].mini);
        }
    // s_cam et le chargement en cours suivent leur caméra.
    int cam = -1, en_cours = -1;
    for (int i = 0; i < n; i++) {
        if (ancien_de[i] == s_cam) cam = i;
        if (s_charge && s_en_cours >= 0 && ancien_de[i] == s_en_cours) en_cours = i;
    }
    // Pièce du filtre : retrouvée par son nom.
    char piece[kCameraPieceMax] = "";
    bool piece_vide = false;
    if (s_piece >= 0) {
        for (int j = 0; j < s_n; j++)
            if (s_cams[j].piece_i == s_piece) {
                snprintf(piece, sizeof(piece), "%s", s_cams[j].piece);
                piece_vide = piece[0] == '\0';
                break;
            }
    }
    for (int i = 0; i < n; i++) s_cams[i] = s_tmp[i];
    for (int i = n; i < s_n; i++) s_cams[i] = Camera{};
    s_n = n;
    s_np = np;
    s_cam = cam >= 0 ? cam : 0;
    if (s_charge) s_en_cours = en_cours;
    if (s_piece >= 0) {
        s_piece = -1;
        for (int i = 0; i < s_n; i++)
            if (std::strcmp(s_cams[i].piece, piece) == 0 && (piece[0] != '\0' || piece_vide)) {
                s_piece = s_cams[i].piece_i;
                break;
            }
    }
    s_recue = true;
    s_recue_ms = maintenant_ms();
    memo_appliquer();
    recaler();
    peindre();
    if (visible() && s_n > 0 && !s_charge) attendre(0);
}

void cameras_ouvrir() {
    CamerasUI& u = g_cameras_ui;
    if (u.popup == nullptr) return;
    if (s_pages.popup == nullptr) {
        // Puces et pastilles : les enfants de leur conteneur, dans l'ordre du YAML.
        for (int i = 0; i < kCamerasPuces; i++)
            u.puce[i] = u.pieces != nullptr && i < static_cast<int>(lv_obj_get_child_count(u.pieces))
                            ? lv_obj_get_child(u.pieces, i)
                            : nullptr;
        for (int i = 0; i < kCamerasPastilles; i++)
            u.pastille[i] = u.pastilles != nullptr && i < static_cast<int>(lv_obj_get_child_count(u.pastilles))
                                ? lv_obj_get_child(u.pastilles, i)
                                : nullptr;
        // Cases de la mosaïque : les enfants du cadre autres que le message (avant que
        // creer_image() n'y ajoute l'image), puis leur nom et leur message.
        if (u.cadre != nullptr) {
            int k = 0;
            const int n = static_cast<int>(lv_obj_get_child_count(u.cadre));
            for (int i = 0; i < n && k < kCamerasVignettes; i++) {
                lv_obj_t* e = lv_obj_get_child(u.cadre, i);
                if (e == u.message || e == u.image) continue;
                u.vignette[k] = e;
                u.vignette_nom[k] = lv_obj_get_child(e, 0);
                u.vignette_message[k] = lv_obj_get_child(e, 1);
                k++;
            }
        }
        s_pages.popup = u.popup;
        pages_brancher(&s_pages);
    }
    memo_charger();
    creer_image();
    recaler();
    animate_popup_open(u.popup);
    ui_mark_activity();
    // Une réouverture : chaque caméra son premier essai sans attendre.
    for (int i = 0; i < s_n; i++) {
        s_cams[i].du_pose = false;
        s_cams[i].echecs = 0;
        s_cams[i].url_ko = false;
    }
    peindre();
    if (colonne() && s_piece >= 0 && u.puce[s_piece + 1] != nullptr) {
        lv_obj_update_layout(u.pieces);   // la colonne (flex) placée avant de défiler
        lv_obj_scroll_to_view(u.puce[s_piece + 1], LV_ANIM_OFF);
    }
    // La liste à chaque ouverture : des jetons neufs. Une liste récente sert sans attendre ;
    // sinon le prochain tic redemande si rien n'est venu (un blueprint pas à jour ne répond pas).
    demander();
    const bool recente = s_recue && s_n > 0 && maintenant_ms() - s_recue_ms < kRedemanderMs;
    attendre(recente ? 0 : static_cast<int>(kErreurRedemanderMs));
}

// choix_peindre() écrit la couleur d'accent et celle du texte de chaque puce en style
// local : elles ne suivent pas un changement de thème sans être repeintes (popup ouvert).
void cameras_rejouer_theme() {
    if (g_cameras_ui.popup != nullptr) peindre_puces();
}

void cameras_puce(int n) {
    if (n < 0 || n > s_np || !colonne()) return;
    const int piece = n - 1;
    if (piece == s_piece) return;
    s_piece = piece;
    // « Toutes » garde la caméra montrée ; une pièce montre sa première caméra (ou garde
    // la montrée si elle y est). La mosaïque s'ouvre sur la page de cette caméra.
    if (!dans_filtre(s_cam)) {
        int f[kCamerasMax];
        if (filtre(f) > 0) s_cam = f[0];
        if (s_cams[s_cam].vue.img.pixels == nullptr || s_cams[s_cam].echecs > 0) s_cams[s_cam].du_pose = false;
    }
    page_de_la_camera();
    page_sans_attente();
    relancer();
}

void cameras_vignette(int n) {
    if (n < 0 || n >= kCamerasVignettes || !en_mosaique()) return;
    int f[kCamerasMax];
    const int nf = filtre(f);
    const int k = s_page * kCamerasVignettes + n;
    if (k >= nf) return;
    s_cam = f[k];
    s_mosaique = false;
    // En grand : une image neuve tout de suite (sa grande gardée, s'il y en a une, d'abord).
    s_cams[s_cam].du_pose = false;
    relancer();
}

void cameras_cadre_touche() {
    if (!s_recue || s_mosaique || nombre_filtre() < 2) return;
    s_mosaique = true;
    page_de_la_camera();
    page_sans_attente();
    relancer();
}

void cameras_tic() {
    // Un chargement en cours : sa fin d'abord (popup ouvert ou fermé).
    if (s_charge) {
        switch (camera_charge_etat()) {
            case CameraCharge::PRETE:
                image_prete(s_en_cours);
                return;
            case CameraCharge::EN_COURS:
                attendre(kSondeMs);
                return;
            default:
                image_erreur(s_en_cours);
                return;
        }
    }
    if (!visible()) {
        liberer();
        return;
    }
    const uint32_t t = maintenant_ms();
    if (!s_recue) {
        // Aucune liste encore : redemandée toutes les 30 s tant que le popup est ouvert.
        if (t - s_demande_ms >= kErreurRedemanderMs) demander();
        attendre(static_cast<int>(kErreurRedemanderMs));
        return;
    }
    if (t - s_demande_ms >= kRedemanderMs) demander();
    charger_suivante();
}

void cameras_hote_ha(const std::string& adresse) {
    ha_base_depuis_hote(adresse.c_str(), s_hote, sizeof(s_hote));
}
