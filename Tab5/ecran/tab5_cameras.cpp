/**
 * [AI-CONTEXT]
 * @file tab5_cameras.cpp
 * @role Popup « Caméras » (ADR-0049, 09/10/2026, discussion #278) : l'image fixe des
 *       caméras de Home Assistant, une page par caméra, rafraîchie tant que le popup est
 *       ouvert. Pas de vidéo : un JPEG à la fois, demandé 5 s après la fin du précédent.
 *         - Liste : action tab5_maj_cameras (adresse, cameras), poussée par le blueprint
 *           en réponse à esphome.tab5_cameras, émis à l'ouverture, toutes les 4 minutes
 *           tant que le popup reste ouvert (le jeton des URL de HA change toutes les
 *           5 minutes, l'ancien reste valable 5 minutes de plus) et après un échec
 *           (30 s au plus souvent : jeton périmé, caméra hors ligne).
 *         - Image : téléchargée et décodée HORS de la boucle principale par le chargeur
 *           (tab5_cameras_charge.h : tâche FreeRTOS, décodeur JPEG matériel, deux
 *           tampons), une à la fois ; URL construite par camera_url() (tab5_parse.h,
 *           section 9). La boucle sonde le chargeur toutes les 100 ms (kSondeMs) et
 *           montre l'image prête : quelques millisecondes, l'écran ne gèle plus.
 *         - Fermé (croix, retour automatique après 45 s sans toucher, autre écran) : le
 *           rappel suivant (cameras_tic), ou la fin du chargement en cours, rend la mémoire
 *           (images, JPEG, connexion), et plus rien n'est demandé.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : la tablette ne
 *       nomme aucune entité ; elle dit seulement qu'elle veut la liste. Une image à la
 *       fois, jamais deux, et rien quand le popup est fermé. Une caméra hors ligne (ou un
 *       HA qui ne répond pas) ne gèle plus l'écran, mais occupe la tâche jusqu'à 12 s :
 *       après un échec, l'essai suivant attend 10 s, puis 30 s, puis 60 s
 *       (kEchecsDelaisMs), remis à zéro par une image reçue, un changement de page ou une
 *       réouverture.
 *       Couleurs : styles de rôle du YAML (cameras_popup.yaml) ; ce fichier n'écrit que du
 *       texte, des affichages et la source de l'image. Pas de cameras_rejouer_theme : rien
 *       n'y est peint d'une couleur. Le widget image est créé ici (dans cameras_cadre) :
 *       l'image: d'ESPHome exige une source fixe.
 * @ai_warning [AI-WARNING] Le widget image montre directement un tampon du chargeur.
 *       Avant camera_charge_liberer(), liberer() le cache et retire sa source ; après un
 *       échec, l'image montrée reste valable (le chargeur écrit dans l'autre tampon) et
 *       reste à l'écran avec « Plus d'image depuis … ». Ne pas la montrer « en attendant »
 *       sur la page d'une autre caméra (peindre() la cache dès que la caméra montrée n'est
 *       pas celle de l'image).
 * @ai_instruction Un texte affiché passe par tr(). Le format de tab5_maj_cameras est un
 *       contrat avec le blueprint, la démo (tools/demo/) et le rendu (tools/rendu/).
 */
#include "tab5_internal.h"
#include "tab5_cameras_charge.h"
#include "tab5_parse.h"
#include "lvgl.h"
#include "lvgl_private.h"  // lv_image_cache_drop() (cache d'images, hors de lvgl.h en 9.5)
#include <cstdio>
#include <cstring>
#include <ctime>

CamerasUI g_cameras_ui;

static_assert(kCamerasPastilles == kCamerasMax, "une pastille par caméra");

