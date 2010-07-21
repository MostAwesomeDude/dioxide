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

void adsr_titanium(struct dioxide *d) {
    static float peak = 1.0;
    switch (d->adsr_phase) {
        case ADSR_ATTACK:
            if (d->adsr_volume < peak) {
                d->adsr_volume += d->inverse_sample_rate / d->attack_time;
            } else {
                d->adsr_volume = peak;
                d->adsr_phase = ADSR_DECAY;
            }
            break;
        case ADSR_DECAY:
            d->adsr_phase = ADSR_SUSTAIN;
            break;
        case ADSR_SUSTAIN:
            break;
        case ADSR_RELEASE:
            if (d->adsr_volume > 0.0) {
                d->adsr_volume -= peak * d->inverse_sample_rate
                    / d->release_time;
            } else {
                d->adsr_volume = 0.0;
            }
            break;
        default:
            break;
    }
}

struct element titanium = {
    generate_titanium,
    adsr_titanium,
};
