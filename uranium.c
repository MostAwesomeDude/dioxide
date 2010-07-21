#include <math.h>
#include <stdio.h>

#include "dioxide.h"

void generate_uranium(struct dioxide *d, float *buffer, unsigned size)
{
    static double phase = 0.0;

    double step, accumulator;
    unsigned i, j;
    struct ladspa_plugin *plugin = d->plugin_chain;

    if (!d->pitch) {
        return;
    }

    step = 2 * M_PI * d->pitch * d->inverse_sample_rate;

    for (i = 0; i < size; i++) {
        accumulator = 0;

        for (j = 1; j * d->pitch < d->spec.freq / 2; j++) {
            accumulator += sin(phase * j) / j;
        }

        phase += step;

        while (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }

        *buffer = accumulator;
        buffer++;
    }
}
