# CPlusScanner

CPlusScanner is a planned C++-based security configuration assessment tool that evaluates virtual machines against selected Center for Internet Security (CIS) Benchmarks.

The initial objective is to build a reliable, auditable, read-only assessment workflow for Windows and Linux virtual machines. The project will begin with the design and benchmark catalogue before implementation begins.

## Project goals

- Assess the security configuration of supported virtual machines against explicitly versioned CIS Benchmarks.
- Support remote assessment over SSH so an operator can initiate scans from another system on the same network.
- Use a lightweight endpoint agent to collect local configuration evidence on each target VM.
- Evaluate collected evidence against machine-readable assessment rules derived from licensed CIS Benchmark material.
- Produce consistent results that explain what was checked, the observed value, the expected condition, and the assessment outcome.
- Keep assessment activity read-only by default. Remediation or configuration changes are outside the initial scope.

## Intended operating model

The intended design has two principal components:

1. **Central controller** — accepts target inventory and scan requests, establishes authenticated SSH connections, coordinates agent execution, collects results, and produces reports.
2. **Endpoint agent** — runs on the target VM, identifies the operating system and version, collects only the configuration data required by an approved assessment, evaluates or packages evidence for evaluation, and returns structured results.

SSH is the transport and orchestration channel. The endpoint agent performs local inspection so that platform-specific configuration checks can be implemented in a controlled and testable way. The first implementation will use a restricted agent command interface rather than accepting arbitrary commands from the controller.

## Security and trust principles

- **Read-only first:** no automatic hardening, remediation, or configuration writes during assessment.
- **Least privilege:** use a dedicated account and grant only the permissions needed for the selected checks.
- **Strong SSH identity:** use key-based authentication and verify host keys; do not silently trust unknown hosts.
- **Narrow interface:** expose a small set of versioned agent operations rather than unrestricted remote shell access.
- **Evidence-driven outcomes:** retain the evidence and reasoning behind each result.
- **Auditable execution:** record target identity, OS/version, benchmark/version, scan time, agent version, and result status.
- **Fail safely:** report unavailable permissions, unsupported platforms, and collection errors as such; do not interpret missing evidence as a pass.
- **Benchmark provenance:** every assessment rule must reference its source benchmark, version, recommendation identifier, and applicable profile.

## Assessment result model

The planned result statuses are:

| Status | Meaning |
|---|---|
| Pass | Collected evidence satisfies the assessment condition. |
| Fail | Collected evidence does not satisfy the assessment condition. |
| Manual | The recommendation requires human validation or cannot be reliably automated. |
| Not Applicable | The recommendation does not apply to the assessed system or selected profile. |
| Error | The check could not be evaluated, for example due to missing permissions or collection failure. |

A report should distinguish a genuine failure from a check that could not be completed.

## Benchmark scope

The initial benchmark catalogue is maintained under [CIS standards](CIS%20standards/README.md). It records the platforms and benchmark versions currently in scope, links to the official CIS source pages, and documents provenance.

The catalogue is a version reference, not a copy of the CIS Benchmark publications. CIS Benchmark documents and detailed recommendation content remain the property of CIS and are subject to CIS terms and licensing. The project will not commit benchmark PDFs or reproduce full recommendation text without the required rights. Assessment rules will be authored from material that the project is licensed or otherwise permitted to use, with attribution and version tracking.

## Current project phase

**Phase 0 — Scope and benchmark inventory**

- Establish project purpose, scope, and design principles.
- Catalogue Windows Desktop and Linux benchmark versions.
- Identify the official CIS source for each benchmark.
- Define the result model and evidence expectations.

**Later phases — To be designed**

- Define the machine-readable rule schema and benchmark profile model.
- Design the agent/controller protocol and SSH trust model.
- Implement platform identification and a minimal read-only collector.
- Validate checks against test VMs and known-good/known-bad configurations.
- Add structured reports and broader platform coverage.

No implementation source tree is being created at this stage.

## Project status

This repository currently contains project documentation and the benchmark catalogue only. Implementation choices, supported profiles, and automation coverage will be documented as they are agreed and tested.
