# Validação do Albion Assistant

## Entrega atual: seleção circular e aura sobre a habilidade

Base Git: `e8910d9`, diff desta entrega em 21/09/2026. Pedido: selecionar status, habilidades e destino do overlay em círculo, mantendo a opção retangular; brilho translúcido também sobre a habilidade. Critérios definidos no chat e conferidos por produto antes da implementação: dois cliques (centro/raio), forma persistida por área, origem/destino independentes, contador preservado no recorte, busca restrita por centro e compatibilidade dos dados existentes. Implementação: `/root` integrou modelo/persistência/reconhecimento/interface; `/root/circle_selector` alterou seletor/testes; `/root/circle_aura` alterou efeitos/testes. Revisores somente leitura.

- **APROVADO — seleção e persistência:** Retângulo usa dois cantos; Círculo usa centro e raio, sem arrastar nem deslocar silenciosamente o centro. O seletor recusa círculos fora da captura e mantém confirmação, cancelamento, reinício e F2. Testes verificam limites, troca de forma, DPI e janelas do próprio processo. `workspace` passou em 142 verificações: campo `shape` opcional, retângulo por padrão, ida/volta, preservação dos identificadores/amostras e recusa de valores inválidos/círculos não quadrados sem sobrescrita. O campo novo reproduziu rejeição antes de sua implementação.
- **APROVADO — reconhecimento:** a busca circular filtra centros antes de ordenar/refinar candidatos e decidir ambiguidade. A imagem de referência e seu número continuam completos dentro do quadrado delimitador. Testes incluem candidato externo melhor que interno, múltiplas escalas e os 22 recortes reais. O primeiro teste circular reproduziu acionamento externo antes do filtro. QA identificou um P2 adicional: um pixel fora do círculo podia tornar um interior uniforme informativo e confirmar ausência. A regressão falhou antes da correção; o teste de informação visual agora também ignora pixels externos. Os testes afetados passaram após o ajuste.
- **APROVADO — integração e efeitos:** a união de captura permanece retangular mesmo com a primeira origem circular; leitores e destinos conservam suas formas independentes. A forma chega à leitura e ao teste de ação. Destino exclusivo dispensa medição. Brilho/Pulso têm aura translúcida interna/externa; Borda/Halo mantêm centro transparente. O raster segue a forma, conserva canais premultiplicados, recorte, pulso e proteção de foco/captura. Testes rejeitam geometrias inválidas e verificam simetria, transparência, gradação e guardas nativas.
- **APROVADO — produto/arquitetura e visual estático:** `/root/circle_product` conferiu diff, correção do P2, documentação e PNGs `build/logs/selector-preview/selector-{circle,rectangle}.png`, `build/logs/ui/ui-hud-circulo.png` e `overlay-effects.png`. Seletor e HUD legíveis, sem sobreposições observadas; círculo e aura coerentes com o pedido. Seu P3 de texto foi corrigido para “Inclua o número. Os cantos são mantidos.” A prancha declara fundo ilustrativo/simulação; a máscara no ícone existe apenas no compositor dessa prancha, sem alterar assets ou recortes salvos.
- **APROVADO — QA de código/harness:** `/root/qa_validator` executou independentemente `workspace` (142/0), `monitor`, `recognition` (22/22 recortes reais e regressão do pixel externo), `selection`, `overlay_effect` e `app_flow` Debug; todos com saída zero. Inspecionou os mesmos quatro PNGs, reconferiu a correção do P2 e encerrou sem P0/P1/P2 remanescente.
- **APROVADO — build Debug:** o comando canônico passou nas 14/14 suítes, com warnings como erros. Evidências em `build/logs/Debug.log` e `Debug-validation.json`.
- **PENDENTE — Release/publicação:** o build canônico compilou e passou em 13/14 suítes; `monitor_tests.exe` não iniciou (`BAD_COMMAND`, `resource busy or locked`). A execução direta e a leitura de hash desse arquivo também foram recusadas pelo Windows. Consulta somente leitura ao Restart Manager identificou PID 6052, `Kaspersky Service 21.26`, serviço `AVP21.26`, como usuário do arquivo. Não foi diagnosticada falha lógica do teste, nem atribuída detecção de malware: a evidência comprova o bloqueio. Nenhum teste foi removido, e o antivírus não foi alterado. `Release-validation.json` registrou `failed`; a publicação foi interrompida corretamente. O EXE anterior permanece em `dist`, SHA-256 `D48F402BD06656BC132F5DDBE83A757F01D46FF5990E7C3D5B59E99346B3535F`. Após liberação, executar novamente o build canônico e a conferência independente do artefato antes de apresentar o executável como atualizado.
- **APROVADO — preservação dos dados durante o build:** o hash do workspace real antes/depois da tentativa permaneceu `A15E988BE8ADC2F1CD61BF649E1613209E74E98DBEBBF59DB54FFA41D1F5D9D9`; testes usam diretórios isolados. Esse é o estado atual, diferente do registrado na entrega anterior.
- **PENDENTE — conferência independente da entrega:** `/root/qa_validator` confirmou o relatório Release reprovado, o bloqueio do teste e o EXE anterior de 710.656 bytes como único arquivo em `dist`, com hash acima. Executou o empacotamento do novo EXE compilado com sucesso, mas não aprovou a entrega sem o teste impedido. Também confirmou o hash preservado do workspace. Não iniciou o aplicativo nem repetiu o teste bloqueado.
- **PENDENTE — jogo e monitor físico:** PNGs e testes das próprias janelas não comprovam posicionamento, foco ou fluidez da aura no jogo, nem validação no monitor de 34 polegadas. O teste real desses novos formatos continua pendente.

