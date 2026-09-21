# Albion Assistant

Aplicativo Windows x64 para acompanhar buffs/debuffs pela imagem do jogo e destacar áreas conforme regras configuráveis. HUDs guardam a configuração da tela; sets guardam as regras; status ficam em uma biblioteca compartilhada. O exemplo de Espírito Assassino com 3 stacks continua disponível.

## Executar

Abra **`dist/AlbionAssistant.exe`**. É o único arquivo necessário para usar o programa: ícone, manifesto, informações de versão e referências de reconhecimento estão embutidos, com runtime C++ estático. Não precisa de launcher, pasta de imagens ou terminal. Abrir novamente traz a instância existente para frente.

Para criar uma HUD, área, status, set ou regra, clique em **Novo**, informe o nome e confirme em **Criar** ou Enter. Esc ou Cancelar fecha sem criar nada nem perder uma edição anterior. Nomes vazios ou repetidos são explicados no próprio cadastro. Ao editar um item existente, o rodapé avisa que há alterações pendentes; **Salvar** ou trocar de item/seção grava a edição.

1. Abra o Albion em janela ou janela sem bordas. Em **Monitor → Conectar ao jogo**, conecte à janela.
2. Em **HUDs**, escolha uma HUD ou crie outra. Adicione áreas com nomes claros, como “Meus status” e “Habilidade E”. Em **Selecionar área**, escolha **Retângulo** (dois cantos) ou **Círculo** (centro e raio), sempre por dois cliques separados, e confirme. Essa forma fica salva na área e define também o formato de seu destaque. Para receber destaque, basta selecionar a área da habilidade; não é preciso medir seu ícone. Nas áreas usadas para buscar status, **Medir ícone de status** pede um recorte justo de um único ícone inteiro. A medição aparece como opcional enquanto a área não estiver ligada a uma regra. Os usos indicados consideram as regras de todos os sets. Esc cancela; F2 move o painel de seleção.
3. Em **Status → Novo status**, dê um nome e indique buff ou debuff. **Capturar referência** permite selecionar o ícone do jogo; **Importar imagem** aceita um recorte existente. Para regras de contagem, digite o valor e **Capture uma amostra** quando esse número estiver visível. Inclua o número completo na prévia: ao escolher Círculo, o recorte conserva os cantos de seu quadrado delimitador. Cadastre cada contagem que deseja usar. Presença e ausência dispensam essas amostras.
4. Em **Sets e regras**, crie um set e suas regras. Escolha o status, a área onde ele aparece, a condição (presente, ausente ou quantidade exata de stacks), a área de destino e o destaque: **Borda**, **Brilho**, **Pulso** ou **Halo**. Escolha uma cor da paleta ou use **Cor da habilidade** para capturar a cor predominante da área de destino no jogo. A captura é pontual: repita o botão se quiser atualizar a cor. Se o ícone estiver escuro ou sem cor nítida, a cor anterior é mantida. A frase abaixo dos campos resume a ação. Você pode reordenar ou desativar regras.
5. **Testar destaque por 5 s** mostra somente a ação, com aviso TESTE e leitura pausada. Depois, em **Monitor**, escolha HUD e set e use **Iniciar leitura**. O painel mostra cada leitura e o estado das ações.

F8 abre o painel; F9 inicia ou para a leitura. O destaque apaga quando o jogo perde foco ou a informação fica incerta/expirada. O padrão de validade é 750 ms.

Ao voltar ao painel, o Monitor conserva a última leitura para consulta, com sua idade e o aviso de que ela não aciona destaque. Parar ou trocar a sessão limpa esse histórico. A busca tolera uma diferença de até dois pixels na medição manual quando o tamanho informado não confirma a identidade; não muda a medida salva nem reduz os critérios de reconhecimento.

