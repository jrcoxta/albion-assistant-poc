# Regras compostas e edição consistente

**Data:** 22/09/2026  
**Status:** aprovado pelo usuário

## Objetivo

Permitir que um único destaque seja ativado por mais de uma condição, usando
**OU**: “quando X ou Y acontecer, destacar E”. Corrigir também a edição de
nomes para que HUD, área, status, perfil e regra usem o mesmo padrão explícito
de renomear, salvar e cancelar.

## Modelo de regra

Uma regra passa a representar uma **ação visual**. Ela possui nome, destino,
efeito, cor, ativação e a opção de aro de tempo. Cada regra contém uma ou mais
condições.

Uma condição possui:

- status a reconhecer;
- área de origem;
- presença, ausência ou stacks exatos;
- amostras de stacks quando necessárias;
- referência de relógio quando o aro estiver ativo.

A regra fica ativa quando **qualquer condição** estiver verdadeira. Quando mais
de uma condição for verdadeira, a primeira condição cadastrada fornece o tempo
restante do aro. A prioridade já existente entre regras diferentes que miram a
mesma área continua: a primeira ação ativa vence.

Exemplo:

> E-ABERTO: quando Espírito Assassino tiver 3 stacks **ou** Veneno estiver
> presente, destacar E com aura dourada.

## Migração e compatibilidade

O `workspace.ini` passa ao schema 3. Cada regra do schema 2 vira uma ação com
uma única condição, preservando todos os campos, amostras, relógio, efeito,
cor, áreas e nomes. Schema 1 continua sendo migrado primeiro para a leitura
compatível existente e depois para uma condição única. Dados inválidos ou
incompletos continuam bloqueando somente o início da leitura.

## Interface

Os nomes deixam de ser campos permanentes de edição:

- HUD: seletor, Renomear HUD e Excluir HUD;
- Área: seletor, Renomear área e Excluir área;
- Status: seletor, Renomear status e Excluir status;
- Perfil: seletor, Renomear perfil e Excluir perfil;
- Regra: seletor, Renomear regra e Excluir regra.

Renomear sempre abre o diálogo com o valor atual e botões Salvar/Cancelar.
O seletor e o cabeçalho são atualizados somente depois de persistir o novo
nome.

No editor de regra, uma lista de condições mostra cada gatilho em linguagem
direta. `Adicionar condição` cria um gatilho dentro da ação selecionada;
`Excluir condição` remove apenas esse gatilho. Uma regra mantém ao menos uma
condição; para remover a última, o usuário exclui a regra.

## Critérios de aceitação

1. Renomear área, status, perfil e regra persiste sem depender de perder foco.
2. Uma regra com X ou Y produz somente uma ação/destaque no destino.
3. Cada condição mantém seu status, origem, stacks e relógio isolados.
4. Uma regra antiga é carregada como ação com uma condição equivalente.
5. Dados de condição pendentes bloqueiam a leitura com mensagem que identifica
   a regra e a condição afetada.
6. O overlay existente, a prioridade entre destinos e a leitura de relógio
   continuam funcionais para regras de uma condição.