namespace {

// Taille du cadre de l'image (cameras_popup.yaml) : 16:9, demandée à HA, qui réduit au
// plus petit facteur JPEG qui reste au-dessus (1/2 d'une 1080p, 3/8 d'une 1440p). Une
// image plus petite est montrée à sa taille, une plus grande réduite par echelle_image().
constexpr int kImageL = 960;
constexpr int kImageH = 540;
constexpr int kRafraichirMs = 5000;            // après une image, avant la suivante
constexpr int kApresErreurMs = 10000;          // après un échec (URL illisible, 1er échec réseau)
constexpr uint32_t kRedemanderMs = 240000;     // liste (jetons) redemandée à HA
constexpr uint32_t kErreurRedemanderMs = 30000;   // après un échec, et tant que rien n'est reçu
// Échecs réseau de suite sur la page montrée : 10 s, 30 s, puis 60 s avant le suivant
// (une caméra hors ligne occupe la tâche jusqu'à 12 s, et HA, à chaque essai). Remis à
// zéro par une image reçue ou un changement de page.
constexpr int kEchecsDelaisMs[] = {10000, 30000, 60000};
constexpr int kSondeMs = 100;         // chargement en cours, popup ouvert
constexpr int kSondeFermeMs = 1000;   // chargement en cours, popup fermé (libérer à sa fin)
constexpr int kImageRayon = 18;       // coins de l'image (cadre style_glass_card)

struct Camera {
    char nom[kCameraNomMax] = {};
    char image[kCameraImageMax] = {};   // vide : trop longue, l'écran dit « Image indisponible »
};

Camera s_cams[kCamerasMax];
int s_n = 0;
bool s_recue = false;
uint32_t s_recue_ms = 0;
char s_adresse[128] = {};   // donnée par le blueprint (vide : celle du client de l'API)
char s_hote[64] = {};       // tirée du client de l'API « Home Assistant »
int s_page = 0;
int s_en_cours = -1;        // caméra du chargement en cours, -1 aucun
int s_montree = -1;         // caméra dont l'image est montrée, -1 aucune
// Deux descripteurs, un par image reçue à tour de rôle : LVGL ne garde rien d'une image
// sous le pointeur d'une autre (lv_image_cache_drop en plus).
lv_image_dsc_t s_dsc[2] = {};
int s_dsc_i = 0;
time_t s_quand = 0;         // heure de cette image
bool s_erreur = false;      // le dernier téléchargement de la page a échoué
bool s_sans_adresse = false;
uint32_t s_demande_ms = 0;
int s_echecs = 0;           // échecs réseau de suite sur la page montrée

int delai_apres_echec() {
    const int n = static_cast<int>(sizeof(kEchecsDelaisMs) / sizeof(kEchecsDelaisMs[0]));
    const int i = s_echecs < 1 ? 0 : (s_echecs > n ? n - 1 : s_echecs - 1);
    return kEchecsDelaisMs[i];
}

bool visible() {
    const CamerasUI& u = g_cameras_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

// Échelle LVGL (256 = taille réelle) pour qu'une image tienne dans le cadre : réduite si
// elle dépasse kImageL × kImageH (HA envoie 1152 × 648 pour une caméra 2304 × 1296),
// jamais agrandie (le popup montre une caméra 640 × 480 à sa taille).
uint32_t echelle_image(int l, int h) {
    if (l <= 0 || h <= 0 || (l <= kImageL && h <= kImageH)) return LV_SCALE_NONE;
    const uint32_t sl = static_cast<uint32_t>(kImageL) * LV_SCALE_NONE / static_cast<uint32_t>(l);
    const uint32_t sh = static_cast<uint32_t>(kImageH) * LV_SCALE_NONE / static_cast<uint32_t>(h);
    return sl < sh ? sl : sh;
}

bool url_absolue(const char* s) { return std::strncmp(s, "http://", 7) == 0 || std::strncmp(s, "https://", 8) == 0; }

void demander() {
    s_demande_ms = esphome::millis();
    if (g_cameras_ui.demander != nullptr) g_cameras_ui.demander();
}

// Un seul rappel en attente (tab5_cameras_attente, mode restart) : pendant un chargement,
// jamais plus loin que la sonde suivante.
void attendre(int ms) {
    if (s_en_cours >= 0) {
        const int sonde = visible() ? kSondeMs : kSondeFermeMs;
        if (ms > sonde) ms = sonde;
    }
    if (g_cameras_ui.attendre != nullptr) g_cameras_ui.attendre(ms);
}

// Le widget image, créé une fois dans le cadre, sous le message.
void creer_image() {
    CamerasUI& u = g_cameras_ui;
    if (u.image != nullptr || u.cadre == nullptr) return;
    u.image = lv_image_create(u.cadre);
    lv_obj_align(u.image, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(u.image, kImageRayon, LV_PART_MAIN);
    lv_obj_add_flag(u.image, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(u.image, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_to_index(u.image, 0);
}

void peindre() {
    if (!visible()) return;
    const CamerasUI& u = g_cameras_ui;
    const bool image = s_n > 0 && s_montree == s_page;
    const char* message = nullptr;
    if (!s_recue) message = tr_noop("En attente de Home Assistant");
    else if (s_n == 0) message = tr_noop("Aucune caméra choisie");
    else if (image) message = nullptr;
    else if (s_sans_adresse) message = tr_noop("Adresse de Home Assistant inconnue");
    else if (s_erreur) message = tr_noop("Image indisponible");
    else message = tr_noop("Chargement...");
    ui_hidden(u.image, !image);
    ui_hidden(u.message, message == nullptr);
    if (message != nullptr) ui_text(u.message, tr(message));
    ui_text(u.nom, s_n > 0 ? s_cams[s_page].nom : "");
    char heure[64] = "";
    if (image && tab5_heure_valide(s_quand)) {
        struct tm t{};
        localtime_r(&s_quand, &t);
        snprintf(heure, sizeof(heure), s_erreur ? tr("Plus d'image depuis %02d:%02d:%02d") : tr("Image de %02d:%02d:%02d"),
                 t.tm_hour, t.tm_min, t.tm_sec);
    }
    ui_text(u.heure, heure);
    ui_hidden(u.pastilles, s_n < 2);
    for (int i = 0; i < kCamerasPastilles; i++) ui_hidden(u.pastille[i], i >= s_n);
    pagination_afficher(u.pastille, s_n, s_page);
}

// Plus rien d'affiché : les deux images (2 × 600 Ko de PSRAM pour une caméra 640 × 480),
// le JPEG et la connexion à HA sont rendus. Pendant un chargement, rien n'est rendu : sa
// fin rappelle liberer() (popup fermé).
void liberer() {
    CamerasUI& u = g_cameras_ui;
    if (u.image != nullptr) {
        ui_hidden(u.image, true);
        lv_image_set_src(u.image, nullptr);
    }
    s_montree = -1;
    camera_charge_liberer();
}

void image_prete(int cam);
void image_erreur(int cam);

// Lance le chargement de l'image de la page montrée, s'il n'y en a pas déjà un en cours.
void charger() {
    if (s_en_cours >= 0 || s_n == 0) return;
    const Camera& c = s_cams[s_page];
    const char* base = s_adresse[0] != '\0' ? s_adresse : s_hote;
    char url[kCameraUrlMax];
    if (!camera_url(Champ{c.image, std::strlen(c.image)}, base, kImageL, kImageH, url, sizeof(url))) {
        s_sans_adresse = c.image[0] != '\0' && !url_absolue(c.image) && base[0] == '\0';
        // Sans l'URL : elle porte le jeton d'accès de HA.
        ESP_LOGW("tab5.cameras", "URL de la caméra %d illisible (%s)", s_page + 1,
                 s_sans_adresse ? "adresse de HA inconnue" : "image vide, trop longue ou refusée");
        s_erreur = true;
        peindre();
        attendre(kApresErreurMs);
        return;
    }
    s_sans_adresse = false;
    s_en_cours = s_page;
    peindre();
    if (!camera_charge_lancer(url)) {
        image_erreur(s_en_cours);
        return;
    }
    attendre(kSondeMs);
}

int nombre_pages() { return s_n; }
int page_courante() { return s_n > 0 ? s_page : -1; }

void afficher_page(int page) {
    if (page < 0 || page >= s_n || page == s_page) return;
    s_page = page;
    s_erreur = false;
    s_echecs = 0;   // une autre caméra : son premier essai sans attendre
    ui_mark_activity();
    peindre();
    // Un chargement en cours (autre caméra) : sa fin enchaîne sur celle-ci.
    if (s_en_cours < 0) attendre(0);
}

// Fin d'un chargement réussi : l'image de `cam` devient celle du widget, montrée ou non
// (peindre() ne la montre que sur sa page).
void image_prete(int cam) {
    CamerasUI& u = g_cameras_ui;
    s_en_cours = -1;
    if (!visible()) {
        liberer();
        return;
    }
    CameraImage img;
    if (cam < 0 || u.image == nullptr || !camera_charge_prendre(&img)) {
        camera_charge_acquitter();
        attendre(0);
        return;
    }
    s_dsc_i = 1 - s_dsc_i;
    lv_image_dsc_t& d = s_dsc[s_dsc_i];
    lv_image_cache_drop(&d);
    d = {};
    d.header.magic = LV_IMAGE_HEADER_MAGIC;
    d.header.cf = LV_COLOR_FORMAT_RGB565;
    d.header.w = static_cast<uint32_t>(img.largeur);
    d.header.h = static_cast<uint32_t>(img.hauteur);
    d.header.stride = static_cast<uint32_t>(img.pas);
    d.data = img.pixels;
    d.data_size = static_cast<uint32_t>(img.pas) * static_cast<uint32_t>(img.hauteur);
    lv_image_set_src(u.image, &d);
    lv_image_set_scale(u.image, echelle_image(img.largeur, img.hauteur));
    s_montree = cam;
    s_quand = tab5_time_source(nullptr);
    if (cam == s_page) {
        s_erreur = false;
        s_echecs = 0;
    }
    peindre();
    attendre(cam == s_page ? kRafraichirMs : 0);
}

// Fin d'un chargement raté (HA injoignable, jeton périmé, caméra hors ligne, JPEG refusé)
// ou impossible à lancer. L'image montrée, s'il y en a une, reste valable.
void image_erreur(int cam) {
    s_en_cours = -1;
    camera_charge_acquitter();
    if (!visible()) {
        liberer();
        return;
    }
    if (cam == s_page) {
        s_erreur = true;
        s_echecs++;
    }
    ESP_LOGW("tab5.cameras", "image de la caméra %d indisponible (%d échec(s) de suite)", cam + 1, s_echecs);
    // Jeton périmé (401) ou caméra hors ligne : la liste redemandée, 30 s au plus souvent.
    if (esphome::millis() - s_demande_ms >= kErreurRedemanderMs) demander();
    peindre();
    attendre(cam == s_page ? delai_apres_echec() : 0);
}

// Geste gauche / droite du popup (tab5_pages.cpp, ADR-0046) ; les pastilles suivent.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_page};

}  // namespace

void cameras_recues(const std::string& adresse, const std::string& cameras) {
    if (payload_trop_long("tab5.cameras", cameras.size())) return;
    // Adresse : une base http(s):// qui tient, sinon ignorée (celle du client de l'API sert).
    if (!adresse.empty() && (adresse.size() >= sizeof(s_adresse) || !url_absolue(adresse.c_str()))) {
        payload_refuse("tab5.cameras", "adresse illisible", adresse.size());
        s_adresse[0] = '\0';
    } else {
        snprintf(s_adresse, sizeof(s_adresse), "%s", adresse.c_str());
    }
    CameraLue lues[kCamerasMax];
    const int n = cameras_lire(Champ{cameras.data(), cameras.size()}, lues);
    for (int i = 0; i < n; i++) {
        Camera c;
        texte_ha_copier(c.nom, sizeof(c.nom), lues[i].nom.p, lues[i].nom.n);
        if (lues[i].image.n < sizeof(c.image)) std::memcpy(c.image, lues[i].image.p, lues[i].image.n);
        else payload_refuse("tab5.cameras", "image trop longue", lues[i].image.n);
        // Le tampon garde son image tant que c'est la même caméra (même nom, même place) :
        // un jeton neuf ne la fait pas disparaître.
        if (i == s_montree && std::strcmp(c.nom, s_cams[i].nom) != 0) s_montree = -1;
        s_cams[i] = c;
    }
    if (s_montree >= n) s_montree = -1;
    s_n = n;
    s_recue = true;
    s_recue_ms = esphome::millis();
    if (s_page >= s_n) s_page = 0;
    // Plus aucune caméra : plus rien ne relancera cameras_tic, l'image est rendue ici.
    if (s_n == 0 && s_en_cours < 0 && s_montree >= 0) liberer();
    peindre();
    if (visible() && s_n > 0 && s_en_cours < 0) attendre(0);
}

void cameras_ouvrir() {
    CamerasUI& u = g_cameras_ui;
    if (u.popup == nullptr) return;
    if (s_pages.popup == nullptr) {
        s_pages.popup = u.popup;
        pages_brancher(&s_pages);
    }
    creer_image();
    if (s_page >= s_n) s_page = 0;
    s_erreur = false;
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre();
    s_echecs = 0;
    // La liste à chaque ouverture : des jetons neufs. Une liste récente sert sans attendre ;
    // sinon le prochain tic redemande si rien n'est venu (un blueprint pas à jour ne répond pas).
    demander();
    const bool recente = s_recue && s_n > 0 && esphome::millis() - s_recue_ms < kRedemanderMs;
    attendre(recente ? 0 : static_cast<int>(kErreurRedemanderMs));
}

void cameras_tic() {
    // Un chargement en cours : sa fin d'abord (popup ouvert ou fermé).
    if (s_en_cours >= 0) {
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
    const uint32_t maintenant = esphome::millis();
    if (!s_recue) {
        // Aucune liste encore : redemandée toutes les 30 s tant que le popup est ouvert.
        if (maintenant - s_demande_ms >= kErreurRedemanderMs) demander();
        attendre(static_cast<int>(kErreurRedemanderMs));
        return;
    }
    if (maintenant - s_demande_ms >= kRedemanderMs) demander();
    charger();
}

void cameras_hote_ha(const std::string& adresse) {
    ha_base_depuis_hote(adresse.c_str(), s_hote, sizeof(s_hote));
}
