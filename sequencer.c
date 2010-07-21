#include <math.h>

#include "dioxide.h"

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
        /* C14 */
        case 10:
            break;
        /* C15 */
        case 77:
            break;
        /* C16 */
        case 78:
            break;
        /* C34 */
        case 1:
            d->volume = scale_pot_float(control.value, 0.0, 1.0);
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
            break;
        /* C19 */
        case 1:
            d->legato = !d->legato;
            break;
        /* C20 */
        case 2:
            d->pitch_wheel_config = ++d->pitch_wheel_config % WHEEL_MAX;
            break;
        /* C21 */
        case 3:
            break;
        default:
            printf("Program change %d\n", control.value);
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

                if (!d->note_count || !d->legato) {
                    d->adsr_phase = ADSR_ATTACK;
                    d->adsr_volume = 0.0;
                }

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

            if (!d->note_count) {
                d->adsr_phase = ADSR_RELEASE;
            } else if (!d->legato) {
                d->adsr_phase = ADSR_ATTACK;
                d->adsr_volume = 0.0;
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
            break;
        case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
            d->connected = 0;
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
