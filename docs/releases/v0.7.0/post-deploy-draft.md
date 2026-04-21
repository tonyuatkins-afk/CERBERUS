# v0.7.0 final: post-deploy artifacts (draft)

**Status:** held. Use these drafts when the sequence below completes:

1. Tony sets up the Cloudflare Tunnel + Worker per DEPLOY.md (Option C).
2. `docker compose up -d` on the HA mini PC; smoke test via
   `curl https://barelybooting.com/api/v1/health`.
3. BEK-V409 end-to-end round trip: boot CERBERUS v0.7.0-rc2 on the
   486, confirm "Upload results?" prompt fires, confirm the submission
   ID + URL come back, visit the URL from modern browser, confirm
   the run is visible in the browse UI.

All three must pass before anything below ships. If any step fails,
the artifacts stay drafts and we iterate.

---

## 1. CERBERUS repo edits

### src/cerberus.h

```diff
-#define CERBERUS_VERSION          "0.7.0-rc2"
+#define CERBERUS_VERSION          "0.7.0"
```

Nothing else changes in the header. `CERBERUS_SCHEMA_VERSION` and
`CERBERUS_INI_FORMAT` stay at "1.0" and "1" respectively because
v0.7.0 final does not break compatibility.

### CHANGELOG.md (prepend to top)

```markdown
## v0.7.0 — 2026-04-XX

**Final release of the community-upload milestone.** Promoted from
`v0.7.0-rc2` after end-to-end validation: CERBERUS on BEK-V409
(Intel i486DX2-66) successfully POSTed a `CERBERUS.INI` through
HTGET to the barelybooting-server v0.1.3 instance deployed on Home
Assistant OS via Cloudflare Tunnel. Submission ID and URL returned,
run visible in the public browse UI. No code changes between rc2
and final.

Cumulative changes from v0.6.2:

- **Network transport detection** (`src/detect/network.c`). Probes
  for NetISA (INT 63h), packet driver (INT 60h-7Fh scan with
  "PKT DRVR" signature), mTCP (`MTCP_CFG` env var), WATTCP
  (`WATTCP` env var). Result emitted as `[network] transport=...`.
- **[upload] INI section.** User metadata (`nickname`, `notes`) from
  `/NICK:` and `/NOTE:` flags, `status` from upload outcome,
  `submission_id` and `url` from server response on success.
- **HTGET shell-out for POST.** `upload_execute()` composes the
  HTGET command, runs it via `system()`, captures response in
  `UPLOAD.TMP`, parses the two-line contract response.
- **UPLOAD STATUS summary section** in the scrollable UI.
- **Frozen INI format at `ini_format=1`** with a documented
  backward-compatibility commitment (`docs/ini-upload-contract.md`).
- **`/NOUPLOAD` / `/UPLOAD` / `/NICK:` / `/NOTE:` flags** added to
  `main.c` argument parsing.

Server side (separate repo, separate releases):
- barelybooting-server v0.1.0 → v0.1.3 shipped across four
  reviewer-driven hardening passes before this tag.
```

### README.md status block

Replace the current `v0.7.0-rc2` status paragraph with:

```markdown
**`v0.7.0` tagged 2026-04-XX.** Community-upload milestone,
real-hardware validated. Twelve tags across the arc:
`v0.1.1-scaffold` → `v0.2-rc1` → `v0.3-rc1` → `v0.4-rc1` →
**`v0.4.0`** → **`v0.5.0`** → **`v0.6.0`** → `v0.6.1` → `v0.6.2` →
`v0.7.0-rc1` → `v0.7.0-rc2` → **`v0.7.0`**. Companion server:
[barelybooting-server v0.1.3](https://github.com/tonyuatkins-afk/barelybooting-server/releases/tag/v0.1.3)
deployed at `barelybooting.com` via Home Assistant Docker +
Cloudflare Tunnel. The INI format is frozen at `ini_format=1`;
server additive changes past this point ship without breaking v0.7
clients.
```

Also bump EXE size to the actual v0.7.0 final binary size (likely
still 164,050 unless any trivial build-side changes land).

### Commit + tag sequence

