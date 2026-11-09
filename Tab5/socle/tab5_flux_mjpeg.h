/**
 * [AI-CONTEXT]
 * @file tab5_flux_mjpeg.h
 * @role Extraction des images d'un flux HTTP « multipart/x-mixed-replace » (MJPEG), partie
 *       pure du lecteur vidéo des caméras (ADR-0073, 09/11/2026) : le chargeur
 *       (Tab5/ecran/tab5_cameras_charge.cpp) lit la réponse de /api/camera_proxy_stream/…
 *       par morceaux dans un tampon, et demande ici « y a-t-il une image complète ? ».
 *       Formats vus (source de HA 2026.11.0 et ffmpeg 9.0.2, ADR-0073) :
 *         HA, images fixes en boucle (défaut, ESPHome) :
 *           « --frameboundary\r\nContent-Type: image/jpeg\r\nContent-Length: N\r\n\r\n<JPEG>\r\n »
 *         ffmpeg (ffmpeg, ONVIF, TP-Link) :
 *           « --ffmpeg\r\nContent-type: image/jpeg\r\nContent-length: N\r\n\r\n<JPEG>\r\n »
 *           alors que HA annonce « boundary=ffserver » dans son Content-Type.
 *         plateforme « mjpeg » : le flux de la caméra recopié tel quel, son délimiteur à elle,
 *           parfois sans Content-Length.
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL, aucune allocation : compilé sur
 *       PC (tools/test_tab5_socle.cpp, g++ en CI). Le délimiteur ANNONCÉ n'est jamais lu : une
 *       ligne « -- » suivie de caractères, puis les en-têtes, puis la ligne vide. La longueur
 *       vient de Content-Length (insensible à la casse) ; à défaut, l'image va jusqu'au
 *       prochain délimiteur « \r\n-- » PRÉCÉDÉ du marqueur de fin de JPEG (FF D9), car ces
 *       quatre octets peuvent se trouver par hasard dans les données.
 * @ai_instruction extraire() relit le tampon depuis son début à chaque appel : l'appelant
 *       retire les `*consomme` premiers octets après chaque appel (memmove) et rappelle
 *       tant qu'il obtient IMAGE. Sans Content-Length, la recherche de la fin est reprise à
 *       zéro à chaque appel (quadratique sur une grande image) : acceptable parce que les
 *       caméras envoient presque toutes la longueur.
 */
#pragma once
#include <cstddef>
#include <cstdint>

