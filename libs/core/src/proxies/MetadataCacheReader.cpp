// SPDX-License-Identifier: GPL-3.0-or-later
#include "corbomite/core/proxies/MetadataCacheReader.h"

namespace Corbomite {

QStringList MetadataCacheReader::backlinksFor(const QString &) const { return {}; }
QStringList MetadataCacheReader::outlinksFor(const QString &) const { return {}; }
QStringList MetadataCacheReader::tagsIn(const QString &) const { return {}; }
QStringList MetadataCacheReader::allTags() const { return {}; }

} // namespace Corbomite