```bash
cd /c/Development/CERBERUS
# Edit src/cerberus.h, CHANGELOG.md, README.md as above.
git add src/cerberus.h CHANGELOG.md README.md
git commit -m "v0.7.0 final: promote rc2 after BEK-V409 round-trip validated" \
           -m "End-to-end upload validated on real 486DX2-66 hardware: CERBERUS \
posted CERBERUS.INI to barelybooting-server v0.1.3 via Cloudflare Tunnel, \
received submission ID + URL, run rendered in the public browse UI. No code \
changes between rc2 and final; this is a retag with the benchmark complete." \
           -m "Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"

# Rebuild CERBERUS.EXE on the dev box (Watcom).
wmake
# Commit the new binary if size changed.

git tag v0.7.0 -m "v0.7.0 — community upload milestone, real-hardware validated"
git push origin main
git push origin v0.7.0

gh release create v0.7.0 --title "v0.7.0 — community upload (final)" \
  --notes-file docs/releases/v0.7.0/release-notes.md \
  CERBERUS.EXE
```

---

## 2. GitHub release notes (v0.7.0 final)

Save this as `docs/releases/v0.7.0/release-notes.md` when shipping,
or pass inline via `gh release create --notes`:

```markdown
# CERBERUS v0.7.0 — community upload, final

Promoted from `v0.7.0-rc2` after the round-trip test on BEK-V409 (Intel i486DX2-66) successfully posted a real CERBERUS.INI through HTGET to barelybooting-server v0.1.3, received a submission ID and URL, and rendered the run in the public browse UI. No code changes between rc2 and final; this release tag marks the benchmark.

## Ecosystem milestone

CERBERUS now has a live companion server at **barelybooting.com/cerberus/**. Submit your hardware from a real-iron DOS run, get back a permanent browsable URL, compare your numbers against the community database.

Anonymous by default. Add `/NICK:<name>` and `/NOTE:"<text>"` to attribute. Opt out entirely with `/NOUPLOAD`. INI format frozen at `ini_format=1`; the server promises backward compatibility for every v0.7+ client forever.

## What's new since v0.6.2

- **Network transport detection.** NetISA, packet driver, mTCP, WATTCP, or none. Emitted as `[network] transport=...`. Real-mode DOS friendly (no interrupt vector overwrite, no TSR installed).
- **Community upload.** HTGET shell-out posts `CERBERUS.INI` to the server. Response is a two-line contract: 8-char submission ID, then the public browse URL. Both go back into the local INI in the `[upload]` section.
- **Upload metadata flags.** `/NICK:` (max 32 chars) and `/NOTE:` (max 128 chars). Both enforced client-side AND server-side.
- **`/NOUPLOAD` and `/UPLOAD` flags.** `/NOUPLOAD` suppresses the prompt entirely; `/UPLOAD` auto-yes for scripted runs.
- **UPLOAD STATUS summary pane** in the scrollable UI. Shows the submission ID, the URL, and the status code (`uploaded` / `offline` / `skipped` / `failed` / `bad_response` / `no_client`).
- **Frozen INI format.** `ini_format=1`. Server MUST accept any future v0.7+ submission without breaking; new fields are additive only.

## Companion server

[barelybooting-server v0.1.3](https://github.com/tonyuatkins-afk/barelybooting-server/releases/tag/v0.1.3) shipped alongside this. Python + Flask + SQLite, ~700 lines of code, deployed as a Docker container with a `cloudflared` sidecar on Home Assistant OS. Red-teamed across four reviewer passes before this tag: rate limiting, input validation, network isolation (internal-only Docker network), CI with `pip-audit`, pinned dependencies, dependency-drift alerts via Dependabot.

## Validation

- **201 host-side assertions** green (local Watcom + CI: 6 of 7 suites in Linux gcc; `test_diag_fpu` excluded in CI due to host-vs-target FP bit-exactness, tracked as [issue #8](https://github.com/tonyuatkins-afk/CERBERUS/issues/8)).
- **Real hardware validated** on BEK-V409 bench box: Intel i486DX2-66, AMI BIOS 11/11/92, S3 Trio64 VGA, SB AWE64 audio. End-to-end upload round trip confirmed.
- **Server contract tests** pass against this exact INI (captured into `barelybooting-server/tests/fixtures/bek_v409_rc2.ini`, asserted on every CI push).

## Download

`CERBERUS.EXE` attached. DOS real-mode, medium memory model, 8088 floor. Drop on any FAT16 volume, run from DOS prompt.

Known issues: GitHub Issues. `docs/ini-upload-contract.md` defines the server contract.
```

---

## 3. Public site updates

Apply these edits to `C:\Development\tonyuatkins-afk.github.io`:

