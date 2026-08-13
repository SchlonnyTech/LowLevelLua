# Plugin API - EXPERIMENTAL

Heavily experimental. Not for production.

## Overview

Plugins are shared libraries (.lllplugin).

## Plugin Structure

lll_plugin_init(PluginAPI *api, PluginInfo *info)

## Locations

./llladdons/
/usr/lib/llladdons/

## Example

int lll_plugin_init(PluginAPI *api, PluginInfo *info) {
    info->name = "My Plugin";
    info->version = "1.0";
    return 0;
}
