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

    if (handle == NULL) {
        printf("Couldn't load plugin %s: %s\n", name, dlerror());
        return;
    }

    ladspa_descriptor = dlsym(handle, "ladspa_descriptor");

    if (ladspa_descriptor == NULL) {
        printf("Couldn't describe plugin %s: %s\n", name, dlerror());
        return;
    }

    while (desc = ladspa_descriptor(i)) {
        plugin = calloc(1, sizeof(struct ladspa_plugin));
        plugin->desc = desc;

        /* Stash the plugin. */
        if (d->available_plugins == NULL) {
            d->available_plugins = plugin;
            plugin->prev = plugin->next = plugin;
        } else {
            iter = d->available_plugins->prev;
            iter->next = d->available_plugins->prev = plugin;
            plugin->prev = iter;
            plugin->next = d->available_plugins;
        }

        printf("Loaded plugin %s\n", desc->Name);
        i++;
    }
}

void setup_plugins(struct dioxide *d) {
    open_plugin(d, "sawtooth_1641.so");
}