## Entrega anterior: tema escuro, atualização do painel e efeitos

Base Git: `11b5dba0d546b24ad831e90fb0b93e449799dba8`, diff desta entrega em 21/09/2026. Pedido: interface profissional preta/vermelha, correção das piscadas e novos efeitos com cor da habilidade. Plano e critérios em [superpowers/plans/2026-09-21-tema-efeitos.md](superpowers/plans/2026-09-21-tema-efeitos.md). Implementação dividida entre `/root` (integração/persistência/captura/testes), `/root/theme_ui` (tema/pintura) e `/root/overlay_effects` (raster/efeitos). Revisores não editaram a implementação.

- **APROVADO — atualização atômica:** a reconstrução suspende pintura, restaura campos/seleções e reativa a janela antes de devolver foco/caret. O teste força `UpdateWindow` durante as notificações de criação/destruição e rejeita pintura intermediária. Clicar na aba/HUD/set já selecionados conserva os controles e não interrompe leitura. Rascunho, cursor, janela originalmente oculta e recuperação após exceção são exercitados. A preparação do teste precisou ativar sua própria janela antes de estabelecer foco; isso não foi tratado como prova de defeito no produto.
- **APROVADO — controles e aparência estática:** `app_flow` testa mensagens nativas de teclado em checkbox e combo. Os PNGs `build/logs/ui/ui-{monitor,huds,status,regras}.png`, gerados por janelas do próprio teste em 860×700, foram inspecionados pelo coordenador e pelos dois revisores. Tema carvão/vermelho/dourado consistente, campos e listas escuros, textos legíveis, sem cortes/sobreposições nos estados observados. Diálogos comuns e barras de rolagem conservam aparência nativa do Windows.
- **APROVADO — efeitos:** Borda, Brilho, Pulso (ciclo de 1,5 s) e Halo usam pixels premultiplicados e mantêm transparente todo o retângulo da habilidade. Testes verificam raster, opacidade, recorte nas bordas, cores/canais, estilos click-through/noactivate, política de exclusão da captura e ocultação imediata com condição falsa, alvo inválido e foco ausente. O pulso reaproveita o bitmap e muda a opacidade. A prancha `build/logs/ui/overlay-effects.png` foi inspecionada: efeitos distintos e centro preservado; dois estados estáticos não comprovam fluidez da animação.
- **APROVADO — cor da habilidade:** o botão salva o editor, resolve o destino atual, pausa a leitura e captura somente essa região uma vez. Extração cromática tem limiar de evidência e preserva a cor em imagens neutras. O teste injeta o capturador na mesma rotina para verificar a ROI recém-editada, painel oculto, leitura parada e restauração após falha. Cor extraída persiste e não é substituída pela paleta ao reconstruir/salvar o editor. Essa injeção não comprova captura DXGI real.
- **APROVADO — dados:** `workspace` passou em 120 verificações. Leitura de `glow` antigo, prioridade de `effect`, quatro valores, rejeição de efeito inválido e gravação atômica preservando HUDs/cores estão cobertas. O teste de campo novo reproduziu rejeição antes da implementação. A captura de cor não acrescenta fonte/ROI ao monitor contínuo. AppData real permaneceu com SHA-256 `988CE316A260EFA645473C50765C0ABDC4F4A550299792B49E9269C54A2944F4`.
- **APROVADO — revisão de produto/arquitetura:** `/root/product_validator` conferiu a base/diff, as quatro páginas, a prancha e o delta final da captura testável. Nenhum P0/P1/P2 remanescente. A hipótese de restauração de foco antes da visibilidade levou à correção da ordem no lock de pintura.
- **APROVADO — QA de código/harness:** `/root/qa_validator` executou independentemente `workspace_tests` (120/0), `app_flow_tests`, `monitor_tests` e `overlay_effect_tests` Debug; todos com saída zero. Inspecionou páginas/prancha e aprovou o escopo sem P0/P1/P2.
- **APROVADO — build e publicação automática:** builds canônicos com `-Clean` passaram nas **14/14 suítes Debug e 14/14 Release**, com warnings como erros. Release publicou somente `dist/AlbionAssistant.exe`, **710.656 bytes**, SHA-256 **`D48F402BD06656BC132F5DDBE83A757F01D46FF5990E7C3D5B59E99346B3535F`**. Logs/relatórios finais em `build/logs/Debug*` e `Release*`; base acima com alterações locais. O commit que contém este registro preserva o diff.
- **APROVADO — conferência independente da entrega:** `/root/qa_validator` confirmou os dois relatórios de 14 suítes e executou `app_flow` e `overlay_effect` Release, packaging e `validate-delivery`, todos aprovados. Conferiu o único arquivo em `dist`, tamanho/hash acima, igualdade com o build testado e hash preservado do workspace real.
- **PENDENTE — interação visual ao vivo e jogo:** a ferramenta de controle do Windows falhou com `foreground window did not report a process id`, inclusive após atualizar a janela e tentar novamente. Não foi possível observar perceptivamente as transições, a fluidez do pulso, a captura da cor no jogo ou os efeitos na HUD física de 34 polegadas. Os testes instrumentados e PNGs não substituem esse escopo. A leitura de 3 stacks já confirmada pelo usuário antes desta entrega não foi revalidada no jogo após os novos efeitos.

