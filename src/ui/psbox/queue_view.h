/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef X4_PSBOX_QUEUE_VIEW_H
#define X4_PSBOX_QUEUE_VIEW_H

/* Pure, C-compatible presentation of Xbox's approximate queue estimate.
 * Every -1 field means unavailable. The estimate describes the service's
 * queue, never a personal countdown; seconds_left is only our local limit. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum X4QueueEstimateStatus {
    X4_QUEUE_EST_NONE = 0,
    X4_QUEUE_EST_AVAILABLE = 1,
    X4_QUEUE_EST_UNAVAILABLE = 2,
    X4_QUEUE_EST_ERROR = 3
};

typedef struct {
    int language;                    /* 0=en, 1=pt-BR, 2=es, 3=fr, 4=de, 5=it. */
    uint64_t elapsed_seconds;
    int allocation_seconds, provisioning_seconds, total_seconds;
    enum X4QueueEstimateStatus estimate_status;
    int estimate_age_seconds;         /* Age of last successful estimate. */
    int estimate_stale;               /* Service returned no fresh estimate. */
    int initial_estimate_seconds;     /* First total estimate, not current total. */
    int initial_estimate_age_seconds; /* Time since that first estimate. */
    int last_poll_age_seconds;        /* Age of latest session-state reply. */
    int seconds_left;                 /* Local request deadline, not Xbox ETA. */
    const char *region;               /* Optional, NUL-terminated. */
} X4QueueViewInput;

typedef struct {
    unsigned count;
    char lines[6][160];
} X4QueueViewText;

typedef struct {
    const char *over_24, *no_estimate, *under_minute;
    const char *hours_minutes, *minutes_seconds, *seconds, *minutes;
    const char *elapsed, *zero_fresh, *zero_stale;
    const char *total_fresh, *total_stale, *total_missing;
    const char *both_parts, *allocation, *provisioning, *one_part;
    const char *age_error, *age_unavailable, *age_stale, *age_fresh, *age_last;
    const char *noage_error, *noage_unavailable, *noage_none, *overdue;
    const char *region_poll, *poll, *region, *deadline;
} X4QueueViewLocale;

