#include <math.h>
#include <signal.h>
#include <stdlib.h>

#include "dioxide.h"

static int time_to_quit = 0;

/* 2 ** (cents/1200) */
static double twelve_cents = 1.0069555500567189;

void handle_sigint(int s) {
    time_to_quit = 1;
    printf("Caught SIGINT, quitting.\n");
}

void write_sound(void *private, Uint8 *stream, int len);

void setup_sound(struct dioxide *d) {
    struct SDL_AudioSpec actual, *wanted = &d->spec;

    wanted->freq = 48000;
    wanted->format = AUDIO_S16;
    wanted->channels = 1;
    wanted->samples = 512;
    wanted->callback = write_sound;
    wanted->userdata = d;

    if (SDL_OpenAudio(wanted, &actual)) {
        printf("Couldn't setup sound: %s\n", SDL_GetError());
        exit(1);
    }

    printf("Opened sound for playback: Rate %d, format %d, samples %d\n",
        actual.freq, actual.format, actual.samples);

    wanted->freq = actual.freq;
    wanted->format = actual.format;
    wanted->samples = actual.samples;

    d->volume = 0.7;

    d->drawbars[0].harmonic = 1;
    d->drawbars[1].harmonic = 3;
    d->drawbars[2].harmonic = 2;
    d->drawbars[3].harmonic = 4;
    d->drawbars[4].harmonic = 6;
    d->drawbars[5].harmonic = 8;
    d->drawbars[6].harmonic = 10;
    d->drawbars[7].harmonic = 12;
    d->drawbars[8].harmonic = 16;

    d->vibrato.center = 1;
    d->vibrato.amplitude = twelve_cents - 1;
}

void write_sound(void *private, Uint8 *stream, int len) {
    struct dioxide *d = private;
    double phase, step, accumulator;
    double rudess_phase, rudess_step;
    unsigned i, j, draws = 0;
    int retval;
    signed short *buf = (signed short*)stream;

    accumulator = step_lfo(d, &d->vibrato, len);

    if (accumulator == 0.0) {
        accumulator = 1;
    }

    phase = d->phase;
    step = M_PI * (d->pitch * accumulator) / d->spec.freq;

    if (d->rudess) {
        rudess_phase = d->rudess_phase;
        rudess_step = step * twelve_cents;
    }

    for (i = 0; i < len / 2; i++) {
        accumulator = 0;

        for (j = 0; j < 9; j++) {
            if (d->drawbars[j].stop) {
                accumulator += d->drawbars[j].stop *
                    sin(phase * d->drawbars[j].harmonic);
                if (d->rudess) {
                    accumulator += d->drawbars[j].stop *
                        sin(rudess_phase * d->drawbars[j].harmonic);
                }
            }
        }

        accumulator *= d->volume * -32767 / d->draws;

        if (d->rudess) {
            accumulator *= 0.5;
        }

        if (accumulator > 32767) {
            accumulator = 32767;
        } else if (accumulator < -32768) {
            accumulator = -32768;
        }

        *buf = (signed short)accumulator;
        buf++;
        phase += step;
        if (phase >= 2 * M_PI) {
            phase -= 2 * M_PI;
        }
        rudess_phase += rudess_step;
        if (rudess_phase >= 2 * M_PI) {
            rudess_phase -= 2 * M_PI;
        }
    }

    d->phase = phase;
    if (d->rudess) {
        d->rudess_phase = rudess_phase;
    }
}

void setup_sequencer(struct dioxide *d) {
    int retval;

    retval = snd_seq_open(&d->seq, "default",
        SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);

    if (retval) {
        printf("Couldn't open sequencer: %s\n", snd_strerror(retval));
        exit(retval);
    }

    snd_seq_set_client_name(d->seq, "Dioxide");

    snd_seq_create_simple_port(d->seq, "Dioxide",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC);
}

void update_pitch(struct dioxide *d) {
    double note;
    double bend;

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

    d->pitch = 440 * pow(2, (note - 69.0) / 12.0);
}

