# CPlusScanner

CPlusScanner is a C++-based security configuration assessment tool intended to evaluate virtual machines against selected Center for Internet Security (CIS) Benchmarks. Its initial design is read-only, evidence-driven, and supports remote operation over SSH.

## Architecture

1. **Controller:** runs on the operator's host, invokes the local OpenSSH client, authenticates to a VM, starts the agent, and validates its structured response.
2. **Agent:** a short-lived C++ process on the target VM. It exposes a narrow, fixed command interface and performs local inspection in later phases.

SSH provides encrypted transport, public-key authentication, and host identity verification. The agent is not a network listener and does not accept arbitrary commands from the controller.

## Current implementation: Part 1 — Connection

The initial C++17 implementation has source support for Windows, Linux, and macOS:

- `agent/main.cpp` — `--ping` and `--system-info` operations; emits one JSON response.
- `controller/main.cpp` — cross-platform OpenSSH process launch and strict host-key-verified ping handshake.
- `include/protocol.hpp` — shared protocol constants and JSON escaping.
- `CMakeLists.txt` — CMake build for both executables.
- [Part 1 connection and setup guide](CONNECTION.md).

**No CIS assessment is implemented yet.** The controller checks connectivity and agent/protocol compatibility only. The agent performs no privileged operations and makes no configuration changes.

## Build

Requirements: C++17 compiler, CMake 3.16+, and OpenSSH client on the controller host. The target VM needs an OpenSSH server.

```sh
cmake -S . -B build
cmake --build build --config Release
```

Build the agent for the target VM's OS and architecture, then install it in the SSH account's `PATH`. Build and run the controller on the operator's host. See [CONNECTION.md](CONNECTION.md) for setup and usage.

## Security principles

- **Read-only first:** no automatic hardening or remediation.
- **Least privilege:** use a dedicated account and grant only permissions required by future checks.
- **Strong SSH identity:** use public-key authentication and verify host keys; do not silently trust unknown hosts.
- **Narrow interface:** expose fixed, versioned agent operations rather than arbitrary remote shell execution.
- **Evidence-driven outcomes:** missing evidence must never be treated as a pass.
- **Benchmark provenance:** every future rule must reference its benchmark version, recommendation identifier, and applicable profile.

## Assessment result model (future phase)

| Status | Meaning |
|---|---|
| Pass | Collected evidence satisfies the assessment condition. |
| Fail | Collected evidence does not satisfy the assessment condition. |
| Manual | Human validation is required or automation is unsuitable. |
| Not Applicable | The recommendation does not apply to the target/profile. |
| Error | The check could not be evaluated, for example due to missing permissions or collection failure. |

## Benchmark catalogue

Benchmark metadata and official CIS references are maintained under [CIS standards](CIS%20standards/README.md). The catalogue is not a copy of CIS Benchmark publications. Future benchmark-derived rules must comply with applicable CIS licensing and attribution requirements.

## Roadmap

- **Part 0 — Scope and benchmark catalogue:** documented.
- **Part 1 — Host-to-VM connection:** initial controller, agent, and protocol implemented; native Windows validation remains outstanding.
- **Part 2 — System inventory:** controller requests and validates OS/version/architecture details from the agent.
- **Part 3 — Assessment framework:** define versioned machine-readable rules and evidence schema from appropriately licensed CIS material.
- **Part 4 — Platform collectors:** implement and test read-only checks against lab VMs.
- **Part 5 — Reporting:** produce auditable results and coverage summaries.

## Repository layout

```text
agent/                 Endpoint agent source
controller/            Host controller source
include/               Shared protocol definitions
CIS standards/         Benchmark metadata and official references
CMakeLists.txt         Cross-platform build configuration
CONNECTION.md          Part 1 setup and protocol guide
```
