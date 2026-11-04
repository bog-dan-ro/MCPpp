# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

"""Run pinned official scenarios; preserve all failures and enforce our scope."""
import argparse
import json
import pathlib
import socket
import subprocess
from example_client import ExampleClient


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("runner")
    parser.add_argument("reports", type=pathlib.Path)
    args = parser.parse_args()
    scope = json.loads((pathlib.Path(__file__).parent.parent / "tools/verification/conformance-scope.json").read_text())
    # Explicitly allow this test client's same-origin URL. The production
    # default still rejects every Origin not on its configured allowlist.
    with socket.socket() as reservation:
        reservation.bind(("127.0.0.1", 0))
        port = reservation.getsockname()[1]
    origin = f"http://127.0.0.1:{port}"
    args.reports.mkdir(parents=True, exist_ok=True)
    summary = {"tool": "@modelcontextprotocol/conformance@0.2.0-alpha.12", "protocol": "2026-07-28", "scenarios": []}
    unexpected = []
    with ExampleClient(args.server, True, extra_args=["--port", str(port), "--origin", origin]) as client:
        for scenario in ["server-stateless", "tools-list", "dns-rebinding-protection", "caching", "http-header-validation"]:
            directory = args.reports / scenario
            # A fresh subdirectory prevents a previous successful report from
            # concealing a current tool startup failure.
            import tempfile
            directory.mkdir(exist_ok=True)
            run_dir = pathlib.Path(tempfile.mkdtemp(prefix="run-", dir=directory))
            run = subprocess.run([args.runner, "server", "--url", origin + "/mcp", "--scenario", scenario,
                                  "--spec-version", "2026-07-28", "--timeout", "5000", "--output-dir", str(run_dir)],
                                 capture_output=True, text=True, timeout=40)
            (run_dir / "stdout.txt").write_text(run.stdout)
            (run_dir / "stderr.txt").write_text(run.stderr)
            reports = list(run_dir.rglob("checks.json"))
            assert len(reports) == 1, run.stdout + run.stderr
            checks = json.loads(reports[0].read_text())
            outcome = {"scenario": scenario, "officialExitCode": run.returncode,
                       "passed": 0, "unsupported": [], "unexpected": []}
            for check in checks:
                if check["status"] == "SUCCESS":
                    outcome["passed"] += 1
                elif check["status"] == "FAILURE":
                    key = scenario + ":" + check["id"]
                    expected = key in scope
                    if scenario == "server-stateless":
                        expected = expected and check.get("details", {}).get("untestable") is True
                    if scenario == "caching":
                        expected = expected and check.get("details", {}).get("error", {}).get("code") == -32601
                    issue = {"id": check["id"], "message": check.get("errorMessage"), "reason": scope.get(key)}
                    outcome["unsupported" if expected else "unexpected"].append(issue)
                    if not expected:
                        unexpected.append(issue)
            assert outcome["passed"] > 0, "Scenario did not exercise any checks"
            summary["scenarios"].append(outcome)
            print(scenario, f"{outcome['passed']} passed, {len(outcome['unsupported'])} outside scope, {len(outcome['unexpected'])} unexpected")
    (args.reports / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    assert not unexpected, unexpected


if __name__ == "__main__":
    main()