void update_draws(struct dioxide *d) {
    unsigned i;

    d->draws = 0;

    for (i = 0; i < 9; i++) {
        d->draws += d->drawbars[i].stop;
    }
}

long scale_pot_long(unsigned pot, long low, long high) {
    long l = pot * (high - low);

    return l / 127 + low;
}

double scale_pot_double(unsigned pot, double low, double high) {
    float f = pot / 127.0;

    return f * (high - low) + low;
}

void handle_controller(struct dioxide *d, snd_seq_ev_ctrl_t control) {
    /* Oxygen pots and dials all go from 0 to 127. */
    switch (control.param) {
        /* C1 */
        case 74:
            d->drawbars[0].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C2 */
        case 71:
            d->drawbars[1].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C3 */
        case 91:
            d->drawbars[2].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C4 */
        case 93:
            d->drawbars[3].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C5 */
        case 73:
            d->drawbars[4].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C6 */
        case 72:
            d->drawbars[5].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C7 */
        case 5:
            d->drawbars[6].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C8 */
        case 84:
            d->drawbars[7].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C9 */
        case 7:
            d->drawbars[8].stop = scale_pot_long(control.value, 0, 8);
            break;
        /* C10 */
        case 75:
            d->volume = scale_pot_double(control.value, 0.0, 1.0);
            printf("Volume %f\n", d->volume);
            break;
        /* C11 */
        case 76:
            break;
        /* C34 */
        case 1:
            d->vibrato.rate = scale_pot_double(control.value, 0.01, 5);
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
            d->rudess = !d->rudess;
            break;
        default:
            break;
    }
}

void poll_sequencer(struct dioxide *d) {
    snd_seq_event_t *event;
    enum snd_seq_event_type type;
    unsigned i, break_flag = 0;

    if (snd_seq_event_input(d->seq, &event) == -EAGAIN) {
        return;
    }

    type = event->type;

    switch (type) {
        case SND_SEQ_EVENT_NOTEON:
            if (d->note_count >= 16) {
                printf("Warning: Too many notes held, ignoring noteon.\n");
            } else {
                for (i = 0; i < d->note_count; i++) {
                    if (d->notes[i] == event->data.note.note) {
                        break_flag = 1;
                        break;
                    }
                }
                if (break_flag) {
                    break;
                }

                d->notes[d->note_count] = event->data.note.note;
                d->note_count++;
            }

            update_pitch(d);

            break;
        case SND_SEQ_EVENT_NOTEOFF:
            if (!d->note_count) {
                break;
            }

            for (i = 0; i < d->note_count; i++) {
                if (d->notes[i] == event->data.note.note) {
                    break;
                }
            }
            d->note_count--;
            for (i; i < d->note_count; i++) {
                d->notes[i] = d->notes[i + 1];
            }

            update_pitch(d);

            break;
        case SND_SEQ_EVENT_CONTROLLER:
            handle_controller(d, event->data.control);
            update_draws(d);
            break;
        case SND_SEQ_EVENT_PGMCHANGE:
            handle_program_change(d, event->data.control);
            break;
        case SND_SEQ_EVENT_PITCHBEND:
            d->pitch_bend = event->data.control.value;
            update_pitch(d);
            break;
        default:
            printf("Got event type %u\n", type);
            break;
    }

    if (d->note_count && d->draws) {
        SDL_PauseAudio(0);
    } else {
        SDL_PauseAudio(1);
    }
}

int main() {
    struct dioxide *d = calloc(1, sizeof(struct dioxide));
    int retval;

    signal(SIGINT, handle_sigint);

    if (d == NULL) {
        exit(EXIT_FAILURE);
    }

    setup_sound(d);
    setup_sequencer(d);

    while (!time_to_quit) {
        poll_sequencer(d);
    }

    SDL_CloseAudio();

    retval = snd_seq_close(d->seq);

    free(d);

    exit(retval);
}
