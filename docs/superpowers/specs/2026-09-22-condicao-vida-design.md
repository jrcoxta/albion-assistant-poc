# Condição por porcentagem de vida

## Objetivo

Permitir que uma regra use a vida do personagem ou do alvo como condição, por exemplo: `Vida do inimigo <= 49% -> destacar D`. A condição participa do mesmo grupo `OU` já usado pelos status e compartilha a ação, o destino e o efeito da regra.

## Configuração

- A HUD continua guardando a área ampla onde a barra aparece.
- Uma área usada como vida recebe uma calibração capturada enquanto a barra está cheia.
- A calibração localiza a faixa horizontal vermelha dentro da área e salva seus limites e a referência visual cheia.
- Na regra, a condição escolhe `Status` ou `Vida`. Para vida, escolhe a área, o operador `<=` ou `>=` e um valor de 1 a 100%.
- Não haverá OCR nem dependência externa nesta etapa.

## Leitura

O leitor recorta os limites calibrados e analisa várias linhas da faixa. Pixels claros ou pouco saturados, como números e moldura, não participam da medição. A porcentagem é o final do preenchimento vermelho contínuo da esquerda para a direita em relação à largura cheia.

Uma leitura incerta produz estado desconhecido e nunca aciona a regra. Para evitar oscilação no limiar, `<= 49%` ativa em 49% ou menos e desativa somente acima de 51%; o inverso é aplicado a `>=`.

## Integração

- O workspace persiste o tipo do gatilho, área, operador, limiar e calibração da barra.
- O plano de monitoramento cria leitores de status e de vida na mesma captura de tela.
- A avaliação da regra mantém a semântica existente: qualquer gatilho verdadeiro ativa a única ação compartilhada.
- Excluir uma área usada por uma condição de vida continua bloqueado ou gera pendência da mesma forma que uma origem de status.

## Validação

- Testes do medidor com barras sintéticas em 0%, 48%, 49%, 50%, 51% e 100%, incluindo texto branco sobreposto.
- Testes de leitura incerta, histerese e regra composta `status OU vida`.
- Round-trip de persistência e fluxo da interface.
- Build completo uma vez após os testes direcionados.
- A validação em jogo deverá usar uma barra cheia para calibrar e uma barra parcialmente consumida para confirmar a tolerância visual real.

## Limite conhecido

A primeira versão suporta barras horizontais que esvaziam da direita para a esquerda, como a HUD observada do Albion. Outras cores ou direções só serão adicionadas quando houver um caso real.
