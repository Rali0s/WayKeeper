#!/usr/bin/env node

import crypto from "node:crypto";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { createRequire } from "node:module";

const scriptDir = path.dirname(fileURLToPath(import.meta.url));
const waykeeperRoot = path.resolve(scriptDir, "..");
const workspaceRoot = path.resolve(waykeeperRoot, "..");
const sourceFile = path.join(workspaceRoot, "amazing1-plasma-revival", "app", "catalog.ts");
const outputRoot = path.join(waykeeperRoot, "library", "segments", "Schematics");
const recordsRoot = path.join(outputRoot, "records");
const require = createRequire(import.meta.url);
const ts = require(path.join(workspaceRoot, "amazing1-plasma-revival", "node_modules", "typescript"));

const clean = (value) => String(value ?? "").replace(/[\t\r\n]+/g, " ").trim();
const slug = (value) => String(value).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "");
const hashFile = (filename) => crypto.createHash("sha256").update(fs.readFileSync(filename)).digest("hex");
const relativeToSegment = (filename) => path.relative(outputRoot, filename).split(path.sep).join("/");

if (!fs.existsSync(sourceFile)) throw new Error(`Missing catalog source: ${sourceFile}`);

const source = fs.readFileSync(sourceFile, "utf8");
const transpiled = ts.transpileModule(source, {
  compilerOptions: { module: ts.ModuleKind.CommonJS, target: ts.ScriptTarget.ES2022 },
  fileName: sourceFile,
}).outputText;
const moduleRecord = { exports: {} };
new Function("exports", "module", transpiled)(moduleRecord.exports, moduleRecord);
const { products } = moduleRecord.exports;

if (!Array.isArray(products) || products.length !== 94) {
  throw new Error(`Expected 94 Amazing1 records, found ${Array.isArray(products) ? products.length : "invalid export"}`);
}

const records = products.filter((product) => product.diagram !== "none");
const available = records.filter((product) => ["patent", "article", "worksheet", "trace"].includes(product.diagram));
const restricted = records.filter((product) => product.diagram === "restricted");
if (records.length !== 64 || available.length !== 19 || restricted.length !== 45) {
  throw new Error(`Inventory drift: expected 64/19/45, found ${records.length}/${available.length}/${restricted.length}`);
}

// These are candidates for a new benign teaching design, not approvals of the
// historical device. The original surveillance-oriented implementations remain
// restricted and no construction details are copied into this segment.
const redesignCandidates = new Map([
  ["HT9", {
    constructionRisk: "low-energy candidate",
    restrictionBasis: "privacy and consent",
    redesign: "Visible, local ultrasonic/audio receiver with no concealment, recording, or transmission.",
    residualRisk: "Use only with informed consent; respect local recording and privacy laws.",
  }],
  ["MFT1", {
    constructionRisk: "low-energy candidate",
    restrictionBasis: "privacy, consent, and radio compliance",
    redesign: "Clearly labeled bench audio transmitter operated into a dummy load or within a lawful short-range educational allocation.",
    residualRisk: "No covert use; transmitting may require authorization and interference controls.",
  }],
  ["MFT1K", {
    constructionRisk: "low-energy candidate",
    restrictionBasis: "privacy, consent, and radio compliance",
    redesign: "Clearly labeled bench audio-transmitter kit operated into a dummy load or within a lawful short-range educational allocation.",
    residualRisk: "No covert use; transmitting may require authorization and interference controls.",
  }],
  ["MFT3K", {
    constructionRisk: "reduced-power redesign candidate",
    restrictionBasis: "privacy, consent, advertised range, and radio compliance",
    redesign: "Replace the long-range concept with a reduced-power, non-radiating bench demonstration into a shielded dummy load.",
    residualRisk: "The historical range claim is not reproduced; no covert use or over-the-air long-range operation.",
  }],
]);
const candidateRecords = records.filter((record) => redesignCandidates.has(record.id));
if (candidateRecords.length !== redesignCandidates.size || candidateRecords.some((record) => record.diagram !== "restricted")) {
  throw new Error("Low-energy candidate list must resolve entirely to restricted catalog records");
}

fs.mkdirSync(recordsRoot, { recursive: true });
for (const existing of fs.readdirSync(recordsRoot)) {
  if (existing.endsWith(".md")) fs.unlinkSync(path.join(recordsRoot, existing));
}

const accessFor = (record) => record.diagram === "restricted" ? "restricted_metadata" : record.diagram === "trace" ? "trace_only" : "linked_source";
const trustFor = (record) => record.diagram === "patent" ? "1" : record.diagram === "article" ? "2" : record.diagram === "worksheet" ? "3" : "4";
const redistributionFor = (record) => record.diagram === "patent" ? "public-record-link" : record.diagram === "restricted" ? "metadata-only" : "link-only-unverified-redistribution";
const safetyFor = (record) => {
  if (record.diagram === "restricted") return "Operational circuit content excluded; preserve identity and provenance only.";
  if (["plasma", "tesla", "high-voltage", "fringe"].includes(record.category)) return "High voltage and stored energy can kill; source is archival and not a safe construction procedure.";
  if (record.category === "optics") return "Control optical exposure and never view an energized beam directly.";
  return "Historical reference; validate against current component, electrical, and safety standards.";
};

