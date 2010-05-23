#include <math.h>

#include "dioxide.h"

double step_lfo(struct dioxide *d, struct lfo *lfo, unsigned count) {
    double phase, step, retval;

    if (lfo->rate == 0) {
        lfo->phase = 0.0;
        return 0.0;
    }

    phase = lfo->phase;
    step = 2 * M_PI * lfo->rate / d->spec.freq;

    while (count--) {
        phase += step;
    }
    while (phase >= 2 * M_PI) {
        phase -= 2 * M_PI;
    }

    retval = lfo->amplitude * sin(phase) + lfo->center;

    lfo->phase = phase;

    return retval;
}
