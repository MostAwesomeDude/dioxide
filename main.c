#include <math.h>
#include <signal.h>
#include <stdlib.h>

#include <asoundlib.h>

#include "SDL.h"
#include "SDL_audio.h"

static int time_to_quit = 0;

void handle_sigint(int s) {
    time_to_quit = 1;
    printf("Caught SIGINT, quitting.\n");
}

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
};

void write_sound(void *private, Uint8 *stream, int len);

void setup_sound(struct dioxide *d) {
    struct SDL_AudioSpec *spec = &d->spec;

    spec->freq = 22050;
    spec->format = AUDIO_S16;
    spec->channels = 1;
    spec->samples = 512;
    spec->callback = write_sound;
    spec->userdata = d;

    if (SDL_OpenAudio(spec, NULL)) {
        printf("Couldn't setup sound: %s\n", SDL_GetError());
        exit(1);
    }

    d->sample_rate = spec->freq;
    d->phase = 0.0;
}

void write_sound(void *private, Uint8 *stream, int len) {
    struct dioxide *d = private;
    double phase, step, accumulator;
    unsigned i, j;
    int retval;
    short *buf = (short*)stream;

    phase = d->phase;
    step = 2 * M_PI * d->pitch / d->sample_rate;

    for (i = 0; i < len / 2; i++) {
        accumulator = 0;

        for (j = 1; j < 5; j++) {
            accumulator += sin(phase * j) / j;
        }

        *buf = -accumulator * d->volume * 32767;
        buf++;
        phase += step;
        if (phase >= 2 * M_PI) {
            phase -= 2 * M_PI;
        }
    }

    d->phase = phase;
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

    if (!d->note_count) {
        return;
    }

    note = d->notes[d->note_count - 1];
    note += d->pitch_bend * (2.0 / 8192.0);

    d->pitch = 440 * pow(2, (note - 69.0) / 12.0);
    printf("New pitch is %f\n", d->pitch);
}

void handle_controller(struct dioxide *d, snd_seq_ev_ctrl_t control) {
    /* Oxygen pots and dials all go from 0 to 127. */
    switch (control.param) {
        /* C1 */
        case 74:
            break;
        /* C2 */
        case 71:
            break;
        default:
            printf("Controller %d\n", control.param);
            printf("Value %d\n", control.value);
            break;
    }
}

void poll_sequencer(struct dioxide *d) {
    snd_seq_event_t *event;
    enum snd_seq_event_type type;
    unsigned i;

    if (snd_seq_event_input(d->seq, &event) == -EAGAIN) {
        return;
    }

    type = event->type;

    switch (type) {
        case SND_SEQ_EVENT_NOTEON:
            if (d->note_count >= 16) {
                printf("Warning: Too many notes held, ignoring noteon.\n");
            } else {
                d->notes[d->note_count] = event->data.note.note;
                d->note_count++;
            }

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

            break;
        case SND_SEQ_EVENT_CONTROLLER:
            handle_controller(d, event->data.control);
        case SND_SEQ_EVENT_PITCHBEND:
            d->pitch_bend = event->data.control.value;
            break;
        default:
            printf("Got event type %u\n", type);
            break;
    }

    update_pitch(d);

    if (d->note_count) {
        SDL_PauseAudio(0);
    } else {
        SDL_PauseAudio(1);
    }
}

int main() {
    struct dioxide d = {0};
    int retval;

    signal(SIGINT, handle_sigint);

    d.volume = 1;

    setup_sound(&d);
    setup_sequencer(&d);

    while (!time_to_quit) {
        poll_sequencer(&d);
    }

    SDL_CloseAudio();

    retval = snd_seq_close(d.seq);
    exit(retval);
}
