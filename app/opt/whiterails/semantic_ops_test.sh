#!/usr/bin/env sh
set -e
echo "Test 1 – Natural language ➜ list files"
printf '{"query":"muéstrame el contenido de /etc"}' | \
  curl -s -XPOST http://localhost:8080/prompt -d @- | jq .

echo "Test 2 – Natural language ➜ crear carpeta"
printf '{"query":"crea una carpeta /tmp/demo_llm"}' | \
  curl -s -XPOST http://localhost:8080/prompt -d @- | jq .
