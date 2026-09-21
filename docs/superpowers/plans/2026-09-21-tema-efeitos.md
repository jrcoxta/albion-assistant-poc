# Tema escuro, estabilidade visual e efeitos

Pedido: melhorar autonomamente a interface e os destaques, mantendo um único aplicativo e os dados existentes.

## Critérios da entrega

- Quatro páginas com superfícies carvão, texto legível, vermelho nos comandos principais e foco de teclado visível.
- Reconstrução atômica dos controles, incluindo restauração de rascunhos. Clicar na aba ou seleção atual não reconstrói nem interrompe a leitura. Visibilidade original e foco são preservados.
- Borda, brilho, pulsação suave e halo disponíveis por regra. Centro transparente, janela sem interceptar cliques; condição falsa, leitura expirada e perda de foco apagam o efeito.
- “Cor da habilidade” captura uma vez a área de destino selecionada. Preserva a cor anterior em falha ou imagem neutra. Não amplia a captura contínua do reconhecimento.
- Configurações anteriores com `glow` continuam válidas; `effect` válido prevalece. Valores inválidos são rejeitados. Sem promessa de leitura do novo formato por binários anteriores.
- Um EXE publicado após testes Debug/Release, revisão independente de produto e QA, inspeção visual das páginas/efeitos e registro das limitações.

## Divisão de arquivos

1. Agente de interface: `app_ui.cpp`, `main.cpp`, `theme.h/.cpp`. Tema nativo e atualização sem pintura intermediária.
2. Agente de efeitos: `overlay.h/.cpp`, `overlay_effect.h/.cpp`, `overlay_effect_tests.cpp`. Raster, animação, geometria e extração de cor.
3. Coordenador: modelo/persistência, integração do runtime, captura pontual, CMake e regressões de fluxo. Nenhum build concorrente no mesmo diretório.
4. Revisores de produto e QA: somente leitura, critérios acima, base `11b5dba`, diff e evidências; correções bloqueantes voltam ao revisor.

## Sequência e validação

- Adicionar testes de migração e efeitos; confirmar falha antes da implementação correspondente.
- Implementar o enum compartilhado e a leitura compatível; integrar os quatro efeitos e padding real na checagem de sobreposição.
- Capturar somente o destino, restaurar o painel em todos os caminhos e salvar a cor apenas se houver evidência cromática.
- Integrar o tema, verificar rascunhos, foco, seleção repetida e janela originalmente oculta.
- Executar suítes afetadas, gerar imagens das quatro páginas e efeitos, observar transições e corrigir achados dos revisores.
- Executar `scripts/build-windows.ps1 -Configuration Debug` e o build Release canônico. Conferir distribuição única, hash e preservação dos dados reais.
- Atualizar README, modelo e validação; versionar a entrega no mesmo branch. Não declarar teste em jogo se apenas o alvo controlado tiver sido exercitado.