## Correção anterior: medição de ícone somente para busca de status

Base Git: `aa640987f44e8ecc6f75807d455a5499971acdc8`, diff desta entrega. Em 21/09/2026, o usuário confirmou que a leitura e o destaque funcionaram, mas mostrou a HUD oferecendo medição tanto para E (destino) quanto para Q (área ainda sem regra). A origem `meu-personagem` já estava medida em 40 px. O problema corrigido é a apresentação de uma exigência de leitura como se fosse necessária para destacar uma habilidade.

Critérios: destino exclusivo mostra somente a seleção da área e dispensa medição; área sem regra apresenta medição opcional; origem e uso duplo mantêm medição; usos configurados consideram todos os sets, inclusive regras desativadas, com `sameName`. Essa classificação é apenas visual: os bloqueios continuam seguindo as regras habilitadas do set ativo. Medidas salvas são preservadas e reaparecem se a área passar a ser origem. Nenhum campo/schema, regra ou algoritmo de reconhecimento foi alterado. O botão e a orientação de início usam “Medir ícone de status”.

- **APROVADO — regressão de interface:** o teste novo falhou antes do patch com “destino E pede medição de ícone”. Depois, `app_flow` Debug passou cobrindo destino sem medida e com medida salva, Q sem regra, área sem coordenadas, origem sem medida, uso duplo em outro set desativado, troca de set e persistência.
- **APROVADO — isolamento do teste:** o primeiro Release foi bloqueado por acesso inválido em `workspace`. Pastas remanescentes do processo falho mostraram o cenário Custom reutilizando o `workspace.ini` vazio de outra fixture: o nome usava PID e relógio sem diferenciar criações no mesmo instante. A regressão determinística com timestamp igual reproduziu a colisão. O helper de teste agora usa contador por processo e exige pasta nova; a correção não altera código do aplicativo. `workspace` Debug passou com 93 verificações, incluindo isolamento e limpeza de temporários coexistentes.
- **APROVADO — produto/arquitetura e visual:** `/root/product_validator` revisou o diff e os quatro PNGs `build/logs/ui/ui-hud-{destino,sem-regra,uso-duplo,leitura}.png`. Textos completos, legíveis e sem sobreposição nesses estados; nenhum achado bloqueante. A correção do helper de teste também foi revisada.
- **APROVADO — QA de lógica/visual:** `/root/qa_validator` revisou os mesmos estados e o helper, executou independentemente `app_flow` e `workspace` Debug finais, ambos com saída zero. Nenhum P0/P1/P2 remanescente no código revisado.
- **APROVADO — build e publicação:** Release canônico final passou nas 13/13 suítes e publicou somente `dist/AlbionAssistant.exe`, **678.400 bytes**, SHA-256 **`8472958CE20A8FDA6208074FB811C1044F88CC28E35456D2CA480EBED534CC89`**. Evidências finais em `build/logs/Release.log` e `Release-validation.json`; a execução inicialmente reprovada não publicou o artefato.
- **APROVADO — entrega independente:** QA executou `app_flow` Release e três execuções consecutivas de `workspace` Release (93/0 em todas), além de empacotamento e validador de entrega. Conferiu as 13 suítes do relatório canônico, o item único em `dist`, o tamanho e a igualdade do SHA-256 entre build e publicação.
- **PENDENTE — nova sessão no jogo:** o relato positivo do usuário refere-se ao programa anterior a esta alteração de interface. Não foi repetida a seleção/captura/ação sobre o jogo nesta entrega. A implementação não alterou os dados reais do usuário; a conferência de AppData manteve SHA-256 `988CE316A260EFA645473C50765C0ABDC4F4A550299792B49E9269C54A2944F4`.