namespace flux {

enum class Etat : uint8_t {
    IMAGE,       // *img décrit une image complète ; *consomme octets à retirer du tampon
    ATTENDRE,    // pas assez d'octets ; *consomme octets déjà inutiles, retirables
    ILLISIBLE,   // ce n'est pas un flux d'images JPEG en morceaux : abandon
    TROP_GRANDE, // l'image dépasse `max_image` (annoncée, ou sans fin trouvée à temps)
    FIN,         // délimiteur de clôture « --nom-- » : le flux est fini
};

struct Image {
    size_t debut = 0;   // décalage du premier octet du JPEG dans le tampon
    size_t n = 0;       // sa taille
};

constexpr size_t kLigneMax = 160;    // ligne de délimiteur
constexpr size_t kEnteteMax = 512;   // tous les en-têtes d'une image, lignes vides comprises

namespace detail {

inline bool crlf(const uint8_t* b, size_t i) { return b[i] == '\r' && b[i + 1] == '\n'; }

// Début de la ligne finissant par « \r\n » à partir de `de` : renvoie la position du « \r »,
// ou `n` si le tampon se termine avant (la ligne peut encore venir).
inline size_t fin_de_ligne(const uint8_t* b, size_t n, size_t de) {
    for (size_t i = de; i + 1 < n; i++)
        if (crlf(b, i)) return i;
    return n;
}

inline char minuscule(uint8_t c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : static_cast<char>(c); }

// La ligne [de, fin) est-elle « Content-Length: <n> » (casse libre, espaces autour) ? Écrit n.
// Plus de dix chiffres ou un caractère autre qu'un chiffre : refusée (valeur ignorée).
inline bool longueur(const uint8_t* b, size_t de, size_t fin, uint64_t* n) {
    static const char kNom[] = "content-length:";
    constexpr size_t kLg = sizeof(kNom) - 1;
    if (fin - de < kLg) return false;
    for (size_t i = 0; i < kLg; i++)
        if (minuscule(b[de + i]) != kNom[i]) return false;
    size_t i = de + kLg;
    while (i < fin && (b[i] == ' ' || b[i] == '\t')) i++;
    size_t chiffres = 0;
    uint64_t v = 0;
    for (; i < fin && b[i] >= '0' && b[i] <= '9'; i++, chiffres++) v = v * 10 + static_cast<uint64_t>(b[i] - '0');
    while (i < fin && (b[i] == ' ' || b[i] == '\t')) i++;
    if (chiffres == 0 || chiffres > 10 || i != fin) return false;
    *n = v;
    return true;
}

}  // namespace detail

// Cherche la première image complète de tampon[0, n). `max_image` : taille maximale d'un JPEG.
inline Etat extraire(const uint8_t* tampon, size_t n, size_t max_image, Image* img, size_t* consomme) {
    using namespace detail;
    *img = Image{};
    *consomme = 0;
    // Les octets de fin de l'image précédente (« \r\n ») sont à jeter.
    size_t pos = 0;
    while (pos < n && (tampon[pos] == '\r' || tampon[pos] == '\n')) pos++;
    *consomme = pos;
    if (pos + 2 > n) return Etat::ATTENDRE;
    if (tampon[pos] != '-' || tampon[pos + 1] != '-') return Etat::ILLISIBLE;
    // Ligne de délimiteur.
    const size_t fin_delim = fin_de_ligne(tampon, n, pos);
    if (fin_delim >= n) return n - pos > kLigneMax ? Etat::ILLISIBLE : Etat::ATTENDRE;
    if (fin_delim - pos > kLigneMax) return Etat::ILLISIBLE;
    if (fin_delim - pos >= 4 && tampon[fin_delim - 1] == '-' && tampon[fin_delim - 2] == '-') return Etat::FIN;
    // En-têtes, jusqu'à la ligne vide.
    size_t ligne = fin_delim + 2;
    uint64_t annonce = 0;
    bool longueur_connue = false;
    size_t corps = 0;
    for (;;) {
        const size_t fin_ligne = fin_de_ligne(tampon, n, ligne);
        if (fin_ligne >= n) return n - pos > kLigneMax + kEnteteMax ? Etat::ILLISIBLE : Etat::ATTENDRE;
        if (fin_ligne == ligne) {   // ligne vide
            corps = ligne + 2;
            break;
        }
        if (fin_ligne - fin_delim > kEnteteMax) return Etat::ILLISIBLE;
        uint64_t v = 0;
        if (longueur(tampon, ligne, fin_ligne, &v)) {
            annonce = v;
            longueur_connue = true;
        }
        ligne = fin_ligne + 2;
    }
    // Corps.
    if (longueur_connue) {
        if (annonce < 4) return Etat::ILLISIBLE;
        if (annonce > max_image) return Etat::TROP_GRANDE;
        const size_t lg = static_cast<size_t>(annonce);
        if (n - corps < lg) return Etat::ATTENDRE;
        if (tampon[corps] != 0xFF || tampon[corps + 1] != 0xD8) return Etat::ILLISIBLE;
        img->debut = corps;
        img->n = lg;
        *consomme = corps + lg;
        return Etat::IMAGE;
    }
    if (n - corps < 2) return Etat::ATTENDRE;
    if (tampon[corps] != 0xFF || tampon[corps + 1] != 0xD8) return Etat::ILLISIBLE;
    for (size_t i = corps + 2; i + 4 <= n; i++) {
        if (tampon[i] == '\r' && tampon[i + 1] == '\n' && tampon[i + 2] == '-' && tampon[i + 3] == '-' &&
            tampon[i - 2] == 0xFF && tampon[i - 1] == 0xD9) {
            if (i - corps > max_image) return Etat::TROP_GRANDE;
            img->debut = corps;
            img->n = i - corps;
            *consomme = i;   // le « \r\n » devant le délimiteur suivant est jeté à l'appel d'après
            return Etat::IMAGE;
        }
    }
    return n - corps > max_image ? Etat::TROP_GRANDE : Etat::ATTENDRE;
}

}  // namespace flux
