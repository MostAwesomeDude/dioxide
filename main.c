#include <complex.h>
#include <math.h>
#include <signal.h>
#include <stdlib.h>

#include <sys/time.h>

#include "dioxide.h"

static int time_to_quit = 0;

void handle_sigint(int s) {
    time_to_quit = 1;
    printf("Caught SIGINT, quitting.\n");
}

void update_adsr(struct dioxide *d);
void update_pitch(struct dioxide *d);
void write_sound(void *private, Uint8 *stream, int len);

void setup_sound(struct dioxide *d) {
    struct SDL_AudioSpec actual, *wanted = &d->spec;
    double temp;
    unsigned i;

    wanted->freq = 22050;
    wanted->format = AUDIO_S16;
    wanted->channels = 1;
    wanted->samples = 512;
    wanted->callback = write_sound;
    wanted->userdata = d;

    if (SDL_OpenAudio(wanted, &actual)) {
        printf("Couldn't setup sound: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    printf("Opened sound for playback: Rate %d, format %d, samples %d\n",
        actual.freq, actual.format, actual.samples);

    wanted->freq = actual.freq;
    wanted->format = actual.format;
    wanted->samples = actual.samples;

    d->volume = 0.7;

    d->lpf_cutoff = d->spec.freq / 3;
    d->lpf_resonance = 4.0;

    printf("Initialized basic synth parameters, frame length is %d usec\n",
        (1000 * 1000 * actual.samples / actual.freq));
}

void write_sound(void *private, Uint8 *stream, int len) {
    struct dioxide *d = private;
    double accumulator;
    unsigned i;
    int retval;
    float *samples, *backburner, *ftemp;
    signed short *buf = (signed short*)stream;
    struct timeval then, now;
    struct ladspa_plugin *plugin;

    gettimeofday(&then, NULL);

    /* Treat len and buf as counting shorts, not bytes.
     * Avoids cognitive dissonance in later code. */
    len /= 2;

    /* Update pitch only once per buffer. */
    update_pitch(d);

    samples = malloc(len * sizeof(float));
    backburner = malloc(len * sizeof(float));

    for (i = 0; i < len; i++) {
        backburner[i] = d->pitch;
    }

    plugin = d->plugin_chain;

    plugin->desc->connect_port(plugin->handle, plugin->input, backburner);
    plugin->desc->connect_port(plugin->handle, plugin->output, samples);
    plugin->desc->run(plugin->handle, len);
#if 0
    printf("initialsamples = [\n");
    for (i = 0; i < len; i++) {
        printf("%f,\n", samples[i]);
    }
    printf("]\n");
#endif
    while (plugin->next) {
        plugin = plugin->next;

        /* Switch the names of the buffers, so that "samples" is always the
         * buffer being rendered to. */
        if (LADSPA_IS_INPLACE_BROKEN(plugin->desc->Properties)) {
            ftemp = backburner;
            backburner = samples;
            samples = ftemp;

            plugin->desc->connect_port(plugin->handle,
                plugin->input, backburner);
        } else {
            plugin->desc->connect_port(plugin->handle, plugin->input, samples);
        }

        plugin->desc->connect_port(plugin->handle, plugin->output, samples);
        plugin->desc->run(plugin->handle, len);
#if 0
        printf("samples = [\n");
        for (i = 0; i < len; i++) {
            printf("%f,\n", samples[i]);
        }
        printf("]\n");
#endif
    }

    for (i = 0; i < len; i++) {
        accumulator = samples[i];

        update_adsr(d);

        accumulator *= d->adsr_volume * d->volume * -32767;

        if (accumulator > 32767) {
            accumulator = 32767;
        } else if (accumulator < -32768) {
            accumulator = -32768;
        }

        *buf = (signed short)accumulator;
        buf++;
    }

    free(samples);
    free(backburner);

    gettimeofday(&now, NULL);

    while (now.tv_sec != then.tv_sec) {
        now.tv_sec--;
        now.tv_sec += 1000 * 1000;
    }

    if (now.tv_usec > then.tv_usec + (1000 * 1000 * len / d->spec.freq)) {
        printf("Long frame: %d usec\n", now.tv_usec - then.tv_usec);
    }
}

void update_adsr(struct dioxide *d) {
    switch (d->adsr_phase) {
        case ADSR_ATTACK:
            if (d->adsr_volume < 1.0) {
                d->adsr_volume += 1.0 / (d->attack_time * d->spec.freq);
            } else {
                d->adsr_volume = 1.0;
                d->adsr_phase = ADSR_DECAY;
            }
            break;
        case ADSR_DECAY:
            if (d->adsr_volume > 0.88) {
                d->adsr_volume -= 0.12 / (d->decay_time * d->spec.freq);
            } else {
                d->adsr_volume = 0.88;
                d->adsr_phase = ADSR_SUSTAIN;
            }
            break;
        case ADSR_SUSTAIN:
            break;
        case ADSR_RELEASE:
            if (d->adsr_volume > 0.0) {
                d->adsr_volume -= 0.88 / (d->release_time * d->spec.freq);
            } else {
                d->adsr_volume = 0.0;
            }
            break;
        default:
            break;
    }
}

void update_pitch(struct dioxide *d) {
    double note, bend, target_pitch, ratio;

    if (!d->note_count) {
        return;
    }

    /* Split the pitch wheel into an upper and lower range. */
    if (d->pitch_bend >= 0) {
        bend = d->pitch_bend * (2.0 / 8192.0);
    } else {
        bend = d->pitch_bend * (12.0 / 8192.0);
    }

    note = d->notes[d->note_count - 1];
    note += bend;

    target_pitch = 440 * pow(2, (note - 69.0) / 12.0);

    ratio = target_pitch / d->pitch;

    if ((d->pitch < 20) ||
        ((step_up > ratio) && (ratio > step_down)) ||
        !d->gliss) {
        d->pitch = target_pitch;
    } else {
        d->pitch = pow(M_E, log(d->pitch) + log(ratio) / 4);
    }
}

long scale_pot_long(unsigned pot, long low, long high) {
    long l = pot * (high - low);

    return l / 127 + low;
}

float scale_pot_float(unsigned pot, float low, float high) {
    float f = pot / 127.0;

    return f * (high - low) + low;
}

float scale_pot_log_float(unsigned pot, float low, float high) {
    float f = pot / 127.0;

    low = log(low);
    high = log(high);

    f = f * (high - low) + low;

    return pow(M_E, f);
}

void handle_controller(struct dioxide *d, snd_seq_ev_ctrl_t control) {
    /* Oxygen pots and dials all go from 0 to 127. */
    switch (control.param) {
        /* C1 */
        case 74:
            d->chorus_delay = scale_pot_log_float(control.value, 2.5, 40);
            break;
        /* C2 */
        case 71:
            d->phaser_rate = scale_pot_float(control.value, 0, 1);
            d->phaser_depth = scale_pot_float(control.value, 0, 1);
            break;
        /* C3 */
        case 91:
            d->phaser_spread = scale_pot_float(control.value, 0, 1.5708);
            break;
        /* C4 */
        case 93:
            d->phaser_feedback = scale_pot_float(control.value, 0, 0.999);
            break;
        /* C5 */
        case 73:
            break;
        /* C6 */
        case 72:
            break;
        /* C7 */
        case 5:
            break;
        /* C8 */
        case 84:
            break;
        /* C9 */
        case 7:
            d->lpf_resonance = scale_pot_float(control.value, 0.0, 4.0);
            break;
        /* C10 */
        case 75:
            d->volume = scale_pot_float(control.value, 0.0, 1.0);
            break;
        /* C11 */
        case 76:
            d->attack_time = scale_pot_float(control.value, 0.001, 1.0);
            break;
        /* C12 */
        case 92:
            d->decay_time = scale_pot_float(control.value, 0.001, 1.0);
            break;
        /* C13 */
        case 95:
            d->release_time = scale_pot_float(control.value, 0.001, 1.0);
            break;
        /* C34 */
        case 1:
            break;
        default:
            printf("Controller %d\n", control.param);
            printf("Value %d\n", control.value);
            break;
    }
}

void handle_program_change(struct dioxide *d, snd_seq_ev_ctrl_t control) {
    switch (control.value) {
        /* C18 */
        case 0:
            d->gliss = !d->gliss;
            break;
        /* C19 */
        case 1:
            d->legato = !d->legato;
            break;
        default:
            printf("Program change %d\n", control.value);
            break;
    }
}

int main() {
    struct dioxide *d = calloc(1, sizeof(struct dioxide));
    int retval;

    signal(SIGINT, handle_sigint);

    if (!d) {
        exit(EXIT_FAILURE);
    }

    setup_sound(d);
    setup_plugins(d);
    setup_sequencer(d);
    hook_plugins(d);

    while (!time_to_quit) {
        poll_sequencer(d);

        if (!d->connected) {
            solicit_connections(d);
        }
    }

    cleanup_plugins(d);

    retval = snd_seq_close(d->seq);
    SDL_CloseAudio();

    free(d);
    exit(retval);
}
