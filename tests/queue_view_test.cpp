// SPDX-License-Identifier: GPL-3.0-only
#include "../src/ui/psbox/queue_view.h"

#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include <string>

static X4QueueViewInput emptyInput() {
    X4QueueViewInput in{};
    in.language = 2; // Existing Spanish expectations stay explicit.
    in.allocation_seconds = in.provisioning_seconds = in.total_seconds = -1;
    in.estimate_age_seconds = in.initial_estimate_seconds = -1;
    in.initial_estimate_age_seconds = in.last_poll_age_seconds = in.seconds_left = -1;
    return in;
}

static std::string text(const X4QueueViewText& view) {
    std::string combined;
    for (unsigned i = 0; i < view.count; ++i) {
        assert(i < 6 && std::strlen(view.lines[i]) < sizeof(view.lines[i]));
        combined += view.lines[i];
        combined += '\n';
    }
    return combined;
}

int main() {
    X4QueueViewText view{};
    x4_queue_view_format(nullptr, &view);
    assert(view.count == 0);

    auto in = emptyInput();
    in.elapsed_seconds = 123;
    x4_queue_view_format(&in, &view);
    assert(view.count == 3);
    assert(text(view).find("Espera transcurrida: 2 min 3 s") != std::string::npos);
    assert(text(view).find("Estimación de Xbox: no disponible") != std::string::npos);
    assert(text(view).find("sin datos") != std::string::npos);

    in.estimate_status = X4_QUEUE_EST_AVAILABLE;
    in.total_seconds = 330;
    in.allocation_seconds = 110;
    in.provisioning_seconds = 130;
    in.initial_estimate_seconds = 300;
    in.initial_estimate_age_seconds = 150;
    in.estimate_age_seconds = 15;
    in.last_poll_age_seconds = 5;
    in.seconds_left = 240;
    in.region = "Brazil South";
    x4_queue_view_format(&in, &view);
    assert(view.count == 6);
    std::string shown = text(view);
    assert(shown.find("alrededor de 6 min") != std::string::npos);
    assert(shown.find("Asignación: aprox. 2 min") != std::string::npos);
    assert(shown.find("Preparación: aprox. 3 min") != std::string::npos);
    assert(shown.find("actualizada hace 15 s") != std::string::npos);
    assert(shown.find("Límite local de esta solicitud: 4 min 0 s") != std::string::npos);
    assert(shown.find("Región: Brazil South") != std::string::npos);
    assert(shown.find("supera la estimación inicial") == std::string::npos);

    // The first estimate expires by its own age, not by the session's elapsed
    // time; a fresh service estimate is still not a personal countdown.
    in.initial_estimate_age_seconds = 301;
    x4_queue_view_format(&in, &view);
    assert(text(view).find("La espera supera la estimación inicial") != std::string::npos);
    assert(text(view).find("alrededor de 6 min") != std::string::npos);

    // A failed/429 update may leave the last known estimate visible with age.
    in.estimate_status = X4_QUEUE_EST_ERROR;
    in.estimate_stale = 1;
    in.estimate_age_seconds = 190;
    x4_queue_view_format(&in, &view);
    shown = text(view);
    assert(shown.find("Última estimación de Xbox") != std::string::npos);
    assert(shown.find("No se pudo actualizar; último dato hace 3 min 10 s") != std::string::npos);
    assert(shown.find("HTTP") == std::string::npos);

    in.total_seconds = 0;
    in.allocation_seconds = in.provisioning_seconds = 0;
    in.initial_estimate_seconds = 0;
    in.estimate_status = X4_QUEUE_EST_UNAVAILABLE;
    x4_queue_view_format(&in, &view);
    assert(text(view).find("Xbox no estima demora adicional") != std::string::npos);
    assert(text(view).find("Asignación: aprox. 0 s") != std::string::npos);
    assert(text(view).find("supera") == std::string::npos);
    in.total_seconds = in.allocation_seconds = in.provisioning_seconds = -1;
    x4_queue_view_format(&in, &view);
    assert(text(view).find("Estimación de Xbox: no disponible") != std::string::npos);

    in.elapsed_seconds = UINT64_MAX;
    in.total_seconds = INT_MAX;
    in.allocation_seconds = INT_MAX;
    in.provisioning_seconds = INT_MAX;
    in.estimate_age_seconds = INT_MAX;
    in.last_poll_age_seconds = INT_MAX;
    in.region = "B\nrazil\r South";
    x4_queue_view_format(&in, &view);
    shown = text(view);
    assert(view.count <= 6);
    assert(shown.find("más de 24 h") != std::string::npos);
    assert(shown.find("B\nrazil") == std::string::npos);
    assert(shown.find("Región: B razil") != std::string::npos);
    assert(shown.find('%') == std::string::npos);

    // The same backend values must be readable in every selectable language.
    // Include all six rows (elapsed, estimate, components, age, poll, local
    // deadline) so a future partial translation cannot pass unnoticed.
    const char* elapsed_labels[6] = {"Wait elapsed", "Espera transcorrida", "Espera transcurrida",
        "Attente écoulée", "Bisherige Wartezeit", "Attesa trascorsa"};
    const char* estimate_labels[6] = {"Xbox estimate", "Estimativa do Xbox", "Estimación de Xbox",
        "Estimation Xbox", "Xbox-Schätzung", "Stima Xbox"};
    const char* allocation_labels[6] = {"Allocation", "Alocação", "Asignación",
        "Attribution", "Zuweisung", "Assegnazione"};
    const char* preparation_labels[6] = {"Preparation", "Preparação", "Preparación",
        "Préparation", "Vorbereitung", "Preparazione"};
    const char* region_labels[6] = {"Region", "Região", "Región", "Région", "Region", "Regione"};
    const char* deadline_labels[6] = {"Local request limit", "Limite local desta solicitação",
        "Límite local de esta solicitud", "Limite locale de cette demande",
        "Lokales Zeitlimit dieser Anfrage", "Limite locale della richiesta"};
    const char* unavailable_labels[6] = {"unavailable", "indisponível", "no disponible",
        "indisponible", "nicht verfügbar", "non disponibile"};
    const char* overdue_labels[6] = {"Wait exceeds initial estimate", "Espera supera estimativa inicial",
        "La espera supera la estimación inicial", "Attente supérieure à l'estimation initiale",
        "Warten länger als erste Schätzung", "Attesa oltre la stima iniziale"};
    const char* zero_labels[6] = {"expects no further wait", "não prevê mais espera",
        "no estima demora adicional", "ne prévoit plus d'attente",
        "erwartet keine weitere Wartezeit", "non prevede altra attesa"};
    const char* stale_labels[6] = {"No recent update", "Sem atualização recente",
        "Sin actualización reciente", "Pas de mise à jour récente",
        "Kein aktuelles Update", "Nessun aggiornamento recente"};
    const char* error_labels[6] = {"Update failed", "Falha na atualização",
        "No se pudo actualizar", "Échec de la mise à jour",
        "Update fehlgeschlagen", "Aggiornamento non riuscito"};
    const char* noage_error_labels[6] = {"Could not update estimate", "Não foi possível atualizar",
        "No se pudo actualizar la estimación", "Mise à jour de l'estimation impossible",
        "Schätzung konnte nicht aktualisiert werden", "Impossibile aggiornare la stima"};
    const char* unavailable_age_labels[6] = {"Xbox gave no new estimate", "Xbox não deu nova estimativa",
        "Xbox no dio nueva estimación", "Xbox n'a pas donné de nouvelle estimation",
        "Xbox gab keine neue Schätzung", "Xbox non ha dato una nuova stima"};
    const char* over_day_labels[6] = {"over 24 h", "mais de 24 h", "más de 24 h",
        "plus de 24 h", "über 24 Std.", "oltre 24 h"};
    const char* under_minute_labels[6] = {"under 1 min", "menos de 1 min", "menos de 1 min",
        "moins de 1 min", "unter 1 Min.", "meno di 1 min"};
    in = emptyInput();
    in.elapsed_seconds = 123;
    in.estimate_status = X4_QUEUE_EST_AVAILABLE;
    in.total_seconds = 330;
    in.allocation_seconds = 110;
    in.provisioning_seconds = 130;
    in.estimate_age_seconds = 15;
    in.initial_estimate_seconds = 300;
    in.initial_estimate_age_seconds = 301;
    in.last_poll_age_seconds = 5;
    in.seconds_left = 240;
    in.region = "Brazil South";
    for (int language = 0; language < 6; ++language) {
        in.language = language;
        x4_queue_view_format(&in, &view);
        assert(view.count == 6);
        shown = text(view);
        assert(shown.find(elapsed_labels[language]) != std::string::npos);
        assert(shown.find(estimate_labels[language]) != std::string::npos);
        assert(shown.find(allocation_labels[language]) != std::string::npos);
        assert(shown.find(preparation_labels[language]) != std::string::npos);
        assert(shown.find(region_labels[language]) != std::string::npos);
        assert(shown.find(deadline_labels[language]) != std::string::npos);
        assert(shown.find(overdue_labels[language]) != std::string::npos);
        assert(shown.find(language == 4 ? "6 Min." : "6 min") != std::string::npos);
        assert(shown.find("240") == std::string::npos); // Local limit is formatted, never mixed with Xbox ETA.

        in.total_seconds = -1;
        in.allocation_seconds = in.provisioning_seconds = -1;
        in.estimate_age_seconds = -1;
        in.initial_estimate_seconds = -1;
        x4_queue_view_format(&in, &view);
        shown = text(view);
        assert(view.count == 5);
        assert(shown.find(unavailable_labels[language]) != std::string::npos);
        assert(shown.find("sin datos") == std::string::npos || language == 2);

        in.total_seconds = 0; // Zero is present, unlike -1.
        in.estimate_status = X4_QUEUE_EST_AVAILABLE;
        x4_queue_view_format(&in, &view);
        shown = text(view);
        assert(shown.find(unavailable_labels[language]) == std::string::npos);
        assert(shown.find(zero_labels[language]) != std::string::npos);

        in.total_seconds = 30;
        in.estimate_age_seconds = 1;
        in.estimate_stale = 1;
        x4_queue_view_format(&in, &view);
        shown = text(view);
        assert(shown.find(under_minute_labels[language]) != std::string::npos);
        assert(shown.find(stale_labels[language]) != std::string::npos);

        in.estimate_stale = 0;
        in.estimate_status = X4_QUEUE_EST_ERROR;
        x4_queue_view_format(&in, &view);
        assert(text(view).find(error_labels[language]) != std::string::npos);
        in.estimate_age_seconds = -1;
        x4_queue_view_format(&in, &view);
        assert(text(view).find(noage_error_labels[language]) != std::string::npos);

        in.estimate_status = X4_QUEUE_EST_UNAVAILABLE;
        in.estimate_age_seconds = 1;
        x4_queue_view_format(&in, &view);
        assert(text(view).find(unavailable_age_labels[language]) != std::string::npos);

        in.elapsed_seconds = UINT64_MAX;
        in.total_seconds = INT_MAX;
        x4_queue_view_format(&in, &view);
        assert(text(view).find(over_day_labels[language]) != std::string::npos);

        in.elapsed_seconds = 123;
        in.total_seconds = 330;
        in.allocation_seconds = 110;
        in.provisioning_seconds = 130;
        in.estimate_age_seconds = 15;
        in.initial_estimate_seconds = 300;
        in.estimate_status = X4_QUEUE_EST_AVAILABLE;
    }

    // Out-of-range language has a deterministic, readable Spanish fallback.
    in.language = INT_MAX;
    x4_queue_view_format(&in, &view);
    assert(text(view).find("Espera transcurrida") != std::string::npos);

    std::cout << "queue_view_test: ok\n";
}
