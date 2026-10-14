-- İHA YKİ simülatörü: ilk şema.

-- Bir İHA'nın tek bir kalkıştan inişe uçuşu (sortie).
CREATE TABLE sortie (
    id          TEXT PRIMARY KEY,
    uav_id      INTEGER          NOT NULL,
    callsign    TEXT             NOT NULL,
    uav_type    TEXT             NOT NULL,
    started_at  TIMESTAMPTZ      NOT NULL DEFAULT now(),
    ended_at    TIMESTAMPTZ,
    final_phase TEXT
);

CREATE TABLE telemetry (
    id              BIGSERIAL PRIMARY KEY,
    sortie_id       TEXT             NOT NULL REFERENCES sortie (id) ON DELETE CASCADE,
    recorded_at     TIMESTAMPTZ      NOT NULL,
    sim_time_s      DOUBLE PRECISION NOT NULL,
    latitude        DOUBLE PRECISION NOT NULL CHECK (latitude BETWEEN -90 AND 90),
    longitude       DOUBLE PRECISION NOT NULL CHECK (longitude BETWEEN -180 AND 180),
    altitude_m      DOUBLE PRECISION NOT NULL,
    groundspeed_mps DOUBLE PRECISION NOT NULL,
    heading_deg     DOUBLE PRECISION NOT NULL,
    battery_pct     DOUBLE PRECISION NOT NULL,
    link_quality    DOUBLE PRECISION NOT NULL,
    phase           TEXT             NOT NULL
);

CREATE INDEX idx_telemetry_sortie_time ON telemetry (sortie_id, recorded_at);

CREATE TABLE alert (
    id         BIGSERIAL PRIMARY KEY,
    sortie_id  TEXT,
    uav_id     INTEGER          NOT NULL,
    callsign   TEXT             NOT NULL,
    type       TEXT             NOT NULL,
    severity   TEXT             NOT NULL,
    message    TEXT             NOT NULL,
    sim_time_s DOUBLE PRECISION NOT NULL,
    raised_at  TIMESTAMPTZ      NOT NULL
);

CREATE INDEX idx_alert_raised_at ON alert (raised_at DESC);
CREATE INDEX idx_alert_sortie ON alert (sortie_id);
