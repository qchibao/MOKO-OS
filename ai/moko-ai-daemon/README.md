# MOKO AI daemon
`moko-ai-daemon` is the unprivileged, provider-agnostic AI orchestration
service. It owns `org.moko.AI1` on the user session bus and delegates only
named capabilities to `moko-ai-actions`. No API keys are stored in this
repository or the live image.