## Correção anterior: referência confundida com captura do jogo

Base Git: `423e460f661e3b39ef03204dd8f701553c587af0`. Em 20/09/2026, o usuário mostrou o Monitor exibindo o ícone de referência após tentar iniciar com calibração pendente. O mesmo buffer servia à biblioteca, à seleção e à captura. A correção separa referência e captura, descarta a captura anterior antes de validar uma nova tentativa e identifica HUD, área e botão no aviso de calibração. O rodapé apresenta uma orientação curta; os detalhes do bloqueio permanecem no resumo rolável, inclusive quando a resolução/escala não corresponde à HUD.

Critérios verificados: somente frames do capturador alimentam a prévia do Monitor; abrir Status não substitui essa captura; a referência continua na biblioteca; início bloqueado por calibração ou campo inválido não exibe imagem antiga; configurações inválidas não são gravadas; a orientação de resolução/escala continua visível. Não houve mudança no reconhecimento, nas regras ou no formato dos dados.

Evidências:

- **APROVADO — regressão:** antes do patch, `app_flow` falhou com “referência do status apareceu como captura após início bloqueado”. A revisão independente identificou o caso de campo inválido antes da limpeza; a regressão falhou antes da correção e passou depois. O ajuste do rodapé também teve regressão de tela incompatível reproduzida e corrigida.
- **APROVADO — testes:** 13/13 suítes passaram em Debug na primeira correção; após os ajustes da revisão, `app_flow` Debug foi recompilado e passou novamente. O build Release final executou 13/13 suítes com sucesso, publicou o EXE e conferiu seus recursos e hash. Evidências em `build/logs/Release.log` e `Release-validation.json` (base acima com alterações locais).
- **APROVADO — interface estática:** coordenador e `/root/qa_validator` inspecionaram `build/logs/ui/ui-monitor-blocked.png` e `ui-monitor-screen-mismatch.png`, gerados pelas janelas do próprio teste. Mostram ausência de captura falsa, orientação completa e rodapé sem corte. A imagem da biblioteca foi conferida em `ui-status.png`. Isso não é teste sobre o jogo.
- **APROVADO — revisão de lógica/UX:** `/root/qa_validator`, somente leitura, reconferiu o diff final e executou `app_flow` Debug independentemente, com saída zero. Nenhum P0/P1/P2 remanescente nesse escopo.
- **APROVADO — entrega independente:** o mesmo revisor conferiu as 13/13 suítes do Release final e executou empacotamento e validador contra o EXE publicado; ambos passaram. Confirmou arquivo único, tamanho e SHA-256 igual ao build.
- **PENDENTE — jogo real:** calibração, captura, reconhecimento e overlay na HUD física de 34 polegadas após esta correção. O snapshot do usuário tinha `iconCalibrated=0` na origem `meu-personagem`; o programa deve continuar bloqueando até calibrar. O frame usado na regressão é uma imagem de teste entregue ao consumidor de captura, não uma prova de captura real. Nenhum dado de AppData foi alterado pela implementação.

Artefato publicado: somente `dist/AlbionAssistant.exe`, **677.376 bytes**, SHA-256 **`34FD68A374FF165D6AF0E130873478B92B747371A26E3A94384934F3178369BD`**. O usuário fechou o aplicativo antes da atualização. O commit que contém este registro preserva o diff validado.

## Entrega anterior: HUDs, status e sets separados

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
