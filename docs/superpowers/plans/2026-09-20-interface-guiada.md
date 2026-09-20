# Interface guiada — Implementation Plan

> **For agentic workers:** Use superpowers:subagent-driven-development. Usuário aprovou execução autônoma; tarefas independentes possuem arquivos exclusivos.

**Goal:** transformar a calibração manual técnica em fluxo visual com perfis e teste do destaque.
**Architecture:** manter Win32 e captura/reconhecimento existentes. Adicionar funções pequenas para perfis e sugestão de ícone, seletor modal sobre imagem estática; root integra painel e captura única de calibração.
**Tech Stack:** C++20, Win32, WIC, DXGI, CMake/MSVC, sem dependências novas.
**Spec:** docs/superpowers/specs/2026-09-20-interface-guiada.md

## Global Constraints
- pt-BR; sem vídeo, injeção, memória do jogo ou gameplay automatizado no produto.
- Seleção por dois cliques, alternativa assistida confirmada; nenhuma configuração parcial em cancelamento.
- Preservar preset2/3, unknown e validade750ms, newest frame sem filas.
- Settings legados e pacote v1 preservados. Novo branch feat/interface-guiada desde709e9ca.

### Task1: perfis e compatibilidade
Arquivos: src/model.h, settings.cpp, profiles.h/.cpp, tests/rules_tests.cpp, profiles_tests.cpp.
Contrato: adicionar Settings.hudName=L"Minha HUD", monitorDevice, monitorDpi=0, iconCalibrated=false. Migração deve considerar o diâmetro legado com regiões completas como calibração existente. HudProfile{filesystem::path path;Settings settings;}; listHudProfiles(folder); saveHudProfile(folder,settings)->path; matchesScreen(settings,w,h,dpi,device)->bool. Nomes inválidos rejeitados, escrita atômica via saveSettings, sem apagar perfis.
- [x] Salvar/carregar Unicode, preservar dois perfis, sugerir somente compatível, recusar path traversal e dimensões distintas, ler legado.
- [x] Implementar extensão INI e funções; GREEN isolado e integrado.

### Task2: sugestão de ícone
Arquivos: src/calibration.h/.cpp, tests/calibration_tests.cpp. Sem alterar recognizer existente.
Contrato: std::optional<Region> suggestIcon(const Image&,int x,int y,const Recognizer&). Imagem completa ou ROI; retorno em coordenadas da mesma imagem. Buscar escalas perto do clique, validar identidade e limite; devolver nullopt se incerto. cropImage(Image,Region)->Image valida limites. Testes usam recortes reais existentes, escalados/posicionados e negativos.
- [x] Centro de buff64px e ausência em outro buff, escalas24..256 e canto de tela.
- [x] Implementar busca local sem estado anterior; GREEN e medir custo da seleção (não loop ao vivo).

### Task3: seletor visual
Arquivos: src/selection.h/.cpp. Contrato Win32: enum SelectionKind{Buffs,Icon,Highlight}; std::optional<Region> selectRegion(HWND owner,HWND target,const Image& snapshot,POINT origin,SelectionKind,const Recognizer* recognizer). Modal; não capturar/injetar teclas no jogo. Desenhar imagem congelada e instruções/preview. Icon usa suggestIcon, permite M para modo manual, Enter/confirmação salva, Esc cancela. Outros modos dois cliques + confirmação. Ampliar recorte e exibir limites; fechar popup volta ao owner. Revalidar tamanho/posição do target antes de aceitar.
- [x] Verificar tipos/cancelamento/retorno pela compilação /W4 e harness local, incluindo F2 e UI limitada pelo espaço disponível.

### Task4: painel guiado e integração
Arquivos: src/main.cpp, overlay.h/.cpp se necessário, CMakeLists.txt, README, docs/validacao.md.
- [x] Organizar quatro etapas com estados simples; profiles e frase de ação, controles técnicos em seção avançada.
- [x] Captura única para selecionar: parar sessão, ocultar painel, focar jogo, obter primeiro frame fresco DXGI com timeout, parar worker e passar snapshot para seletor. UI nunca usa source antiga.
- [x] Aplicar seleção somente confirmada; Icon atualiza iconSize automaticamente e iconCalibrated; guardar metadados da calibração.
- [x] Teste por5segundos em relógio monotônico; banner explícito TESTE; parar/cancelar/trocarperfil encerra teste. Não criar Observation sintética.
- [x] CMake integra cinco testes novos; 7/7 suítes em Debug e Release; revisão independente de perfis, seletor e ciclo de vida.
- [x] Documentar comportamento e limites e preparar entrega v2 separada, preservando a primeira POC.
- [ ] Conferência visual da nova UI e ciclo no jogo: Windows bloqueado durante esta etapa. Roteiro em docs/validacao-v2.md; validação física do monitor de34polegadas também pendente.
