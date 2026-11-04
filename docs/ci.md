# Continuous integration

`.github/workflows/ci.yml` runs on pushes, pull requests, and manual dispatch.
It builds `tools/ci/Dockerfile`, then runs the same CMake driver used locally.
The Docker base image is pinned by digest, GCC by its Debian package version,
and verification tools by the Python/npm lock files. Other system packages
come from Debian testing, including the Boost 1.92 packages required by CMake.
The image is not an archival Debian snapshot.

Four profiles run through `tools/ci/run.cmake`:

| Profile | Coverage |
| --- | --- |
| `verified` | Both JSON adapters, TLS, all protocol schema checks, real Inspector clients and scoped official conformance |
| `sanitizers` | Boost.JSON with ASan, UBSan, and LeakSanitizer enabled |
| `http-only` | Beast without OpenSSL/TLS |
| `core-only` | Reflection and core consumers with Boost/OpenSSL discovery disabled |

In the recorded local run, the profiles passed 32, 18, 13 and 4 CTest entries
respectively. The workflow also passed `actionlint` 1.7.12.

Every profile runs the installed-package and FetchContent consumer checks.
Both public targets must be interface libraries. Installed consumers build
from a moved prefix, and multi-file Beast consumers exercise header-only
linkage.

Run the full workflow workload locally:

```sh
docker build -t mcpp-ci -f tools/ci/Dockerfile .
docker run --rm --init --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" mcpp-ci
```

Run one profile while developing:

```sh
docker run --rm --init --user "$(id -u):$(id -g)" \
  -v "$PWD:/workspace" mcpp-ci \
  cmake -DMCPP_CI_PROFILES=verified -P tools/ci/run.cmake
```

Builds and reports stay in ignored `build-ci-*` directories. GitHub Actions
uploads CTest, Inspector, and original conformance reports even when a check
fails. Action revisions are pinned by commit SHA and the job has read-only
repository permissions. The seven documented conformance exclusions remain
visible in the reports.