Para acrescentar um aro regressivo, marque **Aro com previsão de tempo (experimental)** na regra. Ele aprende a velocidade da sombra radial do buff e continua animando nos trechos encobertos, enquanto a presença e a condição da regra continuam confirmadas. Não começa uma contagem ao atingir 3 stacks. Uma renovação observada reinicia a previsão. O Monitor separa **relógio observado** de **aro estimado**. O teste de 5 segundos continua identificado como **SIMULAÇÃO DO ARO**.

Após três ciclos com términos semelhantes, o aro ajusta seu fim ao desaparecimento observado daquele status. Para aprender, deixe o buff expirar naturalmente algumas vezes, sem parar a leitura ou sair do jogo. O aprendizado vale somente para a sessão de leitura atual; parar, trocar de fonte ou perder o foco reinicia o aprendizado. Um desaparecimento isolado não altera o término. Evidências contraditórias descartam a calibração aprendida. Consumo repetido no mesmo instante pode parecer expiração natural: a captura não distingue as duas causas, portanto o término é aproximado.

O exemplo de Espírito Assassino inclui uma referência própria do relógio. Para outro status, renove o efeito antes de clicar em **Status → Capturar relógio** e selecione o ícone inteiro, iluminado, sem a sombra do relógio. Essa captura preserva a referência de identidade e as amostras de stacks. Trocar a identidade do status invalida a referência temporal anterior. Uma referência ausente ou inválida deixa somente o aro indisponível, com aviso no Monitor; a regra e a aura continuam funcionando.

Antes de reunir leituras suficientes, o aro ainda pode ficar indisponível quando não há fronteira legível. Uma renovação inteiramente encoberta pode escapar da previsão. O aro estimado pode terminar antes ou depois do término real; presença desconhecida/ausente, captura vencida e condição inválida apagam o destaque independentemente da previsão. Outros status ainda precisam de validação no jogo. Testes sintéticos também exercitam ícones de 40, 48, 64 e 96 pixels; ícones muito pequenos podem não fornecer evidência suficiente.

O exemplo inclui amostras nativas de 40 e 64 pixels para os contadores e o relógio. A referência temporal é escolhida pelo tamanho medido na HUD; uma referência personalizada tem prioridade. Isso corrige a incompatibilidade observada na HUD ultrawide de 21/09/2026 e as quedas no reconhecimento do número pequeno, conservando os critérios de confiança. A aura depende da regra; a indisponibilidade do relógio afeta somente o aro.

## Dados e telas

As configurações ficam em **`%LOCALAPPDATA%\AlbionAssistant`**: `workspace.ini`, imagens em `status-images` e diagnósticos quando ativados. Atualizar o EXE ou limpar o build não altera esses dados. O parâmetro opcional `--settings caminho.ini` isola o workspace e as imagens na pasta escolhida.

Na primeira abertura, `settings.ini` e `hud-profiles` da versão anterior são importados automaticamente, sem alterar os originais. HUDs e regras divergentes são preservadas separadamente. Os contadores 2/3 que a configuração anterior usava são mantidos como amostras explícitas dos status importados. Cadastros novos não herdam essas amostras. Depois de criado `workspace.ini`, a importação não se repete; excluir a última HUD não a faz reaparecer.

Quem ainda tiver dados de um build antigo ao lado do EXE deve fechar o aplicativo e copiar `settings.ini`, `hud-profiles` e as referências personalizadas para a pasta de dados antes de usar o executável atual. Não sobrescreva arquivos/perfis já existentes: preserve os originais e confira as referências pelo painel. Não há importação automática de pastas antigas. Os dados deste ambiente já foram migrados durante a consolidação.

Salve uma HUD para cada resolução/layout, por exemplo “Notebook” e “Monitor 34”. HUDs guardam posições, formas, tamanho dos ícones, resolução, monitor e DPI conhecidos. Áreas antigas continuam retangulares até serem selecionadas novamente em outro formato. A mudança de layout dentro do jogo exige nova seleção ou troca manual de HUD. Uma tela incompatível apaga os destaques e exige a escolha da HUD correspondente.

