#include "dioxide.h"

void setup_sequencer(struct dioxide *d) {
    int retval;

    retval = snd_seq_open(&d->seq, "default",
        SND_SEQ_OPEN_INPUT, SND_SEQ_NONBLOCK);

    if (retval) {
        printf("Couldn't open sequencer: %s\n", snd_strerror(retval));
        exit(retval);
    }

    snd_seq_set_client_name(d->seq, "Dioxide");

    retval = snd_seq_create_simple_port(d->seq, "Dioxide",
        SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC);

    if (retval < 0) {
        printf("Couldn't open port: %s\n", snd_strerror(retval));
        exit(retval);
    } else {
        d->seq_port = retval;
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

                d->adsr_phase = ADSR_ATTACK;
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

            if (!d->note_count) {
                d->adsr_phase = ADSR_RELEASE;
            }

            break;
        case SND_SEQ_EVENT_CONTROLLER:
            handle_controller(d, event->data.control);
            break;
        case SND_SEQ_EVENT_PGMCHANGE:
            handle_program_change(d, event->data.control);
            break;
        case SND_SEQ_EVENT_PITCHBEND:
            d->pitch_bend = event->data.control.value;
            break;
        case SND_SEQ_EVENT_PORT_SUBSCRIBED:
        case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
            break;
        default:
            printf("Got event type %u\n", type);
            break;
    }

    if (d->note_count || d->adsr_volume) {
        SDL_PauseAudio(0);
    } else {
        SDL_PauseAudio(1);
    }
}

void solicit_connections(struct dioxide *d) {
    snd_seq_client_info_t *client_info;
    snd_seq_port_info_t *port_info;
    int caps, retval;

    snd_seq_client_info_alloca(&client_info);
    snd_seq_port_info_alloca(&port_info);

    snd_seq_client_info_set_client(client_info, -1);

    while (!snd_seq_query_next_client(d->seq, client_info)) {
        if (!strstr(
            snd_seq_client_info_get_name(client_info), "Oxygen 61")) {
            continue;
        }

        snd_seq_port_info_set_client(port_info,
            snd_seq_client_info_get_client(client_info));
        snd_seq_port_info_set_port(port_info, -1);

        while (!snd_seq_query_next_port(d->seq, port_info)) {
            caps = snd_seq_port_info_get_capability(port_info);

            if (!(caps & SND_SEQ_PORT_CAP_SUBS_READ)) {
                continue;
            }

            retval = snd_seq_connect_from(d->seq, d->seq_port,
                snd_seq_client_info_get_client(client_info),
                snd_seq_port_info_get_port(port_info));

            if (retval) {
                printf("Failed to solicit connection: %s\n",
                    snd_strerror(retval));
            } else {
                printf("Successfully connected a device: %s::%s\n",
                    snd_seq_client_info_get_name(client_info),
                    snd_seq_port_info_get_name(port_info));
                d->connected++;
                return;
            }
        }
    }
}