const conceptDiagram = `\n\`\`\`text\n[HISTORICAL DEVICE] -> [ENERGY / SIGNAL STAGE] -> [CATALOG EFFECT] -> [SAFETY BOUNDARY]\n\`\`\`\n\nThis is deliberately non-operational. Component values, winding data, trigger topology, layouts, targeting details, and construction steps are excluded.\n`;

const catalogRows = [[
  "id", "title", "category", "era", "access_status", "diagram_kind", "diagram_label",
  "evidence", "source_label", "source_url", "diagram_url", "record_path",
  "redistribution_status", "trust_tier", "safety_note", "redesign_candidate", "restriction_basis",
]];

for (const record of records) {
  const filename = `${slug(record.id)}.md`;
  const recordPath = path.join(recordsRoot, filename);
  const diagramUrl = record.diagramUrl ?? "";
  const candidate = redesignCandidates.get(record.id);
  const body = `# ${record.id} — ${record.name}\n\n` +
    `- Segment: Schematics\n` +
    `- Category: ${record.category}\n` +
    `- Era: ${record.era}\n` +
    `- Access status: ${accessFor(record)}\n` +
    `- Diagram class: ${record.diagram}\n` +
    `- Evidence: ${record.evidence}\n` +
    `- Trust tier: ${trustFor(record)}\n` +
    `- Redistribution: ${redistributionFor(record)}\n\n` +
    `## Catalog record\n\n${record.blurb}\n\n${record.spec}\n\n` +
    `## Diagram status\n\n${record.diagramLabel}\n\n` +
    (record.diagram === "restricted" ? conceptDiagram : "") +
    (candidate ? `## Low-energy redesign candidate\n\n- Construction classification: ${candidate.constructionRisk}\n- Original restriction basis: ${candidate.restrictionBasis}\n- Benign redesign boundary: ${candidate.redesign}\n- Residual caution: ${candidate.residualRisk}\n\nThe historical operational circuit remains restricted. This note identifies a possible clean-sheet educational substitute; it does not reclassify or reproduce the original device.\n\n` : "") +
    `## Provenance\n\n- Source: [${record.sourceLabel}](${record.sourceUrl})\n` +
    (diagramUrl ? `- Diagram trail: [Open external source](${diagramUrl})\n` : "- Diagram trail: No separate verified file URL.\n") +
    `\n## Safety note\n\n${safetyFor(record)}\n`;
  fs.writeFileSync(recordPath, body, "utf8");
  catalogRows.push([
    record.id, record.name, record.category, record.era, accessFor(record), record.diagram,
    record.diagramLabel, record.evidence, record.sourceLabel, record.sourceUrl, diagramUrl,
    relativeToSegment(recordPath), redistributionFor(record), trustFor(record), safetyFor(record),
    candidate ? "yes" : "no", candidate?.restrictionBasis ?? "",
  ]);
}

fs.mkdirSync(outputRoot, { recursive: true });
fs.writeFileSync(path.join(outputRoot, "catalog.tsv"), catalogRows.map((row) => row.map(clean).join("\t")).join("\n") + "\n");

const sourceMap = new Map();
for (const record of records) {
  const sourceUrl = record.diagramUrl || record.sourceUrl;
  const key = sourceUrl;
  const existing = sourceMap.get(key) ?? {
    id: `src-${sourceMap.size + 1}`,
    title: record.diagramLabel,
    format: record.diagram === "patent" ? "HTML/PDF patent record" : record.diagram === "article" ? "periodical PDF" : record.diagram === "worksheet" ? "archived PDF locator" : record.diagram === "trace" ? "archive trace" : "metadata source",
    source_url: sourceUrl,
    status: record.diagram === "restricted" ? "restricted-metadata" : record.diagram === "trace" ? "trace-unverified" : "external-linked",
    redistribution_status: redistributionFor(record),
    trust_tier: trustFor(record),
    linked_records: [],
  };
  existing.linked_records.push(record.id);
  sourceMap.set(key, existing);
}
const sources = [...sourceMap.values()];
const sourceRows = [["id", "title", "format", "source_url", "status", "redistribution_status", "trust_tier", "linked_records"]];
for (const item of sources) sourceRows.push([item.id, item.title, item.format, item.source_url, item.status, item.redistribution_status, item.trust_tier, item.linked_records.join(",")]);
fs.writeFileSync(path.join(outputRoot, "sources.tsv"), sourceRows.map((row) => row.map(clean).join("\t")).join("\n") + "\n");

