#include <asoundlib.h>

#include "SDL.h"
#include "SDL_audio.h"

struct lfo {
    double phase;
    unsigned rate;
};

struct lpf {
    double a, b, c, d;
    double sample_prev, sample_ancient;
    double retval_prev, retval_ancient;

    double resonance;
    double cutoff;
};

struct dioxide {
    snd_seq_t *seq;

    struct SDL_AudioSpec spec;

    unsigned sample_rate;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_uframes_t period_frames;

    double volume;
    double phase;
    double pitch;
    signed short pitch_bend;
    unsigned notes[16];
    unsigned note_count;

    struct lfo vibrato;
    struct lpf lpf;

    struct {
        unsigned harmonic;
        unsigned stop;
    } drawbars[8];
    unsigned draws;
};

double step_lfo(struct dioxide *d, struct lfo *lfo);
