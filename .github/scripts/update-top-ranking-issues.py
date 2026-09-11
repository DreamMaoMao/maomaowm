#!/usr/bin/env python3
"""Update a tracking issue with the hottest (top-ranked) open issues.

Issues are ranked by their total reaction heat, i.e. the sum of :+1:
(thumbs up) and :heart: (heart) reactions, and grouped by section. By default
only `enhancement` issues are listed. The rendered Markdown is written back to
a single "tracking" issue, so the issue body always reflects the current
ranking.

This mirrors the behaviour of zed-industries/zed's
`script/update_top_ranking_issues/main.py`, but is dependency free (Python
standard library only) so it can run on a plain `ubuntu-latest` runner.

Usage:
    # Print the ranking to stdout (dry run)
    python3 .github/scripts/update-top-ranking-issues.py

    # Update a tracking issue in place
    GITHUB_TOKEN=... python3 .github/scripts/update-top-ranking-issues.py \
        --issue-number 123

    # Weekly/monthly "hottest new issues" view
    python3 .github/scripts/update-top-ranking-issues.py \
        --issue-number 123 --days 7
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone

# --- Configuration ---------------------------------------------------------

# Sections rendered in the tracking issue. Each value is a GitHub search
# qualifier; sections that end up empty are skipped automatically, so it is
# safe to reference labels that do not exist and adding/removing a section is
# a one-line change.
#
# Only `enhancement` issues are ranked. To also track bug reports, add
#     "Bugs": "label:bug",
# to this dict — the section header is only rendered when there is more than
# one section.
SECTIONS: dict[str, str] = {
    "Enhancements": "label:enhancement",
}

# Issues carrying any of these labels are never ranked.
EXCLUDE_LABELS: list[str] = ["ignore top-ranking issues"]

# Reactions that contribute to an issue's "heat" score.
SCORED_REACTIONS: tuple[str, ...] = ("+1", "heart")
REACTION_EMOJI: dict[str, str] = {"+1": "👍", "heart": "❤️"}

# The GitHub search API can only sort by a single reaction type, so for each
# section we fetch the top issues sorted by :+1: *and* by :heart: and merge
# the two, then rank by the combined score. This keeps issues that are only
# popular via hearts from being missed.
SORT_KEYS: tuple[str, ...] = ("reactions-+1", "reactions-heart")

# How many issues to show per section, and how many to fetch from the API
# before filtering (the search API caps `per_page` at 100).
ISSUES_PER_SECTION = 30
ISSUES_TO_FETCH = 100

API_BASE = "https://api.github.com"
DEFAULT_REPO = "mangowm/mango"
DATETIME_FORMAT = "%B %d, %Y %I:%M %p"


# --- GitHub API helpers ----------------------------------------------------


def resolve_token(cli_token: str | None) -> str | None:
    token = (
        cli_token
        or os.getenv("GITHUB_TOKEN")
        or os.getenv("GH_TOKEN")
        or os.getenv("GITHUB_ACCESS_TOKEN")
    )
    if token:
        return token

    try:
        result = subprocess.run(
            ["gh", "auth", "token"],
            capture_output=True,
            text=True,
            check=True,
        )
        token = result.stdout.strip()
        if token:
            return token
    except (subprocess.CalledProcessError, FileNotFoundError):
        pass

    # No token: fall back to unauthenticated requests. This works for local
    # dry runs, but the search API rate limit is low, so CI must provide a
    # token (the workflow passes the built-in GITHUB_TOKEN).
    print(
        "warning: no GitHub token found, using unauthenticated requests "
        "(low rate limit). Pass --token or set GITHUB_TOKEN for real runs.",
        file=sys.stderr,
    )
    return None


def auth_headers(token: str | None) -> dict[str, str]:
    headers = {
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28",
        "User-Agent": "mango-top-ranking-issues",
    }
    if token:
        headers["Authorization"] = f"token {token}"
    return headers


def api_get(token: str | None, path: str, params: dict[str, str] | None = None) -> dict:
    url = f"{API_BASE}{path}"
    if params:
        url = f"{url}?{urllib.parse.urlencode(params)}"
    request = urllib.request.Request(url, headers=auth_headers(token))
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", "replace")
        sys.exit(f"error: GET {url} failed ({error.code}): {detail}")


def api_patch(token: str | None, path: str, payload: dict) -> None:
    url = f"{API_BASE}{path}"
    data = json.dumps(payload).encode("utf-8")
    headers = auth_headers(token)
    headers["Content-Type"] = "application/json"
    request = urllib.request.Request(url, data=data, method="PATCH", headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            response.read()
    except urllib.error.HTTPError as error:
        detail = error.read().decode("utf-8", "replace")
        sys.exit(f"error: PATCH {url} failed ({error.code}): {detail}")


# --- Ranking ---------------------------------------------------------------


def fetch_section_issues(
    token: str | None,
    repo: str,
    qualifier: str,
    start_date: datetime | None,
) -> list[dict]:
    query_parts = [
        f"repo:{repo}",
        "is:issue",
        "is:open",
    ]
    query_parts += [f'-label:"{label}"' for label in EXCLUDE_LABELS]
    if qualifier:
        query_parts.append(qualifier)
    if start_date is not None:
        query_parts.append(f"created:>={start_date.strftime('%Y-%m-%d')}")

    query = " ".join(query_parts)
    merged: dict[int, dict] = {}

    # The search API can only sort by one reaction type, so we look at the top
    # issues by :+1: and by :heart: separately and merge them before ranking by
    # the combined score.
    for sort_key in SORT_KEYS:
        result = api_get(
            token,
            "/search/issues",
            {
                "q": query,
                "sort": sort_key,
                "order": "desc",
                "per_page": str(ISSUES_TO_FETCH),
            },
        )
        for item in result.get("items", []):
            reactions = item.get("reactions", {})
            breakdown = {name: reactions.get(name, 0) for name in SCORED_REACTIONS}
            score = sum(breakdown.values())
            merged[item["number"]] = {
                "url": item["html_url"],
                "title": item["title"],
                "score": score,
                "breakdown": breakdown,
                "created_at": item["created_at"],
            }

    issues = [issue for issue in merged.values() if issue["score"] > 0]

    issues.sort(key=lambda issue: (-issue["score"], issue["created_at"]))
    return issues[:ISSUES_PER_SECTION]


def build_ranking(
    token: str | None, repo: str, start_date: datetime | None
) -> "dict[str, list[dict]]":
    section_to_issues: dict[str, list[dict]] = {}
    for section, qualifier in SECTIONS.items():
        issues = fetch_section_issues(token, repo, qualifier, start_date)
        if issues:
            section_to_issues[section] = issues

    # Show the hottest sections first.
    return dict(
        sorted(
            section_to_issues.items(),
            key=lambda item: sum(issue["score"] for issue in item[1]),
            reverse=True,
        )
    )


# --- Rendering -------------------------------------------------------------


def render_issue_body(
    section_to_issues: "dict[str, list[dict]]",
    repo: str,
    timezone_name: str,
    window_days: int | None,
) -> str:
    try:
        from zoneinfo import ZoneInfo

        tz = ZoneInfo(timezone_name)
    except Exception:  # pragma: no cover - bad tz name falls back to UTC
        tz = timezone.utc

    now = datetime.now(tz)
    stamp = now.strftime(f"{DATETIME_FORMAT} (%Z)")

    lines: list[str] = []
    if window_days:
        lines.append(
            f"*Hottest issues created in the last {window_days} days, "
            "ranked by 👍 + ❤️ reactions.*"
        )
    else:
        lines.append("*Hottest open issues, ranked by 👍 + ❤️ reactions.*")
    lines.append("")
    lines.append(f"*Updated on {stamp}*")

    total = sum(issue["score"] for issues in section_to_issues.values() for issue in issues)
    if total == 0:
        lines.append("")
        lines.append("_No issues with reactions yet._")

    show_headers = len(section_to_issues) > 1
    for section, issues in section_to_issues.items():
        lines.append("")
        if show_headers:
            lines.append(f"## {section}")
            lines.append("")
        for index, issue in enumerate(issues, start=1):
            breakdown = issue["breakdown"]
            detail = " + ".join(
                f"{breakdown[name]} {REACTION_EMOJI[name]}"
                for name in SCORED_REACTIONS
            )
            lines.append(
                f"{index}. {issue['url']} ({issue['score']} = {detail}) "
                f"— {issue['title']}"
            )

    script_url = (
        f"https://github.com/{repo}/blob/main/.github/scripts/update-top-ranking-issues.py"
    )
    lines.append("")
    lines.append("---")
    lines.append("")
    lines.append(
        "*This issue is updated automatically. To vote, add a 👍 or ❤️ reaction "
        "to an issue. For details, [see the script]"
        f"({script_url}).*"
    )
    return "\n".join(lines) + "\n"


# --- Entrypoint ------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--token", default=None, help="GitHub token (or set GITHUB_TOKEN)")
    parser.add_argument("--repo", default=DEFAULT_REPO, help="owner/name (default: %(default)s)")
    parser.add_argument(
        "--issue-number",
        type=int,
        default=None,
        help="Tracking issue to update. If omitted, the ranking is printed instead.",
    )
    parser.add_argument(
        "--days",
        type=int,
        default=None,
        help="Only rank issues created in the last N days (default: all time).",
    )
    parser.add_argument(
        "--timezone",
        default=os.getenv("TZ", "UTC"),
        help="Timezone for the 'Updated on' stamp (default: %(default)s)",
    )
    args = parser.parse_args()

    token = resolve_token(args.token)
    start_date: datetime | None = None
    if args.days:
        start_date = datetime.now(timezone.utc) - timedelta(days=args.days)

    ranking = build_ranking(token, args.repo, start_date)
    body = render_issue_body(ranking, args.repo, args.timezone, args.days)

    if args.issue_number:
        api_patch(
            token,
            f"/repos/{args.repo}/issues/{args.issue_number}",
            {"body": body},
        )
        print(
            f"Updated https://github.com/{args.repo}/issues/{args.issue_number} "
            f"({sum(len(v) for v in ranking.values())} issues in {len(ranking)} sections)."
        )
    else:
        print(body)


if __name__ == "__main__":
    main()
