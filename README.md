# Albion Assistant

Aplicativo Windows x64 para acompanhar buffs/debuffs pela imagem do jogo e destacar áreas conforme regras configuráveis. HUDs guardam a configuração da tela; status e regras ficam em bibliotecas compartilhadas; perfis selecionam as regras do set.

## Executar

Abra **`dist/AlbionAssistant.exe`**. É o único arquivo necessário para usar o programa: ícone, manifesto, informações de versão e referências de reconhecimento estão embutidos, com runtime C++ estático. Não precisa de launcher, pasta de imagens ou terminal. Abrir novamente traz a instância existente para frente.

Para criar uma HUD, área, status, set ou regra, clique em **Novo**, informe o nome e confirme em **Criar** ou Enter. Esc ou Cancelar fecha sem criar nada nem perder uma edição anterior. Nomes vazios ou repetidos são explicados no próprio cadastro. Ao editar um item existente, o rodapé avisa que há alterações pendentes; **Salvar** ou trocar de item/seção grava a edição.

1. Abra o Albion em janela ou janela sem bordas. Em **Monitorar → Conectar**, conecte à janela.
2. Em **HUDs**, escolha uma HUD ou crie outra. Adicione áreas com nomes claros, como “Meus status” e “Habilidade E”. Em **Selecionar área**, escolha **Retângulo** (dois cantos) ou **Círculo** (centro e raio), sempre por dois cliques separados, e confirme. Essa forma fica salva na área e define também o formato de seu destaque. Para receber destaque, basta selecionar a área da habilidade; não é preciso medir seu ícone. Nas áreas usadas para buscar status, **Medir ícone de status** pede um recorte justo de um único ícone inteiro. A medição aparece como opcional enquanto a área não estiver ligada a uma regra. Os usos indicados consideram as regras da biblioteca. Esc cancela; F2 move o painel de seleção.
3. Em **Status → Novo status**, dê um nome. **Capturar referência** permite selecionar o ícone do jogo; **Importar imagem** aceita um recorte existente.
4. Em **Regras**, crie cada regra uma vez. Escolha status ou vida, a origem, a condição (presente, ausente ou quantidade exata de stacks), a área de destino e **Borda**, **Brilho**, **Pulso**, **Halo** ou **Chamas**. Para stacks, capture a amostra enquanto o número está visível no ícone. Presença e ausência dispensam essa captura. A cor pode ser escolhida na paleta ou capturada da habilidade. Editar a regra altera seu comportamento em todos os perfis que a utilizam.
5. Em **Perfis**, crie um perfil para cada set e use **Adicionar ao perfil** para vincular regras existentes. A ordem define a prioridade; **Ativada neste perfil** vale somente para ele. **Remover do perfil** conserva a regra na biblioteca. Excluir o perfil também conserva as regras. Para excluir uma regra da biblioteca, remova primeiro seus vínculos em todos os perfis.
6. **Testar destaque** mostra somente a ação por 5 s, com aviso de simulação. Depois, em **Monitorar**, escolha HUD e perfil e use **Iniciar**. O painel mostra cada leitura e o estado das ações.

F8 abre o painel; F9 inicia ou para a leitura. O destaque apaga quando o jogo perde foco ou a informação fica incerta/expirada. O padrão de validade é 750 ms.

O filtro **Somente fora do cooldown** é opcional. Ao selecionar uma área pequena de habilidade na HUD, o aplicativo guarda também a imagem daquele instante: confira a prévia e marque **Estava pronta** somente se a habilidade realmente estava fora da recarga. Áreas já selecionadas antes desse recurso podem usar **Capturar habilidade pronta** sem serem reposicionadas. Ativar o filtro em uma regra exige imagem confirmada na HUD atual; durante a leitura, a imagem atual precisa corresponder à referência pronta em um quadro recente. Na área circular, o filtro ignora o chão e os dois pixels mais externos do contorno que variam com o cenário; o centro, o contador e a sombra interna continuam na comparação. Um cooldown reconhecido ou uma imagem incerta apagam o destaque. Uma habilidade por área/HUD: ao trocar o ícone de habilidade no mesmo slot, recapture a referência enquanto a habilidade correspondente estiver pronta.

