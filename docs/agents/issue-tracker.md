# Issue tracker: Gitea

Issues and specs for this repo live as **Gitea issues** at
`https://gitea.home/admin/mobile-set-editor/issues`.

`tea` is installed (login `gitea.home`, user `admin`) and is fine for quick reads, but
it does not cover dependencies, so the canonical form of every operation below is a
`curl` against Gitea's REST API. Gitea's API is GitHub-shaped but **not**
GitHub-compatible — read "Gotchas" before writing a call.

## Setup

The repo is private, so every call needs a token. Generate one at
`https://gitea.home/user/settings/applications` with scopes **`write:issue`** and
`read:repository`.

**`issue` is a separate scope category from `repository`.** A token with only
`read/write:repository` reads the repo fine and then 403s on every `/issues` and
`/labels` call — labels live under the issue category too. Gitea cannot edit an
existing token's scopes; a wrong one has to be deleted and regenerated. `write:issue`
implies read, so it is the only issue scope needed.

```sh
export GITEA_TOKEN=<token>
export GITEA_API=https://gitea.home/api/v1/repos/admin/mobile-set-editor
```

The owner/repo pair comes from `git remote -v`. Authenticate with a header, never a
query parameter — a `?token=` lands in the server's access log:

```sh
curl -sS -H "Authorization: token $GITEA_TOKEN" -H 'Content-Type: application/json' ...
```

An agent session's shell does not persist environment variables between commands, so
the token lives in a file instead and is read per-invocation:

```sh
GITEA_TOKEN=$(cat ~/.config/gitea/token) curl -sS -H "Authorization: token $GITEA_TOKEN" ...
```

If neither `GITEA_TOKEN` nor `~/.config/gitea/token` is present, stop and ask the user
for one. Do not read it out of `~/.git-credentials`.

## Conventions

Bodies are multi-line markdown, so build request payloads with `jq -n` rather than
hand-rolled JSON — it handles the escaping:

- **Create an issue**:
  ```sh
  jq -n --arg t "$TITLE" --rawfile b body.md --argjson l "[$LABEL_ID]" \
     '{title:$t, body:$b, labels:$l}' \
  | curl -sS -X POST "$GITEA_API/issues" -H "Authorization: token $GITEA_TOKEN" \
         -H 'Content-Type: application/json' -d @-
  ```
- **Read an issue**: `curl -sS "$GITEA_API/issues/<index>"` and, separately,
  `curl -sS "$GITEA_API/issues/<index>/comments"` — the issue payload carries a
  `comments` *count*, not the comment bodies.
- **List issues**: `curl -sS "$GITEA_API/issues?state=open&labels=wayfinder:map&limit=50"`.
  `labels` is a comma-separated list of **names** here (unlike create — see Gotchas).
- **Comment**: `curl -sS -X POST "$GITEA_API/issues/<index>/comments" -d '{"body":"..."}'`
- **Apply / remove labels**: `POST "$GITEA_API/issues/<index>/labels"` with
  `{"labels":[<id>]}` / `DELETE "$GITEA_API/issues/<index>/labels/<label-id>"`
- **Close**: `PATCH "$GITEA_API/issues/<index>"` with `{"state":"closed"}`

Paginate with `?page=N&limit=50`; Gitea caps `limit` at the instance's
`MAX_RESPONSE_ITEMS` (50 by default) and returns a partial page without complaint.

## Gotchas

These are where Gitea diverges from GitHub, and each one fails quietly:

- **Creating an issue takes label *ids*, not names** — `labels` in
  `CreateIssueOption` is an array of integers. Names are silently dropped, leaving an
  unlabelled issue. Resolve ids once per session:
  `curl -sS "$GITEA_API/labels?limit=50" | jq -r '.[] | "\(.id) \(.name)"'`.
  Create a missing label with `POST "$GITEA_API/labels" {"name":..,"color":"#rrggbb"}`.
  Filtering by label on *list* does use names.
