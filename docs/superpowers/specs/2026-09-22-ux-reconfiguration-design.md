# Reformulação de usabilidade — Albion Assistant

**Data:** 2026-09-22
**Status:** aprovado para planejamento

## Objetivo

Transformar a interface em um cadastro claro de configurações relacionadas. O usuário deve conseguir criar, editar, testar e excluir HUDs, status, perfis e regras sem etapas escondidas, botões técnicos fora de contexto ou dependências difíceis de descobrir.

O aplicativo continua sendo uma ferramenta de leitura visual e overlay: captura de áreas da tela, reconhecimento de imagens e efeitos visuais. Não controla o jogo.

## Princípios

- Abas sempre acessíveis e ordenadas pela relação entre seus cadastros.
- Cada tela funciona como um cadastro mestre-detalhe: lista de itens, seleção e edição do item selecionado.
- Criar um item não exige uma segunda confirmação para ele existir.
- Alterações válidas são salvas automaticamente; não há botões ambíguos de salvar por seção.
- A interface só pede informações quando elas se tornam necessárias.
- Itens incompletos podem existir, mas a leitura só inicia com as dependências necessárias prontas.
- Exclusões explicam o impacto concreto antes de confirmar.

## Navegação

As abas serão:

1. **HUDs**
2. **Status**
3. **Perfis e regras**
4. **Monitorar**

No primeiro uso, sem HUD cadastrada, o aplicativo abre em **HUDs**. Com uma HUD ativa já configurada, pode abrir em **Monitorar**.

## HUDs

A aba cadastra a organização visual de uma tela de jogo.

- Lista de HUDs e cadastro da HUD selecionada.
- Cada HUD informa resolução e escala em que foi criada.
- `Nova HUD` cria e seleciona imediatamente uma HUD com nome temporário editável.
- Áreas da HUD possuem nome, formato, posição e tamanho.
- Formatos suportados: retângulo e círculo.
- Ações: adicionar área, selecionar na tela, renomear e excluir.
- A área selecionada tem prévia visual.
- Áreas de destino, como `Q`, `W` e `E`, não exigem calibração de ícone: sua seleção define a posição do overlay.
- A calibração de ícone só é solicitada quando uma área for usada como origem de leitura de um status.

Estado vazio:

> Nenhuma HUD cadastrada. Crie uma configuração para a tela em que você joga e depois selecione as áreas usadas pelas regras.

## Status

A aba é uma biblioteca de imagens de status monitoráveis.

- Lista de status e detalhes do status selecionado.
- Campos e ações: nome, capturar referência, importar imagem, renomear e excluir.
- Não exibe tipo buff/debuff, exemplos prontos, amostras de stacks ou relógio por padrão.
- Tipo buff/debuff é removido da experiência e de dados novos, pois não altera o funcionamento atual.
- Ao excluir um status usado por regras, a confirmação informa as regras impactadas e oferece revisão antes da remoção.

Estado vazio:

> Ainda não há status para acompanhar. Cadastre a imagem de um buff ou debuff que aparece na sua HUD.

## Perfis e regras

Um perfil representa o conjunto de regras de um set, por exemplo, `Mortífico`.

A tela tem lista de perfis, lista de regras do perfil selecionado e editor da regra selecionada.

Uma regra é construída e resumida em linguagem direta:

> Quando **[status]** estiver **[condição]**, destacar **[área]** com **[efeito]**.

### Condições

- Está presente
- Não está presente
- Tem exatamente N stacks
- Tem ao menos N stacks

Condições de stacks mostram uma seção contextual, contendo:

- valor de stacks;
- estado e miniatura da amostra daquele valor;
- capturar amostra;
- substituir amostra;
- excluir amostra.

Excluir uma amostra não apaga a regra: a marca como pendente até receber uma nova imagem.

### Efeito

A regra define:

- área de destino;
- efeito: aura, borda, pulsação ou aro de tempo;
- cor sugerida pela habilidade/ícone, ajustável pelo usuário;
- intensidade: discreto, normal ou forte;
- opção `Mostrar tempo restante` quando aplicável.

O relógio só é configurado quando `Mostrar tempo restante` for escolhido. A regra pede sua referência naquele momento.

Cada regra possui botão de teste. Ele mostra o efeito configurado na área configurada. Regras incompletas explicam a pendência em vez de executar um teste parcial.

Estado vazio de regras:

> Este perfil ainda não possui regras. Crie uma regra para relacionar um status a um destaque na tela.

## Monitorar

É a aba operacional. Não é usada para cadastrar configurações.

Quando houver configuração pronta, mostra:

- HUD e perfil ativos;
- iniciar leitura, parar e testar efeito;
- opção de incluir o overlay em compartilhamentos/capturas;
- situação de cada regra em linguagem útil: aguardando, status encontrado, quantidade detectada ou efeito ativo.

Quando existirem pendências, não tenta iniciar a leitura. Exibe uma lista objetiva:

> A leitura ainda não pode ser iniciada. Complete os itens abaixo.

Cada pendência informa item, motivo e botão `Resolver`, que abre o cadastro exato. Exemplos:

- `Regra Espírito Assassino · 3 stacks: capture a amostra de 3 stacks.`
- `Regra Espírito Assassino · 3 stacks: escolha a área de destaque.`
- `Status Espírito Assassino: capture uma imagem de referência.`

## Criação, edição e exclusão

### Criação e edição

- `Novo` cria e seleciona o item imediatamente.
- O nome temporário fica em edição.
- Ao confirmar o nome ou mudar de campo, a alteração é salva.
- Nomes vazios ou duplicados no mesmo contexto permanecem no campo com mensagem clara.
- Não há sequência de clicar em `Novo`, preencher e clicar novamente em `Novo`.

### Exclusão

- Excluir HUD informa que suas áreas serão removidas, sem apagar status ou perfis.
- Excluir área informa quais regras perderão origem ou destino.
- Excluir status informa as regras que o usam.
- Excluir perfil remove suas regras junto com ele.
- Excluir regra não afeta outros itens.
- Excluir amostra torna somente a regra correspondente pendente.

## Estados de validação

Cada item pode estar:

- **Pronto**: atende ao que suas regras exigem.
- **Pendente**: falta uma informação.
- **Com impacto**: outra alteração exige revisão.

A existência de cadastros pendentes é permitida. O bloqueio ocorre somente ao iniciar leitura quando uma dependência da regra ativa estiver incompleta.

## Remoções e simplificações previstas

- Remover da UI e do modelo novo o tipo de status `buff/debuff`.
- Remover comandos, IDs e caminhos obsoletos de exemplo pronto e de amostras globais de status.
- Remover botões de salvar com escopo ambíguo após implantar persistência automática.
- Remover calibração de áreas usadas apenas como destino de overlay.
- Mover captura de relógio para a regra que exibe tempo.

## Critérios de aceitação

1. Um usuário sem dados abre o aplicativo e entende como criar a primeira HUD sem depender de seletores vazios.
2. É possível criar, renomear e excluir HUD, área, status, perfil, regra e amostra de stack com um fluxo consistente.
3. A criação exige apenas uma ação de `Novo`, seguida da edição do item criado.
4. Nenhuma tela mostra tipo de status, exemplo pronto ou amostras de stacks fora do contexto de uma regra.
5. Uma regra de stacks permite capturar, substituir e excluir sua amostra.
6. Uma área de destino de overlay não pede calibração.
7. O monitor informa pendências em linguagem clara e abre o cadastro que as resolve.
8. A leitura existente e os efeitos de overlay permanecem funcionais após a migração de dados.
