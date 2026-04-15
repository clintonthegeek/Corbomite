// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "corbomite/models/PropertyType.h"

#include <markoff-parser/YamlValue.h>

class QJsonValue;

namespace Corbomite {

/// Inference rule (matches Obsidian's PropertyInfo.widget contract):
/// - bool → Checkbox
/// - int / double → Number
/// - string strictly parseable as ISO date (no time component) → Date
/// - string strictly parseable as ISO datetime (with time component) → DateTime
/// - YAML sequence → List
/// - else (including null, unrecognised) → Text
PropertyType inferPropertyType(const Markoff::YamlValue &value);

/// Convert a QJsonValue (as stored in CachedMetadata.frontmatter) into a
/// YamlValue equivalent — recursive for arrays / objects. Null, bools,
/// numbers, strings, arrays, objects are all handled. Used to feed
/// MetadataCache's JSON-shaped frontmatter into the type-inference +
/// editor pipeline.
Markoff::YamlValue qJsonValueToYaml(const QJsonValue &v);

}  // namespace Corbomite
