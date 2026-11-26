# CIS standards

This folder tracks the CIS Benchmark catalogue in scope for CPlusScanner. It is a metadata and provenance index; it does not contain copies of CIS Benchmark publications.

## Catalogue

- [Windows Desktop](Windows%20Desktop.md)
- [Linux](Linux.md)

## How the catalogue is used

The catalogue is the starting point for deciding which benchmark/profile combination an assessment should target. Before implementation, each supported recommendation will need a rule definition with at least:

- Benchmark platform, release, and version
- Recommendation identifier and title/reference
- Applicable profile and level
- Assessment method and required evidence
- Expected condition and evaluation logic
- Automation status (automated, partial, or manual)
- Source/provenance and rule review date

A version number alone is not enough to establish that a check is implemented correctly. Rule coverage and test evidence will be tracked separately once licensed benchmark source material and rule authoring begin.

## Copyright and licensing

CIS Benchmark documents are copyrighted and governed by CIS terms and licensing. This repository therefore contains only benchmark names, version metadata, project-authored notes, and links to official CIS pages. It does not redistribute benchmark PDFs or reproduce the detailed recommendation content. Any future use of benchmark-derived material must comply with the applicable CIS licence and attribution requirements.

## Official CIS resources

- [CIS Benchmarks](https://www.cisecurity.org/cis-benchmarks)
- [CIS Benchmarks resources and downloads](https://resources.cisecurity.org/benchmarks)
- [CIS WorkBench terms of use](https://workbench.cisecurity.org/terms-of-use)

**Catalogue checked:** 26 November 2026. Benchmark versions can change; verify against the official CIS page before adding or updating implementation rules.
