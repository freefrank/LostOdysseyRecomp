"""Reconcile reviewed roadmap items with the repository's bound GitHub Project.

Read-only plan by default. --apply writes project drafts/fields, never repository
issues, comments, source code, Git refs, or releases. Uses the signed-in gh CLI.
"""
import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / "docs/project-management"


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    # Sync clients and antivirus scanners may briefly hold a Windows file.
    # Keep the complete temporary file; retry only the atomic local replacement.
    for attempt in range(11):
        try:
            temporary.replace(path)
            break
        except PermissionError:
            if attempt == 10:
                raise
            time.sleep(0.1)


def gh(*args):
    result = subprocess.run(["gh", *args], capture_output=True, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    return json.loads(result.stdout)


def graphql(query, variables=None):
    payload = json.dumps({"query": query, "variables": variables or {}})
    result = subprocess.run(["gh", "api", "graphql", "--input", "-"], input=payload,
                            capture_output=True, encoding="utf-8")
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip())
    response = json.loads(result.stdout)
    if response.get("errors"):
        raise RuntimeError(json.dumps(response["errors"]))
    return response["data"]


def digest(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def remote_project(project_id):
    return graphql("""query($id:ID!) {node(id:$id) {... on ProjectV2 {
      id title url number owner {... on User {login} ... on Organization {login}}
      repositories(first:100){nodes{nameWithOwner}}
      fields(first:100){nodes{... on ProjectV2FieldCommon{id name dataType}
        ... on ProjectV2SingleSelectField{options{id name}}}}
    }}}""", {"id": project_id})["node"]


def remote_items(project_id):
    items, cursor = [], None
    while True:
        connection = graphql("""query($id:ID!,$after:String){node(id:$id){... on ProjectV2{
          items(first:100,after:$after){pageInfo{hasNextPage endCursor} nodes{id
            content{... on DraftIssue{id title body} ... on Issue{id title url state}
                    ... on PullRequest{id title url state}}
            fieldValues(first:100){nodes{
              ... on ProjectV2ItemFieldTextValue{text field{... on ProjectV2FieldCommon{name}}}
              ... on ProjectV2ItemFieldDateValue{date field{... on ProjectV2FieldCommon{name}}}
              ... on ProjectV2ItemFieldSingleSelectValue{name field{... on ProjectV2FieldCommon{name}}}
            }}
          }}
        }}}""", {"id": project_id, "after": cursor})["node"]["items"]
        for item in connection["nodes"]:
            item["values"] = {v["field"]["name"]: v.get("text", v.get("date", v.get("name")))
                              for v in item["fieldValues"]["nodes"] if "field" in v}
            items.append(item)
        if not connection["pageInfo"]["hasNextPage"]:
            return items
        cursor = connection["pageInfo"]["endCursor"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--manifest", type=Path, default=DATA / "items.json")
    parser.add_argument("--report", type=Path, default=ROOT / "out/project-management/sync-report.json")
    args = parser.parse_args()
    binding = read_json(DATA / "project.json")
    manifest = read_json(args.manifest)
    if manifest.get("repository") != binding["repository"]:
        raise RuntimeError("Manifest repository does not match the project binding")
    state_path = DATA / "sync-state.json"
    state = read_json(state_path) if state_path.exists() else {"items": {}}
    project = remote_project(binding["id"])
    if (project["owner"]["login"] != binding["owner"] or project["number"] != binding["number"]
            or binding["repository"] not in {r["nameWithOwner"] for r in project["repositories"]["nodes"]}):
        raise RuntimeError("Project identity/repository binding mismatch")
    fields = {f["name"]: f for f in project["fields"]["nodes"]}
    current = remote_items(binding["id"])
    by_key, by_url = {}, {}
    for item in current:
        key = item["values"].get("Source key")
        if not key and (item.get("content") or {}).get("body", "").startswith("<!-- lo-project:"):
            key = item["content"]["body"].split("-->", 1)[0].removeprefix("<!-- lo-project:").strip()
        if key:
            if key in by_key:
                raise RuntimeError("Duplicate remote source key: " + key)
            by_key[key] = item
        if (item.get("content") or {}).get("url"):
            by_url[item["content"]["url"]] = item
    seen, issue_urls = set(), set()
    for item in manifest["items"]:
        key = item["key"]
        if key in seen:
            raise RuntimeError("Duplicate manifest source key: " + key)
        seen.add(key)
        issue_url = item.get("issue_url")
        if issue_url:
            if issue_url in issue_urls:
                raise RuntimeError("Duplicate manifest issue URL: " + issue_url)
            if not issue_url.startswith("https://github.com/" + binding["repository"] + "/issues/"):
                raise RuntimeError("Issue URL is outside the bound repository: " + issue_url)
            issue_urls.add(issue_url)
        for name, value in item["fields"].items():
            if value is None:
                continue
            if name not in fields:
                raise RuntimeError("Missing project field: " + name)
            if fields[name]["dataType"] == "SINGLE_SELECT" and value not in {o["name"] for o in fields[name]["options"]}:
                raise RuntimeError(f"Unknown {name} option: {value}")
    report = {"project": project["url"], "apply": args.apply, "created": [], "updated": [],
              "unchanged": [], "conflicts": [], "operations": []}
    for desired in manifest["items"]:
        key = desired["key"]
        item = by_key.get(key) or by_url.get(desired.get("issue_url"))
        previous = state["items"].get(key, {})
        if not item and previous.get("id"):
            report["conflicts"].append({"key": key, "reason": "Previously synced item is absent; inspect deletion/archive before recreating"})
            continue
        if not item:
            report["created"].append(key)
            report["operations"].append({"key": key, "operation": "link issue" if desired.get("issue_url") else "create draft"})
            if args.apply:
                if desired.get("issue_url"):
                    linked = gh("project", "item-add", str(binding["number"]), "--owner", binding["owner"],
                                "--url", desired["issue_url"], "--format", "json")
                    item = {"id": linked["id"], "content": {"url": desired["issue_url"]}, "values": {}}
                else:
                    result = graphql("""mutation($input:AddProjectV2DraftIssueInput!){
                        addProjectV2DraftIssue(input:$input){projectItem{id content{... on DraftIssue{id title body}}}}}
                        """, {"input": {"projectId": binding["id"], "title": desired["title"], "body": desired["body"]}})
                    item = result["addProjectV2DraftIssue"]["projectItem"]
                    item["values"] = {}
                state["items"][key] = {"id": item["id"], "fields": {}}
                if not desired.get("issue_url"):
                    state["items"][key].update(title=desired["title"], body_sha256=digest(desired["body"]))
                save_json(state_path, state)
            else:
                item = {"id": None, "content": {}, "values": {}}
        changed = False
        content = item.get("content") or {}
        if not desired.get("issue_url") and "body" in content:
            if content["title"] != desired["title"] or content["body"] != desired["body"]:
                if previous.get("body_sha256") != digest(content["body"]) or previous.get("title") != content["title"]:
                    report["conflicts"].append({"key": key, "reason": "Remote draft text changed; reconcile human edits"})
                else:
                    changed = True
                    report["operations"].append({"key": key, "operation": "update draft text"})
                    if args.apply:
                        graphql("""mutation($input:UpdateProjectV2DraftIssueInput!){updateProjectV2DraftIssue(input:$input){draftIssue{id}}}""",
                                {"input": {"draftIssueId": content["id"], "title": desired["title"], "body": desired["body"]}})
                        state["items"][key].update(title=desired["title"], body_sha256=digest(desired["body"]))
        changes = []
        for name, value in {**desired["fields"], "Source key": key}.items():
            if value is None or item["values"].get(name) == value:
                continue
            old = item["values"].get(name)
            if old is not None and old != previous.get("fields", {}).get(name):
                report["conflicts"].append({"key": key, "field": name, "remote": old, "desired": value})
                continue
            field = fields[name]
            if field["dataType"] == "SINGLE_SELECT":
                encoded = {"singleSelectOptionId": next(o["id"] for o in field["options"] if o["name"] == value)}
            elif field["dataType"] == "DATE":
                encoded = {"date": value}
            else:
                encoded = {"text": value}
            changes.append((name, value, {"projectId": binding["id"], "itemId": item["id"], "fieldId": field["id"], "value": encoded}))
            report["operations"].append({"key": key, "field": name, "before": old, "after": value})
        if changes:
            changed = True
            if args.apply:
                mutation = "mutation {" + " ".join(f"f{i}:updateProjectV2ItemFieldValue(input:{to_graphql(change[2])}){{projectV2Item{{id}}}}"
                                                   for i, change in enumerate(changes)) + "}"
                graphql(mutation)
                tracked = state["items"].setdefault(key, {"id": item["id"], "fields": {}})
                tracked["fields"].update({name: value for name, value, _ in changes})
        if key not in report["created"]:
            report["updated" if changed else "unchanged"].append(key)
        if args.apply:
            tracked = state["items"].setdefault(key, {"id": item["id"], "fields": {}})
            for name, value in {**desired["fields"], "Source key": key}.items():
                if item["values"].get(name) == value and value is not None:
                    tracked["fields"][name] = value
            save_json(state_path, state)
        if args.apply:
            save_json(args.report, report)
        if args.apply and (changed or key in report["created"]):
            print(key, flush=True)
    save_json(args.report, report)
    print(json.dumps({k: len(report[k]) for k in ("created", "updated", "unchanged", "conflicts", "operations")}))
    if report["conflicts"]:
        raise SystemExit(2)


def to_graphql(value):
    if isinstance(value, dict):
        return "{" + ",".join(k + ":" + to_graphql(v) for k, v in value.items()) + "}"
    if isinstance(value, list):
        return "[" + ",".join(to_graphql(v) for v in value) + "]"
    return json.dumps(value, ensure_ascii=True)


if __name__ == "__main__":
    main()
