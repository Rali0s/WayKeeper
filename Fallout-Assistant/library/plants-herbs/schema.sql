PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS schema_metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

INSERT INTO schema_metadata(key, value) VALUES ('schema_version', '1')
ON CONFLICT(key) DO UPDATE SET value = excluded.value;

CREATE TABLE IF NOT EXISTS source_document (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    publisher TEXT NOT NULL,
    publication_year INTEGER,
    trust_tier INTEGER NOT NULL CHECK (trust_tier BETWEEN 1 AND 4),
    category TEXT NOT NULL,
    scope TEXT NOT NULL,
    source_url TEXT NOT NULL,
    license_note TEXT NOT NULL,
    safety_role TEXT NOT NULL,
    local_pdf TEXT NOT NULL,
    local_text TEXT NOT NULL,
    sha256 TEXT NOT NULL,
    page_count INTEGER NOT NULL CHECK (page_count > 0),
    accessed_on TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS document_page (
    document_id TEXT NOT NULL REFERENCES source_document(id) ON DELETE CASCADE,
    page_number INTEGER NOT NULL CHECK (page_number > 0),
    page_text TEXT NOT NULL,
    PRIMARY KEY (document_id, page_number)
);

CREATE VIRTUAL TABLE IF NOT EXISTS document_page_fts USING fts5(
    document_id UNINDEXED,
    page_number UNINDEXED,
    page_text,
    tokenize = 'unicode61 remove_diacritics 2'
);

CREATE TABLE IF NOT EXISTS plant (
    id INTEGER PRIMARY KEY,
    scientific_name TEXT NOT NULL UNIQUE,
    family TEXT,
    genus TEXT,
    species TEXT,
    taxonomic_authority TEXT,
    verification_status TEXT NOT NULL DEFAULT 'unreviewed'
        CHECK (verification_status IN ('unreviewed', 'botanically_reviewed', 'medical_reviewed', 'retired')),
    emergency_use_allowed INTEGER NOT NULL DEFAULT 0 CHECK (emergency_use_allowed IN (0, 1)),
    notes TEXT
);

CREATE TABLE IF NOT EXISTS plant_name (
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    name_type TEXT NOT NULL CHECK (name_type IN ('common', 'scientific_synonym', 'pharmacopoeial', 'indigenous', 'trade')),
    language TEXT,
    region TEXT,
    source_document_id TEXT REFERENCES source_document(id),
    PRIMARY KEY (plant_id, name, name_type)
);

CREATE TABLE IF NOT EXISTS plant_part (
    id INTEGER PRIMARY KEY,
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    part_name TEXT NOT NULL,
    preparation TEXT,
    UNIQUE (plant_id, part_name, preparation)
);

CREATE TABLE IF NOT EXISTS evidence_statement (
    id INTEGER PRIMARY KEY,
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    plant_part_id INTEGER REFERENCES plant_part(id) ON DELETE SET NULL,
    statement_type TEXT NOT NULL CHECK (statement_type IN (
        'identification', 'traditional_use', 'clinical_use', 'unsupported_claim',
        'preparation', 'dose', 'adverse_effect', 'contraindication', 'warning', 'interaction'
    )),
    statement TEXT NOT NULL,
    evidence_level TEXT NOT NULL CHECK (evidence_level IN (
        'regulatory_label', 'clinical_evidence', 'pharmacopoeial', 'traditional_use',
        'preclinical_only', 'insufficient_evidence', 'safety_signal'
    )),
    source_document_id TEXT NOT NULL REFERENCES source_document(id),
    source_page INTEGER NOT NULL CHECK (source_page > 0),
    source_section TEXT,
    reviewer TEXT,
    reviewed_on TEXT,
    emergency_use_allowed INTEGER NOT NULL DEFAULT 0 CHECK (emergency_use_allowed IN (0, 1))
);

CREATE TABLE IF NOT EXISTS hazard (
    id INTEGER PRIMARY KEY,
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    hazard_type TEXT NOT NULL CHECK (hazard_type IN (
        'toxic_part', 'contact', 'phototoxic', 'allergy', 'pregnancy', 'pediatric',
        'liver', 'kidney', 'bleeding', 'sedation', 'contamination', 'misidentification', 'other'
    )),
    severity TEXT NOT NULL CHECK (severity IN ('caution', 'serious', 'potentially_fatal')),
    description TEXT NOT NULL,
    source_document_id TEXT NOT NULL REFERENCES source_document(id),
    source_page INTEGER NOT NULL CHECK (source_page > 0)
);

CREATE TABLE IF NOT EXISTS interaction (
    id INTEGER PRIMARY KEY,
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    medicine_or_class TEXT NOT NULL,
    interaction_summary TEXT NOT NULL,
    evidence_level TEXT NOT NULL,
    source_document_id TEXT NOT NULL REFERENCES source_document(id),
    source_page INTEGER NOT NULL CHECK (source_page > 0)
);

CREATE TABLE IF NOT EXISTS lookalike (
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    lookalike_plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    distinguishing_features TEXT NOT NULL,
    consequence TEXT NOT NULL,
    source_document_id TEXT NOT NULL REFERENCES source_document(id),
    source_page INTEGER NOT NULL CHECK (source_page > 0),
    PRIMARY KEY (plant_id, lookalike_plant_id)
);

CREATE TABLE IF NOT EXISTS geographic_presence (
    plant_id INTEGER NOT NULL REFERENCES plant(id) ON DELETE CASCADE,
    region TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('native', 'introduced', 'cultivated', 'reported', 'unknown')),
    source_document_id TEXT NOT NULL REFERENCES source_document(id),
    source_page INTEGER NOT NULL CHECK (source_page > 0),
    PRIMARY KEY (plant_id, region, source_document_id)
);

CREATE INDEX IF NOT EXISTS evidence_by_plant ON evidence_statement(plant_id, statement_type);
CREATE INDEX IF NOT EXISTS hazards_by_plant ON hazard(plant_id, severity);
CREATE INDEX IF NOT EXISTS interactions_by_plant ON interaction(plant_id);

