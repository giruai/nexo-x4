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

static inline void x4_queue_view_duration(uint64_t seconds, char *out, size_t size)
{
    if (seconds > 24u * 3600u) snprintf(out, size, "más de 24 h");
    else if (seconds >= 3600u) snprintf(out, size, "%llu h %llu min",
        (unsigned long long)(seconds / 3600u), (unsigned long long)((seconds % 3600u) / 60u));
    else if (seconds >= 60u) snprintf(out, size, "%llu min %llu s",
        (unsigned long long)(seconds / 60u), (unsigned long long)(seconds % 60u));
    else snprintf(out, size, "%llu s", (unsigned long long)seconds);
}

static inline void x4_queue_view_approx(int seconds, char *out, size_t size)
{
    if (seconds < 0) { snprintf(out, size, "sin estimación"); return; }
    if (seconds == 0) { snprintf(out, size, "0 s"); return; }
    if (seconds > 24 * 3600) { snprintf(out, size, "más de 24 h"); return; }
    if (seconds < 60) { snprintf(out, size, "menos de 1 min"); return; }
    unsigned minutes = ((unsigned)seconds + 59u) / 60u;
    if (minutes >= 60u) snprintf(out, size, "%u h %u min", minutes / 60u, minutes % 60u);
    else snprintf(out, size, "%u min", minutes);
}

static inline void x4_queue_view_format(const X4QueueViewInput *input, X4QueueViewText *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!input) return;
    char elapsed[40], total[40], allocation[40], provisioning[40], age[40];
    x4_queue_view_duration(input->elapsed_seconds, elapsed, sizeof(elapsed));
    snprintf(out->lines[out->count++], sizeof(out->lines[0]), "Espera transcurrida: %s", elapsed);

    int has_total = input->total_seconds >= 0;
    if (has_total && input->total_seconds == 0) {
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            "%s: Xbox no estima demora adicional",
            input->estimate_status == X4_QUEUE_EST_AVAILABLE && !input->estimate_stale ?
                "Estimación" : "Última estimación");
    } else if (has_total) {
        x4_queue_view_approx(input->total_seconds, total, sizeof(total));
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            "%s de Xbox: alrededor de %s",
            input->estimate_status == X4_QUEUE_EST_AVAILABLE && !input->estimate_stale ?
                "Estimación" : "Última estimación", total);
    } else {
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            "Estimación de Xbox: no disponible");
    }

    if (input->allocation_seconds >= 0 || input->provisioning_seconds >= 0) {
        x4_queue_view_approx(input->allocation_seconds, allocation, sizeof(allocation));
        x4_queue_view_approx(input->provisioning_seconds, provisioning, sizeof(provisioning));
        if (input->allocation_seconds >= 0 && input->provisioning_seconds >= 0)
            snprintf(out->lines[out->count++], sizeof(out->lines[0]),
                "Asignación: aprox. %s · Preparación: aprox. %s", allocation, provisioning);
        else snprintf(out->lines[out->count++], sizeof(out->lines[0]), "%s: aprox. %s",
                input->allocation_seconds >= 0 ? "Asignación" : "Preparación",
                input->allocation_seconds >= 0 ? allocation : provisioning);
    }

    if (input->estimate_age_seconds >= 0) {
        x4_queue_view_duration((uint64_t)input->estimate_age_seconds, age, sizeof(age));
        const char *update = input->estimate_status == X4_QUEUE_EST_ERROR ?
            "No se pudo actualizar; último dato" :
            input->estimate_status == X4_QUEUE_EST_UNAVAILABLE ?
            "Xbox no dio nueva estimación; último dato" :
            input->estimate_stale ? "Sin actualización reciente; último dato" :
            input->estimate_status == X4_QUEUE_EST_AVAILABLE ? "Estimación actualizada" :
            "Última estimación recibida";
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            "%s hace %s", update, age);
    } else {
        const char *update = input->estimate_status == X4_QUEUE_EST_ERROR ?
            "No se pudo actualizar la estimación" :
            input->estimate_status == X4_QUEUE_EST_UNAVAILABLE ?
            "Xbox no dio una nueva estimación" :
            "Actualización de estimación: sin datos";
        snprintf(out->lines[out->count++], sizeof(out->lines[0]), "%s", update);
    }
    if (input->initial_estimate_seconds > 0 &&
        input->initial_estimate_age_seconds > input->initial_estimate_seconds) {
        size_t n = strlen(out->lines[out->count - 1]);
        snprintf(out->lines[out->count - 1] + n, sizeof(out->lines[0]) - n,
            " · La espera supera la estimación inicial");
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
            x4_queue_view_duration((uint64_t)input->last_poll_age_seconds, age, sizeof(age));
            if (region[0]) snprintf(out->lines[out->count++], sizeof(out->lines[0]),
                "Región: %.48s · Respuesta Xbox: hace %s", region, age);
            else snprintf(out->lines[out->count++], sizeof(out->lines[0]),
                "Respuesta Xbox: hace %s", age);
        } else snprintf(out->lines[out->count++], sizeof(out->lines[0]), "Región: %.48s", region);
    }
    if (input->seconds_left >= 0 && out->count < 6) {
        x4_queue_view_duration((uint64_t)input->seconds_left, age, sizeof(age));
        snprintf(out->lines[out->count++], sizeof(out->lines[0]),
            "Límite local de esta solicitud: %s", age);
    }
}

#ifdef __cplusplus
}
#endif
#endif
