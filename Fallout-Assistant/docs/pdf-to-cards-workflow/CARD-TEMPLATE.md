# WAYKEEPER CARD TEMPLATE

Copy the block below into `cards/candidates/<phase>/<id>.md`. Use lowercase
dot-separated IDs and repository-relative source paths.

```markdown
---
schema: waykeeper-card-v1
id: topic.action
title: Short field-readable title
status: candidate
reviewed: null
risk: low
trust: official-government-guidance
source_doc_id: catalog-document-id
source_pdf: library/pdfs/path/to/source.pdf
source_pages: 1
source_sha256: 64-lowercase-hex-characters
source_published: YYYY-MM-DD-or-YYYY
cross_check_url: https://current.official.example/guidance
cross_checked: YYYY-MM-DD
tags: comma, separated, lowercase
aliases: plain phrases, a user may type
---

# SHORT FIELD-READABLE TITLE

Give one direct instruction or one bounded identification rule. State the
condition that makes it applicable. Keep numbers and units unambiguous.

## LIMITS

State when the instruction does not apply, what it cannot accomplish, and when
to follow authorities or obtain professional help.

## SOURCE NOTE

Supported by `source_doc_id`, physical PDF page 1. Current official guidance was
checked at `cross_check_url` on `cross_checked`.
```

## Promotion changes

Only a named human reviewer promotes a candidate:

```yaml
status: reviewed
reviewed: YYYY-MM-DD
reviewer: reviewer-name-or-team
```

Rejected cards use:

```yaml
status: rejected
reviewed: null
rejection_reason: concise reason
```

Do not use `reviewed` for an AI-only or mechanical validation pass.
