#pragma once

#include <QDBusObjectPath>
#include <QMap>
#include <QVariantMap>

namespace MokoSystemControl {

using DbusInterfaceMap = QMap<QString, QVariantMap>;
using DbusManagedObjects = QMap<QDBusObjectPath, DbusInterfaceMap>;

} // namespace MokoSystemControl

Q_DECLARE_METATYPE(MokoSystemControl::DbusInterfaceMap)
Q_DECLARE_METATYPE(MokoSystemControl::DbusManagedObjects)
