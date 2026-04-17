// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace Corbomite {

/// Host's plugin API level. Plugins declare X-Corbomite-ApiLevel <= this
/// to load. When we make a hard ABI break, bump this integer; compat
/// shims for level N-1 stay in place for one major Corbomite version.
///
/// A plugin that omits X-Corbomite-ApiLevel is treated as declaring level
/// 1 (today's API). A plugin declaring a level higher than this constant
/// is refused at PluginManager::enablePlugin() time and surfaces in
/// PluginsPage as "Requires plugin API level >= N" with its enable
/// button disabled.
inline constexpr int CORBOMITE_PLUGIN_API_LEVEL = 1;

} // namespace Corbomite
