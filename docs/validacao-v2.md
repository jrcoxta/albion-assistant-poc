# Validação da interface guiada — 20/09/2026

A v2 acrescenta o painel em quatro etapas, perfis de HUD independentes, seleção sobre imagem congelada, sugestão de recorte do ícone e demonstração do destaque por cinco segundos. Mantém o reconhecimento e o motor de regras da primeira POC.

## Verificações automatizadas

Os testes ficam ativos em Debug e Release, inclusive nas verificações que usam exceções em vez de `assert`.

**Resultado final:** 7/7 suítes aprovadas em Release e 7/7 em Debug, sem warnings do compilador nos logs finais. Logs locais: `.orchestration/build-guided-release.log`, `.orchestration/build-guided-debug.log` e `build/windows/Guided{Release,Debug}/Testing/Temporary/LastTest.log`.

| Suíte | Cobertura |
|---|---|
| rules | 115 verificações de regra, validade, fonte da observação, persistência e migração de calibração antiga. |
| recognition | 22/22 recortes reais do jogo reconhecidos conforme os rótulos; negativos, escalas e variação radial preservados. |
| profiles | 61 verificações: salvar/carregar Unicode, dois perfis independentes, nomes inválidos, colisões, resolução/DPI/dispositivo e identificação de INI legado. |
| calibration | 20/20 recortes reais com ícone localizados, 2/2 ausentes rejeitados; imagens sintéticas com ícones de 24 a 256 px, cantos, negativos, ambiguidade e canvas de 2880 × 1800. |
| selection | Seleção manual, confirmação, Esc, Alt+F4, botão fechar, destruição da janela principal, F2 para mover o painel e limites de recorte. Layout compartilhado com a UI mantém controles e prévia dentro de 800 × 600 com DPI solicitado de 200% e 250%. Harness usa janelas próprias. |
| panel_layout | Painel cabe nas áreas úteis simuladas de notebook, Full HD, 2880 × 1800 e ultrawide, respeitando o DPI quando há espaço. |
| app_flow | Carregar não salva edições sobre outro perfil; colisão de nome é recusada; Nova HUD mantém ação/referência salvas; reconstrução do painel preserva edição incompleta; demonstração encerra leitura, limpa observações e expira sem iniciar leitura implicitamente. |

Os testes de estado da demonstração **não comprovam que a borda e o aviso apareceram na tela**. Os testes de layout também não substituem a conferência em monitores físicos com DPI diferentes.

## Revisão

A revisão independente cobriu seletor, calibração e integração. As correções incluem preservar o perfil anterior na troca de ambiente, impedir sobrescrita por renomeação, migrar o INI legado mesmo quando já existe uma HUD com o nome padrão, preservar a proporção da prévia e reconstruir os controles após mudança de DPI sem validar um campo ainda em edição.

O tamanho e o monitor conhecidos fazem parte da compatibilidade de uma HUD. A alteração da posição dos elementos dentro do próprio jogo continua exigindo seleção ou troca manual de perfil.

## Validação visual pendente

O Windows ficou bloqueado durante esta etapa. Por isso, a interface guiada nova ainda precisa ser conferida visualmente no desktop desbloqueado. O teste real documentado em `validacao.md` pertence à primeira POC; ele não é apresentado como teste visual da v2.

Quando a sessão estiver disponível, conferir:

1. Fechar a primeira POC e abrir `Iniciar-v2.cmd`.
2. Conectar, selecionar região, ícone e destino; conferir a prévia e o cancelamento.
3. Verificar a demonstração com borda e aviso TESTE por cinco segundos.
4. Ativar Espírito Assassino no jogo e conferir o ciclo sem número → 2 → 3 → ausente.
5. No monitor de 34 polegadas, criar outro perfil e conferir retorno ao perfil do notebook.

A POC continua limitada ao preset e aos dígitos 2/3, sem calcular segundos restantes do relógio radial. Não há automação de gameplay no produto.
