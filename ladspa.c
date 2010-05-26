#include <dlfcn.h>
#include <stdio.h>

#include "dioxide.h"

void open_plugin(struct dioxide *d, const char *name) {
    void* handle;
    LADSPA_Descriptor_Function ladspa_descriptor;
    const LADSPA_Descriptor *desc;
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

    if (d->plugin_list == NULL) {
        d->plugin_list = calloc(16, sizeof(LADSPA_Descriptor*));
        d->plugin_count = 0;
    }

    while (desc = ladspa_descriptor(i)) {
        printf("Loaded plugin %s\n", desc->Name);
        d->plugin_list[d->plugin_count] = desc;
        d->plugin_count++;
        i++;
    }
}

void setup_plugins(struct dioxide *d) {
    open_plugin(d, "sawtooth_1641.so");
}
