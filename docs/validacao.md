# Validação do Albion Assistant

## Verificações do produto

O build executa nove suítes, com verificações ativas também em Release:

Em 20/09/2026, **9/9 passaram em Release e 9/9 em Debug**. A compilação limpa e a incremental foram verificadas; a incremental reportou `ninja: no work to do`. O script foi exercitado em Windows PowerShell 5.1 e PowerShell 7, incluindo avisos em stderr, falhas e codificação do log.

| Suíte | Cobertura |
|---|---|
| rules | Regra, validade, origem das observações, Unicode, configuração inválida e migração |
| recognition | 22 recortes reais rotulados, negativos, escalas e relógio radial; recursos embutidos idênticos aos PNGs e mesmos resultados de reconhecimento |
| profiles | Perfis independentes, nomes inválidos e colisões, resolução/DPI/monitor e dados legados |
| calibration | 20 recortes reais presentes localizados e 2 ausentes rejeitados; escalas de 24–256 px, cantos e ambiguidade |
| selection | Confirmação, cancelamento, destruição da janela principal, F2 e controles/prévia dentro da área disponível |
| panel_layout | Painel cabe nas áreas úteis simuladas de notebook, Full HD e ultrawide |
| app_flow | Persistência, edições incompletas após troca de DPI, isolamento da pasta de dados e estado/expiração da demonstração |
| packaging | Executável real Windows x64, PNGs embutidos, ícone, versão, manifesto sem elevação e suporte a DPI |
| build_script | Aviso em stderr com saída zero é aceito; saída de erro interrompe a etapa; log UTF-8 e preferências do chamador preservados |

Os logs ficam em `build/logs` e `build/{release,debug}/Testing/Temporary`. O build trata warnings de C++ como erros. O executável é publicado somente depois dos testes.

O teste de empacotamento inspeciona o EXE produzido. Os testes de interface usam janelas do próprio harness e verificam estado e geometria; não comprovam aparência, foco ou borda visível em um jogo real.

## Evidência do ciclo real

Em 20/09/2026, o núcleo de captura/reconhecimento/overlay foi exercitado no Albion, em Caerleon Market, cliente 2880 × 1800. Buffs: `(86,218,514,98)`; destaque: `(1216,1576,104,104)`; ícone de 64 px. A ferramenta de interface acionou Q no teste autorizado; o produto não envia teclas ao jogo. O vídeo não foi utilizado.

| Captura (ms desde a sessão Windows) | Reconhecimento | Destaque |
|---:|---|---|
| 12664872 | Ausente | Apagado |
| 12673015 | Presente, contador ilegível | Apagado |
| 12674781 | 2 stacks | Apagado |
| 12676859 | 3 stacks | Visível |
| 12688057 | Buff terminou | Apagado |

O registro está em [evidencias/ciclo-ao-vivo.csv](evidencias/ciclo-ao-vivo.csv). Um segundo ciclo com a regra desativada reconheceu 2 e 3 sem acender a borda. O hit testing da borda foi verificado com processo observador separado e controle negativo. Esses testes antecedem a interface guiada e a consolidação do executável; não são uma validação visual dessas mudanças.

## Pendências reais

O Windows permaneceu bloqueado durante as alterações de interface e empacotamento. Ainda é necessário conferir a interface completa, o aviso TESTE e o ciclo do buff no desktop desbloqueado. O monitor físico de 34 polegadas, HDR, outras escalas reais e sessões longas de combate não foram validados.

Para conferir: abra `dist/AlbionAssistant.exe`, conecte, confirme as três seleções, use a demonstração de cinco segundos e observe o ciclo sem número → 2 → 3 → ausente. Depois crie o perfil do monitor de 34 polegadas e verifique a troca de volta ao notebook.
