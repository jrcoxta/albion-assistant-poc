# Albion Assistant — POC com configuração guiada

Aplicativo Windows em português para destacar uma região da HUD quando Espírito Assassino chega a **3 stacks**. A ação inicial destaca Golpe Fantasma. A leitura usa apenas a imagem visível do jogo; a POC não aperta habilidades nem joga pelo usuário.

## Abrir e usar

1. Abra o Albion em modo janela ou janela sem bordas, com a HUD visível.
2. Feche outra instância do assistente e abra **Iniciar-v2.cmd** nesta pasta. A versão guiada está em `build/windows/GuidedRelease/AlbionAssistant.exe`. No pacote portátil v2, abra `Iniciar.cmd`.
3. Em **1 · Conectar**, clique em **Conectar ao Albion**. Escolha uma HUD salva e clique em **Carregar**, ou crie uma **Nova HUD** com um nome como “Notebook” ou “Monitor 34”.
4. Em **2 · Selecionar**, marque dois cantos opostos da região onde aparecem os buffs. Inclua espaço para o ícone mudar de posição quando outros buffs surgirem. A seleção acontece sobre uma imagem congelada, sem arrastar. Confira e clique em **Usar seleção**; Esc cancela.
5. Com Espírito Assassino visível no jogo, use **Selecionar ícone** e clique nele. Confira a sugestão na prévia ampliada. Se necessário, escolha **Ajustar manual** ou pressione M e marque dois cantos; o recorte será quadrado. O tamanho é obtido da seleção, sem precisar digitar pixels. F2 move o painel de seleção se ele cobrir a informação desejada.
6. Use **Selecionar destaque** para marcar dois cantos ao redor de Golpe Fantasma ou da informação que deseja destacar. Confirme a seleção.
7. Em **3 · Criar ação**, confira a frase “Quando Espírito Assassino tiver 3 stacks, destacar a área selecionada de Golpe Fantasma”. Escolha a condição, a cor e mantenha **Regra ativa** marcada.
8. Em **4 · Conferir e usar**, clique em **Testar destaque · 5 s**. A demonstração mostra uma borda e um aviso **TESTE**, sem depender de stacks e com a leitura real parada. Ao terminar, o painel retorna. Depois, clique em **Iniciar leitura**.

**F8** abre o painel; **F9** inicia ou para a leitura. Ambos encerram uma demonstração em andamento. Ao abrir o painel ou trocar de aplicativo, o destaque desaparece. Retornando ao jogo com a leitura ligada, a captura retoma. A configuração é salva ao confirmar uma seleção, testar, iniciar ou clicar em **Salvar HUD e ação**.

O painel exibe o estado atual e a última imagem capturada. A imagem pode continuar visível depois de parar; o texto acima informa se a leitura está válida. **Salvar amostra** grava essa região em `samples`, ao lado do executável.

## Regras e calibração

Esta POC edita **uma regra por HUD**: nome, conjunto de equipamento, condição (presente, ausente ou stacks iguais), valor, ativação e cor do destaque. As HUDs ficam em `hud-profiles`, ao lado do arquivo de configuração. A configuração ativa permanece em `settings.ini`; arquivos antigos são migrados preservando as regiões e a regra. Também é possível usar `--settings caminho-do-arquivo.ini`.

Use **Salvar HUD e ação** antes de carregar outra HUD ou criar uma nova se quiser conservar edições feitas nos campos. Carregar restaura a versão salva; Nova HUD inicia outra calibração e mantém a última ação e referência salvas. Um nome diferente salva outra HUD, sem apagar a anterior; nomes já utilizados por outra HUD são recusados.

O reconhecimento de stacks é específico para os números **2 e 3** das referências fornecidas. Um número que não pode ser lido permanece desconhecido; a ausência de número não é interpretada como 1. O relógio radial é tratado como variação visual e **não fornece uma contagem de segundos restantes**. Uma regra de stacks só acende quando o número é reconhecido. Para a condição de presença, a identidade do ícone é suficiente.

Em **Avançado**, **Outra referência** aceita um recorte PNG/BMP de um único ícone. Isso substitui sua identidade visual, mas mantém os modelos de dígitos 2/3 do preset. Não constitui suporte validado para qualquer buff ou arma. **Usar preset** restaura Espírito Assassino. Após trocar a referência, selecione o ícone novamente. Diâmetro manual, validade da leitura e salvar amostra também ficam em Avançado.

Os perfis registram dimensões do jogo, monitor e DPI conhecidos. A tela de conexão sugere uma HUD compatível, mas você escolhe qual carregar. Uma mudança incompatível impede o início da leitura; ao confirmar uma seleção nesse novo ambiente, a calibração ganha outro nome e preserva a HUD salva anterior. A posição das áreas é relativa à janela do jogo, sem esticar coordenadas entre notebook e ultrawide.

Mudanças na posição ou escala dos elementos **dentro do jogo** ainda exigem selecionar novamente ou carregar a HUD correspondente. As polegadas não definem as coordenadas: um monitor de 34 polegadas precisa de seu próprio perfil se a resolução ou o layout forem diferentes. O monitor físico de 34 polegadas ainda não foi validado.

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

Use `-BuildDirectory build/windows/GuidedRelease` para gerar uma compilação separada da primeira POC. Cada compilação executa os testes de regras/configuração, reconhecimento, perfis, sugestão de ícones, seletor e layout. O runtime C++ é vinculado estaticamente. Os arquivos em `assets` precisam acompanhar o executável.

Para investigar a leitura, use `AlbionAssistant.exe --diagnostics`: gera `diagnostics.csv` com mudanças de estado e até 30 imagens da região por execução. Os tempos registrados são milissegundos desde o início da sessão Windows; o score é similaridade visual, não probabilidade. `--diagnostics --show-overlay-in-capture` inclui a borda nas capturas para inspeção visual; nesse modo, as áreas de leitura e destaque precisam estar separadas.

Os testes e as verificações da primeira POC estão em `docs/validacao.md`; os da interface guiada ficam em `docs/validacao-v2.md`. O projeto anterior em `C:/projetos/pessoais/albion-assistant` e o pacote da primeira POC foram preservados.
