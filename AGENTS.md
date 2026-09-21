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

## Validação independente obrigatória

- Seguir [docs/processo.md](docs/processo.md) em cada entrega. Definir critérios observáveis antes de implementar; testar o resultado contra eles.
- Delegar revisão a subagentes independentes do autor. Em features, mudanças de interface, persistência ou build, usar dois revisores: produto/arquitetura e QA. Em correções pequenas, um revisor pode reunir esses papéis; o autor nunca é o único aprovador.
- Os revisores recebem requisito, critérios, base Git e diff atual; não herdam o raciocínio do implementador. Revisão é somente leitura, sem subdelegação ou alterações no checkout. QA pode gerar logs de testes; coordenar comandos para não disputar build/logs.
- Corrigir achados bloqueantes, repetir os testes afetados e pedir nova conferência ao mesmo revisor. Não ignorar achados nem entrar em repetição sem diagnóstico. Mudanças posteriores invalidam a aprovação do escopo afetado.
- Build bloqueia entrega com arquivos extras, testes vazios/falhos ou EXE publicado diferente do testado. Não desativar validadores para conseguir publicar. Não apagar arquivos inesperados automaticamente.
- Registrar pareceres identificados, evidências e pendências em docs/validacao.md, atualizado no mesmo arquivo e versionado pelo Git. Build aprovado não significa interface/jogo aprovado; informar APROVADO, REPROVADO ou PENDENTE por escopo.
