/**
 * [AI-CONTEXT]
 * @file alarm_clock.cpp
 * @role Implémentation du moteur de réveil + annonce des rendez-vous.
 *       Contrat et justifications d'architecture : voir alarm_clock.h.
 */
#include "alarm_clock.h"
#include "tab5_custom.h"

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

bool alarm_calendar_ready() { return !cal_jours_data[0].nom_jour.empty(); }

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
  const bool cal_ok = alarm_calendar_ready() && offset >= 0 && offset < 15;
  // Sans données calendrier (boot, HA jamais connecté), les deux modes
  // calendrier retombent sur l'heure fixe : rater une embauche coûte plus cher
  // qu'une sonnerie en trop, et l'utilisateur peut toujours arrêter.
  const bool travaille = cal_ok && !cal_jours_data[offset].est_repos;
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
  const int start = parse_shift_start(cal_jours_data[offset].heures_ouverture);
  if (start < 0) return c.fixed_min;  // travail confirmé mais horaire inconnu

  int m = clamp_i(start - c.lead_min, c.earliest_min, c.latest_min);

  // ── « Fermeture » de la veille : repos minimum. C'est l'autre moitié de
  // l'horaire du calendrier — après une fermeture à 21:00, un repos de 9 h
  // interdit de sonner avant 06:00. Bornée par `latest_min` : le repos ne peut
  // pas faire arriver en retard.
  if (c.rest_hours > 0 && offset >= 1) {
    const int fin_veille = parse_shift_end(cal_jours_data[offset - 1].heures_ouverture);
    if (fin_veille >= 0 && !cal_jours_data[offset - 1].est_repos) {
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

bool alarm_snooze(time_t now, int minutes) {
  if (minutes < 1) minutes = 1;
  s_snooze_count++;
  s_snooze_until = now + static_cast<time_t>(minutes) * 60;
  return true;
}

int alarm_snooze_count() { return s_snooze_count; }
time_t alarm_snooze_until() { return s_snooze_until; }

void alarm_dismiss(time_t now) {
  s_snooze_until = 0;
  s_snooze_count = 0;
  if (now > s_skip_before) s_skip_before = now;
  alarm_invalidate();
}

// ═══════════════════════════════════════════════════════════════════════════
// Libellés
// ═══════════════════════════════════════════════════════════════════════════

static void hhmm(int minute_of_day, char* out, size_t n) {
  snprintf(out, n, "%02d:%02d", minute_of_day / 60, minute_of_day % 60);
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
  hhmm(s_next_min, h, sizeof(h));

  if (s_next_offset == 0) {
    return std::string("Aujourd'hui ") + h;
  }
  if (s_next_offset == 1) {
    return std::string("Demain ") + h;
  }
  struct tm day;
  if (!local_day_from_offset(s_next_offset, day)) return std::string(h);
  // Majuscule initiale sur le jour : c'est un début de libellé.
  std::string j = fr_day_long_utf8(day.tm_wday);
  if (!j.empty()) j[0] = static_cast<char>(j[0] - 32);
  return j + " " + h;
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
  if (s_next_offset >= 15 || cal_jours_data[s_next_offset].est_repos) {
    return std::string(date) + " \xC2\xB7 repos";
  }
  const std::string& h = cal_jours_data[s_next_offset].heures_ouverture;
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
const char* const kModeNames[AlarmMode::COUNT] = {
    "Heure fixe",
    "Jours travaill\xC3\xA9s",
    "Avant l'ouverture",
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
const char* alarm_mode_name(int idx) { return kModeNames[clamp_i(idx, 0, AlarmMode::COUNT - 1)]; }

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
  uint32_t ms;  // durée approximative d'un passage
};
// Les durées sont mesurées à la règle RTTTL : une noire vaut 60000/b ms, et
// chaque note vaut (60000/b) × (4/d). Elles n'ont pas besoin d'être exactes :
// elles servent à dimensionner l'attente du cycle, qui se termine de toute
// façon dès que `rtttl.is_playing` retombe.
const Melody kMelodies[ALARM_MELODY_COUNT] = {
    {"Douce", "Douce:d=8,o=6,b=92:c,e,g,c7,p,g,e,c,2p", 3800},
    {"Classique", "Reveil:d=16,o=6,b=140:c7,c7,p,c7,c7,4p,c7,c7,p,c7,c7,4p", 3400},
    {"Insistante", "Urgence:d=32,o=7,b=180:c,c,c,c,8p,c,c,c,c,8p,c,c,c,c,4p", 2600},
    {"Carillon", "Carillon:d=4,o=5,b=90:e,c,d,1g4,2p,g4,d,e,1c", 8000},
};
}  // namespace

const char* alarm_melody_name(int idx) {
  return kMelodies[clamp_i(idx, 0, ALARM_MELODY_COUNT - 1)].name;
}
const char* alarm_melody_rtttl(int idx) {
  return kMelodies[clamp_i(idx, 0, ALARM_MELODY_COUNT - 1)].score;
}
uint32_t alarm_melody_ms(int idx) {
  return kMelodies[clamp_i(idx, 0, ALARM_MELODY_COUNT - 1)].ms;
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

int rdv_count() { return s_rdv_n; }

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

// ═══════════════════════════════════════════════════════════════════════════
// Rendu LVGL
// ═══════════════════════════════════════════════════════════════════════════

// Bascule visuelle commune aux 5 interrupteurs du popup : bordure colorée
// (highlight_button_border, déjà utilisé par l'assistant vocal) + libellé
// ON/OFF assorti. Une seule recette, donc aucun interrupteur ne peut prendre
// une apparence différente des autres.
static void set_toggle(lv_obj_t* btn, lv_obj_t* lbl, bool on, const char* on_txt, const char* off_txt,
                       uint32_t on_color) {
  if (btn != nullptr) highlight_button_border(btn, on, on_color);
  if (lbl != nullptr) {
    lv_label_set_text(lbl, on ? on_txt : off_txt);
    lv_obj_set_style_text_color(lbl, lv_color_hex(on ? on_color : UIColor::TEXT_DIM), LV_PART_MAIN);
  }
}

void alarm_render_settings(const AlarmSettingsUI& ui, time_t now, int melody_idx, float volume,
                           bool crescendo, bool tts_on, bool rdv_on, int rdv_lead_min) {
  const AlarmCfg& c = g_alarm_cfg;
  char buf[96];

  set_toggle(ui.btn_enable, ui.lbl_enable, c.enabled, "R\xC3\xA9veil actif", "R\xC3\xA9veil \xC3\xA9teint",
             UIColor::SUCCESS);
  if (ui.icon_enable != nullptr) {
    // F0020 = alarm, F0023 = alarm-off (codepoints vérifiés dans le TTF du projet).
    lv_label_set_text(ui.icon_enable, c.enabled ? "\U000F0020" : "\U000F0023");
    lv_obj_set_style_text_color(ui.icon_enable,
                                lv_color_hex(c.enabled ? UIColor::SUCCESS : UIColor::TEXT_DIM), LV_PART_MAIN);
  }

  if (ui.lbl_time != nullptr) {
    hhmm(c.fixed_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_time, buf);
  }

  // Chips des jours. Le sélecteur ne gouverne QUE l'heure fixe : on l'estompe
  // quand le mode en cours ne s'en sert pas pour les jours travaillés, plutôt
  // que de le masquer (il reste utile pour le repli des jours de repos).
  static const char* const kDays[7] = {"L", "M", "M", "J", "V", "S", "D"};
  const bool days_govern_all = (c.mode == AlarmMode::FIXE);
  for (int i = 0; i < 7; i++) {
    const bool on = (c.days_mask >> i) & 1;
    if (ui.day_btn[i] != nullptr) highlight_button_border(ui.day_btn[i], on, UIColor::ACCENT);
    if (ui.day_lbl[i] != nullptr) {
      lv_label_set_text(ui.day_lbl[i], kDays[i]);
      lv_obj_set_style_text_color(ui.day_lbl[i],
                                  lv_color_hex(on ? (days_govern_all ? UIColor::TEXT_PRIMARY : UIColor::ACCENT)
                                                  : UIColor::TEXT_DIM),
                                  LV_PART_MAIN);
    }
  }

  for (int i = 0; i < AlarmMode::COUNT; i++) {
    if (ui.mode_btn[i] != nullptr) highlight_button_border(ui.mode_btn[i], c.mode == i, UIColor::INFO);
  }
  if (ui.lbl_mode_hint != nullptr) {
    const char* hint = "";
    switch (c.mode) {
      case AlarmMode::FIXE:
        hint = "Sonne \xC3\xA0 l'heure fixe, les jours coch\xC3\xA9s ci-dessus.";
        break;
      case AlarmMode::TRAVAIL:
        hint = "Sonne \xC3\xA0 l'heure fixe, uniquement les jours travaill\xC3\xA9s.";
        break;
      default:
        hint = "Sonne avant l'ouverture lue dans le calendrier.";
        break;
    }
    lv_label_set_text(ui.lbl_mode_hint, hint);
  }

  if (ui.lbl_lead != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.lead_min);
    lv_label_set_text(ui.lbl_lead, buf);
  }
  if (ui.lbl_early != nullptr) {
    hhmm(c.earliest_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_early, buf);
  }
  if (ui.lbl_late != nullptr) {
    hhmm(c.latest_min, buf, sizeof(buf));
    lv_label_set_text(ui.lbl_late, buf);
  }
  if (ui.lbl_rest != nullptr) {
    if (c.rest_hours <= 0) {
      lv_label_set_text(ui.lbl_rest, "\xE2\x80\x94");
    } else {
      snprintf(buf, sizeof(buf), "%d h", c.rest_hours);
      lv_label_set_text(ui.lbl_rest, buf);
    }
  }

  set_toggle(ui.btn_repos, ui.lbl_repos, c.rest_mode == AlarmRepos::FIXE, "Repos : heure fixe",
             "Repos : silence", UIColor::WARNING);

  if (ui.lbl_next != nullptr) lv_label_set_text(ui.lbl_next, alarm_next_label(now).c_str());
  if (ui.lbl_next_sub != nullptr) lv_label_set_text(ui.lbl_next_sub, alarm_next_detail(now).c_str());

  if (ui.lbl_melody != nullptr) lv_label_set_text(ui.lbl_melody, alarm_melody_name(melody_idx));
  if (ui.slider_vol != nullptr) {
    const int pct = static_cast<int>(volume * 100.0f + 0.5f);
    // lv_slider_set_value ne déclenche pas LV_EVENT_VALUE_CHANGED : pas de
    // rebouclage sur on_value (même motif que ui_sync_volume_widgets).
    lv_slider_set_value(ui.slider_vol, pct, LV_ANIM_OFF);
    if (ui.lbl_vol != nullptr) {
      snprintf(buf, sizeof(buf), "%d %%", pct);
      lv_label_set_text(ui.lbl_vol, buf);
    }
  }
  set_toggle(ui.btn_cresc, ui.lbl_cresc, crescendo, "Progressif", "Volume constant", UIColor::ACCENT);

  if (ui.lbl_snooze != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.snooze_min);
    lv_label_set_text(ui.lbl_snooze, buf);
  }
  if (ui.lbl_maxring != nullptr) {
    snprintf(buf, sizeof(buf), "%d min", c.max_ring_min);
    lv_label_set_text(ui.lbl_maxring, buf);
  }

  set_toggle(ui.btn_tts, ui.lbl_tts, tts_on, "Annonce parl\xC3\xA9""e", "Sonnerie seule", UIColor::INFO);
  set_toggle(ui.btn_rdv, ui.lbl_rdv, rdv_on, "Annonce des RDV", "RDV silencieux", UIColor::INFO);
  if (ui.lbl_rdv_lead != nullptr) {
    snprintf(buf, sizeof(buf), "%d min avant", rdv_lead_min);
    lv_label_set_text(ui.lbl_rdv_lead, buf);
  }
  if (ui.lbl_rdv_next != nullptr) {
    const std::string n = rdv_next_label(now);
    lv_label_set_text(ui.lbl_rdv_next, n.empty() ? "Aucun rendez-vous \xC3\xA0 venir" : n.c_str());
    lv_obj_set_style_text_color(ui.lbl_rdv_next,
                                lv_color_hex(n.empty() ? UIColor::TEXT_DIM : UIColor::TEXT_PRIMARY),
                                LV_PART_MAIN);
  }
}

static void ring_paint_clock(const AlarmRingUI& ui, time_t now) {
  if (ui.lbl_time == nullptr) return;
  struct tm t;
  if (localtime_r(&now, &t) == nullptr) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  lv_label_set_text(ui.lbl_time, buf);
}

void alarm_ring_show(const AlarmRingUI& ui, time_t now, const std::string& sub) {
  if (ui.root == nullptr) return;
  ring_paint_clock(ui, now);
  if (ui.lbl_sub != nullptr) lv_label_set_text(ui.lbl_sub, sub.c_str());
  if (ui.icon != nullptr) lv_label_set_text(ui.icon, "\U000F0020");
  alarm_ring_refresh(ui, now, g_alarm_cfg.snooze_min, alarm_snooze_count());
  lv_obj_clear_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(ui.root);
}

void alarm_ring_refresh(const AlarmRingUI& ui, time_t now, int snooze_min, int snooze_count) {
  ring_paint_clock(ui, now);
  char buf[64];
  if (ui.lbl_title != nullptr) {
    if (snooze_count > 0) {
      snprintf(buf, sizeof(buf), "R\xC3\xA9p\xC3\xA9tition %d", snooze_count);
    } else {
      snprintf(buf, sizeof(buf), "R\xC3\xA9veil");
    }
    lv_label_set_text(ui.lbl_title, buf);
  }
  if (ui.lbl_snooze != nullptr) {
    snprintf(buf, sizeof(buf), "R\xC3\xA9p\xC3\xA9ter \xC2\xB7 %d min", snooze_min);
    lv_label_set_text(ui.lbl_snooze, buf);
  }
}

void alarm_ring_hide(const AlarmRingUI& ui) {
  if (ui.root != nullptr) lv_obj_add_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
}

void alarm_render_status_icon(lv_obj_t* icon, time_t now) {
  if (icon == nullptr) return;
  if (!g_alarm_cfg.enabled) {
    lv_label_set_text(icon, "\U000F0023");  // alarm-off
    lv_obj_set_style_text_color(icon, lv_color_hex(UIColor::INACTIVE), LV_PART_MAIN);
    return;
  }
  lv_label_set_text(icon, "\U000F0020");  // alarm
  // Vert quand la prochaine sonnerie est réellement calculée, ambre quand le
  // réveil est armé mais qu'aucun jour n'est retenu (piège classique : mode
  // « jours travaillés » + semaine de congés, ou tous les jours décochés).
  const bool armed = alarm_next_ring(now) != 0;
  lv_obj_set_style_text_color(icon, lv_color_hex(armed ? UIColor::SUCCESS : UIColor::WARNING), LV_PART_MAIN);
}
