#pragma once

#include <obs-module.h>

/**
 * Setolia Text Sources
 *
 * Registers text source wrappers for reserve, singing, and setlist.
 * Each wrapper delegates rendering/properties to the native OBS text source.
 */

void register_setolia_text_sources();

// True if unversionedId (obs_source_get_unversioned_id) is one of the SETOLIA text source types, of any version.
bool is_setolia_text_source_id(const char *unversionedId);