- **`index` vs `id`.** The `#number` a human sees is `index`. Every issue URL path uses
  `index`. `id` is the instance-global database id and appears in payloads — using it as
  a path segment silently addresses a different issue. Dependencies take `index`
  (GitHub's equivalent endpoint wants the database id; do not carry that habit over).
- **Issues and PRs share one number space**, as on GitHub. An issue payload with a
  non-null `pull_request` field is a PR.
- **No sub-issues.** Gitea has no parent/child issue relation. Hierarchy is expressed
  by a task list in the parent body plus a `Part of #<n>` line in the child.
- **Dependencies must be enabled on the repo.** Settings → Issues → "Enable Repository
  Issue Dependencies". Off, the dependency endpoints 404 in a way that reads like a
  missing issue.

## Pull requests as a triage surface

**PRs as a request surface: no.** _(Set to `yes` if this repo treats external PRs as
feature requests; `/triage` reads this flag.)_

## When a skill says "publish to the issue tracker"

Create a Gitea issue.

## When a skill says "fetch the relevant ticket"

`GET "$GITEA_API/issues/<index>"` plus `GET "$GITEA_API/issues/<index>/comments"`.

## Wayfinding operations

Used by `/wayfinder`. The **map** is a single issue; tickets are ordinary issues linked
to it by body reference, since Gitea has no sub-issues.

- **Map**: one issue labelled `wayfinder:map`, holding the Destination / Notes /
  Decisions-so-far / Fog body. Find it with
  `GET "$GITEA_API/issues?state=open&labels=wayfinder:map"`. Its body carries a
  markdown task list of every child ticket (`- [ ] #12 <name>`), which is the only
  record of membership — keep it current when charting a new ticket.
- **Child ticket**: an ordinary issue whose body opens with `Part of #<map>`. Labels:
  `wayfinder:<type>` — `research` / `prototype` / `grilling` / `task`. Once claimed, the
  ticket is assigned to the driving dev.
- **Blocking**: Gitea's **native issue dependencies**, which render in the UI and gate
  the close button. Add an edge with:
  ```sh
  curl -sS -X POST "$GITEA_API/issues/<child>/dependencies" \
       -H "Authorization: token $GITEA_TOKEN" -H 'Content-Type: application/json' \
       -d '{"owner":"admin","repo":"mobile-set-editor","index": <blocker-index>}'
  ```
  **`owner` and `repo` are required even for a same-repo edge.** Sending `index` alone
  looks correct and 404s with `repository does not exist [id: 0, uid: 0, owner_name: ,
  name: ]` — Gitea defaulted the empty strings and looked up a repo named `""`.
  `POST .../issues/<n>/dependencies` means "*n* is blocked by the issue in the body";
  `POST .../issues/<n>/blocks` is the same edge stated the other way. Read an issue's
  blockers with `GET "$GITEA_API/issues/<n>/dependencies"`, which returns full issue
  objects — **including closed ones**. A ticket is unblocked when every issue that
  endpoint returns has `state == "closed"`; there is no `blocked_by` count to shortcut it.
- **Frontier query**: list the map's open children, then drop any with an assignee or an
  open blocker:
  ```sh
  curl -sS -H "Authorization: token $GITEA_TOKEN" \
       "$GITEA_API/issues?state=open&limit=50" \
  | jq -r '.[] | select(.pull_request == null) | select(.assignees == null)
           | select([.labels[].name] | index("wayfinder:map") | not) | .number' \
  | while read -r n; do
      curl -sS -H "Authorization: token $GITEA_TOKEN" "$GITEA_API/issues/$n/dependencies" \
      | jq -e 'all(.state == "closed")' >/dev/null && printf '%s ' "$n"
    done
  ```
  Feed the candidate list through a pipe as above rather than `for n in $cands`: this
  machine's shell is zsh, which does not word-split an unquoted variable, so the loop
  form silently passes the whole list as one URL.
  First in map task-list order wins.
- **Claim**: `PATCH "$GITEA_API/issues/<n>"` with `{"assignees":["<username>"]}`, the
  session's first write. Gitea has no `@me`; use the name from
  `GET https://gitea.home/api/v1/user | jq -r .login`.
- **Resolve**: `POST "$GITEA_API/issues/<n>/comments"` with the answer, then
  `PATCH "$GITEA_API/issues/<n>"` `{"state":"closed"}`, then append a context pointer
  (gist + link) to the map issue's Decisions-so-far via a `PATCH` of the map body.
