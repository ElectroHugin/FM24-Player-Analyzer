#include "Version.h"

namespace fm {

QString appVersion()
{
    return QStringLiteral("1.0.0");
}

QString appName()
{
    // Version-agnostic display name (the app supports multiple FM releases via
    // the import version selector). The internal data-folder identifier stays
    // "FM24PlayerAnalyzer" for backward compatibility — see AppPaths.
    return QStringLiteral("FM Player Analyzer");
}

} // namespace fm
