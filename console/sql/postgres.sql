CREATE TYPE operating_system AS
(
    machine TEXT,
    version TEXT,
    system TEXT,
    release TEXT
);

CREATE TABLE agent
(
    id TEXT PRIMARY KEY,
    last TIMESTAMPTZ NOT NULL,
    host TEXT,
    ip_addresses TEXT[],
    os operating_system
);

CREATE TABLE in_capability
(
    agent_id TEXT REFERENCES agent PRIMARY KEY,
    name TEXT NOT NULL,
    version INT NOT NULL
);

CREATE TABLE out_capability
(
    agent_id TEXT REFERENCES agent PRIMARY KEY,
    name TEXT NOT NULL,
    version INT NOT NULL,
    configs TEXT[] NOT NULL
);