static inline const X4QueueViewLocale *x4_queue_view_locale(int language)
{
    static const X4QueueViewLocale locales[6] = {
        {
            "over 24 h", "no estimate", "under 1 min", "%llu h %llu min", "%llu min %llu s", "%llu s", "%u min",
            "Wait elapsed: %s", "Estimate: Xbox expects no further wait", "Last estimate: Xbox expected no further wait",
            "Xbox estimate: about %s", "Last Xbox estimate: about %s", "Xbox estimate: unavailable",
            "Allocation: approx. %s · Preparation: approx. %s", "Allocation", "Preparation", "%s: approx. %s",
            "Update failed; last value %s ago", "Xbox gave no new estimate; last value %s ago",
            "No recent update; last value %s ago", "Estimate updated %s ago", "Last estimate received %s ago",
            "Could not update estimate", "Xbox gave no new estimate", "Estimate update: no data",
            " · Wait exceeds initial estimate", "Region: %.48s · Xbox reply: %s ago", "Xbox reply: %s ago",
            "Region: %.48s", "Local request limit: %s"
        },
        {
            "mais de 24 h", "sem estimativa", "menos de 1 min", "%llu h %llu min", "%llu min %llu s", "%llu s", "%u min",
            "Espera transcorrida: %s", "Estimativa: Xbox não prevê mais espera", "Última estimativa: Xbox não previa mais espera",
            "Estimativa do Xbox: cerca de %s", "Última estimativa do Xbox: cerca de %s", "Estimativa do Xbox: indisponível",
            "Alocação: aprox. %s · Preparação: aprox. %s", "Alocação", "Preparação", "%s: aprox. %s",
            "Falha na atualização; último dado há %s", "Xbox não deu nova estimativa; último dado há %s",
            "Sem atualização recente; último dado há %s", "Estimativa atualizada há %s", "Última estimativa recebida há %s",
            "Não foi possível atualizar a estimativa", "Xbox não deu nova estimativa", "Atualização da estimativa: sem dados",
            " · Espera supera estimativa inicial", "Região: %.48s · Resposta Xbox: há %s", "Resposta Xbox: há %s",
            "Região: %.48s", "Limite local desta solicitação: %s"
        },
        {
            "más de 24 h", "sin estimación", "menos de 1 min", "%llu h %llu min", "%llu min %llu s", "%llu s", "%u min",
            "Espera transcurrida: %s", "Estimación: Xbox no estima demora adicional", "Última estimación: Xbox no estima demora adicional",
            "Estimación de Xbox: alrededor de %s", "Última estimación de Xbox: alrededor de %s", "Estimación de Xbox: no disponible",
            "Asignación: aprox. %s · Preparación: aprox. %s", "Asignación", "Preparación", "%s: aprox. %s",
            "No se pudo actualizar; último dato hace %s", "Xbox no dio nueva estimación; último dato hace %s",
            "Sin actualización reciente; último dato hace %s", "Estimación actualizada hace %s", "Última estimación recibida hace %s",
            "No se pudo actualizar la estimación", "Xbox no dio una nueva estimación", "Actualización de estimación: sin datos",
            " · La espera supera la estimación inicial", "Región: %.48s · Respuesta Xbox: hace %s", "Respuesta Xbox: hace %s",
            "Región: %.48s", "Límite local de esta solicitud: %s"
        },
        {
            "plus de 24 h", "sans estimation", "moins de 1 min", "%llu h %llu min", "%llu min %llu s", "%llu s", "%u min",
            "Attente écoulée : %s", "Estimation : Xbox ne prévoit plus d'attente", "Dernière estimation : Xbox ne prévoyait plus d'attente",
            "Estimation Xbox : environ %s", "Dernière estimation Xbox : environ %s", "Estimation Xbox : indisponible",
            "Attribution : env. %s · Préparation : env. %s", "Attribution", "Préparation", "%s : env. %s",
            "Échec de la mise à jour ; dernière valeur il y a %s", "Xbox n'a pas donné de nouvelle estimation ; dernière valeur il y a %s",
            "Pas de mise à jour récente ; dernière valeur il y a %s", "Estimation mise à jour il y a %s", "Dernière estimation reçue il y a %s",
            "Mise à jour de l'estimation impossible", "Xbox n'a pas donné de nouvelle estimation", "Mise à jour de l'estimation : aucune donnée",
            " · Attente supérieure à l'estimation initiale", "Région : %.48s · Réponse Xbox : il y a %s", "Réponse Xbox : il y a %s",
            "Région : %.48s", "Limite locale de cette demande : %s"
        },
        {
            "über 24 Std.", "keine Schätzung", "unter 1 Min.", "%llu Std. %llu Min.", "%llu Min. %llu Sek.", "%llu Sek.", "%u Min.",
            "Bisherige Wartezeit: %s", "Schätzung: Xbox erwartet keine weitere Wartezeit", "Letzte Schätzung: Xbox erwartete keine weitere Wartezeit",
            "Xbox-Schätzung: etwa %s", "Letzte Xbox-Schätzung: etwa %s", "Xbox-Schätzung: nicht verfügbar",
            "Zuweisung: ca. %s · Vorbereitung: ca. %s", "Zuweisung", "Vorbereitung", "%s: ca. %s",
            "Update fehlgeschlagen; letzter Wert vor %s", "Xbox gab keine neue Schätzung; letzter Wert vor %s",
            "Kein aktuelles Update; letzter Wert vor %s", "Schätzung aktualisiert vor %s", "Letzte Schätzung erhalten vor %s",
            "Schätzung konnte nicht aktualisiert werden", "Xbox gab keine neue Schätzung", "Schätzungsupdate: keine Daten",
            " · Warten länger als erste Schätzung", "Region: %.48s · Xbox-Antwort: vor %s", "Xbox-Antwort: vor %s",
            "Region: %.48s", "Lokales Zeitlimit dieser Anfrage: %s"
        },
        {
            "oltre 24 h", "nessuna stima", "meno di 1 min", "%llu h %llu min", "%llu min %llu s", "%llu s", "%u min",
            "Attesa trascorsa: %s", "Stima: Xbox non prevede altra attesa", "Ultima stima: Xbox non prevedeva altra attesa",
            "Stima Xbox: circa %s", "Ultima stima Xbox: circa %s", "Stima Xbox: non disponibile",
            "Assegnazione: circa %s · Preparazione: circa %s", "Assegnazione", "Preparazione", "%s: circa %s",
            "Aggiornamento non riuscito; ultimo dato %s fa", "Xbox non ha dato una nuova stima; ultimo dato %s fa",
            "Nessun aggiornamento recente; ultimo dato %s fa", "Stima aggiornata %s fa", "Ultima stima ricevuta %s fa",
            "Impossibile aggiornare la stima", "Xbox non ha dato una nuova stima", "Aggiornamento stima: nessun dato",
            " · Attesa oltre la stima iniziale", "Regione: %.48s · Risposta Xbox: %s fa", "Risposta Xbox: %s fa",
            "Regione: %.48s", "Limite locale della richiesta: %s"
        }
    };
    return &locales[language >= 0 && language < 6 ? language : 2];
}

static inline void x4_queue_view_duration(int language, uint64_t seconds, char *out, size_t size)
{
    const X4QueueViewLocale *locale = x4_queue_view_locale(language);
    if (seconds > 24u * 3600u) snprintf(out, size, "%s", locale->over_24);
    else if (seconds >= 3600u) snprintf(out, size, locale->hours_minutes,
        (unsigned long long)(seconds / 3600u), (unsigned long long)((seconds % 3600u) / 60u));
    else if (seconds >= 60u) snprintf(out, size, locale->minutes_seconds,
        (unsigned long long)(seconds / 60u), (unsigned long long)(seconds % 60u));
    else snprintf(out, size, locale->seconds, (unsigned long long)seconds);
}

