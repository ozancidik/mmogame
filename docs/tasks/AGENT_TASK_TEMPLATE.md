# Agent task: <task>

- Objective: <observable result>
- Owner/runtime: <codex or claude>
- Specialist role: <one of the 12 canonical roles>
- Branch/worktree/base commit: <absolute path and SHA>
- Task manifest: <absolute JSON path>
- Mode: <read-only review or implementation>
- Owned paths: <exact repo-relative files or directory prefixes; no globs>
- Forbidden/shared paths: <other agents' paths; root routing owned by coordinator>
- Dependencies: <agent outputs and exact reviewed commit/diff>
- Acceptance criteria: <concrete behavior and failure cases>

## Runtime delegation evidence

Record actual agent id, selected role, runtime, task prompt, status and returned
summary. Config files, role-playing in one context and creating a worktree do
not count as dispatch. Do not claim Claude executed without a Claude result.
If dispatch is unavailable, report the limitation and do not start gameplay work.

## Specialist handoff

Provide objective, owned paths, base SHA, changed paths/diff, authority and data
contracts, risks, expected result and prohibited actions to every agent. Never
assume it inherited the parent conversation. Parallel writers need separate
worktrees and disjoint ownership. One writer per shared file, including binaries.

## Implementation and review

- Design: formulas, units, PvP/PvE, gold/item sinks; measured vs proposed values.
- Development: server-owned mutations; RPC ownership/input validation; UObject
  lifetime and delegate re-entry; persistence schema and failure handling.
- Independent review: QA plus network/security for combat, rewards or persistence;
  performance for scaling; asset review for Content; devops for build/deployment.
- Documentation: API changes, commands and outstanding decisions with evidence.

## Verification record

| Command / scenario | Exit / result | Evidence path or summary | Limitations |
| --- | --- | --- | --- |
| <exact command> | <pass/fail/not run> | <log/check count> | <why> |

Builds require the exclusive shared lock and process check in
`Scripts/CheckAgentTask.ps1 -Build`. Never kill an editor or launch concurrent
Unreal automation. Record the real exit status. Static review is not PIE,
replication or dedicated server evidence. Native tests, multi-client PIE and
packet loss/reconnect scenarios are separate checks when authorized.

## Integration handoff

List changed files, findings by severity, completed acceptance criteria,
remaining risks and exact proposed commits/patches. Coordinator serializes
integration and reruns affected checks. No automatic commit, merge, push,
deployment, reset, clean, worktree removal or deletion.
