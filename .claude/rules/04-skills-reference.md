# OpenSpec Commands

## Minimal Default Flow
```
/opsx:propose → /opsx:apply → /opsx:verify → /opsx:archive
```

| Command | Action |
|---|---|
| `/opsx:propose` | Propose a new change with all artifacts in one step |
| `/opsx:apply` | Implement tasks from an approved change |
| `/opsx:verify` | Verify implementation matches change artifacts before archiving |
| `/opsx:archive` | Archive a completed change |

## Advanced Flow (with Explore)
```
/opsx:explore → /opsx:new → /opsx:continue [...] → /opsx:apply → /opsx:verify → /opsx:archive
                          ↳ /opsx:ff (shortcut: replaces new + continue[...])
                                                              ↳ /opsx:sync (optional: sync specs without archiving)
```

| Command | Action |
|---|---|
| `/opsx:explore` | Think through ideas, investigate problems, clarify requirements |
| `/opsx:new` | Start a new change (step-by-step artifact workflow) |
| `/opsx:continue` | Create the next artifact for a change in progress |
| `/opsx:ff` | Shortcut: replaces `new + continue[...]` — generates all artifacts at once |
| `/opsx:apply` | Implement tasks from an approved change |
| `/opsx:verify` | Verify implementation matches change artifacts before archiving |
| `/opsx:sync` | *(Optional)* Sync delta specs to main specs; change stays active until archive |
| `/opsx:archive` | Archive a completed change |

## Utilities

| Command | Action |
|---|---|
| `/opsx:bulk-archive` | Archive multiple completed changes at once |
| `/opsx:onboard` | Guided onboarding walkthrough |
