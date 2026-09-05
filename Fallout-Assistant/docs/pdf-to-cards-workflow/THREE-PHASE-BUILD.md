# PDF-TO-CARD THREE-PHASE BUILD

## Phase 1 - Pilot and proof of control

**Target:** 25-50 reviewed cards from 3-5 authoritative operational PDFs.

**Purpose:** Prove exact page citations, compact ANSI writing, mechanical
validation, current-source conflict checking, review states, and runtime search
behavior before scaling.

**Initial source lanes:** emergency supplies, shelter-in-place, poisonous-plant
exposure prevention, and similarly bounded public-safety topics. Medical
treatment, radiation dosage, herbal treatment, weapons, and improvised
electrical repair remain excluded from the first promotion batch.

**Deliverables:**

- card schema and reusable template;
- candidate directory isolated from reviewed cards;
- 25-card initial candidate batch;
- source-page and checksum validator;
- review tracker with accept, revise, merge, and reject states;
- no runtime publication before named human review.

**Estimated review:** 8-20 hours for 25-50 cards.

**Size:** approximately 100-300 KiB for cards and metadata; under 2 MiB with
indexes and reports.

**Exit gate:** At least 25 cards pass evidence, currency, safety, and ANSI review
with zero unresolved high-risk claims.

## Phase 2 - Operational library

**Target:** 200-400 reviewed cards.

**Purpose:** Cover the common offline decisions WayKeeper should answer quickly:
water, sanitation, shelter, weather, food safety, first-response boundaries,
navigation, communications, power safety, fire prevention, field maintenance,
and recovery logistics.

**Build method:** Process sources in 25-50-card batches. Each batch receives a
source allow-list, candidate generation, conflict scan, human review, ANSI test,
and deterministic index build. No batch inherits trust from an earlier batch.

**Engineering work:** Replace the hard-coded C++ card vector and manual keyword
branches with a data-driven reviewed-card loader, aliases, compact search index,
and regression tests. Candidate and rejected cards remain unavailable in the
F1 Cards and Guide surfaces.

**Estimated review:** 40-120 total human hours, depending on risk mix.

**Size:** approximately 0.5-1.6 MiB of cards and 5-15 MiB including indexes,
aliases, validation reports, and optional source snippets.

**Exit gate:** 200 or more reviewed cards, complete topic coverage report,
runtime tests passing on desktop and ARM64/QEMU, and no source page that cannot
be opened from its card.

## Phase 3 - Mature total

**Target:** approximately 400-900 reviewed cards. The target is coverage, not a
quota; the mature set may be smaller when the remaining corpus is narrative,
duplicative, obsolete, or unsafe.

**Purpose:** Add specialized regional, agriculture, repair, communications,
medical-boundary, and recovery cards while retaining long manuals as references.

**Controls:** High-risk cards require two authoritative sources, named review,
version history, and scheduled re-review. Regulations and time-sensitive
guidance carry an expiry or `verify-current` flag. Copyrighted works are
paraphrased and cited rather than redistributed as extracts.

**Estimated review:** 70-300 cumulative human hours.

**Size:** approximately 1-4 MiB of cards and 10-25 MiB with the complete search
and provenance layer. Page thumbnails or diagrams may add 50-250 MiB and should
be optional. The source PDFs remain the dominant Full-edition cost at roughly
849 MiB.

**Exit gate:** Topic coverage is documented; every live card has source,
checksum, page, reviewer, and review date; stale cards fail closed; the Lite
edition can operate without bundling the full PDF corpus.

## Batch numbering

Use directories and trackers in this form:

```text
cards/candidates/phase-1/
cards/candidates/phase-2-batch-01/
cards/candidates/phase-2-batch-02/
cards/candidates/phase-3-batch-01/
```

Each batch is independently reviewable, reversible, and publishable.
