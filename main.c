#include <math.h>
#include <stdlib.h>

#include <asoundlib.h>

struct dioxide {
    snd_seq_t *seq;
    snd_pcm_t *pcm;

    unsigned sample_rate;
    snd_pcm_uframes_t buffer_frames;
    snd_pcm_format_t format;

    double phase;
};

void setup_pcm(struct dioxide *d) {
    snd_pcm_hw_params_t *params;
    int retval;

    snd_pcm_hw_params_alloca(&params);

    retval = snd_pcm_open(&d->pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);

    if (retval) {
        printf("Couldn't open PCM: %s\n", snd_strerror(retval));
        exit(retval);
    }

    d->sample_rate = 44100;

    snd_pcm_hw_params_any(d->pcm, params);
    snd_pcm_hw_params_set_rate_resample(d->pcm, params, 0);
    snd_pcm_hw_params_set_access(d->pcm, params,
        SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_channels(d->pcm, params, 1);
    snd_pcm_hw_params_set_rate_near(d->pcm, params, &d->sample_rate, 0);

    snd_pcm_hw_params(d->pcm, params);

    snd_pcm_hw_params_get_buffer_size(params, &d->buffer_frames);
    snd_pcm_hw_params_get_format(params, &d->format);

    printf("Using sample rate %u, %u frames, format %s\n",
        d->sample_rate,
        d->buffer_frames,
        snd_pcm_format_description(d->format));

    if (retval) {
        printf("Couldn't set PCM parameters: %s\n", snd_strerror(retval));
        exit(retval);
    }

    snd_pcm_prepare(d->pcm);
    snd_pcm_start(d->pcm);

    d->phase = 0.0;
}

void write_pcm(struct dioxide *d) {
    snd_pcm_sframes_t available;
    double phase, step;
    unsigned i, offset = 0;
    int retval;
    unsigned char ptr[440];

    available = snd_pcm_avail_update(d->pcm);

    if (available < 220 * 2) {
        return;
    }

    available = 220 * 2;

    phase = d->phase;
    step = 2 * M_PI * 220 / d->sample_rate;

    for (i = 0; i < available; i++) {
        ptr[i] = (sin(phase) + 1) * 127;
        phase += step;
        if (phase >= 2 * M_PI) {
            phase -= 2 * M_PI;
        }
    }

    while (available > 0) {
        retval = snd_pcm_writei(d->pcm, ptr + offset, available);

        if (retval < 0) {
            printf("Couldn't writei: %s\n", snd_strerror(retval));
            printf("State: %s\n",
                snd_pcm_state_name(snd_pcm_state(d->pcm)));
            return;
        }

        available -= retval;
        offset += retval;
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

void poll_sequencer(struct dioxide *d) {
    snd_seq_event_t *event;
    enum snd_seq_event_type type;

    if (snd_seq_event_input(d->seq, &event) == -EAGAIN) {
        return;
    }

    type = event->type;

    switch (type) {
        case SND_SEQ_EVENT_NOTEON:
            printf("Noteon %u\n", event->data.note.note);
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            printf("Noteoff %u\n", event->data.note.note);
            break;
        default:
            printf("Got event type %u\n", type);
            break;
    }
}

int main() {
    struct dioxide d = {0};
    int retval;

    setup_pcm(&d);
    setup_sequencer(&d);

    while (1) {
        poll_sequencer(&d);
        write_pcm(&d);
    }

    snd_pcm_close(d.pcm);
    retval = snd_seq_close(d.seq);
    exit(retval);
}
