# Biblioteca de regras compartilhadas — 27/09/2026

Base de desenvolvimento: `master` local `cd1326100d0d38cb1628ad74df1aba7be4051ae9`; branch `feat/regras-compartilhadas`. A versão estável e o EXE já publicado permanecem na master. Não há remoto configurado.

## Objetivo e critérios observáveis

1. Criar uma regra na biblioteca, vinculá-la a dois perfis e verificar que editar sua condição, amostras ou efeito em **Regras** atualiza ambos, inclusive após reiniciar o aplicativo.
2. Em **Perfis**, reordenar e ativar/desativar os vínculos independentemente; remover uma regra do perfil ou excluir o perfil não apaga a definição compartilhada. Excluir uma regra da biblioteca ainda usada por qualquer perfil deve ser recusado com orientação concreta.
3. Abrir e salvar workspaces dos schemas 1–5 sem perder HUDs, status, regras, IDs, gatilhos, amostras, relógio, filtro de cooldown, ordem ou estado ativo. Regras antigas de nomes iguais em perfis diferentes recebem nomes distintos na biblioteca, sem união automática. Falhas na migração não sobrescrevem o arquivo original.
4. Apenas as regras ativadas do perfil e da HUD ativos entram no monitor; prioridade, veto de cooldown, três stacks exatos e apagamento em leitura desconhecida, expirada ou fonte inválida permanecem. Área ausente na HUD selecionada ou divergência de tamanho/DPI da janela bloqueia o início e explica o motivo.
5. A interface separa **HUDs → Status → Regras → Perfis → Monitorar**; regras de status e de vida continuam editáveis. Biblioteca vazia e perfil vazio orientam o próximo passo. Nenhuma automação de gameplay, launcher ou distribuição paralela.

## Abordagem

- `Workspace.rules` armazena a definição única (`StatusRule`); cada perfil armazena vínculos ordenados `{ruleId, enabled}`. A habilitação pertence ao vínculo; materializar um plano de monitoramento aplica esse valor numa cópia somente para execução. A regra mantém nomes de áreas (slots fixos), não coordenadas de uma HUD.
- Schema 6 grava definições e vínculos em seções próprias. Para schemas 1–5, cada regra vira uma entrada independente da biblioteca; preservar o ID e sufixar nomes globais colidentes. O limite da biblioteca comporta as 2.048 regras que 64 perfis com 32 regras poderiam conter antes.
- A migração acontece em memória ao carregar o arquivo antigo; o salvamento usa a verificação do temporário e substituição atômica já existentes. Regras sem vínculo permanecem disponíveis. Vínculos repetidos/inexistentes são dados inválidos; somente pendências das regras ativadas do perfil ativo bloqueiam o monitor.
- Antes de salvar o schema 6 sobre um arquivo 1–5, criar e conferir uma cópia imutável `workspace.ini.before-schema6*.ini` ao lado do original. A versão estável anterior não lê schema 6; voltar a ela requer restaurar essa cópia, descartando edições posteriores à migração. Nunca sobrescrever backups existentes.
- Mudanças no editor interrompem o monitor antes do commit para não deixar um plano copiado desatualizado. O editor de regras manipula o catálogo; o de perfis manipula apenas a lista de vínculos e sua prioridade.
- Não existe comando de clonagem na interface atual e ele não integra esta entrega. Se criado futuramente, clonar perfil deve conservar IDs de vínculos, ordem e ativação; clonar regra deve criar novo ID e definição independente com seus gatilhos e amostras.

## Verificação e entrega

- Testes de persistência/migração, fluxo de UI e monitor, incluindo negativos do Espírito Assassino e cooldown; compilações Debug e Release com os validadores ativos.
- Revisão independente de produto/arquitetura da abordagem já solicitada sobre a base anterior `f8fa49a` com o diff que agora é `cd13261`. O parecer preliminar foi **PENDENTE** até explicitar capacidade, migração, vínculos inválidos, reconstrução do monitor e prioridade; os contratos acima atendem esses pontos e exigem reconferência sobre o diff da feature.
- Antes de publicar `dist/AlbionAssistant.exe`, seguir `docs/processo.md` com dois revisores independentes, testes apropriados e registro de resultados em `docs/validacao.md`. Um build aprovado não valida aparência no jogo.
