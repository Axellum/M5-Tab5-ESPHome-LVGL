/**
 * [AI-CONTEXT]
 * @file tab5_parse.cpp
 * @role Implémentation de tab5_parse.h : logique pure, compilée à l'identique dans le
 *       firmware, dans les tests hôte (tools/test_parse.cpp) et dans le harnais libFuzzer
 *       (tools/fuzz/fuzz_parse.cpp). Chaque fonction est la boucle de l'écran qu'elle
 *       remplace, déplacée telle quelle (lot F de l'audit du 30/09/2026).
 */
#include "tab5_parse.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ─── 1. Prévisions ───
// Avant : parse_and_update_heures_bulk() et parse_and_update_jours_bulk(),
// Tab5/ecran/tab5_forecast.cpp. Tampon de pile plutôt qu'une copie std::string du payload
// (jusqu'à 2 048 octets, fragmentation de la SRAM).

int previsions_premier_creneau(const char* payload) { return std::atoi(payload); }

void previsions_heures_lire(const char* payload, HourForecastData heures[15]) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        char* parts[6];
        const int num_parts = split_fields(token, '|', parts, 6);

        if (num_parts >= 5) {
            int idx = std::atoi(parts[0]);
            if (idx >= 0 && idx < 15) {
                heures[idx].heure_texte = parts[1];
                heures[idx].condition = parts[2];
                heures[idx].temp = std::atof(parts[3]);
                heures[idx].pluvio = std::atof(parts[4]);
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

void previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        // Découper chaque token par '|' — in-place, pas de std::vector
        char* parts[10];  // 9 champs attendus + marge
        const int num_parts = split_fields(token, '|', parts, 10);

        if (num_parts >= 9) {
            int jour = std::atoi(parts[0]);
            if (jour >= 0 && jour < 15) {
                jours[jour].nom_jour = parts[1];
                jours[jour].condition = parts[2];
                jours[jour].tmin = std::atof(parts[3]);
                jours[jour].tmax = std::atof(parts[4]);
                jours[jour].est_repos = (parts[5][0] == '1');
                jours[jour].est_dimanche = (parts[6][0] == '1');
                jours[jour].est_passe = (parts[7][0] == '1');
                jours[jour].heures_ouverture = parts[8];
                // HA calcule l'index 0 sur SON « aujourd'hui » au moment du push : on
                // date la case 0 avec le jour local de réception (écart possible
                // seulement si le push chevauche minuit à la seconde près). Heure pas
                // encore synchronisée → -1 : le réveil reste sur l'heure fixe jusqu'au
                // push suivant (cycle /10 min) plutôt que de deviner.
                if (jour == 0) ancre = local_day_number_today();
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

// ─── 2. Vigilance ───
// Avant : le début de parse_and_update_vigilance(), Tab5/ecran/tab5_services.cpp.

void vigilance_lire(const char* payload, VigilanceLue& v) {
    // 1024 (était 512) : la phrase de vigilance peut être longue, un payload
    // complet dépassait parfois 512 et tronquait les derniers champs (#T165).
    strncpy(v.buf, payload, sizeof(v.buf) - 1);
    v.buf[sizeof(v.buf) - 1] = '\0';
    // strtok_r saute les champs vides consécutifs ("||"), comme l'ancien lambda :
    // un champ vide décalerait les suivants. Contrat HA inchangé — HA envoie
    // toujours "Vert" plutôt qu'une chaîne vide.
    char* saveptr = nullptr;
    for (int i = 0; i < kVigilanceChamps; i++) {
        char* tok = strtok_r(i == 0 ? v.buf : nullptr, "|", &saveptr);
        v.champs[i] = tok ? tok : "";
    }
}

NiveauVigilance vigilance_niveau(const char* s) {
    if (strcmp(s, "Jaune") == 0) return NiveauVigilance::JAUNE;
    if (strcmp(s, "Orange") == 0) return NiveauVigilance::ORANGE;
    if (strcmp(s, "Rouge") == 0) return NiveauVigilance::ROUGE;
    return NiveauVigilance::AUTRE;
}

int vigilance_actives(const VigilanceLue& v, VigilanceActive out[kVigilanceActivesMax]) {
    int n = 0;
    for (int i = 0; i < kVigilancePhenomenes && n < kVigilanceActivesMax; i++) {
        const char* state = v.champs[2 + i];
        if (strlen(state) == 0 || strcmp(state, "Vert") == 0 || strcmp(state, "unknown") == 0) continue;
        out[n++] = VigilanceActive{i, state};
    }
    return n;
}

// ─── 3. Alertes HA, historique des alertes, bandeau info ───
// Avant : la boucle de parse_and_update_ha_alerts_bulk(), le début de ha_alerte_texte()
// et de compose_info_code() (Tab5/ecran/tab5_central.cpp), la boucle
// d'alertes_historique_recu() (Tab5/ecran/tab5_alertes.cpp).

LecteurAlertesHa::LecteurAlertesHa(const char* payload) {
    strncpy(buf_, payload, sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

AlerteHaJeton LecteurAlertesHa::suivant() {
    AlerteHaJeton j{AlerteHaType::FIN, 0, nullptr, nullptr, nullptr};
    char* token = strtok_r(premier_ ? buf_ : nullptr, ";", &save_);
    premier_ = false;
    if (token == nullptr) return j;
    // En-tête « @n:N » (alertes du 06/10/2026, lot 3) : HA a N alertes à lire en tout,
    // plus que les 4 bandeaux. Un jeton d'un seul champ : l'ancien firmware l'ignore.
    if (strncmp(token, "@n:", 3) == 0) {
        j.type = AlerteHaType::TOTAL;
        j.total = atoi(token + 3);
        return j;
    }
    char* parts[3];
    const int num_parts = split_fields(token, '|', parts, 3);
    if (num_parts < 3) {
        j.type = AlerteHaType::AUTRE;
        return j;
    }
    j.type = AlerteHaType::ALERTE;
    j.id = parts[0];
    j.niveau = parts[1];
    j.texte = parts[2];
    return j;
}

AlerteTexteLu alerte_texte_lire(const char* brut) {
    if (strncmp(brut, "@maj:", 5) == 0) return {AlerteTexteCode::MAJ, brut + 5, 0};
    if (strncmp(brut, "@indispo:", 9) == 0) return {AlerteTexteCode::INDISPO, brut + 9, atoi(brut + 9)};
    if (strncmp(brut, "@vigi:", 6) == 0) return {AlerteTexteCode::VIGI, brut + 6, 0};
    return {AlerteTexteCode::TEXTE, brut, 0};
}

int alertes_historique_lire(const char* payload, AlerteHistoriqueLue out[], int max, int& illisibles) {
    int nb = 0;
    illisibles = 0;
    const char* p = payload;
    while (*p != '\0' && nb < max) {
        const char* fin = strchr(p, ';');
        const size_t n = fin ? static_cast<size_t>(fin - p) : strlen(p);
        // « apparue|lue|terminée|gravité|libellé » : le libellé est le reste (il ne contient
        // ni « | » ni « ; », HA les remplace) ; une entrée sans ses cinq champs est ignorée.
        Champ f[5] = {};
        if (champs_decouper_reste(p, n, '|', f, 5) == 5) {
            AlerteHistoriqueLue& e = out[nb];
            e.apparue = champ_entier(f[0], kAlerteEpochMax, 0);
            e.lue = champ_entier(f[1], kAlerteEpochMax, 0);
            e.terminee = champ_entier(f[2], kAlerteEpochMax, 0);
            e.gravite = f[3].n > 0 ? f[3].p[0] : 'O';
            if (e.gravite != 'R' && e.gravite != 'J') e.gravite = 'O';
            e.texte = f[4];
            if (e.apparue != 0) nb++;
            else illisibles++;
        } else {
            illisibles++;
        }
        if (fin == nullptr) break;
        p = fin + 1;
    }
    return nb;
}

void info_code_lire(const char* apres_prefixe, InfoCodeLu& out) {
    snprintf(out.buf, sizeof(out.buf), "%s", apres_prefixe);
    char* f[6];
    const int n = split_fields(out.buf, '|', f, 6);
    auto champ = [&](int i) -> const char* { return i < n ? f[i] : ""; };
    out.nb_maj = atoi(champ(0));
    out.titre = champ(1);
    out.nb_err = atoi(champ(2));
    out.nb_indispo = atoi(champ(3));
    out.jaune = atoi(champ(4)) != 0;
    out.vigi = champ(5);
}
