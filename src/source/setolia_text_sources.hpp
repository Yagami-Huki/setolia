#pragma once

#include <obs-module.h>

/**
 * Setolia Text Sources
 *
 * Registers text source wrappers for reserve, singing, and setlist.
 * Each wrapper delegates rendering/properties to the native OBS text source.
 */

void register_setolia_text_sources();
