# CERBERUS ↔ Server upload contract

**Scope**: This document is the API contract between the DOS upload
client (CERBERUS v0.7.0+, this repo) and the results-browser server
(separate repo, TBD). It is the spec Part B must implement.

**Status**: Draft, pre-Part-B-kickoff. Signed off: nobody yet. Will be
frozen when Part B session begins.

## Endpoint

```
POST /api/v1/submit
Host: barelybooting.com
Content-Type: text/plain
Content-Length: <byte length of INI body>
X-Cerberus-Version: 0.7.0
X-Hardware-Signature: <value of [cerberus] signature key>
```

Request body: the raw contents of `CERBERUS.INI` as written by
`report_write_ini()`, byte-for-byte. No compression, no encoding. Typical
size: 4-8 KB.

### Client behaviour

- Single POST per run. No retries, no queue.
- Connection timeout: 10 seconds.
- Response timeout: 10 seconds.
- Failure modes (no network, DNS fail, connection refused, non-200,
  timeout) all result in `[upload] status=failed` in the local INI.
  The binary never crashes on upload failure.

## Response

### Success (HTTP 200)

```
HTTP/1.0 200 OK
Content-Type: text/plain
Content-Length: <len>

a1b2c3d4
https://barelybooting.com/cerberus/run/a1b2c3d4
```

Body: exactly two lines.
- Line 1: 8-character hex submission ID. Unique per submission.
- Line 2: public URL to view this submission.

Client parses these two lines and displays them in the scrollable
summary's UPLOAD STATUS section.

### Failure

Any non-200 response, connection refused, or timeout is treated as
"upload failed." The client does not parse body content on failure; it
emits `[upload] status=failed` and continues.

Expected non-200 cases the server may emit:
- `400` — malformed INI (ini_format mismatch or parse error)
- `413` — body too large (server MUST accept bodies up to 64 KB)
- `429` — rate limited (server decision; client does not retry)
- `500` — server internal error

The client does not distinguish these; any non-200 is just "failed."

## INI fields the server parses

### [cerberus] — run metadata (required)

```
version=0.7.0
schema_version=1.0
signature_schema=1
ini_format=1
mode=quick
runs=1
signature=ab12cd34
results=87
run_signature=<40-char-sha1-or-16-char-prefix>
```

- `ini_format` is the server's switch: use `=1` parser for v0.7.0+
  submissions. Additive INI changes do NOT bump `ini_format`; only
  breaking changes do.
- `signature` is the 8-char hardware-identity hash. Multiple runs on
  the same machine share this. Primary aggregation key.
- `run_signature` is the per-run unique hash. Primary row key.
- `version` is the CERBERUS client version. Server MUST accept any
  version ≥ `0.7.0` (the version this contract was introduced).

### [network] — transport detected

```
transport=pktdrv
```

Value is one of: `netisa`, `pktdrv`, `mtcp`, `wattcp`, `none`. The
server may log this for debugging but does not use it for primary
classification.

### [upload] — user-provided metadata (optional)

```
nickname=tony
notes=BEK-V409 with fresh BIOS
status=uploaded
submission_id=a1b2c3d4
```

- `nickname` and `notes` populated from `/NICK` and `/NOTE` flags.
  Both empty-string when not provided. Max lengths: nickname 32,
  notes 128. Client enforces.
- `status` reflects upload outcome written by the CLIENT after POST:
  `uploaded`, `failed`, `skipped` (user declined), `offline` (no
  network). Server ignores this field on inbound (it describes the
  client's own state).
- `submission_id` is populated by the CLIENT after a successful POST
  with the value returned in response line 1. Server ignores on
  inbound; useful if the user later re-examines the local INI.

### [environment], [cpu], [fpu], [memory], [cache], [bus], [video], [audio], [bios], [diagnose], [bench], [consistency]

These are the detection / diagnosis / benchmark / consistency
sections. The server is free to parse any or all of these for its
browse UI, aggregation, or comparison features. Exact keys are
documented in `docs/ini-format.md`. The server MUST tolerate unknown
keys (additive compatibility).

## Server-side commitments (what Part B must do)

1. **Accept any `ini_format=1` submission from any `version≥0.7.0`
   client.** Additive new keys in later CERBERUS versions must not
   cause 400s.
2. **Return the two-line success response format** exactly as
   specified. If the server wants to add more lines in the future,
   it must do so via a `v2` endpoint (`/api/v2/submit`) leaving v1
   semantics intact.
3. **Generate an 8-char hex submission ID per POST.** Uniqueness
   guaranteed within the submission database.
4. **Persist the raw INI body.** The server may parse it for indexing
   but must keep the original bytes available for re-parse or
   download. This matters for Homage-style archival — submissions
   are research data.
5. **Expose `/cerberus/run/<id>`** for the submitter to view their
   submission. Public read by default; no auth required to view.
6. **Honor the 64 KB body limit.** CERBERUS INIs are typically 4-8 KB;
   allow headroom for future growth.

## Versioning

- `ini_format=1` is the v0.7.0 baseline and stays through all v0.x.x
  client releases with additive changes.
- If CERBERUS ever needs to break format (remove a key, rename a
  section), it bumps `ini_format=2` and the server adds a parallel
  v2 parser. Old submissions in `ini_format=1` continue to work.
- The HTTP endpoint path `/api/v1/submit` stays for `ini_format=1`
  clients forever. New ini_formats get new endpoint paths.

## What this contract does NOT cover

- Authentication / authorization. v0.7.0 is anonymous-submit. If a
  future version adds auth, it's a separate contract.
- Compare / aggregate APIs. The server's public browse UI surface is
  entirely Part B's concern and not specified here.
- HTTPS. v0.7.0 is HTTP/1.0 over port 80. NetISA's TLS path is
  reserved for v0.8.0+.
- Upload queuing or offline buffering. Each run attempts one POST;
  failures leave the INI locally and the user re-runs when the
  network is up.

## Changelog for this contract

- 2026-04-20 — draft written alongside v0.7.0 Part A implementation.
  Part B session inherits this as a requirements spec.
