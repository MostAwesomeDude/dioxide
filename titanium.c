#include <math.h>
#include <stdio.h>

#include "dioxide.h"

/* These are technically twice the correct frequency. It makes the maths a bit
 * easier conceptually. */
static float drawbar_pitches[9] = {
    0.5,
    1.5,
    1,
    2,
    3,
    4,
    5,
    6,
    8,
};

void generate_titanium(struct dioxide *d, struct note *note, float *buffer, unsigned size)
{
    double step, accumulator;
    unsigned i, j, attenuation;

    step = 2 * M_PI * note->pitch * d->inverse_sample_rate;

    for (i = 0; i < size; i++) {
        accumulator = 0;
        attenuation = 0;

        d->metal->adsr(d, note);

        for (j = 0; j < 9; j++) {
            if (d->drawbars[j]) {
                attenuation++;
                accumulator += (1.0/8.0) * d->drawbars[j] *
                    sin(note->phase * drawbar_pitches[j]);
            }
        }

        if (attenuation) {
            accumulator /= attenuation;
        }

        note->phase += step;

        while (note->phase > 2 * M_PI) {
            note->phase -= 2 * M_PI;
        }

        *buffer += accumulator * note->adsr_volume;
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
                note->adsr_phase = ADSR_SUSTAIN;
            }
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
