# MOKO system actions
`moko-ai-actions` is the v0.1 capability boundary. It allows read-only system
status, bounded home-file search, canonical home-file opening and explicitly
allowlisted MOKO application ids. Model output is never executed as a shell
command and the action layer does not run as root.
