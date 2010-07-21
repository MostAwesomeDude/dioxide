#include <math.h>
#include <stdio.h>

#include "dioxide.h"

/* These are technically twice the correct frequency. It makes the maths a bit
 * easier conceptually. */
static float drawbar_pitches[9] = {
    1,
    3,
    2,
    4,
    6,
    8,
    10,
    12,
    16,
};

void generate_titanium(struct dioxide *d, float *buffer, unsigned size)
{
    static double phase = 0.0;

    double step, pitch, accumulator;
    unsigned i, j, max_j;

    /* Halve the normal step and pitch, to compensate for the drawbars being
     * raised an octave. */
    step = M_PI * d->pitch * d->inverse_sample_rate;
    pitch = d->pitch * 0.5;

    for (i = 0; i < size; i++) {
        accumulator = 0;

        for (j = 0; j < 9; j++) {
            accumulator += sin(phase * j);
        }

        accumulator /= 9;

        phase += step;

        while (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }

        *buffer = accumulator;
        buffer++;
    }
}

struct element titanium = {
    generate_titanium,
};
