# CIS Benchmark Catalogue — Linux

This file records the Linux distributions and benchmark versions selected for the initial CPlusScanner scope.

**Catalogue checked:** 26 November 2026  
**Official CIS source pages:** [Ubuntu Linux](https://www.cisecurity.org/benchmark/ubuntu_linux), [Red Hat Enterprise Linux](https://www.cisecurity.org/benchmark/red_hat_linux), [Debian Linux](https://www.cisecurity.org/benchmark/debian_linux), [Linux Mint](https://www.cisecurity.org/benchmark/linux_mint), [Talos Linux](https://www.cisecurity.org/benchmark/talos_linux).

| Distribution / release | Benchmark version | Official CIS source |
|---|---:|---|
| Ubuntu Linux 26.04 LTS | 1.0.0 | [Ubuntu Linux](https://www.cisecurity.org/benchmark/ubuntu_linux) |
| Red Hat Enterprise Linux 10 | 1.0.1 | [Red Hat Enterprise Linux](https://www.cisecurity.org/benchmark/red_hat_linux) |
| Red Hat Enterprise Linux 9 | 3.0.0 | [Red Hat Enterprise Linux](https://www.cisecurity.org/benchmark/red_hat_linux) |
| Red Hat Enterprise Linux 8 | 4.0.0 | [Red Hat Enterprise Linux](https://www.cisecurity.org/benchmark/red_hat_linux) |
| Debian Linux 13 | 1.1.0 | [Debian Linux](https://www.cisecurity.org/benchmark/debian_linux) |
| Debian Linux 12 | 2.0.0 | [Debian Linux](https://www.cisecurity.org/benchmark/debian_linux) |
| Debian Linux 11 | 2.0.0 | [Debian Linux](https://www.cisecurity.org/benchmark/debian_linux) |
| Linux Mint 22 | 1.0.0 | [Linux Mint](https://www.cisecurity.org/benchmark/linux_mint) |
| Talos Linux | 2.0.0 | [Talos Linux](https://www.cisecurity.org/benchmark/talos_linux) |

## Implementation notes

- Distribution and release detection must precede benchmark selection. Similar package families do not make distributions interchangeable.
- Talos Linux is an immutable Kubernetes-focused OS and will require a different collection strategy from conventional general-purpose Linux distributions. It should not be treated as a standard SSH shell target without validating its supported management and inspection interfaces.
- Linux checks will eventually collect evidence from relevant files, permissions, services, kernel parameters, package state, and runtime configuration. Checks must document whether evidence is static, runtime, or both.
- Profile selection (for example, Level 1 or Level 2) must be explicit for each assessment.
- Record unsupported controls and inaccessible evidence as Manual or Error rather than Pass.

## Copyright and use

This repository stores benchmark metadata and links, not CIS publications. Obtain benchmark documents from CIS and follow the applicable CIS terms and licensing. Do not commit full benchmark PDFs or copy their recommendation text into this repository unless the project has the necessary redistribution rights.
