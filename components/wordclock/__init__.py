"""Word Clock helper component.

Header-only: ships the C++ used by the effect lambdas in wordclock.yaml
(grid mapping, word clock engine, animations).  Declaring `wordclock:` in the
YAML makes ESPHome copy this folder into the build and include the headers, so
nothing has to be copied into your esphome config directory.
"""
import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@Wob76"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/wordclock/wc_effects.h"'))
