/**
 * [AI-CONTEXT]
 * @file alarm_clock.cpp
 * @role Implémentation du moteur de réveil + annonce des rendez-vous — logique
 *       PURE (ni ESPHome ni LVGL), compilée aussi par tools/test_alarm_clock.cpp
 *       (g++ en CI). Le rendu LVGL est dans alarm_render.cpp (lot 8b, 25/09/2026).
 *       Contrat et justifications d'architecture : voir alarm_clock.h.
 */
#include "alarm_clock.h"
#include "tab5_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

AlarmCfg g_alarm_cfg;

// ═══════════════════════════════════════════════════════════════════════════
// Helpers de bas niveau
// ═══════════════════════════════════════════════════════════════════════════

static int clamp_i(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

// "HH:MM-HH:MM" -> minutes depuis minuit du DÉBUT, -1 si illisible.
// Le champ vient de `cal_jours_data[].heures_ouverture`, rempli par
// parse_and_update_jours_bulk() depuis l'automation HA (section 5). Il est vide
// quand la journée est du repos, mais AUSSI quand le service commence la veille
// (le gabarit HA ne remplit les heures que si `target_date in ev_start`) — d'où
// le -1 distinct de « pas de travail », que l'appelant traite différemment.
static int parse_shift_start(const std::string& h) {
  if (h.size() < 5 || h[2] != ':') return -1;
  const int hh = atoi(h.substr(0, 2).c_str());
  const int mm = atoi(h.substr(3, 2).c_str());
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
  return hh * 60 + mm;
}

// "HH:MM-HH:MM" -> minutes depuis minuit de la FIN (la « fermeture »), -1 sinon.
static int parse_shift_end(const std::string& h) {
  if (h.size() < 11 || h[5] != '-' || h[8] != ':') return -1;
  const int hh = atoi(h.substr(6, 2).c_str());
  const int mm = atoi(h.substr(9, 2).c_str());
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
  return hh * 60 + mm;
}

// Epoch local d'une heure murale précise à J+offset. On construit le tm complet
// puis mktime() : surtout PAS « minuit + n×60 », qui décale d'une heure les deux
// nuits de bascule heure d'été/hiver. Un réveil se règle sur l'heure de
// l'horloge murale, pas sur une durée depuis minuit.
static time_t epoch_at(int day_offset, int minute_of_day) {
  struct tm t;
  if (!local_day_from_offset(day_offset, t)) return 0;
  t.tm_hour = minute_of_day / 60;
  t.tm_min = minute_of_day % 60;
  t.tm_sec = 0;
  t.tm_isdst = -1;
  const time_t r = mktime(&t);
  return (r == static_cast<time_t>(-1)) ? 0 : r;
}

// bit 0 = lundi … bit 6 = dimanche (tm_wday : 0 = dimanche).
static bool day_selected(uint8_t mask, int tm_wday) {
  const int bit = (tm_wday + 6) % 7;
  return (mask >> bit) & 1;
}

// cal_index_for_offset() : tab5_core.cpp (partagée avec le planning de la carte centrale).

// Prêt = calendrier reçu ET couvrant aujourd'hui.
bool alarm_calendar_ready() {
  return !cal_jours_data[0].nom_jour.empty() && cal_index_for_offset(0) >= 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Cache de la prochaine sonnerie
// ═══════════════════════════════════════════════════════════════════════════
// Le tick d'1 s ne doit pas balayer 8 jours à chaque passage. On calcule une
// fois, on garde, et on ne recalcule que si (a) un réglage ou le calendrier a
// bougé, ou (b) plus d'une minute s'est écoulée — ce second filet rattrape tout
// seul une resynchro SNTP ou une bascule d'heure sans qu'aucun appelant n'ait à
// y penser.
static bool s_dirty = true;
static time_t s_cached_at = 0;
static time_t s_next_ts = 0;
static int s_next_offset = -1;
static int s_next_min = -1;

// Plancher de recherche : aucune sonnerie ne peut être programmée à ou avant
// cette date. C'est ce qui empêche un réveil arrêté à 05:16 de re-sonner
// immédiatement, et ce qui est sauvegardé en NVS pour couvrir un redémarrage.
static time_t s_skip_before = 0;

static time_t s_snooze_until = 0;
static int s_snooze_count = 0;

// Horodatage du dernier passage d'alarm_due(). Sert à distinguer le temps qui
// s'est écoulé PENDANT que l'appareil tournait (rien à rattraper : le tick d'1 s
// a déjà vu chaque seconde) du temps écoulé SANS tick — appareil éteint, boucle
// bloquée, sonnerie en cours. Seul le second justifie de rattraper une sonnerie
// dont l'heure vient de passer. 0 = premier passage depuis le démarrage.
static time_t s_last_tick = 0;

void alarm_invalidate() { s_dirty = true; }

uint32_t alarm_skip_floor() { return static_cast<uint32_t>(s_skip_before); }
void alarm_set_skip_floor(uint32_t v) {
  s_skip_before = static_cast<time_t>(v);
  s_dirty = true;
}

// Heure de sonnerie retenue pour le jour J+offset, en minutes depuis minuit.
// -1 = ce jour-là, le réveil ne sonne pas.
static int ring_minute_for_day(int offset, const struct tm& day) {
  const AlarmCfg& c = g_alarm_cfg;
  // idx = la case de cal_jours_data[] qui correspond à ce jour-là (cf.
  // cal_index_for_offset) : jamais `offset` directement.
  const int idx = cal_index_for_offset(offset);
  const bool cal_ok = alarm_calendar_ready() && idx >= 0;
  // Sans données calendrier (boot, HA jamais connecté, ou données trop vieilles
  // pour couvrir ce jour), les deux modes calendrier retombent sur l'heure fixe :
  // rater une embauche coûte plus cher qu'une sonnerie en trop, et l'utilisateur
  // peut toujours arrêter.
  const bool travaille = cal_ok && !cal_jours_data[idx].est_repos;
  // int, PAS bool : c'est une heure en minutes depuis minuit (ou -1). Déclarée
  // `bool` par erreur, elle valait 1 dès que fixed_min était non nul, et tous
  // les modes à heure fixe sonnaient à 00:01 — attrapé par -Wint-in-bool-context
  // à la compilation du 05/08/2026, jamais par un test fonctionnel.
  const int fixe_si_coche = day_selected(c.days_mask, day.tm_wday) ? c.fixed_min : -1;

  if (c.mode == AlarmMode::FIXE || !cal_ok) return fixe_si_coche;

  if (!travaille) {
    // Jour de repos : soit silence, soit l'heure fixe — et dans ce cas le
    // sélecteur de jours s'applique (c'est bien un réveil « à heure fixe »).
    return (c.rest_mode == AlarmRepos::FIXE) ? fixe_si_coche : -1;
  }

  // ── Jour travaillé : c'est le calendrier qui commande, pas le sélecteur de
  // jours. Décocher le samedi ne doit pas faire rater une embauche du samedi.
  if (c.mode == AlarmMode::TRAVAIL) return c.fixed_min;

  // ── Mode EMBAUCHE : ouverture − délai, borné.
  const int start = parse_shift_start(cal_jours_data[idx].heures_ouverture);
  if (start < 0) return c.fixed_min;  // travail confirmé mais horaire inconnu

  int m = clamp_i(start - c.lead_min, c.earliest_min, c.latest_min);

  // ── « Fermeture » de la veille : repos minimum. C'est l'autre moitié de
  // l'horaire du calendrier — après une fermeture à 21:00, un repos de 9 h
  // interdit de sonner avant 06:00. Bornée par `latest_min` : le repos ne peut
  // pas faire arriver en retard.
  // idx >= 1 et non offset >= 1 : avec des données d'hier, la veille d'aujourd'hui
  // est connue (case 0) — elle ne l'était pas avant le recalage.
  if (c.rest_hours > 0 && idx >= 1) {
    const int fin_veille = parse_shift_end(cal_jours_data[idx - 1].heures_ouverture);
    if (fin_veille >= 0 && !cal_jours_data[idx - 1].est_repos) {
      // La veille finit à `fin_veille` minutes après SON minuit, donc
      // fin_veille - 1440 minutes après le minuit du jour visé.
      const int plancher = fin_veille - 1440 + c.rest_hours * 60;
      if (plancher > m) m = clamp_i(plancher, c.earliest_min, c.latest_min);
    }
  }
  return m;
}

time_t alarm_next_ring(time_t now) {
  if (now <= 0) return 0;
  if (!s_dirty && (now - s_cached_at) < 60 && s_cached_at != 0) return s_next_ts;

  s_dirty = false;
  s_cached_at = now;
  s_next_ts = 0;
  s_next_offset = -1;
  s_next_min = -1;
  if (!g_alarm_cfg.enabled) return 0;

  // Plancher de recherche. Deux bornes, on garde la plus tardive :
  //   - `s_skip_before` : la sonnerie déjà consommée ou explicitement arrêtée ;
  //   - le dernier tick : tout ce qui est ANTÉRIEUR a déjà été examiné seconde
  //     par seconde, il n'y a rien à rattraper. C'est ce qui évite qu'un réglage
  //     posé une minute dans le passé (« il est 07:00:30, je règle 07:00 ») ne
  //     déclenche la sonnerie sur-le-champ.
  // Au tout premier passage après un démarrage, `s_last_tick` vaut 0 : là, et
  // seulement là, on ouvre la fenêtre de grâce — un redémarrage ou une OTA à
  // 06:44 ne doit pas avaler le réveil de 06:45.
  const time_t not_before = (s_last_tick > 0) ? s_last_tick : (now - ALARM_GRACE_S);
  const time_t floor_ts = (not_before > s_skip_before) ? not_before : s_skip_before;

  // 8 jours : couvre toutes les combinaisons jour-de-semaine + une semaine de
  // congés complète en mode calendrier avec repos silencieux.
  for (int offset = 0; offset <= 8; offset++) {
    struct tm day;
    if (!local_day_from_offset(offset, day)) return 0;  // SNTP pas encore prêt
    const int m = ring_minute_for_day(offset, day);
    if (m < 0) continue;
    const time_t ts = epoch_at(offset, m);
    if (ts == 0 || ts <= floor_ts) continue;
    s_next_ts = ts;
    s_next_offset = offset;
    s_next_min = m;
    return ts;
  }
  return 0;
}

// Corps réel : `s_last_tick` porte encore la valeur du passage PRÉCÉDENT, dont
// alarm_next_ring() se sert comme plancher. C'est pour ça que la mise à jour est
// faite par l'enveloppe ci-dessous, à la sortie, et pas ici — l'écrire au début
// mettrait le plancher à `now` et une sonnerie tombant exactement sur `now` ne
// serait jamais retenue (`ts <= floor_ts`).
static bool alarm_due_(time_t now) {
  // Désactiver le réveil doit tout couper, y compris une répétition en cours :
  // c'est `alarm_dismiss()` (appelé par la bascule d'activation) qui s'en
  // charge, pas ce test. Ici, un snooze armé passe donc bien avant le reste.
  if (s_snooze_until > 0) {
    if (now < s_snooze_until) return false;
    s_snooze_until = 0;
    return true;
  }

  if (!g_alarm_cfg.enabled) return false;
  const time_t next = alarm_next_ring(now);
  if (next == 0 || now < next) return false;

  s_skip_before = next;  // cette sonnerie-ci est consommée
  s_snooze_count = 0;
  alarm_invalidate();
  return true;
}

bool alarm_due(time_t now) {
  if (now <= 0) return false;
  const bool ring = alarm_due_(now);
  // Avancé à CHAQUE passage, y compris ceux qui ne sonnent pas : c'est la trace
  // « cette seconde-là a bien été examinée ». alarm_due() n'étant pas appelé
  // pendant qu'une sonnerie retentit, `s_last_tick` peut avoir plusieurs minutes
  // de retard en sortie de sonnerie — sans conséquence, `s_skip_before` est
  // alors plus récent et l'emporte comme plancher.
  s_last_tick = now;
  return ring;
}

void alarm_snooze(time_t now, int minutes) {
  if (minutes < 1) minutes = 1;
  s_snooze_count++;
  s_snooze_until = now + static_cast<time_t>(minutes) * 60;
}

int alarm_snooze_count() { return s_snooze_count; }

void alarm_dismiss(time_t now) {
  s_snooze_until = 0;
  s_snooze_count = 0;
  if (now > s_skip_before) s_skip_before = now;
  alarm_invalidate();
}

void alarm_reset_state() {
  s_dirty = true;
  s_cached_at = 0;
  s_next_ts = 0;
  s_next_offset = -1;
  s_next_min = -1;
  s_skip_before = 0;
  s_snooze_until = 0;
  s_snooze_count = 0;
  s_last_tick = 0;
}

// ═══════════════════════════════════════════════════════════════════════════
// Libellés
// ═══════════════════════════════════════════════════════════════════════════

void alarm_hhmm(int minute_of_day, char* out, size_t n) {
  // Borné à une journée : -O2 sait alors que chaque champ tient sur deux chiffres
  // (sinon -Wformat-truncation). Les appelants passent toujours une heure valide.
  const unsigned m = static_cast<unsigned>(minute_of_day) % 1440u;
  snprintf(out, n, "%02u:%02u", m / 60u, m % 60u);
}

std::string alarm_next_label(time_t now) {
  // Une répétition en cours PASSE DEVANT le prochain réveil calculé : c'est elle
  // qui va sonner. Annoncer « Demain 05:15 » pendant qu'une répétition court
  // dans 6 minutes serait le seul moment où cet écran mentirait.
  if (s_snooze_until > now) {
    struct tm t;
    if (localtime_r(&s_snooze_until, &t) != nullptr) {
      const int reste = static_cast<int>((s_snooze_until - now + 59) / 60);
      char buf[56];
      snprintf(buf, sizeof(buf), "R\xC3\xA9p\xC3\xA9tition %02d:%02d (%d min)", t.tm_hour, t.tm_min, reste);
      return std::string(buf);
    }
  }
  if (!g_alarm_cfg.enabled) return "D\xC3\xA9sactiv\xC3\xA9";
  if (alarm_next_ring(now) == 0) return "Aucune sonnerie pr\xC3\xA9vue";

  char h[8];
  alarm_hhmm(s_next_min, h, sizeof(h));

  if (s_next_offset == 0) {
    return std::string("Aujourd'hui ") + h;
  }
  if (s_next_offset == 1) {
    return std::string("Demain ") + h;
  }
  struct tm day;
  if (!local_day_from_offset(s_next_offset, day)) return std::string(h);
  // Majuscule initiale sur le jour : c'est un début de libellé.
  return fr_capitalized(fr_day_long_utf8(day.tm_wday)) + " " + h;
}

std::string alarm_next_detail(time_t now) {
  if (s_snooze_until > now) {
    char buf[80];
    snprintf(buf, sizeof(buf),
             "R\xC3\xA9p\xC3\xA9tition n\xC2\xB0%d \xC2\xB7 « Arr\xC3\xAAter » pour reprendre le cycle normal",
             s_snooze_count);
    return std::string(buf);
  }
  if (!g_alarm_cfg.enabled) return "Touchez l'interrupteur pour l'armer";
  if (alarm_next_ring(now) == 0) return "Aucun jour retenu dans les 8 prochains";

  struct tm day;
  if (!local_day_from_offset(s_next_offset, day)) return "";

  char date[64];
  snprintf(date, sizeof(date), "%s %d %s", fr_day_long_utf8(day.tm_wday), day.tm_mday,
           fr_month_long_utf8(day.tm_mon + 1));

  if (!alarm_calendar_ready()) {
    return std::string(date) + " \xC2\xB7 en attente du calendrier";
  }
  // Jour non couvert (données trop vieilles pour aller jusque-là) : la sonnerie a
  // été calculée sur l'heure fixe, comme sans calendrier.
  const int idx = cal_index_for_offset(s_next_offset);
  if (idx < 0) {
    return std::string(date) + " \xC2\xB7 en attente du calendrier";
  }
  if (cal_jours_data[idx].est_repos) {
    return std::string(date) + " \xC2\xB7 repos";
  }
  const std::string& h = cal_jours_data[idx].heures_ouverture;
  if (h.size() >= 11) {
    // « Travail 06:45 – 15:30 » : tiret demi-cadratin UTF-8, comme le popup
    // calendrier (cal_render_day_detail).
    return std::string(date) + " \xC2\xB7 Travail " + h.substr(0, 5) + " \xE2\x80\x93 " + h.substr(6, 5);
  }
  return std::string(date) + " \xC2\xB7 Travail (horaire inconnu)";
}

// ═══════════════════════════════════════════════════════════════════════════
// Préréglages de jours + noms des modes
// ═══════════════════════════════════════════════════════════════════════════
namespace {
struct DaysPreset {
  const char* name;
  int mask;  // -1 = « Personnalisé » : sélectionner cette entrée ne change rien
};
// L'entrée « Personnalisé » DOIT rester la dernière : alarm_days_preset_index()
// s'en sert comme valeur de repli quand aucun masque ne correspond.
const DaysPreset kPresets[ALARM_DAYS_PRESET_COUNT] = {
    {"Tous les jours", 0x7F}, {"Lundi-Vendredi", 0x1F}, {"Lundi-Samedi", 0x3F},
    {"Week-end", 0x60},       {"Personnalis\xC3\xA9", -1},
};
}  // namespace

const char* alarm_days_preset_name(int idx) {
  return kPresets[clamp_i(idx, 0, ALARM_DAYS_PRESET_COUNT - 1)].name;
}
int alarm_days_preset_mask(int idx) {
  return kPresets[clamp_i(idx, 0, ALARM_DAYS_PRESET_COUNT - 1)].mask;
}
int alarm_days_preset_index(uint8_t mask) {
  for (int i = 0; i < ALARM_DAYS_PRESET_COUNT; i++) {
    if (kPresets[i].mask >= 0 && static_cast<uint8_t>(kPresets[i].mask) == mask) return i;
  }
  return ALARM_DAYS_PRESET_COUNT - 1;  // « Personnalisé »
}

// ═══════════════════════════════════════════════════════════════════════════
// Mélodies RTTTL
// ═══════════════════════════════════════════════════════════════════════════
// Format : nom:d=durée par défaut,o=octave par défaut,b=tempo:notes.
// Éditer une ligne suffit — aucune police, aucun octet de flash, aucun réglage
// HA ne dépend de leur contenu (seul le NOMBRE d'entrées est repris par le
// select exposé à HA, cf. ALARM_MELODY_COUNT).
namespace {
struct Melody {
  const char* name;
  const char* score;
};
// Pas de durée : le cycle de sonnerie s'arrête sur `rtttl.is_playing`, pas sur
// une estimation (le champ `ms` et alarm_melody_ms(), jamais lus, sont retirés
// le 25/09/2026, audit lot 8a).
const Melody kMelodies[ALARM_MELODY_COUNT] = {
    {"Douce", "Douce:d=8,o=6,b=92:c,e,g,c7,p,g,e,c,2p"},
    {"Classique", "Reveil:d=16,o=6,b=140:c7,c7,p,c7,c7,4p,c7,c7,p,c7,c7,4p"},
    {"Insistante", "Urgence:d=32,o=7,b=180:c,c,c,c,8p,c,c,c,c,8p,c,c,c,c,4p"},
    {"Carillon", "Carillon:d=4,o=5,b=90:e,c,d,1g4,2p,g4,d,e,1c"},
};
}  // namespace

const char* alarm_melody_name(int idx) {
  return kMelodies[clamp_i(idx, 0, ALARM_MELODY_COUNT - 1)].name;
}
const char* alarm_melody_rtttl(int idx) {
  return kMelodies[clamp_i(idx, 0, ALARM_MELODY_COUNT - 1)].score;
}

// Crescendo : 35 % du volume cible au premier cycle, puis +13 points par cycle
// jusqu'au plein volume au 6e. Le plancher n'est pas 0 : un réveil qu'on
// n'entend pas les 20 premières secondes ne réveille personne.
float alarm_ring_gain(float base, int cycle, bool crescendo) {
  if (base < 0.0f) base = 0.0f;
  if (base > 1.0f) base = 1.0f;
  if (!crescendo) return base;
  constexpr float FLOOR = 0.35f;
  constexpr float STEP = 0.13f;
  float f = FLOOR + STEP * static_cast<float>(cycle < 0 ? 0 : cycle);
  if (f > 1.0f) f = 1.0f;
  return base * f;
}

// ═══════════════════════════════════════════════════════════════════════════
// Rendez-vous
// ═══════════════════════════════════════════════════════════════════════════
namespace {
struct Rdv {
  time_t start = 0;
  bool announced = false;
  std::string titre;
};
Rdv s_rdv[ALARM_RDV_MAX];
int s_rdv_n = 0;
}  // namespace


void rdv_clear() {
  for (int i = 0; i < ALARM_RDV_MAX; i++) {
    s_rdv[i].start = 0;
    s_rdv[i].announced = false;
    s_rdv[i].titre.clear();
  }
  s_rdv_n = 0;
}

void rdv_store(const std::string& payload) {
  // On mémorise ce qui a DÉJÀ été annoncé avant d'écraser la liste : HA repousse
  // la même liste toutes les 5 minutes, et sans cet appariement par epoch chaque
  // poussée ferait ré-annoncer le rendez-vous en cours en boucle.
  time_t done[ALARM_RDV_MAX];
  int done_n = 0;
  for (int i = 0; i < s_rdv_n; i++) {
    if (s_rdv[i].announced && done_n < ALARM_RDV_MAX) done[done_n++] = s_rdv[i].start;
  }

  rdv_clear();
  size_t pos = 0;
  while (pos < payload.size() && s_rdv_n < ALARM_RDV_MAX) {
    const size_t rec_end = payload.find('~', pos);
    const std::string rec = payload.substr(pos, rec_end == std::string::npos ? std::string::npos : rec_end - pos);
    pos = (rec_end == std::string::npos) ? payload.size() : rec_end + 1;
    if (rec.empty()) continue;

    const size_t bar = rec.find('|');
    if (bar == std::string::npos) continue;
    const time_t ts = static_cast<time_t>(strtoll(rec.substr(0, bar).c_str(), nullptr, 10));
    if (ts <= 0) continue;

    Rdv& r = s_rdv[s_rdv_n];
    r.start = ts;
    r.titre = rec.substr(bar + 1);
    r.announced = false;
    for (int k = 0; k < done_n; k++) {
      if (done[k] == ts) { r.announced = true; break; }
    }
    s_rdv_n++;
  }
}

// « 14:30 » depuis un epoch, en heure locale.
static bool rdv_hhmm(time_t ts, char* out, size_t n) {
  struct tm t;
  if (localtime_r(&ts, &t) == nullptr) return false;
  snprintf(out, n, "%02d:%02d", t.tm_hour, t.tm_min);
  return true;
}

bool rdv_due(time_t now, int lead_min, std::string& out_screen, std::string& out_speech) {
  if (now <= 0 || lead_min < 0) return false;
  const time_t lead = static_cast<time_t>(lead_min) * 60;

  for (int i = 0; i < s_rdv_n; i++) {
    Rdv& r = s_rdv[i];
    if (r.announced || r.start <= 0) continue;
    if (now < r.start - lead) continue;
    // Rendez-vous déjà commencé depuis plus d'une minute quand on le découvre
    // (poussée tardive, appareil rallumé) : on le marque lu sans rien annoncer.
    if (now > r.start + 60) { r.announced = true; continue; }
    r.announced = true;

    char h[8];
    if (!rdv_hhmm(r.start, h, sizeof(h))) return false;
    const int reste = static_cast<int>((r.start - now + 59) / 60);  // minutes, arrondi au-dessus

    char scr[192];
    if (reste > 0) {
      snprintf(scr, sizeof(scr), "%s \xC2\xB7 %s (dans %d min)", h, r.titre.c_str(), reste);
    } else {
      snprintf(scr, sizeof(scr), "%s \xC2\xB7 %s (maintenant)", h, r.titre.c_str());
    }
    out_screen = scr;

    // Version parlée : « 14 h 30 » plutôt que « 14:30 » (les moteurs TTS
    // français lisent mal le deux-points), et une phrase complète ponctuée pour
    // que la prosodie ne parte pas en liste.
    char spk[224];
    const int hh = atoi(std::string(h).substr(0, 2).c_str());
    const int mm = atoi(std::string(h).substr(3, 2).c_str());
    char heure_parlee[24];
    if (mm == 0) {
      snprintf(heure_parlee, sizeof(heure_parlee), "%d heures", hh);
    } else {
      snprintf(heure_parlee, sizeof(heure_parlee), "%d heures %d", hh, mm);
    }
    if (reste > 0) {
      snprintf(spk, sizeof(spk), "Rappel : %s, \xC3\xA0 %s, dans %d minute%s.", r.titre.c_str(),
               heure_parlee, reste, reste > 1 ? "s" : "");
    } else {
      snprintf(spk, sizeof(spk), "Rappel : %s, c'est maintenant.", r.titre.c_str());
    }
    out_speech = spk;
    return true;
  }
  return false;
}

std::string rdv_next_label(time_t now) {
  const Rdv* best = nullptr;
  for (int i = 0; i < s_rdv_n; i++) {
    if (s_rdv[i].start <= now) continue;
    if (best == nullptr || s_rdv[i].start < best->start) best = &s_rdv[i];
  }
  if (best == nullptr) return "";
  char h[8];
  if (!rdv_hhmm(best->start, h, sizeof(h))) return "";
  return std::string(h) + " \xC2\xB7 " + best->titre;
}
