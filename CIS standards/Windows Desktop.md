# CIS Benchmark Catalogue — Windows Desktop

This file records the Windows Desktop benchmark versions selected for the initial CPlusScanner scope.

**Catalogue checked:** 26 November 2026  
**Official CIS source:** [Microsoft Windows Desktop](https://www.cisecurity.org/benchmark/microsoft_windows_desktop) and [CIS Benchmarks catalogue](https://www.cisecurity.org/cis-benchmarks).

| Platform | Benchmark version | Source |
|---|---:|---|
| Windows 11 Enterprise | 5.1.1 | [CIS Microsoft Windows Desktop](https://www.cisecurity.org/benchmark/microsoft_windows_desktop) |
| Windows 11 Stand-alone | 5.0.1 | [CIS Benchmarks catalogue](https://www.cisecurity.org/cis-benchmarks) |
| Windows 10 Stand-alone | 5.0.0 | [CIS Microsoft Windows Desktop](https://www.cisecurity.org/benchmark/microsoft_windows_desktop) |
| Windows 10 Enterprise | 4.0.0 | [CIS Microsoft Windows Desktop](https://www.cisecurity.org/benchmark/microsoft_windows_desktop) |

## Implementation notes

- Treat Enterprise and Stand-alone as distinct benchmark variants; do not assume their recommendation sets are interchangeable.
- Preserve the benchmark version and recommendation identifier with every rule.
- Profile selection (for example, Level 1 or Level 2) must be explicit for each assessment.
- Windows checks will eventually need a platform-specific collection layer, such as registry, local security policy, services, audit policy, and PowerShell-backed evidence collection. The implementation must record the actual evidence source and avoid changing target configuration.

## Copyright and use

This repository stores benchmark metadata and links, not CIS publications. Obtain benchmark documents from CIS and follow the applicable CIS terms and licensing. Do not commit full benchmark PDFs or copy their recommendation text into this repository unless the project has the necessary redistribution rights.
