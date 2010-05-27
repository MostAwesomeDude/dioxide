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

void select_plugin(struct dioxide *d, unsigned id) {
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
}

void setup_plugins(struct dioxide *d) {
    struct ladspa_plugin *plugin;
    unsigned i = 1;

    open_plugin(d, "sawtooth_1641.so");
    open_plugin(d, "lp4pole_1671.so");

    /* Sawtooth generator */
    select_plugin(d, 1642);
    plugin = find_plugin_by_id(d->plugin_chain, 1642);
    plugin->output = 1;

    /* LPF */
    select_plugin(d, 1672);
    plugin = find_plugin_by_id(d->plugin_chain, 1672);
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

    /* Sawtooth generator */
    plugin = find_plugin_by_id(d->plugin_chain, 1642);

    if (!plugin) {
        printf("Couldn't set up sawtooth!\n");
    } else {
        plugin->desc->connect_port(plugin->handle, 0, &d->pitch);
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
