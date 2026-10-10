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
 *         - Image : cameras_image (online_image, tab5-cameras.yaml), une seule pour toutes
 *           les caméras ; URL construite par camera_url() (tab5_parse.h, section 9).
 *         - Fermé (croix, retour automatique après 45 s sans toucher, autre écran) : le
 *           rappel suivant (cameras_tic) ou la fin du téléchargement en cours libère
 *           l'image décodée, et plus rien n'est demandé.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : la tablette ne
 *       nomme aucune entité ; elle dit seulement qu'elle veut la liste. Le téléchargement
 *       bloque la boucle principale le temps que HA réponde (http_request est synchrone
 *       jusqu'aux en-têtes) puis le temps du décodage JPEG (un seul appel) : d'où une
 *       image à la fois, jamais deux, et rien quand le popup est fermé. Une caméra hors
 *       ligne (ou un HA qui ne répond pas) gèle l'écran jusqu'au timeout de http_request
 *       (12 s, tab5-assist.yaml) à CHAQUE essai : après un échec, l'essai suivant attend
 *       10 s, puis 30 s, puis 60 s (kEchecsDelaisMs), remis à zéro par une image reçue, un
 *       changement de page ou une réouverture. Ne pas raccourcir ces délais sans l'avoir
 *       mesuré sur la tablette avec une caméra débranchée.
 *       Un téléchargement sans fin ni échec au bout de 60 s est abandonné par
 *       online_image.release (connexion fermée), jamais seulement oublié.
 *       Couleurs : styles de rôle du YAML (cameras_popup.yaml) ; ce fichier n'écrit que du
 *       texte, des affichages et la source de l'image. Pas de cameras_rejouer_theme : rien
 *       n'y est peint d'une couleur.
 * @ai_warning [AI-WARNING] Le widget image montre directement le tampon de cameras_image
 *       (lv_image_dsc_t d'ESPHome). online_image le LIBÈRE et remet son descripteur à zéro
 *       sur un échec de décodage, sur release() et quand la taille de l'image change :
 *       le widget doit être caché avant (peindre() le cache dès que la caméra montrée
 *       n'est pas celle du tampon, cameras_image_erreur() quand la largeur est retombée à
 *       0). Ne pas le montrer « en attendant » une image d'une autre caméra.
 * @ai_instruction Un texte affiché passe par tr(). Le format de tab5_maj_cameras est un
 *       contrat avec le blueprint, la démo (tools/demo/) et le rendu (tools/rendu/).
 */
#include "tab5_internal.h"
#include "tab5_parse.h"
#include "esphome/components/image/image.h"
#include "lvgl.h"
#include <cstdio>
#include <cstring>
#include <ctime>

CamerasUI g_cameras_ui;

static_assert(kCamerasPastilles == kCamerasMax, "une pastille par caméra");

namespace {

// Taille de l'image (cameras_popup.yaml, resize de tab5-cameras.yaml) : 16:9, réduite par
// HA au plus petit facteur JPEG qui reste au-dessus (1/2 d'une 1080p, 3/8 d'une 1440p).
constexpr int kImageL = 960;
constexpr int kImageH = 540;
constexpr int kRafraichirMs = 5000;            // après une image, avant la suivante
constexpr int kApresErreurMs = 10000;          // après un échec (URL illisible, 1er échec réseau)
constexpr uint32_t kRedemanderMs = 240000;     // liste (jetons) redemandée à HA
constexpr uint32_t kErreurRedemanderMs = 30000;   // après un échec, et tant que rien n'est reçu
// Échecs réseau de suite sur la page montrée : 10 s, 30 s, puis 60 s avant le suivant.
// Une caméra hors ligne gèle l'écran jusqu'au timeout de http_request (12 s) à chaque
// essai : plus ils sont espacés, moins l'écran gèle. Remis à zéro par une image reçue ou
// un changement de page.
constexpr int kEchecsDelaisMs[] = {10000, 30000, 60000};
constexpr uint32_t kTelechargementPerduMs = 60000;   // au-delà, plus rien n'est attendu

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
int s_en_cours = -1;        // caméra du téléchargement en cours, -1 aucun
uint32_t s_en_cours_ms = 0;
int s_montree = -1;         // caméra dont l'image est dans le tampon, -1 aucune
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

bool url_absolue(const char* s) { return std::strncmp(s, "http://", 7) == 0 || std::strncmp(s, "https://", 8) == 0; }

void demander() {
    s_demande_ms = esphome::millis();
    if (g_cameras_ui.demander != nullptr) g_cameras_ui.demander();
}

void attendre(int ms) {
    if (g_cameras_ui.attendre != nullptr) g_cameras_ui.attendre(ms);
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

// Plus rien d'affiché : l'image décodée (~1 Mo de PSRAM) est rendue et une connexion en
// cours est fermée (online_image.release). Le tampon de téléchargement d'ESPHome, lui, ne
// rétrécit pas : il garde la taille du plus gros JPEG reçu.
void liberer() {
    ui_hidden(g_cameras_ui.image, true);
    s_montree = -1;
    if (g_cameras_ui.liberer != nullptr) g_cameras_ui.liberer();
}

// Télécharge l'image de la page montrée, s'il n'y a pas déjà un téléchargement en cours.
void charger() {
    const CamerasUI& u = g_cameras_ui;
    if (s_en_cours >= 0 || s_n == 0 || u.charger == nullptr) return;
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
    s_en_cours_ms = esphome::millis();
    peindre();   // « Chargement... » d'abord : le téléchargement bloque la boucle
    // Peut rappeler cameras_image_erreur() tout de suite (URL refusée, HA injoignable).
    u.charger(url);
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
    // Un téléchargement en cours (autre caméra) : sa fin enchaîne sur celle-ci.
    if (s_en_cours < 0) attendre(0);
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
    if (!visible()) {
        // Un téléchargement en cours libérera à sa fin (cameras_image_prete / _erreur).
        if (s_en_cours < 0) liberer();
        return;
    }
    const uint32_t maintenant = esphome::millis();
    if (!s_recue) {
        // Aucune liste encore : redemandée toutes les 30 s tant que le popup est ouvert.
        if (maintenant - s_demande_ms >= kErreurRedemanderMs) demander();
        attendre(static_cast<int>(kErreurRedemanderMs));
        return;
    }
    if (s_en_cours >= 0) {
        if (maintenant - s_en_cours_ms < kTelechargementPerduMs) return;   // sa fin enchaîne
        // Jamais fini ni échoué : la connexion est fermée et le tampon rendu (release),
        // pour qu'une image en retard ne s'affiche pas sur la page d'une autre caméra et
        // que le téléchargement suivant puisse partir.
        liberer();
        s_en_cours = -1;
        s_erreur = true;
        s_echecs++;
        peindre();
        attendre(delai_apres_echec());
        return;
    }
    if (maintenant - s_demande_ms >= kRedemanderMs) demander();
    charger();
}

void cameras_image_prete() {
    CamerasUI& u = g_cameras_ui;
    const int cam = s_en_cours;
    s_en_cours = -1;
    if (!visible()) {
        liberer();
        return;
    }
    if (cam < 0) return;
    // Le tampon est celui de `cam`, montrée ou non : le widget le suit dès maintenant
    // (lv_image_set_src relit le descripteur, que le tampon ait bougé ou non, et invalide).
    s_montree = cam;
    s_quand = tab5_time_source(nullptr);
    if (u.image != nullptr && u.source != nullptr) lv_image_set_src(u.image, u.source->get_lv_image_dsc());
    if (cam == s_page) {
        s_erreur = false;
        s_echecs = 0;
    }
    peindre();
    attendre(cam == s_page ? kRafraichirMs : 0);
}

void cameras_image_erreur() {
    CamerasUI& u = g_cameras_ui;
    const int cam = s_en_cours;
    s_en_cours = -1;
    // Échec pendant le décodage : online_image a libéré le tampon (largeur 0).
    if (u.source == nullptr || u.source->get_width() <= 0) {
        ui_hidden(u.image, true);
        s_montree = -1;
    }
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

void cameras_hote_ha(const std::string& adresse) {
    ha_base_depuis_hote(adresse.c_str(), s_hote, sizeof(s_hote));
}
