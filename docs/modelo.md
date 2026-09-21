# HUDs, status e sets

## Contrato do produto

HUD configura o computador: resolução, DPI, monitor e regiões nomeadas, com escala do ícone nas regiões de leitura. Status são uma biblioteca compartilhada de buffs/debuffs com ID estável, nome, referência e amostras rotuladas de stacks. Sets possuem listas de regras: status + região de origem + presença/ausência/stacks + borda/brilho/cor + região de destino.

Uma HUD não carrega nem apaga regras. Um set não muda coordenadas. Regiões com o mesmo nome em HUDs diferentes permitem reutilizar o mesmo set. Nomes de regiões são únicos na HUD; nomes de entidades são únicos em sua categoria. Referências entre regras e status usam IDs. Regra sem dependência/calibração válida bloqueia início com explicação. O primeiro destaque verdadeiro da lista vence quando duas regras usam o mesmo destino.

Condição de stacks oferece somente valores cadastrados para aquele status. Contador ilegível permanece desconhecido. Presença/ausência não exige amostras de stacks. O preset existente de Espírito Assassino é preservado pela migração, mas não define os rótulos da interface nem as opções dos outros status.

## Persistência e migração

`workspace.ini` em AppData guarda as três coleções, gravado atomicamente com schema explícito. Os arquivos legados permanecem intactos. Na primeira abertura, importar a configuração ativa e HUDs anteriores, preservando regras divergentes em sets distintos. Contadores 2/3 usados implicitamente pelo legado tornam-se amostras explícitas dos status personalizados importados; novos cadastros não recebem essas amostras. A existência do arquivo novo impede reimportação, inclusive depois de excluir todas as HUDs. `--settings` continua isolando dados de teste na pasta escolhida.

## Critérios de aceite

1. Criar, salvar, selecionar e excluir HUDs, inclusive a última, sem alterar status/sets ou ressuscitá-la após reiniciar.
2. Mesmo set em duas HUDs com coordenadas distintas; troca de set preserva HUD, troca de HUD preserva set.
3. Biblioteca genérica com captura de referência e amostras de stacks; condições presença, ausência e stacks cadastrados.
4. Duas ou mais regras/status simultâneos com observações independentes; parar/expirar/fonte inválida apaga ações. Borda e brilho configuráveis, com teste visual separado da leitura.
5. Configuração anterior migrada sem sobrescrita; um único EXE; revisores independentes e limites de validação documentados.

## Implementação

- [x] Persistência: `workspace.h/.cpp`, testes `workspace_tests.cpp`. Importar tipos antigos para o modelo separado; validar IDs, nomes, limites e referências; exclusões e gravação atômica.
- [x] Visão: `recognition.h/.cpp` e testes. Contadores rotulados por status, sem herança oculta do preset. Manter a regressão dos 22 recortes reais.
- [x] Aplicação: separar páginas Monitor, HUDs, Status, Sets/regras; runtime com captura de uma ROI agregada e reconhecimento uma vez por par status/região/frame, ações independentes. Manter fluxo de seleção de dois cliques e demonstração de 5 segundos.
- [x] Verificação: migração/exclusão, alternância de HUD/set, múltiplas regras, expiração e colisão de destino, rótulos genéricos, layout/DPI. Produto e QA aprovaram o diff; 13/13 testes em Release e Debug e único artefato conferido. Teste real da nova interface/jogo/monitor permanece pendente em `validacao.md`.

Base: `0eb22d5`. Produto/arquitetura: `/root/product_validator` aprovou o desenho antes da implementação. Cada agente edita somente arquivos designados; commits e builds integrados são do coordenador.
