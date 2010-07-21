#include "dioxide.h"


void generate_uranium(struct dioxide *d, float *buffer, unsigned size)
{
    struct ladspa_plugin *plugin = d->plugin_chain;

    plugin = d->plugin_chain;

    plugin->desc->connect_port(plugin->handle, plugin->output, buffer);
    plugin->desc->run(plugin->handle, size);
}
