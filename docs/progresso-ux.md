# Progresso da reformulação de usabilidade

Atualizado em 22/09/2026.

| Entrega | Situação | Evidência |
|---|---|---|
| Separar stacks e relógio por regra | Concluída | Commit `5ad65cf`; 15/15 testes passaram. |
| Ordem das abas: HUDs → Status → Perfis e regras → Monitorar | Concluída | Commit `5a5c4cd`; 15/15 testes passaram. |
| Nome antes de criar e edição automática | Concluída neste fluxo | Usuário revisou o fluxo: primeiro nome inline; itens adicionais com Criar/Cancelar. 15/15 testes e teste manual de HUD. |
| Estados vazios dos cadastros | Concluída | HUDs, áreas, status, perfis e regras orientam criação; pendências do Monitorar ainda faltam. |
| Editor Quando → Então → Testar | Pendente | Inclui capturar, substituir e excluir amostras por regra. |
| Monitorar com botão Resolver | Pendente | Depende de pendências estruturadas na interface. |
| Revisão visual e teste funcional no jogo | Pendente | Executar somente após a interface estar completa. |

## Onde acompanhar

- Código: branch `feat/interface-guiada`.
- Histórico: `git log --oneline` na pasta do projeto.
- Executável em teste: `dist/AlbionAssistant.exe`.
- Resultado do último build: `build/logs/Release-validation.json`.
- Plano detalhado: `docs/superpowers/plans/2026-09-22-ux-reconfiguration.md`.

Cada entrega só é marcada como concluída depois de passar pelo build Windows, que compila, executa os 15 testes e publica o executável único em `dist`.