static inline void x4_queue_view_approx(int language, int seconds, char *out, size_t size)
{
    const X4QueueViewLocale *locale = x4_queue_view_locale(language);
    if (seconds < 0) { snprintf(out, size, "%s", locale->no_estimate); return; }
    if (seconds == 0) { snprintf(out, size, locale->seconds, 0ull); return; }
    if (seconds > 24 * 3600) { snprintf(out, size, "%s", locale->over_24); return; }
    if (seconds < 60) { snprintf(out, size, "%s", locale->under_minute); return; }
    unsigned minutes = ((unsigned)seconds + 59u) / 60u;
    if (minutes >= 60u) snprintf(out, size, locale->hours_minutes,
        (unsigned long long)(minutes / 60u), (unsigned long long)(minutes % 60u));
    else snprintf(out, size, locale->minutes, minutes);
}

static inline void x4_queue_view_format(const X4QueueViewInput *input, X4QueueViewText *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!input) return;
    const X4QueueViewLocale *locale = x4_queue_view_locale(input->language);
    char elapsed[40], total[40], allocation[40], provisioning[40], age[40];
    x4_queue_view_duration(input->language, input->elapsed_seconds, elapsed, sizeof(elapsed));
    snprintf(out->lines[out->count++], sizeof(out->lines[0]), locale->elapsed, elapsed);

    int has_total = input->total_seconds >= 0;
    if (has_total && input->total_seconds == 0) {
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), "%s",
            input->estimate_status == X4_QUEUE_EST_AVAILABLE && !input->estimate_stale ?
                locale->zero_fresh : locale->zero_stale);
    } else if (has_total) {
        x4_queue_view_approx(input->language, input->total_seconds, total, sizeof(total));
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            input->estimate_status == X4_QUEUE_EST_AVAILABLE && !input->estimate_stale ?
                locale->total_fresh : locale->total_stale, total);
    } else {
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), "%s", locale->total_missing);
    }

    if (input->allocation_seconds >= 0 || input->provisioning_seconds >= 0) {
        x4_queue_view_approx(input->language, input->allocation_seconds, allocation, sizeof(allocation));
        x4_queue_view_approx(input->language, input->provisioning_seconds, provisioning, sizeof(provisioning));
        if (input->allocation_seconds >= 0 && input->provisioning_seconds >= 0)
            snprintf(out->lines[out->count++], sizeof(out->lines[0]), locale->both_parts, allocation, provisioning);
        else snprintf(out->lines[out->count++], sizeof(out->lines[0]), locale->one_part,
                input->allocation_seconds >= 0 ? locale->allocation : locale->provisioning,
                input->allocation_seconds >= 0 ? allocation : provisioning);
    }

    if (input->estimate_age_seconds >= 0) {
        x4_queue_view_duration(input->language, (uint64_t)input->estimate_age_seconds, age, sizeof(age));
        const char *update = input->estimate_status == X4_QUEUE_EST_ERROR ?
            locale->age_error :
            input->estimate_status == X4_QUEUE_EST_UNAVAILABLE ?
            locale->age_unavailable :
            input->estimate_stale ? locale->age_stale :
            input->estimate_status == X4_QUEUE_EST_AVAILABLE ? locale->age_fresh : locale->age_last;
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), update, age);
    } else {
        const char *update = input->estimate_status == X4_QUEUE_EST_ERROR ?
            locale->noage_error :
            input->estimate_status == X4_QUEUE_EST_UNAVAILABLE ?
            locale->noage_unavailable : locale->noage_none;
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), "%s", update);
    }
    if (input->initial_estimate_seconds > 0 &&
        input->initial_estimate_age_seconds > input->initial_estimate_seconds) {
        size_t n = strlen(out->lines[out->count - 1]);
        snprintf(out->lines[out->count - 1] + n, sizeof(out->lines[0]) - n,
            "%s", locale->overdue);
    }

    char region[49] = {0};
    if (input->region) {
        size_t i = 0;
        for (; i < sizeof(region) - 1 && input->region[i]; ++i) {
            unsigned char ch = (unsigned char)input->region[i];
            region[i] = ch < 32 || ch == 127 ? ' ' : (char)ch;
        }
        region[i] = 0;
    }
    if (region[0] || input->last_poll_age_seconds >= 0) {
        if (input->last_poll_age_seconds >= 0) {
            x4_queue_view_duration(input->language, (uint64_t)input->last_poll_age_seconds, age, sizeof(age));
            if (region[0]) snprintf(out->lines[out->count++], sizeof(out->lines[0]),
                locale->region_poll, region, age);
            else snprintf(out->lines[out->count++], sizeof(out->lines[0]),
                locale->poll, age);
        } else snprintf(out->lines[out->count++], sizeof(out->lines[0]), locale->region, region);
    }
    if (input->seconds_left >= 0 && out->count < 6) {
        x4_queue_view_duration(input->language, (uint64_t)input->seconds_left, age, sizeof(age));
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), locale->deadline, age);
    }
}

#ifdef __cplusplus
}
#endif
#endif
