CREATE TABLE agent
(
    id TEXT PRIMARY KEY,
    last TIMESTAMPTZ NOT NULL,
    host TEXT,
    machine TEXT,
    operating_system TEXT,
    os_version TEXT,
    os_release TEXT
);

CREATE TABLE ip_address
(
    id SERIAL PRIMARY KEY,
    agent_id TEXT REFERENCES agent,
    address TEXT NOT NULL
)

CREATE TABLE in_capability
(
    id SERIAL PRIMARY KEY,
    agent_id TEXT REFERENCES agent,
    name TEXT NOT NULL,
    version INT NOT NULL
);

CREATE TABLE out_capability
(
    id SERIAL PRIMARY KEY,
    agent_id TEXT REFERENCES agent,
    name TEXT NOT NULL,
    version INT NOT NULL
);

CREATE TABLE configuration
(
    id SERIAL PRIMARY KEY,
    out_cap_id INT REFERENCES out_capability,
    name TEXT NOT NULL
);

