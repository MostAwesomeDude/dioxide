#include <stdlib.h>

#include <asoundlib.h>

int main() {
    snd_seq_t *seq;
    snd_seq_event_t *event;
    int retval;

    retval = snd_seq_open(&seq, "default", SND_SEQ_OPEN_INPUT, 0);

    if (retval) {
        printf("Couldn't open sequencer: %d\n", retval);
        exit(retval);
    }

    snd_seq_set_client_name(seq, "Dioxide");

    snd_seq_create_simple_port(seq, "Dioxide",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    while (1) {
        snd_seq_event_input(seq, &event);
    }

    retval = snd_seq_close(seq);
    exit(retval);
}
