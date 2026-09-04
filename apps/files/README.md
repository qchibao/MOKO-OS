# MOKO Files

Native Qt 6/QML file manager backed by `FileModel`. File operations use Qt
filesystem APIs, never shell command construction. Deletion requires an
explicit one-time confirmation token and the application refuses normal use as
root.
