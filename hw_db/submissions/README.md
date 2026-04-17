# Hardware Submission Workflow

When CERBERUS encounters hardware it doesn't recognize (CPU, FPU, video chipset, audio chipset, or BIOS family), it writes a structured capture to `CERBERUS.UNK` and prompts the user to submit.

## How to submit

1. Find the `CERBERUS.UNK` file (same directory as `CERBERUS.INI`, or `%TEMP%` on read-only media)
2. Open a GitHub issue using the "hardware submission" template
3. Paste the contents of `CERBERUS.UNK`
4. Include any human-known identification (machine make/model/year, receipt-shelf CPU stepping, etc.)

## What happens next

A maintainer reviews the capture, adds a row to the appropriate `hw_db/*.csv`, and commits. The next CERBERUS release recognizes your hardware.

## What to do if you have the info yourself

Even better — open a pull request directly. Add a row to the relevant CSV, cite your source in the commit message (datasheet, InstLatx64 dump URL, sandpile.org page, VOGONS wiki entry, etc.), and submit. The Python build script validates the format.
