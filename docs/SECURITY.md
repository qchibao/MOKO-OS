# MOKO OS v0.1 security principles

1. Developer Preview must never auto-partition or overwrite disks.
2. MOKO AI runs without root privileges.
3. System-changing AI actions go through a permission broker.
4. Network/API credentials must be stored outside source control.
5. Logs must redact tokens, passwords and user content where possible.
6. The Live ISO uses a development autologin account only during the bootstrap milestone; production builds must use a real greeter/authentication flow.
7. Secure Boot signing is a later milestone; do not present unsigned Developer Preview images as Secure Boot-ready.
8. Every installer milestone requires explicit rollback and recovery testing.
