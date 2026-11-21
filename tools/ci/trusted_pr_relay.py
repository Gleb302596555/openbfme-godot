#!/usr/bin/env python3
"""trusted_pr_relay.py: opens the pull request a trusted person's agent could not open itself (owner 2026-11-21).

An agent on an arena platform works with the platform's GitHub token, which reaches only its own account's fork: `gh pr create` into the owner's
repository fails with 403. When its work is done it pushes an empty marker commit to the branch of its fork:

    git commit --allow-empty -m "PR READY: <title>" -m "<the pull request's description>"

This tool (.github/workflows/trusted-pr-relay.yml, every 15 minutes) walks the forks of the people in tools/ci/trusted_forks.txt, finds their
branches `trusted/*` and `arena/*` whose head commit is such a marker, and opens the pull request into main (title and description from the marker,
maintainer_can_modify off, a first line naming who it was opened for) unless one is open for that head branch already. A pull request opened with
the workflow's token starts no workflow by itself (GitHub's rule), so the workflow then starts fork-pr.yml by hand for it (the build and own tests,
whose result lands on the head commit). Nothing of the fork's code runs here: only GitHub's API.

    GITHUB_TOKEN=... GITHUB_REPOSITORY=owner/repo trusted_pr_relay.py [--dry-run]

Prints one line per pull request opened (`OPENED <number> <user>:<branch>`), exit 0; an API failure is printed and the next branch is tried.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

API = "https://api.github.com"
MARK = "PR READY:"
PREFIXES = ("trusted/", "arena/")
LIST = Path(__file__).resolve().parent / "trusted_forks.txt"


def call(method: str, path: str, token: str, body: dict | None = None):
    req = urllib.request.Request(API + path, method=method, data=json.dumps(body).encode() if body is not None else None,
                                 headers={"Authorization": f"Bearer {token}", "Accept": "application/vnd.github+json",
                                          "X-GitHub-Api-Version": "2022-11-28", "User-Agent": "trusted-pr-relay"})
    with urllib.request.urlopen(req, timeout=30) as r:
        data = r.read()
        return json.loads(data) if data else None


def people() -> list[str]:
    out = []
    for line in LIST.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            out.append(line)
    return out


def marker(message: str) -> tuple[str, str] | None:
    """(title, description) of a marker commit's message, or None."""
    first, _, rest = message.partition("\n")
    if not first.startswith(MARK):
        return None
    title = first[len(MARK):].strip()
    return (title, rest.strip()) if title else None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    token = os.environ["GITHUB_TOKEN"]
    repo = os.environ["GITHUB_REPOSITORY"]
    owner, name = repo.split("/")
    for user in people():
        try:
            branches = call("GET", f"/repos/{user}/{name}/branches?per_page=100", token) or []
        except urllib.error.HTTPError as e:
            print(f"{user}: no fork {user}/{name} ({e.code})")
            continue
        for b in branches:
            branch = b["name"]
            if not branch.startswith(PREFIXES):
                continue
            try:
                commit = call("GET", f"/repos/{user}/{name}/commits/{b['commit']['sha']}", token)
                found = marker(commit["commit"]["message"])
                if not found:
                    continue
                open_prs = call("GET", f"/repos/{repo}/pulls?state=open&head={user}:{branch}", token) or []
                if open_prs:
                    continue
                title, body = found
                body = (f"Opened by the trusted-PR relay for @{user} (their agent's token could not open it; `tools/ci/trusted_pr_relay.py`), "
                        f"from `{user}:{branch}` at `{b['commit']['sha'][:9]}`.\n\n{body}")
                if a.dry_run:
                    print(f"WOULD OPEN {user}:{branch}: {title}")
                    continue
                pr = call("POST", f"/repos/{repo}/pulls", token, {"title": title, "head": f"{user}:{branch}", "base": "main", "body": body,
                                                                  "maintainer_can_modify": False})
                call("POST", f"/repos/{repo}/actions/workflows/fork-pr.yml/dispatches", token, {"ref": "main", "inputs": {"pr": str(pr["number"])}})
                print(f"OPENED {pr['number']} {user}:{branch}")
            except urllib.error.HTTPError as e:
                print(f"{user}:{branch}: GitHub said {e.code}: {e.read().decode('utf-8', 'replace')[:300]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
