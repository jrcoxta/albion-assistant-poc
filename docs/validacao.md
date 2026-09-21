# Validação do Albion Assistant

## Entrega atual: HUDs, status e sets separados

Base Git: `0eb22d54dfc959e4fbbaf9ace58fced58dd99f94`, diff desta entrega incluindo arquivos novos. Os critérios foram registrados em [modelo.md](modelo.md) antes da implementação. O usuário testou o programa anterior e relatou que funcionou muito bem; esse retorno não substitui a validação das mudanças atuais.

Em 20/09/2026, **13/13 testes passaram em Debug e 13/13 em Release**, com caches recriados após a correção de dependências. Release foi compilado do zero e publicado somente depois dos testes. O artefato final tem **676.864 bytes**, SHA-256 **`4BCAA3E8F54E137A9C5049ABF8EC1143DEB54354134633FC92B55A383DDAD54E`**. Os relatórios em `build/logs` registram a base acima com alterações locais; o commit que contém este documento preserva o diff examinado.

| Escopo | Evidência atual |
|---|---|
| Interface separada | Quatro PNGs reais renderizados pelo teste do próprio aplicativo em `build/logs/ui`; coordenador e QA conferiram textos, campos e ausência de sobreposição nesses estados. Não são capturas sobre o jogo. |
| Estado e persistência | `app_flow` exercita navegação, gravação, DPI com rascunho, exclusão confirmada/cancelada, independência HUD/set/status, contadores personalizados, demonstração temporária e migração. |
| Modelo e migração | `workspace` verifica roundtrip Unicode, schema, corrupção, substituição atômica, última HUD excluída sem reimportação, nomes antigos e configurações divergentes. |
| Regras simultâneas | `monitor` verifica compartilhamento por status/área, independência, expiração e prioridade por destino. `app_flow` inclui dois ícones distintos na mesma ROI e remoção seletiva de um deles. |
| Reconhecimento | Regressão dos 22 recortes reais; contadores personalizados 5/7; amostra única 2/3 contra o outro número; recusa de amostra sem número; escalas e ambiguidade. |
| Empacotamento | **APROVADO automaticamente**: `packaging`, `delivery`, hash da cópia publicada igual ao build e pasta contendo somente `dist/AlbionAssistant.exe`. |
| Dependências incrementais | **APROVADO**: `build_dependencies` altera somente um header, observa novo resultado no programa, verifica recompilação do consumidor e preserva a fonte independente. Confere também dependências críticas dos objetos reais e bloqueia cache antigo sem elas. |
| Novo fluxo no jogo/monitor físico | **PENDENTE**. O teste estático da interface não comprova seleção, foco, captura e brilho em todas as telas reais. |

Revisão independente: `/root/product_validator` **APROVOU** produto/arquitetura e a reconferência dos achados em código. `/root/qa_validator` **APROVOU** lógica, persistência, reconhecimento testado, build e entrega: executou separadamente `workspace` (91 verificações), `monitor`, `recognition` (22/22 recortes reais e demais casos) e `app_flow`, todos com saída zero. Também inspecionou o EXE publicado com `packaging` e `validate-delivery`, conferiu o hash acima e os logs completos das duas configurações. Confirmou a aparência estática das quatro páginas; o teste real continua pendente. Não restaram achados P0/P1/P2 no diff revisado.

Os revisores trabalharam somente em leitura, sem implementação. Desenvolvimento dividido entre `/root` e agentes com arquivos delimitados. Aprovação de código/testes não é aprovação universal de todos os buffs/debuffs do Albion.

Achados identificados e tratados neste ciclo:

- Uma amostra única podia confundir 2 com 3; o reconhecedor agora exige correspondência absoluta mais forte e conserva a rejeição por ambiguidade. Amostras sem tinta interna de contador são recusadas antes da gravação.
- Configurações personalizadas anteriores usavam implicitamente dígitos do exemplo; a primeira importação os materializa como amostras explícitas. Cadastros novos começam sem contadores. Nomes antigos são normalizados com preservação de Unicode e sem sobrescrever entidades.
- Regras de presença/ausência não carregam nem dependem de arquivos de contadores. Uma referência com dimensões inválidas bloqueia o início e identifica o status afetado.
- O valor legado zero só desativa uma regra de contagem; presença/ausência permanecem habilitadas como antes.
- O build incremental estava deixando de registrar dependências de headers por um prefixo localizado mal decodificado. O diagnóstico veio de um crash no teste de calibração com objeto antigo; a correção e sua prova de recompilação incremental fazem parte desta entrega.

Limites: reconhecimento por amostras de ícones e contadores brancos no canto inferior direito, sem OCR universal. Ícones muito parecidos/monocromáticos e efeitos diferentes dos recortes disponíveis precisam de teste específico. Brilho significa margem suave estática. O relógio radial não fornece segundos restantes.

## Verificações da entrega anterior

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

## Revisão independente da entrega anterior

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

## Evidência histórica do ciclo real

Em 20/09/2026, o núcleo de captura/reconhecimento/overlay foi exercitado no Albion, em Caerleon Market, cliente 2880 × 1800. Buffs: `(86,218,514,98)`; destaque: `(1216,1576,104,104)`; ícone de 64 px. A ferramenta de interface acionou Q no teste autorizado; o produto não envia teclas ao jogo. O vídeo não foi utilizado.

| Captura (ms desde a sessão Windows) | Reconhecimento | Destaque |
|---:|---|---|
| 12664872 | Ausente | Apagado |
| 12673015 | Presente, contador ilegível | Apagado |
| 12674781 | 2 stacks | Apagado |
| 12676859 | 3 stacks | Visível |
| 12688057 | Buff terminou | Apagado |

O registro está em [evidencias/ciclo-ao-vivo.csv](evidencias/ciclo-ao-vivo.csv). Um segundo ciclo com a regra desativada reconheceu 2 e 3 sem acender a borda. O hit testing da borda foi verificado com processo observador separado e controle negativo. Esses testes antecedem a interface guiada e a consolidação do executável; não são uma validação visual dessas mudanças.

## Testes reais ainda necessários

O Windows permaneceu bloqueado durante as alterações anteriores de interface e empacotamento. Depois, o usuário abriu o programa e relatou sucesso no jogo. As novas páginas, a leitura simultânea de vários status e o brilho ainda precisam ser conferidos nesse ambiente. O monitor físico de 34 polegadas, HDR, outras escalas reais e sessões longas de combate não foram validados.

Para conferir: siga o roteiro do [README](../README.md), começando pelo set importado. Depois cadastre outro status, use duas regras simultâneas e verifique os destaques. Crie uma HUD para o monitor de 34 polegadas com os mesmos nomes de áreas e reutilize o set; voltar ao notebook deve restaurar suas posições. A demonstração de cinco segundos verifica a ação; o ciclo real sem número → 2 → 3 → ausente verifica a regra.
