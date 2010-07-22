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

void generate_titanium(struct dioxide *d, struct note *note, float *buffer, unsigned size)
{
    static double phase = 0.0;

    double step, pitch, accumulator;
    unsigned i, j, max_j;

    /* Halve the normal step and pitch, to compensate for the drawbars being
     * raised an octave. */
    step = M_PI * note->pitch * d->inverse_sample_rate;
    pitch = note->pitch * 0.5;

    for (i = 0; i < size; i++) {
        accumulator = 0;

        d->metal->adsr(d, note);

        for (j = 0; j < 9; j++) {
            accumulator += sin(phase * j);
        }

        accumulator /= 9;

        phase += step;

        while (phase > 2 * M_PI) {
            phase -= 2 * M_PI;
        }

        *buffer = accumulator * note->adsr_volume;
        buffer++;
    }
}

void adsr_titanium(struct dioxide *d, struct note *note) {
    static float peak = 1.0;
    switch (note->adsr_phase) {
        case ADSR_ATTACK:
            if (note->adsr_volume < peak) {
                note->adsr_volume += d->inverse_sample_rate / d->attack_time;
            } else {
                note->adsr_volume = peak;
                note->adsr_phase = ADSR_DECAY;
            }
            break;
        case ADSR_DECAY:
            note->adsr_phase = ADSR_SUSTAIN;
            break;
        case ADSR_SUSTAIN:
            break;
        case ADSR_RELEASE:
            if (note->adsr_volume > 0.0) {
                note->adsr_volume -= peak * d->inverse_sample_rate
                    / d->release_time;
            } else {
                note->adsr_volume = 0.0;
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
