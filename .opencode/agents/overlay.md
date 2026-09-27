---
description: Corrige somente overlay Win32 layered compacto.
mode: primary
permission:
  "*": deny
  read: allow
  glob: allow
  grep: allow
  edit:
    "*": deny
    "src/overlay.h": allow
    "src/overlay.cpp": allow
    "/mnt/c/projetos/pessoais/albion-assistant-poc/src/overlay.h": allow
    "/mnt/c/projetos/pessoais/albion-assistant-poc/src/overlay.cpp": allow
  bash:
    "*": deny
    "git diff*": allow
    "git status*": allow
  question: deny
  task: deny
  external_directory: deny
---
Modifique apenas src/overlay.h e src/overlay.cpp. Não use nem altere main/CMake/capture. Sem commits. Compilação pelo coordenador.