### cerberus.html

Version strings across meta / OG / Twitter / Schema.org:
- `v0.7.0-rc2` → `v0.7.0` (everywhere)
- `0.7.0-rc2` → `0.7.0` (Schema.org softwareVersion, downloadUrl)

Status block paragraph:
> "**Where things stand:** `v0.7.0` shipped 2026-04-XX, community
> upload validated end-to-end on real 486 hardware. [...] The
> companion barelybooting-server v0.1.3 is live at
> `barelybooting.com/cerberus/`."

Download section:
- Link to `v0.7.0` release page instead of `v0.7.0-rc2`.

Links section:
- Add `v0.7.0 release page` entry.

### log.html

Prepend new entry (already drafted as HTML below). Before `entry-2026-04-21-barelybooting-server`.

```html
<article class="log-entry" id="entry-2026-04-2X-cerberus-v070-final">
    <div class="log-date">
        <time datetime="2026-04-2X">2026-04-2X</time>
    </div>
    <h2>CERBERUS v0.7.0 final: community upload end-to-end validated</h2>
    <p>Promoted <code>v0.7.0-rc2</code> to <code>v0.7.0</code> after the round-trip test on BEK-V409 (Intel i486DX2-66) posted a real <code>CERBERUS.INI</code> through HTGET to the <a href="https://github.com/tonyuatkins-afk/barelybooting-server">barelybooting-server</a> v0.1.3 instance at <a href="https://barelybooting.com/cerberus/">barelybooting.com/cerberus/</a>, received submission ID and URL, and rendered the run in the public browse UI. No code changes between rc2 and final; the tag marks the validation benchmark.</p>
    <p>Twelve tags across the arc: <code>v0.1.1-scaffold</code> through <code>v0.7.0</code>. Companion server lives on a Home Assistant mini PC behind a Cloudflare Tunnel; DOS clients post over plain HTTP per the frozen <code>ini_format=1</code> contract while modern browser viewers get TLS at the edge. A Cloudflare Worker fans out <code>/api/*</code> and <code>/cerberus/*</code> to the tunnel while leaving the landing page on GitHub Pages untouched.</p>
    <p>Contract integrity: server parses a real BEK-V409 INI (captured into the CI fixtures) on every push, so any future client-server drift fails loudly. 201 host-side CERBERUS assertions green; 41 server-side tests green. <a href="/cerberus.html">Project page</a>. <a href="https://github.com/tonyuatkins-afk/CERBERUS/releases/tag/v0.7.0">v0.7.0 release</a>.</p>
</article>
```

### sitemap.xml

Refresh `lastmod` on `cerberus.html` and `log.html` and `index.html`
to the deployment date (YYYY-MM-DD).

---

## 4. barelybooting-server post-deploy

### README.md

Add a banner near the top:

```markdown
**Deployment status:** Live at
[barelybooting.com/cerberus/](https://barelybooting.com/cerberus/)
since 2026-04-XX. Home Assistant OS + Docker + Cloudflare Tunnel;
see [DEPLOY.md](DEPLOY.md) for the stack details.
```

No code or version bumps expected on the server side for this
milestone. If a v0.7.0 validation finds a real bug in the server,
ship as v0.1.4.

---

## 5. Memory updates (Claude's side)

After everything ships, Claude (me) updates these memory files so
future sessions have current context:

- `project_cerberus.md`: bump status to v0.7.0 final, add deployment
  note.
- `project_barelybooting.md`: add "server live at barelybooting.com".
- `project_ha_host.md`: note the barelybooting-server stack is
  running there.

---

## Checklist

Copy this into a GitHub issue or the ship-day todo list:

- [ ] Cloudflare tunnel + Worker configured (DEPLOY.md step 2-5)
- [ ] `docker compose up -d` on HA box, health check returns 200
- [ ] BEK-V409 upload round trip: ID + URL returned, page renders
- [ ] `src/cerberus.h` version bumped to 0.7.0
- [ ] `CERBERUS.EXE` rebuilt with Watcom
- [ ] `CHANGELOG.md` prepended
- [ ] `README.md` status block updated
- [ ] Commit + tag `v0.7.0` + push
- [ ] `gh release create v0.7.0` with the notes above + EXE attached
- [ ] barelybooting-server `README.md` banner added
- [ ] Public site `cerberus.html` + `log.html` + `sitemap.xml`
      updated and pushed
- [ ] Memory files refreshed
