# Albion Assistant

- Comunicação e documentação pt-BR. C++20, MSVC x64, CMake, Win32/D3D11; usar APIs nativas e poucas dependências.
- Um aplicativo, um checkout ativo e uma entrega: dist/AlbionAssistant.exe. Histórico no Git; não criar launchers, pacotes ou pastas v1/v2/Guided nem cópias paralelas do produto.
- Usuário aprovou implementação autônoma e orquestração via OpenCode. Não parar para aprovar decisões reversíveis.
- Produto apenas captura externa e sinalização visual. Não ler memória, injetar, interceptar rede nem automatizar gameplay no aplicativo.
- Regra configurável: Espírito Assassino com exatamente 3 stacks destaca área selecionada de Golpe Fantasma; desconhecido/expirado/fonte inválida sempre apaga. Sem número visível é stacks desconhecido, não inferir 1.
- Usuário descartou o vídeo do fluxo. Usar prints fornecidos e jogo aberto. Não apagar vídeo.
- Seleção de região por dois cliques separados, sem exigir arraste. Posições relativas à área cliente e calibração explícita por tamanho/HUD.
- Captura orientada a frames disponíveis, sem limite fixo 30/60 e sem fila crescente. Download somente ROI. Não alegar velocidade ou precisão sem medidas.
- Build: powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build-windows.ps1. Release é o padrão; -Configuration Debug para desenvolvimento, -Clean limpa somente o cache escolhido. Presets CMake e descoberta MSVC x64 via vswhere. Preservar BOM UTF-8 do script. Testes devem passar antes de publicar dist/AlbionAssistant.exe.
- Recursos do reconhecimento, ícone, manifesto e metadados ficam embutidos no EXE. Configurações, perfis e amostras vivem em %LOCALAPPDATA%/AlbionAssistant; nunca apagar dados do usuário ao recompilar ou limpar build.
- Não recriar nem executar smoke-windows.ps1; incidente Kaspersky pendente no projeto anterior. Não alterar antivírus ou exclusões.
- Cada agente edita apenas seus arquivos designados. Não commit/push por subagentes. Testes de regras e reconhecimento devem continuar ativos em Release.