const grouped = new Map();
for (const record of records) {
  const key = accessFor(record);
  const items = grouped.get(key) ?? [];
  items.push(record);
  grouped.set(key, items);
}
const index = [
  "# Schematics Segment Index",
  "",
  "Generated from the Information Unlimited / Amazing1 preservation catalog.",
  "",
  `- Total schematic-related records: ${records.length}`,
  `- Linked diagrams and traces: ${available.length}`,
  `- Restricted metadata records: ${restricted.length}`,
  "",
];
for (const [group, items] of grouped) {
  index.push(`## ${group}`, "");
  for (const item of items) index.push(`- [${item.id} — ${item.name}](records/${slug(item.id)}.md) — ${item.diagramLabel}`);
  index.push("");
}
fs.writeFileSync(path.join(outputRoot, "INDEX.md"), index.join("\n"), "utf8");

const candidateList = [
  "# Low-Energy Redesign Candidates",
  "",
  "These entries are candidates for clean-sheet, low-energy educational substitutes. They are not declarations that the historical devices are safe, lawful, or unrestricted, and they do not expose the original operational circuits.",
  "",
  `Candidate count: ${candidateRecords.length}`,
  "",
  "| ID | Historical entry | Construction classification | Why the original stays restricted | Benign redesign boundary |",
  "| --- | --- | --- | --- | --- |",
];
for (const record of candidateRecords) {
  const candidate = redesignCandidates.get(record.id);
  candidateList.push(`| [${record.id}](records/${slug(record.id)}.md) | ${record.name} | ${candidate.constructionRisk} | ${candidate.restrictionBasis} | ${candidate.redesign} |`);
}
candidateList.push(
  "",
  "## Exclusion rule",
  "",
  "Telephone-line interfaces, covert-use circuits, high-voltage and stored-energy devices, launchers, incapacitation devices, EMP/HERF and pulsed-power devices, and hazardous lasers are not candidates. A future buildable educational design must be independently engineered around consent, enclosure, current limiting, lawful spectrum use, and modern component safety; it must not be reconstructed from the restricted circuit.",
  "",
);
fs.writeFileSync(path.join(outputRoot, "LOW-ENERGY-CANDIDATES.md"), candidateList.join("\n"), "utf8");

const manifest = {
  schema_version: 1,
  segment_id: "schematics.amazing1-archive",
  segment_name: "Schematics",
  title: "Information Unlimited / Amazing1 Schematic Archive",
  generated_on: "2026-08-17",
  source_catalog: "../../../../amazing1-plasma-revival/app/catalog.ts",
  entrypoint: "INDEX.md",
  catalog: "catalog.tsv",
  sources: "sources.tsv",
  low_energy_candidates: "LOW-ENERGY-CANDIDATES.md",
  record_root: "records",
  counts: { total_records: records.length, linked_or_traced: available.length, restricted_metadata: restricted.length, low_energy_redesign_candidates: candidateRecords.length },
  content_policy: "Restricted records contain identity, provenance, and non-operational concept metadata only.",
  offline_content: "Metadata and record cards are local; third-party diagrams remain external links until rights and authenticity are verified.",
};
fs.writeFileSync(path.join(outputRoot, "segment.json"), JSON.stringify(manifest, null, 2) + "\n", "utf8");

const readme = `# WayKeeper Schematics Segment\n\nThis directory is a self-contained ingestion segment for the Information Unlimited / Amazing1 preservation archive.\n\n## Contents\n\n- \`segment.json\` — machine-readable segment manifest\n- \`catalog.tsv\` — 64 normalized schematic-related records\n- \`sources.tsv\` — deduplicated provenance and external-source table\n- \`INDEX.md\` — human-readable collection index\n- \`LOW-ENERGY-CANDIDATES.md\` — restricted entries suitable only for clean-sheet, low-energy educational redesign\n- \`records/\` — one offline-readable Markdown card per record\n- \`checksums.sha256\` — integrity hashes for every generated artifact\n\nThe 19 linked/traced records point to patents, a period article, an optics worksheet locator, or surviving file traces. The 45 restricted records preserve catalog identity and provenance only. Four are separately marked as possible clean-sheet, low-energy educational redesigns; their original covert circuits remain restricted. No operational EMP/HERF, weapon, incapacitation, covert-surveillance, dangerous pulsed-power, coil-launcher, or Class IV laser circuit is stored here.\n\nRebuild from the site catalog with:\n\n\`\`\`sh\nnode scripts/build_amazing1_schematics_segment.mjs\n\`\`\`\n`;
fs.writeFileSync(path.join(outputRoot, "README.md"), readme, "utf8");

const checksumTargets = [];
for (const filename of ["README.md", "INDEX.md", "LOW-ENERGY-CANDIDATES.md", "catalog.tsv", "sources.tsv", "segment.json"]) checksumTargets.push(path.join(outputRoot, filename));
for (const filename of fs.readdirSync(recordsRoot).filter((name) => name.endsWith(".md")).sort()) checksumTargets.push(path.join(recordsRoot, filename));
const checksums = checksumTargets.map((filename) => `${hashFile(filename)}  ${relativeToSegment(filename)}`).join("\n") + "\n";
fs.writeFileSync(path.join(outputRoot, "checksums.sha256"), checksums, "utf8");

console.log(`Wrote ${records.length} records (${available.length} linked/traced, ${restricted.length} restricted) to ${outputRoot}`);
