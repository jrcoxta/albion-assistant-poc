# Validação do Albion Assistant

## Verificações do produto

O build executa dez suítes, com verificações ativas também em Release:

Em 20/09/2026 (horário de Brasília), **10/10 passaram em Release e 10/10 em Debug**. O coordenador `/root` executou build limpo Release no Windows PowerShell 5.1 e Debug no PowerShell 7.6.5; a compilação incremental reportou `ninja: no work to do`. Na consolidação anterior eram nove suítes; a décima protege o contrato de entrega.

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
| delivery | 12 cenários inválidos: EXE ausente/divergente, extras/oculto/pastas, redirecionamento; script real em fixture isolada prova reprovação, relatório failed, preservação da entrega anterior e recusa de limpeza por junction |

Os logs ficam em `build/logs` e `build/{release,debug}/Testing/Temporary`. Os relatórios `Release-validation.json` e `Debug-validation.json` registram revisão Git, alterações locais, estado automático e hash do artefato. O build trata warnings de C++ como erros, recusa testes vazios e publica somente depois dos testes. Em seguida confere o layout de `dist`, igualdade SHA-256 com o build e recursos do EXE publicado.

O teste de empacotamento inspeciona o EXE produzido. Os testes de interface usam janelas do próprio harness e verificam estado e geometria; não comprovam aparência, foco ou borda visível em um jogo real.

## Revisão independente do processo e da entrega

Base da revisão: `4033b125f8acda8eb006b53c5626eb503b4c23e5` e diff desta entrega, incluindo arquivos novos. A execução inicial registra `git_dirty: true`; o commit que contém este registro preserva as mudanças examinadas. Os relatórios locais posteriores identificam a revisão efetivamente usada em cada build.

Artefato examinado: `dist/AlbionAssistant.exe`, 551.936 bytes, SHA-256 `C348052A4668F38ED436EFD6B1F0FE4C9882C8DB2A82EB2A171FA8EACD6D1B7A`. O QA inspecionou esse arquivo diretamente com `packaging_tests.exe` e o validador de entrega, sem iniciar a interface.

| Responsabilidade | Responsável | Parecer |
|---|---|---|
| Implementação e execução completa | `/root` | Build/testes aprovados; este papel não constitui aprovação independente |
| Produto e liderança técnica | `/root/product_validator` | APROVADO para critérios, processo, distribuição única e documentação de migração; nenhum P0/P1/P2 restante no diff examinado |
| QA independente | `/root/qa_validator` | APROVADO para build e entrega Windows; executou os 12 casos em PS5.1 e PS7, inspecionou o EXE publicado e conferiu os logs finais Release/Debug; nenhum P0/P1/P2 restante |
| Interface atual no jogo e monitor de 34 polegadas | QA visual no ambiente real | PENDENTE; não abrangido pelos testes automáticos desta entrega |

Os revisores receberam requisitos, base Git e arquivos, sem herdar o raciocínio do implementador, e não editaram o código. O processo permanente está em [processo.md](processo.md) e é exigido por `AGENTS.md`.

Achados fechados neste ciclo:

- **Distribuição:** extras podiam permanecer em `dist`; agora são recusados antes de instalar, sem exclusão automática. O caso adverso executa o script real em pasta temporária, sem tocar no `dist` ou AppData reais.
- **Artefato e rastreabilidade:** a cópia publicada agora passa na inspeção e deve ter o mesmo hash do build. O relatório diferencia estado automático, revisão independente e teste visual; um teste integrado comprova que falha substitui um relatório anterior de sucesso.
- **Ausência de testes:** o CTest real, com filtro sem correspondências e `--no-tests=error`, retornou erro. O build usa essa opção em todas as execuções.
- **Limpeza:** o ancestral `build` e a pasta de logs também são checados contra redirecionamento. O teste integrado prova que `-Clean` recusa uma junction sem apagar o marcador no destino nem gravar logs nele.
- **Compatibilidade:** Debug via PowerShell 7 revelou que CTest iniciava Windows PowerShell herdando módulos incompatíveis (`Get-FileHash` indisponível). A falha foi reproduzida; somente os subprocessos PowerShell de testes passam a iniciar sem `PSModulePath` herdado. A nova execução completa passou 10/10. Os 12 casos de entrega também passaram diretamente em PS5.1 e PS7.6.5, com subprocessos do mesmo host.
- **Escopo:** os critérios de stacks foram delimitados à regra padrão de exatamente 3; condições configuráveis de presença/ausência não devem ser reprovadas por esse critério.

O build Debug foi conferido com hash de `dist` antes/depois e não alterou o Release. Nenhum teste deste ciclo alterou dados em AppData. A migração histórica foi feita na consolidação; o revisor confirmou os dados presentes, mas não refez a comparação com os arquivos antigos já retirados.

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
