# WAYKEEPER PDF-TO-CARD WORKFLOW

This directory defines the controlled path from an offline PDF passage to a
small WayKeeper ANSI card. Generated prose is never published directly as
reviewed survival guidance.

The publishing rule is:

```text
PDF -> PAGE-BOUND TEXT -> CANDIDATE -> SOURCE CHECK -> SAFETY CHECK
    -> HUMAN REVIEW -> REVIEWED CARD -> RUNTIME INDEX
```

## 1. Select sources

Use the source allow-list in the active phase tracker. Prefer current material
from government agencies, standards bodies, universities, and recognized
medical or humanitarian organizations. Treat a document as reference-only when
it is fictional, spiritual, occult, historical, primarily narrative, legally
restricted, obsolete, or too hazardous to summarize safely.

No document is trusted wholesale. One unsafe or obsolete passage is enough to
exclude that passage without excluding the entire document.

## 2. Establish source identity

Before drafting a card, record:

- the `library/catalog.tsv` document ID;
- the repository-relative PDF path;
- the PDF's SHA-256 checksum;
- the printed or PDF page that supports the card;
- the source publication date when known; and
- a current official cross-check URL for time-sensitive or safety-critical
  claims.

PDF page numbers in card metadata mean physical PDF pages unless a card
explicitly says `printed page`. This keeps `:GOTO <page>` deterministic.

## 3. Inspect the complete source page

Read the full extracted page and render the page to an image. Check headings,
captions, tables, warnings, neighboring bullets, footnotes, and whether the
passage continues on another page. Never build a card from a search-result
fragment alone.

## 4. Draft one decision per card

A card should answer one field question or guide one bounded action. It should
fit the 640x480 fixed reader without depending on an image.

Required content:

1. A direct instruction or identification rule.
2. The condition in which it applies.
3. A `LIMITS` section describing when it does not apply or when to defer to
   local authorities or professional help.
4. A `SOURCE NOTE` naming the exact PDF page and cross-check status.

Use plain language and paraphrase the source. Do not copy substantial book text.
Retain exact numbers only when the source and current cross-check agree.

## 5. Classify risk

Use the highest applicable risk:

- `low`: packing, navigation, organization, identification without treatment.
- `moderate`: sheltering, sanitation, food handling, exposure prevention.
- `high`: medical treatment, radiation, chemicals, electrical work, fire,
  weapons, dosage, or any instruction where an error can cause serious harm.

High-risk candidates require two compatible authoritative sources and an
explicit escalation instruction before review.

## 6. Validate mechanically

Run:

```sh
python3 scripts/validate_card_candidates.py
```

The validator checks required metadata, unique IDs, source existence, source
hashes, page ranges, candidate review state, and required ANSI sections. A clean
validator result does not mean the card is medically or operationally correct.

## 7. Review and promote

Review uses four gates:

1. **Evidence:** the cited page supports every material claim.
2. **Currency:** current official guidance does not conflict with the card.
3. **Safety:** limitations and escalation language cover foreseeable misuse.
4. **ANSI:** the card is concise, readable, and searchable at 79 columns.

Promotion requires a named human reviewer. Change `status` to `reviewed`, add a
review date, and move the file from `cards/candidates/<phase>/` into the reviewed
card source directory. Only reviewed cards may enter the runtime index.

## 8. Reject, merge, or retain as reference

- Mark `rejected` when guidance is unsafe, contradicted, unverifiable, or too
  dependent on missing context.
- Merge candidates that answer the same field decision.
- Leave useful long-form material in the Library rather than forcing it into a
  card.

## Definition of done

A phase is complete when its target number of reviewed cards has passed all
four gates, all rejected candidates have a recorded reason, the validator and
tests pass, and the card index can be regenerated deterministically.
