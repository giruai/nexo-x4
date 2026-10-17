// SPDX-License-Identifier: GPL-3.0-only
#include "../src/ui/psbox/queue_view.h"

#include <cassert>
#include <climits>
#include <cstring>
#include <iostream>
#include <string>

static X4QueueViewInput emptyInput() {
    X4QueueViewInput in{};
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

    std::cout << "queue_view_test: ok\n";
}
