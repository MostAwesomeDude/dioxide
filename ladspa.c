#include <dlfcn.h>
#include <stdio.h>

#include "dioxide.h"

void open_plugin(struct dioxide *d, const char *name) {
    void* handle;
    LADSPA_Descriptor_Function ladspa_descriptor;
    const LADSPA_Descriptor *desc;
    struct ladspa_plugin *plugin, *iter;
    unsigned i = 0;

    handle = dlopen(name, RTLD_NOW | RTLD_LOCAL);

    if (!handle) {
        printf("Couldn't load plugin %s: %s\n", name, dlerror());
        return;
    }

    ladspa_descriptor = dlsym(handle, "ladspa_descriptor");

    if (!ladspa_descriptor) {
        printf("Couldn't describe plugin %s: %s\n", name, dlerror());
        return;
    }

    while (desc = ladspa_descriptor(i)) {
        plugin = calloc(1, sizeof(struct ladspa_plugin));
        plugin->desc = desc;

        /* Stash the plugin. */
        if (!d->available_plugins) {
            d->available_plugins = plugin;
        } else {
            iter = d->available_plugins;
            while (iter->next) {
                iter = iter->next;
            }
            iter->next = plugin;
        }

        printf("Loaded plugin %s\n", desc->Name);
        i++;
    }

    plugin->dl_handle = handle;
}

struct ladspa_plugin* select_plugin(struct dioxide *d, unsigned id) {
    struct ladspa_plugin *plugin, *iter;

    iter = d->available_plugins;
    while (iter) {
        if (iter->desc->UniqueID == id) {
            break;
        }
        iter = iter->next;
    }
    if (!iter) {
        printf("Couldn't select plugin %d\n", id);
        return;
    }

    plugin = calloc(1, sizeof(struct ladspa_plugin));
    plugin->desc = iter->desc;

    plugin->handle = plugin->desc->instantiate(plugin->desc, d->spec.freq);
    if (!plugin->handle) {
        printf("Failed to instantiate plugin %d\n", id);
        free(plugin);
        return;
    }

    if (plugin->desc->activate) {
        plugin->desc->activate(plugin->handle);
    }

    /* Stash the plugin. */
    if (!d->plugin_chain) {
        d->plugin_chain = plugin;
    } else {
        iter = d->plugin_chain;
        while (iter && iter->next) {
            iter = iter->next;
        }
        iter->next = plugin;
    }

    return plugin;
}

void setup_plugins(struct dioxide *d) {
    struct ladspa_plugin *plugin;
    unsigned i = 1;

    open_plugin(d, "caps.so");
    open_plugin(d, "sawtooth_1641.so");
    open_plugin(d, "lp4pole_1671.so");

    /* Sawtooth generator */
    plugin = select_plugin(d, 1641);
    plugin->input = 0;
    plugin->output = 1;

    /* Chorus */
    plugin = select_plugin(d, 2583);
    plugin->input = 0;
    plugin->output = 7;

    /* Phaser */
    plugin = select_plugin(d, 2586);
    plugin->input = 0;
    plugin->output = 5;

    /* LPF */
    plugin = select_plugin(d, 1672);
    plugin->input = 2;
    plugin->output = 3;

    printf("Prepared plugin chain\n");
    plugin = d->plugin_chain;
    while (plugin) {
        printf("%d: %s (%p)\n", i, plugin->desc->Name, plugin->handle);
        plugin = plugin->next;
        i++;
    }
}

struct ladspa_plugin* find_plugin_by_id(struct ladspa_plugin *plugin,
                                        unsigned id) {
    struct ladspa_plugin *iter = plugin;

    while (iter) {
        if (iter->desc->UniqueID == id) {
            return iter;
        }
        iter = iter->next;
    }

    return NULL;
}

void hook_plugins(struct dioxide *d) {
    struct ladspa_plugin *plugin;

    /* Phaser */
    plugin = find_plugin_by_id(d->plugin_chain, 2586);

    if (!plugin) {
        printf("Couldn't set up phaser!\n");
    } else {
        plugin->desc->connect_port(plugin->handle, 1, &d->phaser_rate);
        plugin->desc->connect_port(plugin->handle, 2, &d->phaser_depth);
        plugin->desc->connect_port(plugin->handle, 3, &d->phaser_spread);
        plugin->desc->connect_port(plugin->handle, 4, &d->phaser_feedback);
    }

    /* Chorus */
    plugin = find_plugin_by_id(d->plugin_chain, 2583);

    if (!plugin) {
        printf("Couldn't set up chorus!\n");
    } else {
        plugin->desc->connect_port(plugin->handle, 1, &d->chorus_delay);

        static float chorus_width = 7;
        static float chorus_feedforward = 0.5;
        static float chorus_feedback = 0.4;
        plugin->desc->connect_port(plugin->handle, 2, &chorus_width);
        plugin->desc->connect_port(plugin->handle, 5, &chorus_feedforward);
        plugin->desc->connect_port(plugin->handle, 6, &chorus_feedback);
    }


    /* LPF */
    plugin = find_plugin_by_id(d->plugin_chain, 1672);

    if (!plugin) {
        printf("Couldn't set up low-pass filter!\n");
    } else {
        plugin->desc->connect_port(plugin->handle, 0, &d->lpf_cutoff);
        plugin->desc->connect_port(plugin->handle, 1, &d->lpf_resonance);
    }
}

void cleanup_plugins(struct dioxide *d) {
    struct ladspa_plugin *plugin, *doomed;

    plugin = d->plugin_chain;
    while (plugin) {
        doomed = plugin;
        if (plugin->desc->deactivate) {
            plugin->desc->deactivate(plugin->handle);
        }

        plugin = plugin->next;
        free(doomed);
    }

    plugin = d->available_plugins;
    while (plugin) {
        doomed = plugin;
        if (plugin->dl_handle) {
            dlclose(plugin->dl_handle);
        }

        plugin = plugin->next;
        free(doomed);
    }
}
