#include <asoundlib.h>

#include <fftw3.h>

#include "SDL.h"
#include "SDL_audio.h"

struct lfo {
    double phase;

    double rate;
    double center;
    double amplitude;
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

    int rudess;
    double second_phase;

    double *fft_in;
    fftw_complex *fft_out, *lpf_fft;
    fftw_plan fft_plan, ifft_plan;

    double lpf_cutoff;
};

double step_lfo(struct dioxide *d, struct lfo *lfo, unsigned count);
