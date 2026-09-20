---
description: Implementa somente captura DXGI nativa para a POC Albion.
mode: primary
permission:
  "*": deny
  read: allow
  glob: allow
  grep: allow
  edit:
    "*": deny
    "src/capture.h": allow
    "src/capture.cpp": allow
    "/mnt/c/projetos/pessoais/albion-assistant-poc/src/capture.h": allow
    "/mnt/c/projetos/pessoais/albion-assistant-poc/src/capture.cpp": allow
  bash:
    "*": deny
    "git diff*": allow
    "git status*": allow
    "pwd": allow
  todowrite: allow
  question: deny
  task: deny
  external_directory: deny
---
Implemente somente src/capture.h e src/capture.cpp. Não modifique arquivos de outros agentes, não commit/push, não controle o jogo. O coordenador fará compilação MSVC e testes após sua implementação.
