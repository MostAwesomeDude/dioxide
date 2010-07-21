#include <math.h>
#include <stdio.h>

#include "dioxide.h"

void generate_uranium(struct dioxide *d, float *buffer, unsigned size)
{
    static double phase = 0.0;

    double step, accumulator;
    unsigned i, j, max_j;
    struct ladspa_plugin *plugin = d->plugin_chain;

    step = 2 * M_PI * d->pitch * d->inverse_sample_rate;

    for (i = 0; i < size; i++) {
        accumulator = 0;

        /* Weird things I've discovered.
         * BLITs aren't necessary. This is strictly additive.
         *
         * If the number of additions is above 120 or so, stuff gets really
         * shitty-sounding. The magic number of 129 should suffice for most
         * things.
         *
         * If the number of additions is even, everything goes to shit. This
         * helped: http://www.music.mcgill.ca/~gary/307/week5/bandlimited.html
         */
        max_j = d->spec.freq / d->pitch / 3;
        if (max_j > 129) {
            max_j = 129;
        } else if (!(max_j % 2)) {
            max_j--;
        }

        for (j = 1; j < max_j; j++) {
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

struct element uranium = {
    generate_uranium,
};
