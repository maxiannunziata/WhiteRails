#ifndef SCHEMA_H
#define SCHEMA_H

// Minified SERVICE_SCHEMA for validating service JSON files
static const char *SERVICE_SCHEMA = "{\"type\":\"object\",\"required\":[\"name\",\"actions\",\"condition\"],\"properties\":{\"name\":{\"type\":\"string\"},\"interval\":{\"type\":\"integer\"},\"condition\":{\"type\":\"string\"},\"actions\":{\"type\":\"array\",\"items\":{\"type\":\"object\",\"required\":[\"type\"],\"properties\":{\"type\":{\"type\":\"string\"},\"path\":{\"type\":\"string\"},\"command\":{\"type\":\"string\"},\"message\":{\"type\":\"string\"}}}}}}";

#endif // SCHEMA_H