O filtro vale para **todas as regras que destacam o mesmo destino**, inclusive regras antigas sem o filtro marcado, para que uma regra posterior não acenda a habilidade ainda em recarga. Por exemplo, se duas regras destacam D, ativar o filtro em uma delas impede que qualquer uma destaque D durante o cooldown; outros destinos não são afetados. O teste visual de cinco segundos continua sendo simulação. Desative **Incluir overlay no compartilhamento** ao monitorar uma habilidade filtrada, para que o próprio destaque não contamine sua imagem. A leitura por imagem ainda requer validação com capturas da mesma habilidade pronta e em cooldown no jogo.

Ao voltar ao painel, o Monitor conserva a última leitura para consulta, com sua idade e o aviso de que ela não aciona destaque. Parar ou trocar a sessão limpa esse histórico. A busca tolera uma diferença de até dois pixels na medição manual quando o tamanho informado não confirma a identidade; não muda a medida salva nem reduz os critérios de reconhecimento.

Para acrescentar um aro regressivo, marque **Aro com previsão de tempo (experimental)** na regra. Ele aprende a velocidade da sombra radial do buff e continua animando nos trechos encobertos, enquanto a presença e a condição da regra continuam confirmadas. Não começa uma contagem ao atingir 3 stacks. Uma renovação observada reinicia a previsão. O Monitor separa **relógio observado** de **aro estimado**. O teste de 5 segundos é **SIMULAÇÃO DO ARO**, sem medir o buff; no uso real, a aura pode acender sozinha quando a frente temporal não é confiável. Em regras com condições alternativas, o aro segue apenas o status que acionou a regra: um status sem referência temporal própria não recebe a previsão de outro.

Após três ciclos com términos semelhantes, o aro ajusta seu fim ao desaparecimento observado daquele status. Para aprender, deixe o buff expirar naturalmente algumas vezes, sem parar a leitura ou sair do jogo. O aprendizado vale somente para a sessão de leitura atual; parar, trocar de fonte ou perder o foco reinicia o aprendizado. Um desaparecimento isolado não altera o término. Evidências contraditórias descartam a calibração aprendida. Consumo repetido no mesmo instante pode parecer expiração natural: a captura não distingue as duas causas, portanto o término é aproximado.

O exemplo de Espírito Assassino inclui uma referência própria do relógio. Para outro status, renove o efeito antes de clicar em **Status → Capturar relógio** e selecione o ícone inteiro, iluminado, sem a sombra do relógio. Essa captura preserva a referência de identidade e as amostras de stacks. Trocar a identidade do status invalida a referência temporal anterior. Uma referência ausente ou inválida deixa somente o aro indisponível, com aviso no Monitor; a regra e a aura continuam funcionando.

Antes de reunir leituras suficientes, o aro ainda pode ficar indisponível quando não há fronteira legível. Uma renovação inteiramente encoberta pode escapar da previsão. O aro estimado pode terminar antes ou depois do término real; presença desconhecida/ausente, captura vencida e condição inválida apagam o destaque independentemente da previsão. Outros status ainda precisam de validação no jogo. Testes sintéticos também exercitam ícones de 40, 48, 64 e 96 pixels; ícones muito pequenos podem não fornecer evidência suficiente.

O exemplo inclui amostras nativas de 40 e 64 pixels para os contadores e o relógio. A referência temporal é escolhida pelo tamanho medido na HUD; uma referência personalizada tem prioridade. Isso corrige a incompatibilidade observada na HUD ultrawide de 21/09/2026 e as quedas no reconhecimento do número pequeno, conservando os critérios de confiança. A aura depende da regra; a indisponibilidade do relógio afeta somente o aro.

## Dados e telas

As configurações ficam em **`%LOCALAPPDATA%\AlbionAssistant`**: `workspace.ini`, imagens em `status-images` e diagnósticos quando ativados. Atualizar o EXE ou limpar o build não altera esses dados. O parâmetro opcional `--settings caminho.ini` isola o workspace e as imagens na pasta escolhida.

