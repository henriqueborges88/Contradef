# Security policy

## Reporting a vulnerability

Use [GitHub private vulnerability reporting](https://github.com/henriqueborges88/Contradef/security/advisories/new) to report security issues privately. You can also find **Report a vulnerability** under the repository's **Security** tab.

Include the affected commit or version, Windows and Intel Pin versions, enabled modules, expected behavior, and a minimal reproduction using a benign executable whenever possible. Memory errors, crashes, deadlocks, and unsafe handling of target-controlled data are relevant reports.

Do not disclose an unpatched vulnerability in a public issue or pull request. Do not attach credentials, private traces, or live malware samples. Coordinate any public disclosure through the private report.

## Maintenance

Security fixes are developed against `main`. Include the exact commit you tested when reporting a problem.

Automated checks exercise selected production safety routines with a simulated Pin interface. They do not establish that a DLL builds correctly or that instrumentation is safe for every target. Runtime validation requires the actual supported Windows and Intel Pin environment.
