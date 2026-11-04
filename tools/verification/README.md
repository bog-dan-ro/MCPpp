# Development-only verification tools

These dependencies are never linked to or included by the C++ library.

- `requirements.txt` pins the Python Draft 2020-12 validation stack.
- `package.json` and `package-lock.json` pin official Inspector 2.13.0 and
  conformance 0.2.0-alpha.12, including transitive npm integrity hashes.
- `conformance-scope.json` documents seven individual upstream checks requiring
  unimplemented APIs or diagnostic fixtures. Original failing reports are
  retained. It is not a declaration of full conformance.

Install explicitly and enable CMake's opt-in checks as shown in
[`docs/validation.md`](../../docs/validation.md). Standard configuration/build
does not fetch packages. `node_modules` and Python environments are untracked.
