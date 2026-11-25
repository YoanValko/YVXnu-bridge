# Security policy

YVXnu-bridge contains experimental kernel-module and host-device code. Treat
it as untrusted research software and test it only in a disposable VM or on a
machine with a tested recovery path.

## Reporting a vulnerability

Please do not post secrets, exploit details, or working proof-of-concept code
in a public issue. Use GitHub's private vulnerability reporting for this
repository when available. If it is unavailable, contact the maintainer
through GitHub before opening a public issue and include the affected commit,
host kernel, and a minimal description.

Reports about crashes or data loss should include whether any kernel module was
loaded and whether the test ran inside a VM.