Na primeira abertura, `settings.ini` e `hud-profiles` da versão anterior são importados automaticamente, sem alterar os originais. HUDs e regras divergentes são preservadas separadamente. Os contadores 2/3 que a configuração anterior usava são mantidos como amostras explícitas dos status importados. Cadastros novos não herdam essas amostras. Depois de criado `workspace.ini`, a importação não se repete; excluir a última HUD não a faz reaparecer.

Workspaces das versões anteriores (schemas 1–5) são abertos sem apagar o arquivo original. Cada regra antiga ganha uma definição própria na biblioteca e o perfil mantém seu vínculo, a ordem e a ativação. Regras antigas com o mesmo nome em perfis diferentes recebem sufixos; a aplicação não as une automaticamente. No **primeiro salvamento**, o programa guarda uma cópia exata ao lado do arquivo, chamada `workspace.ini.before-schema6.ini` (ou com sufixo numérico se o nome já existir), antes de substituir o workspace pelo schema 6. Se não puder conferir esse backup, mantém o workspace anterior. As imagens em `status-images` não são apagadas.

Para **voltar ao EXE anterior** após salvar com a versão nova, feche ambos os programas e restaure uma cópia `workspace.ini.before-schema6*.ini` como `workspace.ini`. Se houver várias, escolha a cópia correspondente **à migração que quer desfazer**; confirme data e conteúdo antes de substituir o arquivo atual. O sufixo `-1`, `-2` etc. indica apenas que já existia uma cópia com o nome anterior. O EXE anterior não lê schema 6; o retorno ao backup descarta edições de regras/perfis feitas depois daquela cópia. A restauração não exige recapturar HUD ou status existentes no backup. Mantenha as cópias enquanto precisar da opção de retorno.

Se a nova versão criou `workspace.ini` pela primeira vez (não havia arquivo antigo para copiar), conserve esse arquivo em outro lugar antes de removê-lo da pasta de dados para usar o EXE anterior. Havendo `settings.ini`/`hud-profiles` legados, o EXE anterior poderá importá-los novamente; configurações criadas somente no schema 6 não aparecerão nele.

Quem ainda tiver dados de um build antigo ao lado do EXE deve fechar o aplicativo e copiar `settings.ini`, `hud-profiles` e as referências personalizadas para a pasta de dados antes de usar o executável atual. Não sobrescreva arquivos/perfis já existentes: preserve os originais e confira as referências pelo painel. Não há importação automática de pastas antigas. Os dados deste ambiente já foram migrados durante a consolidação.

Salve uma HUD para cada resolução/layout, por exemplo “Notebook” e “Monitor 34”. HUDs guardam posições, formas, tamanho dos ícones, resolução, monitor e DPI conhecidos. Áreas antigas continuam retangulares até serem selecionadas novamente em outro formato. A mudança de layout dentro do jogo exige nova seleção ou troca manual de HUD. Uma tela incompatível apaga os destaques e exige a escolha da HUD correspondente.

Origem e destino têm formas independentes: uma faixa retangular de status pode acionar um círculo sobre o E. Na busca circular, enquadre o ícone inteiro; candidatos com o centro fora do círculo são ignorados. A imagem capturada continua incluindo o quadrado delimitador para preservar o contador nos cantos.

Use os mesmos nomes de áreas em HUDs diferentes para reutilizar as regras. Trocar de perfil mantém a HUD; trocar de HUD mantém o perfil. Renomear uma área exige atualizar as regras que a referenciam. Dependências ausentes são mostradas em Monitorar e bloqueiam a leitura, em vez de ignorar regras silenciosamente.

As edições são salvas antes de trocar de página ou seleção. **Excluir HUD** remove apenas a HUD e suas áreas, após confirmação. **Excluir perfil** conserva as regras da biblioteca. Um status utilizado por qualquer regra, mesmo avulsa, exige excluir ou editar a regra dependente antes de remover o status. As amostras embutidas do exemplo 2/3 são fixas; capture outra referência para substituí-las por amostras próprias.

## Desenvolvimento