Origem e destino têm formas independentes: uma faixa retangular de status pode acionar um círculo sobre o E. Na busca circular, enquadre o ícone inteiro; candidatos com o centro fora do círculo são ignorados. A imagem capturada continua incluindo o quadrado delimitador para preservar o contador nos cantos.

Use os mesmos nomes de áreas em HUDs diferentes para reutilizar um set. Trocar de set mantém a HUD; trocar de HUD mantém o set. Renomear uma área exige atualizar as regras que a referenciam. Dependências ausentes são mostradas no Monitor e bloqueiam a leitura, em vez de ignorar regras silenciosamente.

As edições são salvas antes de trocar de página ou seleção; há também botões de salvar. **Excluir HUD** remove apenas a HUD e suas áreas, após confirmação. **Excluir set** remove suas regras e preserva a biblioteca. Um status utilizado por regras precisa ser desvinculado antes da exclusão. As amostras embutidas do exemplo 2/3 são fixas; capture outra referência para substituí-las por amostras próprias.

## Desenvolvimento

Requisitos: Windows x64, Visual Studio Build Tools com C++/MSVC, SDK do Windows, CMake e Ninja. O script encontra as ferramentas do Visual Studio e funciona no Windows PowerShell 5.1 e PowerShell 7.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Esse comando configura, compila, executa os testes e publica **`dist/AlbionAssistant.exe`** somente após aprovação dos testes. Execuções seguintes são incrementais. O progresso é resumido; o log completo fica em `build/logs/Release.log`.

O build recusa arquivos extras em `dist`, falha se nenhum teste for encontrado e confere o EXE publicado, incluindo seu SHA-256. O resultado automático fica em `build/logs/Release-validation.json`. A revisão independente segue [docs/processo.md](docs/processo.md); os pareceres e testes visuais pendentes ficam em [docs/validacao.md](docs/validacao.md). Passar no build não aprova automaticamente a interface no jogo.

- `-Configuration Debug`: compila e testa para desenvolvimento, sem substituir a entrega Release.
- `-Clean`: recria apenas o cache da configuração escolhida, preservando os dados do usuário.
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

Cada set aceita até 32 regras; cada HUD, até 32 áreas. A biblioteca aceita até 64 status e amostras rotuladas de **1 a 99 stacks**. Esses números são rótulos de imagens cadastradas, não OCR universal: o usuário precisa capturar o contador visível. A leitura espera ícones com contador branco no canto inferior direito, como nas amostras do Albion. Recortes têm entre 24 e 256 pixels. Contador ilegível permanece desconhecido; ausência de número não significa 1. O acompanhamento opcional do relógio estima somente uma fração visual, sem cálculo de segundos restantes.

Vários status são acompanhados no mesmo ciclo; regras do mesmo status na mesma área compartilham a leitura. Cada ação usa sua própria condição. Se duas regras verdadeiras usam a mesma área de destino, vence a primeira na lista. Borda e Halo contornam a forma selecionada com centro transparente. Brilho cria uma aura translúcida sobre a habilidade e ao redor dela; Pulso varia suavemente essa intensidade. Todos permitem clicar na habilidade. A captura de cor não aumenta a área de leitura contínua.

O cadastro é genérico, mas a precisão depende do recorte e da aparência do status. Ícones muito semelhantes ou monocromáticos precisam de validação específica; o conjunto inteiro de habilidades do jogo ainda não foi testado.

A captura usa DXGI e frames disponíveis, sem limitador fixo de 30/60 FPS nem fila crescente. Precisa de sessão Windows desbloqueada e jogo visível. Tela cheia exclusiva, HDR, monitor girado e janela distribuída entre monitores ainda não foram validados. Não há leitura de memória do jogo, injeção, interceptação de rede nem login automático.

`--diagnostics` grava mudanças dos estados das ações na pasta de dados. `--diagnostics --show-overlay-in-capture` inclui os destaques na captura para inspeção; exige áreas de leitura e destaque separadas. As evidências e pendências estão em [docs/validacao.md](docs/validacao.md).
