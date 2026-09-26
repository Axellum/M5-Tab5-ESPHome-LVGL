/**
 * [AI-CONTEXT]
 * @file alarm_clock.h
 * @role Moteur du réveil matin + annonce des rendez-vous. Logique PURE (dates,
 *       calendrier, machine d'état) : aucun `id()` ESPHome, aucun appel réseau.
 *       Les entités exposées à Home Assistant et les scripts vivent dans
 *       Tab5/tab5-alarm.yaml ; le rendu LVGL des deux fenêtres dans
 *       ui_components/alarm_popup.yaml et alarm_ring_overlay.yaml, peints par
 *       alarm_render.h/.cpp. Aucun include ESPHome ni LVGL ici : le moteur se
 *       compile sur PC (tools/test_alarm_clock.cpp, g++ en CI — lot 8b).
 *
 * @architecture_constraint Le réveil doit sonner SANS Home Assistant. Tout ce
 *       dont il a besoin est déjà local : l'heure vient de SNTP, les horaires de
 *       travail des 15 prochains jours sont dans `cal_jours_data[]`
 *       (tab5_core.h, poussé par HA toutes les 10 min et mis en cache), et la
 *       sonnerie est une mélodie RTTTL synthétisée sur l'appareil. HA n'ajoute
 *       que du confort (annonce parlée, sonnerie personnalisée par URL).
 *
 * @perf Le tick d'1 s ne fait qu'UNE comparaison d'entiers : la prochaine
 *       sonnerie est calculée une fois puis mise en cache, et n'est recalculée
 *       que lorsque le cache est invalidé (réglage modifié, poussée calendrier,
 *       sonnerie/snooze/arrêt) ou au plus une fois par minute par sécurité
 *       (bascule heure d'été, resynchro SNTP).
 *
 * @ai_instruction Ne JAMAIS recopier ici l'arithmétique de dates : utiliser
 *       `local_day_from_offset()` de tab5_core.h, qui est la seule version
 *       correcte vis-à-vis des bascules heure d'été/hiver.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>

// ═══════════════════════════════════════════════════════════════════════════
// Configuration — miroir C++ des entités ESPHome de tab5-alarm.yaml
// ═══════════════════════════════════════════════════════════════════════════
// Le C++ ne peut pas lire `id(...)` : `script.tab5_alarm_refresh` recopie les
// entités dans `g_alarm_cfg` puis invalide le cache. Une seule direction
// (entités → struct), donc aucune divergence possible.

namespace AlarmMode {
// Heure fixe, les jours de la semaine cochés. Le calendrier est ignoré.
constexpr int FIXE = 0;
// Heure fixe, mais UNIQUEMENT les jours où le calendrier annonce du travail.
constexpr int TRAVAIL = 1;
// Heure dérivée de l'embauche : début du service − délai, borné par
// [au plus tôt, au plus tard] et par le repos minimum après la fermeture de la
// veille. C'est le mode « ouverture / fermeture ».
constexpr int EMBAUCHE = 2;
constexpr int COUNT = 3;
}  // namespace AlarmMode

namespace AlarmRepos {
constexpr int SILENCE = 0;  // jour sans travail : pas de sonnerie
constexpr int FIXE = 1;     // jour sans travail : sonner à l'heure fixe
}  // namespace AlarmRepos

struct AlarmCfg {
  bool enabled = false;
  // bit 0 = lundi … bit 6 = dimanche. NE S'APPLIQUE QU'À L'HEURE FIXE : une
  // journée pilotée par le calendrier (embauche) sonne quel que soit le jour,
  // sinon décocher le samedi ferait rater une embauche du samedi matin.
  uint8_t days_mask = 0x1F;   // lundi → vendredi
  int fixed_min = 7 * 60;     // heure fixe, en minutes depuis minuit
  int mode = AlarmMode::FIXE;
  int lead_min = 90;              // EMBAUCHE : délai avant le début du service
  int earliest_min = 5 * 60;      // EMBAUCHE : borne basse
  int latest_min = 9 * 60;        // EMBAUCHE : borne haute (gagne sur le repos min)
  int rest_hours = 0;             // EMBAUCHE : repos mini après la fermeture de la veille (0 = off)
  int rest_mode = AlarmRepos::SILENCE;
  int snooze_min = 9;
  // Sécurité : la sonnerie s'arrête seule au bout de ce délai si personne ne
  // touche rien (sinon un réveil déclenché pendant les vacances sonne des jours).
  int max_ring_min = 15;
  // Réglages de la sonnerie, que le moteur ne lit pas : recopiés ici pour que
  // les scripts et le rendu lisent une valeur déjà gardée contre le NaN d'un
  // `number` pas encore restauré, au lieu de refaire la garde à chaque usage
  // (audit du 26/09/2026, lot 7.3). Défauts = ceux des entités.
  int melody = 1;             // index de alarm_melody_name() (« Classique »)
  float volume = 0.8f;        // 0..1 (l'entité est en %)
  int rdv_lead_min = 15;      // annonce d'un rendez-vous : minutes avant
};
extern AlarmCfg g_alarm_cfg;

// ═══════════════════════════════════════════════════════════════════════════
// Calcul de la prochaine sonnerie
// ═══════════════════════════════════════════════════════════════════════════

// À appeler après TOUT changement de réglage, après chaque poussée calendrier
// (tab5_maj_previsions_jours_bulk) et après chaque sonnerie/snooze/arrêt.
void alarm_invalidate();

// Epoch de la prochaine sonnerie, 0 si aucune n'est programmée (réveil éteint,
// ou aucun jour retenu dans les 8 prochains). Recalcule si le cache est sale.
time_t alarm_next_ring(time_t now);

// true UNE SEULE FOIS quand l'heure est atteinte. Appelé par le tick d'1 s, et
// par lui seul : la fonction a des effets de bord (elle consomme le snooze et
// avance le plancher de recherche), l'appeler ailleurs ferait rater une sonnerie.
//
// Rattrapage : seul compte le temps écoulé SANS tick. En marche normale le
// plancher est le tick précédent, donc rien n'est jamais rattrapé — régler
// l'heure une minute dans le passé ne déclenche pas la sonnerie sur-le-champ.
// Au tout premier appel après un démarrage, et seulement là, la fenêtre
// ALARM_GRACE_S s'ouvre : un redémarrage ou une OTA à 06:44 ne doit pas avaler
// le réveil de 06:45. Un réveil vieux de plusieurs heures n'est jamais rattrapé.
bool alarm_due(time_t now);
constexpr int ALARM_GRACE_S = 120;

// Répète la sonnerie dans `minutes` (au moins 1). Toujours armé : il n'y a pas
// de nombre maximum de répétitions (l'ancien en-tête en promettait un, le code
// rendait toujours true). Le compte est lisible par alarm_snooze_count().
void alarm_snooze(time_t now, int minutes);
int alarm_snooze_count();

// Arrêt définitif : annule un snooze en cours et interdit à la sonnerie du jour
// de repartir (la recherche suivante démarre après `now`).
void alarm_dismiss(time_t now);

// Plancher de recherche, à sauvegarder dans un global ESPHome `restore_value`
// pour qu'un redémarrage juste après un arrêt ne refasse pas sonner le réveil
// pendant la fenêtre de grâce.
uint32_t alarm_skip_floor();
void alarm_set_skip_floor(uint32_t v);

// Remet le moteur dans l'état du démarrage (cache, plancher, snooze, dernier
// tick). Pour les tests hôte (tools/test_alarm_clock.cpp) : le firmware ne
// l'appelle pas, l'éditeur de liens l'écarte du binaire.
void alarm_reset_state();

// true dès que Home Assistant a poussé au moins un lot de prévisions/horaires
// (`cal_jours_data[]` rempli) ET que ce lot couvre encore aujourd'hui : les cases
// sont datées par `cal_jours_anchor_day` (jour local du push) et relues avec le
// bon décalage — HA muet depuis hier, aujourd'hui est la case 1 (25/09/2026).
// Tant que c'est false, les modes calendrier retombent sur l'heure fixe : mieux
// vaut sonner pour rien que rater l'embauche.
bool alarm_calendar_ready();

// ═══════════════════════════════════════════════════════════════════════════
// Libellés (popup réveil, text_sensor « Prochain réveil » exposé à HA)
// ═══════════════════════════════════════════════════════════════════════════

// « Demain 05:15 » / « Aujourd'hui 07:00 » / « Mercredi 05:15 » / « Désactivé »
std::string alarm_next_label(time_t now);
// « mercredi 6 août · Travail 06:45 – 15:30 » / « … · repos » / « … · en attente
// du calendrier ». Les DEUX libellés annoncent la répétition en cours quand il y
// en a une : c'est elle qui va sonner, pas le réveil du lendemain.
std::string alarm_next_detail(time_t now);

// ═══════════════════════════════════════════════════════════════════════════
// Préréglages de jours — le select exposé à HA, les 7 chips sur la dalle
// ═══════════════════════════════════════════════════════════════════════════
// Home Assistant reçoit un select lisible (« Lundi-Vendredi ») plutôt que sept
// interrupteurs ; le réglage fin jour par jour se fait sur la tablette. Les deux
// écrivent le MÊME masque, et le select retombe sur « Personnalisé » dès que le
// masque ne correspond à aucun préréglage.
constexpr int ALARM_DAYS_PRESET_COUNT = 5;
const char* alarm_days_preset_name(int idx);
// Masque du préréglage, -1 pour « Personnalisé » (qui ne change rien).
int alarm_days_preset_mask(int idx);
// Index du préréglage qui correspond au masque, sinon celui de « Personnalisé ».
int alarm_days_preset_index(uint8_t mask);


// ═══════════════════════════════════════════════════════════════════════════
// Mélodies RTTTL embarquées (aucun octet de flash : ce sont des chaînes)
// ═══════════════════════════════════════════════════════════════════════════
constexpr int ALARM_MELODY_COUNT = 4;
// Nom affiché (popup + select HA). Index borné en interne.
const char* alarm_melody_name(int idx);
// Partition RTTTL jouée par `rtttl.play`. Modifier une mélodie = éditer cette
// table, rien d'autre (ni police, ni flash, ni réglage HA).
const char* alarm_melody_rtttl(int idx);

// Volume d'un cycle de sonnerie (0..1) : `base` au premier cycle puis montée
// progressive jusqu'à `base` si le crescendo est actif, sinon `base` d'emblée.
// Le crescendo démarre à ALARM_CRESC_FLOOR × base pour être audible tout de suite.
float alarm_ring_gain(float base, int cycle, bool crescendo);

// ═══════════════════════════════════════════════════════════════════════════
// Rendez-vous à venir (annonce « un certain temps avant »)
// ═══════════════════════════════════════════════════════════════════════════
// Poussés par HA (script.tab5_rdv_prochains → service tab5_maj_rdv_prochains).
// Le firmware garde la liste et déclenche l'annonce LUI-MÊME à la seconde près :
// une coupure HA entre la poussée et l'échéance ne fait pas rater l'annonce.
constexpr int ALARM_RDV_MAX = 8;

// payload = "epoch|titre~epoch|titre~…" (epoch UTC du début, titres nettoyés
// des séparateurs côté HA). Une entrée déjà annoncée le reste après une nouvelle
// poussée : l'appariement se fait sur l'epoch, pas sur la position.
void rdv_store(const std::string& payload);
void rdv_clear();

// Cherche un rendez-vous dont l'échéance moins `lead_min` est atteinte et qui
// n'a pas encore été annoncé. Le marque annoncé et remplit les deux textes :
//   out_screen = « 14:30 · Dentiste (dans 15 min) »   (bandeau de la carte centrale)
//   out_speech = « Rappel : Dentiste, à 14 h 30, dans 15 minutes. »  (TTS)
bool rdv_due(time_t now, int lead_min, std::string& out_screen, std::string& out_speech);

// « 14:30 · Dentiste » du prochain rendez-vous non passé, "" s'il n'y en a pas.
std::string rdv_next_label(time_t now);

// « 07:05 » : minutes depuis minuit → HH:MM (partagé avec le rendu).
void alarm_hhmm(int minute_of_day, char* out, size_t n);