Requisitos: Windows x64, Visual Studio 2026 com C++/MSVC e MSBuild, SDK do Windows e CMake. O script encontra as ferramentas do Visual Studio e funciona no Windows PowerShell 5.1 e PowerShell 7.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Esse comando configura, compila, executa os testes e publica **`dist/AlbionAssistant.exe`** somente após aprovação dos testes. Execuções seguintes são incrementais. O progresso é resumido; o log completo fica em `build/logs/Release.log`.

O build recusa arquivos extras em `dist`, falha se nenhum teste for encontrado e confere o EXE publicado, incluindo seu SHA-256. O resultado automático fica em `build/logs/Release-validation.json`. A revisão independente segue [docs/processo.md](docs/processo.md); os pareceres e testes visuais pendentes ficam em [docs/validacao.md](docs/validacao.md). Passar no build não aprova automaticamente a interface no jogo.

- `-Configuration Debug`: compila e testa para desenvolvimento, sem substituir a entrega Release.
- `-Clean`: recria apenas o cache da configuração escolhida, preservando os dados do usuário. Na primeira compilação após migrar do gerador Ninja para Visual Studio, use `-Clean` para substituir o cache antigo.
- `-Run`: abre o aplicativo após sucesso.

Os presets `windows-release` e `windows-debug` em `CMakePresets.json` também podem ser usados por uma IDE ou Developer PowerShell. A versão do produto é definida uma vez em `CMakeLists.txt`; o histórico de mudanças fica no Git.

| Pasta | Responsabilidade |
|---|---|
| `src` | Interface, captura, reconhecimento, regras e persistência |
| `resources` | Ícone, manifesto e recursos Windows |
| `assets` | Fontes das imagens embutidas e negativos de teste |
| `tests` | Testes e capturas reais de referência |
| `scripts` | Build e publicação |
| `docs` | Evidências e limites da validação |
| `build` | Cache e testes gerados; fora do Git |
| `dist` | Um único executável de entrega; fora do Git |

## Limites da POC

Cada perfil aceita até 32 vínculos de regras; cada HUD, até 32 áreas. A biblioteca aceita até 2.048 regras, 64 status e amostras rotuladas de **1 a 99 stacks**. Esses números são rótulos de imagens cadastradas, não OCR universal: o usuário precisa capturar o contador visível. A leitura espera ícones com contador branco no canto inferior direito, como nas amostras do Albion. Recortes têm entre 24 e 256 pixels. Contador ilegível permanece desconhecido; ausência de número não significa 1. O acompanhamento opcional do relógio estima somente uma fração visual, sem cálculo de segundos restantes.

Vários status são acompanhados no mesmo ciclo; regras do mesmo status na mesma área compartilham a leitura. Cada ação usa sua própria condição. Se duas regras verdadeiras usam a mesma área de destino, vence a primeira na lista. Borda e Halo contornam a forma selecionada com centro transparente. Brilho cria uma aura translúcida sobre a habilidade e ao redor dela; Pulso varia suavemente essa intensidade. Chamas combina línguas de luz e faíscas que percorrem o contorno, podendo sair até 32 px da área selecionada, sem esconder o centro; sua animação é atualizada pelo timer da interface. Todos permitem clicar na habilidade. A captura de cor não aumenta a área de leitura contínua.

O cadastro é genérico, mas a precisão depende do recorte e da aparência do status. Ícones muito semelhantes ou monocromáticos precisam de validação específica; o conjunto inteiro de habilidades do jogo ainda não foi testado.

A captura usa DXGI e frames disponíveis, sem limitador fixo de 30/60 FPS nem fila crescente. Precisa de sessão Windows desbloqueada e jogo visível. Tela cheia exclusiva, HDR, monitor girado e janela distribuída entre monitores ainda não foram validados. Não há leitura de memória do jogo, injeção, interceptação de rede nem login automático.

`--diagnostics` grava mudanças dos estados das ações na pasta de dados. No **Monitor**, a opção **Mostrar overlay no compartilhamento** inclui os destaques no Discord e em outras capturas; ela fica desligada por padrão e é salva. `--show-overlay-in-capture` continua disponível para diagnóstico. As evidências e pendências estão em [docs/validacao.md](docs/validacao.md).
