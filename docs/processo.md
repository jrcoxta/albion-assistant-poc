# Processo de entrega

Uma aplicação, um executável e histórico no Git. Cada mudança passa por critérios de aceite, implementação e conferência independente. O coordenador organiza o ciclo; a aprovação não depende apenas de quem escreveu o código.

## Papéis

| Papel | Responsabilidade e saída |
|---|---|
| Produto (PO) | Traduzir o pedido em 3–5 critérios observáveis, incluindo o que não faz parte da entrega. Conferir se o resultado resolve o pedido. |
| Liderança técnica | Conferir a abordagem e o diff: integração com o programa existente, simplicidade, dados preservados, tratamento de falhas e uma única distribuição. |
| Desenvolvimento | Implementar a menor mudança que atende aos critérios, reproduzir bugs e executar testes. Corrigir achados; não dar a própria aprovação final. |
| QA | Conferir critérios por caminhos de sucesso e falha. Executar verificações, inspecionar o EXE entregue e registrar o que não conseguiu testar. |

São responsabilidades atribuídas a agentes, não quatro implementações nem quatro novos projetos. Para esta POC, o coordenador pode implementar e dois revisores independentes cobrem produto/liderança e QA. Para uma correção pequena, basta um revisor independente reunindo os três papéis de validação. Quem desenvolveu não ocupa o posto de aprovador daquela mudança.

## Ciclo curto

1. **Definir:** registrar no chat o problema, os critérios de aceite e o escopo. Em features ou mudanças de interface/dados/build, um agente de produto/liderança confere a abordagem antes da implementação. Não pedir nova aprovação ao usuário para decisões reversíveis já autorizadas.
2. **Implementar:** usar o mesmo checkout e o build existente. Cada agente que escreve recebe arquivos delimitados. Criar teste para comportamento novo ou bug reproduzível; não criar testes que só comparam texto de documentação.
3. **Verificar:** executar o build e os testes apropriados. O caminho padrão é `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1`. Debug é necessário se a alteração afeta configuração, recursos, plataforma ou código condicionado ao build. Repetir build limpo quando houver mudança de compilação/empacotamento ou indício de cache incorreto.
4. **Revisar:** delegar a agentes que não implementaram. Produto/liderança confere o diff e QA confronta o resultado com os critérios. Dar requisito, base Git, arquivos alterados e comandos; não encaminhar a conclusão do autor como prova. Revisores não editam fontes, não mudam Git nem criam outros agentes. Testes que escrevem nos mesmos logs devem ser sequenciais.
5. **Corrigir e conferir:** reproduzir achados, corrigir a causa, rodar os testes afetados e devolver ao mesmo revisor. P0/P1/P2 no escopo impedem aprovação; P3 pode ficar registrado com impacto. Divergência exige evidência, não votação. Após três rodadas sem convergir, refazer o diagnóstico e explicar o impasse, sem repetir cegamente nem declarar sucesso.
6. **Entregar:** atualizar `docs/validacao.md` com revisão, responsáveis, resultados, hash do EXE e pendências. Uma alteração posterior exige nova conferência do que foi afetado. Manter o histórico no Git, sem criar relatórios ou executáveis v1/v2.

O ciclo ocorre por mudança ou entrega coerente, não a cada edição de linha. Revisores podem trabalhar em paralelo quando seus testes não disputam arquivos. Não é necessário instalar outro plugin ou manter um serviço de agentes. Os testes são automáticos no build; a orquestração e os pareceres dos agentes acontecem durante a tarefa e são registrados explicitamente.

## Contrato automático do build

- Antes de publicar, `dist` só pode conter `AlbionAssistant.exe`, ou ainda estar vazia/ausente. Qualquer outro arquivo, inclusive oculto, launcher, ZIP ou subpasta, interrompe a operação sem apagar dados.
- Diretórios de build redirecionados são recusados antes de gravar logs ou limpar cache. A pasta de entrega e o EXE também não podem ser redirecionados.
- Compilação com warnings tratados como erros; CTest com falha em testes vazios ou reprovados. Uma falha impede a instalação.
- Release publica no mesmo `dist/AlbionAssistant.exe`. Debug não altera a entrega.
- Depois da publicação, o EXE deve passar na inspeção de recursos e ter SHA-256 igual ao executável testado. Essa inspeção não executa a interface.
- `build/logs/<configuração>-validation.json` registra início/fim, estado automático, revisão Git quando disponível, alterações locais, caminho e SHA-256. Uma tentativa falha registra falha; uma interrompida pode permanecer como `running`. Nunca interpretar relatório antigo, `running` ou `failed` como aprovação atual. Recusa de diretório redirecionado ocorre antes do relatório, para não escrever fora do projeto.
- O relatório automático não aprova revisão independente nem teste visual. Esses pareceres ficam em `docs/validacao.md`. A evidência local é da tentativa mais recente; o resumo versionado preserva a rastreabilidade da entrega.

## Parecer de cada revisor

Retornar um registro curto, suficiente para outra pessoa conferir:

- **Escopo e identidade:** papel, nome do agente, base Git/diff examinado e hash do artefato quando aplicável.
- **Critérios:** esperado × observado, com comando/resultado ou evidência visual real.
- **Achados:** gravidade, arquivo/linha, cenário de reprodução e efeito para o usuário.
- **Decisão:** APROVADO, REPROVADO ou PENDENTE para aquele escopo; indicar o que falta e quem executa a próxima ação.

P0 é falha crítica imediata; P1 compromete funcionalidade principal ou dados; P2 quebra um critério da entrega; P3 é melhoria sem impedir o uso no escopo aprovado. Problemas preexistentes fora do escopo ficam identificados, sem gerar uma auditoria ilimitada.

## Critérios permanentes desta POC

| Critério | Evidência mínima |
|---|---|
| Um único programa | Validador de entrega + inspeção do EXE em `dist`; nenhum launcher ou pacote paralelo |
| Dados preservados | Testes de persistência/perfis e conferência de que build/limpeza não escrevem em AppData |
| Regra segura | Na regra padrão de exatamente 3 stacks, 3 destaca; 2, ilegível, ausente, expirado ou fonte inválida apagam; cenários negativos nos testes |
| Telas e seleção | Testes de layout/DPI e dois cliques; teste visual real para confirmar aparência, foco e posicionamento |
| Validação honesta | Parecer independente e pendências visíveis; simulação não substitui monitor físico ou jogo real |

Se o desktop estiver bloqueado, QA visual fica **PENDENTE**. Um build pode estar pronto para esse teste sem o produto ser apresentado como integralmente validado. O monitor de 34 polegadas só recebe aprovação depois de teste nesse ambiente.
