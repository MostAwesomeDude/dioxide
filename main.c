#include <stdlib.h>

#include <asoundlib.h>

struct dioxide {
    snd_seq_t *seq;
};

void setup_sequencer(struct dioxide *d) {
    int retval;

    retval = snd_seq_open(&d->seq, "default", SND_SEQ_OPEN_INPUT, 0);

    if (retval) {
        printf("Couldn't open sequencer: %d\n", retval);
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

    snd_seq_event_input(d->seq, &event);
    type = event->type;

    switch (type) {
        case SND_SEQ_EVENT_NOTEON:
            printf("Noteon\n");
            break;
        case SND_SEQ_EVENT_NOTEOFF:
            printf("Noteoff\n");
            break;
        default:
            printf("Got event type %u\n", type);
            break;
    }
}

int main() {
    struct dioxide d = {0};
    int retval;

    setup_sequencer(&d);

    while (1) {
        poll_sequencer(&d);
    }

    retval = snd_seq_close(d.seq);
    exit(retval);
}
