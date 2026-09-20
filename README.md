# Albion Assistant

Aplicativo Windows x64 que destaca uma região da HUD quando Espírito Assassino chega a **3 stacks**. Captura a imagem visível do jogo e exibe uma borda sobre Golpe Fantasma; não envia teclas de combate.

## Executar

Abra **`dist/AlbionAssistant.exe`**. É o único arquivo necessário para usar o programa: ícone, manifesto, informações de versão e referências de reconhecimento estão embutidos, com runtime C++ estático. Não precisa de launcher, pasta de imagens ou terminal. Abrir novamente traz a instância existente para frente.

1. Abra o Albion em janela ou janela sem bordas. Em **Conectar**, conecte ao jogo e escolha uma HUD salva ou **Nova HUD**.
2. Em **Selecionar**, marque dois cantos da região dos buffs. Com Espírito Assassino visível, aponte o ícone e confira a prévia. **Ajustar manual** ou M permite dois cliques; o tamanho é obtido do recorte. Marque também a área de Golpe Fantasma. Confirme cada seleção; Esc cancela e F2 move o painel para liberar a área coberta.
3. Em **Criar ação**, confira a condição, a cor e **Regra ativa**.
4. Em **Conferir e usar**, **Testar destaque** mostra uma demonstração por cinco segundos, com aviso TESTE. Depois, **Iniciar leitura** usa a regra real.

F8 abre o painel; F9 inicia ou para a leitura. O destaque apaga quando o jogo perde foco ou a informação fica incerta/expirada. O padrão de validade é 750 ms.

## Dados e telas

As configurações ficam em **`%LOCALAPPDATA%\AlbionAssistant`**: `settings.ini`, `hud-profiles`, `samples` e diagnósticos quando ativados. Atualizar o EXE ou limpar o build não altera esses dados. O parâmetro opcional `--settings caminho.ini` usa uma configuração e pasta de dados explícitas.

Salve uma HUD para cada resolução/layout, por exemplo “Notebook” e “Monitor 34”. Os perfis guardam posições, tamanho do ícone, resolução, monitor e DPI conhecidos. O programa sugere uma configuração compatível; você escolhe qual carregar. Calibrar outro ambiente preserva o perfil salvo anterior. A mudança de layout dentro do jogo exige nova seleção ou troca manual do perfil.

Use **Salvar HUD e ação** antes de carregar outra HUD ou criar uma nova para conservar edições. Carregar restaura a versão salva. Nova HUD mantém a última ação e referência salvas e inicia outra calibração. Um nome já utilizado por outra HUD não pode sobrescrevê-la.

## Desenvolvimento

Requisitos: Windows x64, Visual Studio Build Tools com C++/MSVC, SDK do Windows, CMake e Ninja. O script encontra as ferramentas do Visual Studio e funciona no Windows PowerShell 5.1 e PowerShell 7.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

Esse comando configura, compila, executa os testes e publica **`dist/AlbionAssistant.exe`** somente após aprovação dos testes. Execuções seguintes são incrementais. O progresso é resumido; o log completo fica em `build/logs/Release.log`.

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

Há uma regra por HUD, com condições de presença, ausência ou stacks **2/3**. Contador ilegível permanece desconhecido; não inferimos 1. O relógio radial é uma variação visual, sem cálculo de segundos restantes. **Avançado → Outra referência** troca a identidade do ícone, mantendo os dígitos desse preset; isso não é reconhecimento universal de buffs.

A captura usa DXGI e frames disponíveis, sem limitador fixo de 30/60 FPS nem fila crescente. Precisa de sessão Windows desbloqueada e jogo visível. Tela cheia exclusiva, HDR, monitor girado e janela distribuída entre monitores ainda não foram validados. Não há leitura de memória do jogo, injeção, interceptação de rede nem login automático.

`--diagnostics` grava mudanças de estado e amostras na pasta de dados. `--diagnostics --show-overlay-in-capture` inclui a borda na captura para inspeção; exige áreas de leitura e destaque separadas. As evidências e pendências estão em [docs/validacao.md](docs/validacao.md).
