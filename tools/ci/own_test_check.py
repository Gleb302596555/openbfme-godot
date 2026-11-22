"""own_test_check.py: a pull request that adds tests shows them passing before it is checked (owner 2026-11-19: PR 721 came with a test that
had never compiled; it was found in a batch and cost a bisect of five builds).

Every test prefix the pull request's added TEST_CASE names start with (`PORT-289 ...` -> PORT-289, `LOCAL-90 ...` -> LOCAL-90) needs a line, in
the pull request's description, a comment of the pull request or of the issue it closes (not the
bots' own), that names the prefix and carries doctest's own summary with at least one case
and none failed:

    PORT-289*: [doctest] test cases:  4 |  4 passed | 0 failed | 7020 skipped

A pull request that adds no TEST_CASE is not this check's (the PR checker reports a missing test). Used by .github/workflows/own-test.yml (at
once, on the pull request's events, no build) and by tools/colab/pr_checker.py (before its build).

    own_test_check.py --pr <number>     (GITHUB_TOKEN, GITHUB_REPOSITORY): exit 1 and a comment when a prefix has no passing line
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import urllib.request

ADDED_CASE = re.compile(r'^\+\s*TEST_CASE\("([^"]+)"', re.M)
# the bots' own comments (the checker, the batch, this check, Claude's answers) never stand for the agent's run
BOT = re.compile(r"^\s*<!-- (prcheck|batch|own-test|claude-answer|returned|stop-closed)")
SUMMARY = re.compile(r"\[doctest\]\s*test cases:\s*(\d+)\s*\|\s*(\d+)\s*passed\s*\|\s*(\d+)\s*failed")
MARK = "<!-- own-test-missing {head} -->"
# the rule holds for pull requests opened after it was announced (the prompts of 2026-11-19 ask for the line); older ones are not refused
RULE_SINCE = "2026-11-19T19:15:00Z"


def prefixes(diff: str) -> list[str]:
    """The added test cases' prefixes (the first word of the name, without a trailing colon)."""
    out = set()
    for name in ADDED_CASE.findall(diff):
        first = re.split(r"[\s:]", name.strip(), maxsplit=1)[0]
        if first:
            out.add(first)
    return sorted(out)


def passing_line(text: str, prefix: str) -> bool:
    """a line of `text` names `prefix` and carries a doctest summary with cases run and none failed."""
    pat = re.compile(rf"(?<![\w-]){re.escape(prefix)}(?![\w])", re.I)
    for line in text.splitlines():
        m = SUMMARY.search(line)
        if m and pat.search(line) and int(m.group(1)) > 0 and int(m.group(2)) == int(m.group(1)) and int(m.group(3)) == 0:
            return True
    return False


def missing(diff: str, texts: list[str]) -> list[str]:
    """The added test prefixes no text shows passing."""
    all_text = "\n".join(t for t in texts if t)
    return [p for p in prefixes(diff) if not passing_line(all_text, p)]


def message(miss: list[str], head: str) -> str:
    return (MARK.format(head=head) + "\n❌ **Свой тест не показан пройденным** (правило владельца 2026-11-19): для "
            + ", ".join(f"`{p}*`" for p in miss)
            + " нет строки с итогом doctest в описании PR или в твоём комментарии. Собери, прогони свой тест и вставь его итог одной строкой, "
              "например:\n```\n" + f"{miss[0]}*: [doctest] test cases:  N |  N passed | 0 failed | M skipped\n```\n"
              "Проверяльщик и партия не берут PR без этой строки: тест, который не компилируется или красный, находится здесь, а не в партии.")


def api(path: str, token: str, method: str = "GET", body: dict | None = None, accept: str = "application/vnd.github+json"):
    req = urllib.request.Request(f"https://api.github.com{path}", method=method, data=json.dumps(body).encode() if body else None,
                                 headers={"Authorization": f"Bearer {token}", "Accept": accept, "X-GitHub-Api-Version": "2022-11-28"})
    with urllib.request.urlopen(req) as r:
        data = r.read()
    return data.decode("utf-8", "replace") if accept.endswith("diff") else (json.loads(data) if data else None)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--pr", type=int)
    ap.add_argument("--issue", type=int, help="a comment in an issue: check every open PR that closes it")
    a = ap.parse_args(argv)
    token, repo = os.environ["GITHUB_TOKEN"], os.environ["GITHUB_REPOSITORY"]
    if a.issue is not None:
        prs = api(f"/repos/{repo}/pulls?state=open&per_page=100", token) or []
        mine = [p["number"] for p in prs if re.search(rf"(?:closes|fixes|resolves)\s+#{a.issue}\b", p.get("body") or "", re.I)]
        return max([check(n, token, repo) for n in mine] or [0])
    if a.pr is None:
        raise SystemExit("--pr or --issue")
    return check(a.pr, token, repo)


def check(n: int, token: str, repo: str) -> int:
    class A:  # the names the body below uses
        pr = n
    a = A
    pr = api(f"/repos/{repo}/pulls/{a.pr}", token)
    head, author = pr["head"]["sha"], pr["user"]["login"]
    if (pr.get("created_at") or "") < RULE_SINCE:
        print(f"PR #{a.pr}: opened before the rule ({pr.get('created_at')}): not checked")
        return 0
    try:
        diff = api(f"/repos/{repo}/pulls/{a.pr}", token, accept="application/vnd.github.v3.diff")
    except Exception:  # past GitHub's diff limit: the files' patches
        files = api(f"/repos/{repo}/pulls/{a.pr}/files?per_page=100", token) or []
        diff = "\n".join(f.get("patch") or "" for f in files)
    comments = api(f"/repos/{repo}/issues/{a.pr}/comments?per_page=100", token) or []
    texts = [pr.get("body") or ""] + [c.get("body") or "" for c in comments]
    m = re.search(r"(?:closes|fixes|resolves)\s+#(\d+)", pr.get("body") or "", re.I)
    if m:  # an agent's report in its issue (the local agents' habit) counts too
        texts += [c.get("body") or "" for c in (api(f"/repos/{repo}/issues/{m.group(1)}/comments?per_page=100", token) or [])]
    texts = [t for t in texts if not BOT.match(t)]
    miss = missing(diff, texts)
    if not miss:
        print(f"PR #{a.pr}: every added test prefix ({', '.join(prefixes(diff)) or 'none'}) is shown passing")
        return 0
    mark = MARK.format(head=head)
    if not any(mark in (c.get("body") or "") for c in comments):
        try:
            api(f"/repos/{repo}/issues/{a.pr}/comments", token, method="POST", body={"body": message(miss, head)})
        except Exception as e:  # a fork's read-only token: the failed check still says it
            print(f"no comment posted: {e}")
    print(f"PR #{a.pr}: no passing doctest line for {', '.join(miss)}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
