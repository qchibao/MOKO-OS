# MOKO OS v0.1 security principles

1. Developer Preview must never auto-partition or overwrite disks.
2. MOKO AI runs without root privileges.
3. System-changing AI actions go through a permission broker.
4. Network/API credentials must be stored outside source control.
5. Logs must redact tokens, passwords and user content where possible.
6. The Live ISO uses a development autologin account only during the bootstrap milestone; production builds must use a real greeter/authentication flow.
7. Secure Boot signing is a later milestone; do not present unsigned Developer Preview images as Secure Boot-ready.
8. Every installer milestone requires explicit rollback and recovery testing.
9. AI providers return intents only. `moko-ai-actions` accepts a fixed capability
   set and never exposes arbitrary shell execution.
10. AI application launches use an explicit app-id allowlist; file actions accept
    only canonical existing paths inside the current user's home directory.
11. Live boot profiles are fixed enum values. Boot menu input cannot become a
    shell command, executable path or arbitrary environment value.
12. The Developer Preview omits the installer and `udisks2`, masks the udisks2
    service defensively, and runs a block-mount audit before greetd. A failed
    audit prevents greetd from starting and prevents the graphical health gate
    from passing.
13. Release QEMU tests attach the ISO as read-only optical media and do not
    create or attach a writable virtual disk.
