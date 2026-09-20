# Albion Assistant — POC ao vivo

Aplicativo Windows em português para destacar uma região da HUD quando Espírito Assassino chega a **3 stacks**. A ação inicial destaca Golpe Fantasma. A leitura usa apenas a imagem visível do jogo; a POC não aperta habilidades nem joga pelo usuário.

## Abrir e usar

1. Abra o Albion em modo janela ou janela sem bordas, com a HUD visível.
2. Abra **Iniciar.cmd** nesta pasta. A versão compilada está em `build/windows/Release/AlbionAssistant.exe`.
3. Clique em **Conectar ao Albion**. Na primeira configuração, selecione **Área dos buffs**: clique em dois cantos opostos da região que contém a linha de status do personagem. Não precisa arrastar. Inclua espaço para o ícone mudar de posição quando outros buffs aparecerem.
4. Selecione **Área do destaque** com dois cliques ao redor de Golpe Fantasma, ou da informação que deseja destacar. Esc cancela a seleção.
5. Informe o **diâmetro do ícone em pixels da captura**, incluindo sua borda. Na HUD testada em 2880 × 1800 foi usado 64 px. Esse valor depende da escala da HUD, não das polegadas do monitor.
6. Mantenha a condição **stacks iguais a 3**, escolha a cor e marque **Regra ativa**. Clique em **Iniciar leitura**. O jogo volta ao primeiro plano.

**F8** abre o painel; **F9** inicia ou para a leitura. Ao abrir o painel ou trocar de aplicativo, o destaque desaparece. Retornando ao jogo com a leitura ligada, a captura retoma. A configuração é salva ao selecionar áreas, iniciar ou clicar em Salvar.

O painel exibe o estado atual e a última imagem capturada. A imagem pode continuar visível depois de parar; o texto acima informa se a leitura está válida. **Salvar amostra** grava essa região em `samples`, ao lado do executável.

## Regras e calibração

Esta POC edita **uma regra por configuração**: nome, perfil, condição (presente, ausente ou stacks iguais), valor, ativação e cor do destaque. O nome do perfil é uma identificação; não é um gerenciador de múltiplos perfis. Configurações distintas podem ser abertas com `--settings caminho-do-arquivo.ini`.

O reconhecimento de stacks é específico para os números **2 e 3** das referências fornecidas. Um número que não pode ser lido permanece desconhecido; a ausência de número não é interpretada como 1. O relógio radial é tratado como variação visual e **não fornece uma contagem de segundos restantes**. Uma regra de stacks só acende quando o número é reconhecido. Para a condição de presença, a identidade do ícone é suficiente.

**Ícone de referência** aceita um recorte PNG/BMP de um único ícone. Isso substitui sua identidade visual, mas mantém os modelos de dígitos 2/3 do preset. Não constitui suporte validado para qualquer buff ou arma. Para voltar ao preset, remova o valor `referencePath` da seção `[rule]` do arquivo de configuração com o programa fechado.

Selecione novamente as áreas e ajuste o diâmetro quando mudar a resolução, escala ou posição da HUD. Mudar o tamanho do cliente invalida as áreas anteriores. Mudar de monitor interrompe a sessão atual; reconecte e confira a calibração antes de reiniciar. Um monitor de 34 polegadas pode funcionar após essa calibração; esta entrega não foi testada fisicamente nesse monitor.

## Captura e limites

- Captura DXGI do monitor que contém o jogo, copiando somente a região selecionada para processamento. O worker acompanha os frames disponíveis; não existe limitador fixo de 30/60 FPS nem fila de imagens atrasadas.
- Overlay nativo transparente aos cliques, sem tomar foco, exibido somente com o jogo em primeiro plano e informação recente.
- Leitura incerta, inválida ou expirada apaga o destaque. A validade padrão é 750 ms. Parar ou trocar de sessão invalida resultados anteriores.
- Precisa de uma sessão Windows desbloqueada e do jogo visível. Não reconecta o personagem automaticamente. Captura de tela cheia exclusiva, monitor girado e janela distribuída entre monitores não são suportes validados; use janela sem bordas com a região inteira em um único monitor.
- Nenhuma leitura de memória do jogo, injeção, interceptação de rede ou automação de combate.

## Compilar e testar

Requer Windows x64, Visual Studio Build Tools com C++/MSVC, SDK do Windows, CMake e Ninja. O script encontra as ferramentas instaladas pelo Visual Studio.

```powershell
powershell -NoProfile -File .\scripts\build-windows.ps1 -Configuration Release
powershell -NoProfile -File .\scripts\build-windows.ps1 -Configuration Debug
```

Cada compilação executa os testes de regras/configuração e reconhecimento. O runtime C++ é vinculado estaticamente. Os arquivos em `assets` precisam acompanhar o executável.

Para investigar a leitura, use `AlbionAssistant.exe --diagnostics`: gera `diagnostics.csv` com mudanças de estado e até 30 imagens da região por execução. Os tempos registrados são milissegundos desde o início da sessão Windows; o score é similaridade visual, não probabilidade. `--diagnostics --show-overlay-in-capture` inclui a borda nas capturas para inspeção visual; nesse modo, as áreas de leitura e destaque precisam estar separadas.

Os testes e as verificações realizadas estão documentados em `docs/validacao.md`. O projeto anterior em `C:/projetos/pessoais/albion-assistant` foi preservado.
