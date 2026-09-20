# Validação da POC — 20/09/2026

## Ambiente observado

Windows x64, Albion Online Client aberto, HUD em 2880 × 1800. Região de leitura: `(86,218,514,98)`; destaque: `(1216,1576,104,104)`; ícone calibrado em 64 px. Jogo em primeiro plano, personagem em Caerleon Market. A habilidade Q foi acionada manualmente pela ferramenta de interface durante o teste autorizado. O aplicativo entregue não envia teclas ao jogo. O vídeo anexado não foi usado.

## Resultado integrado

Na versão Release final, com a borda incluída na captura somente para diagnóstico, foi observado o ciclo abaixo. Os tempos são do registro de transições do próprio aplicativo; não constituem um benchmark contínuo de latência.

| Captura (ms) | Estado reconhecido | Destaque |
|---:|---|---|
| 12664872 | Buff ausente | Apagado |
| 12673015 | Presente, sem número legível | Apagado |
| 12674781 | 2 stacks | Apagado |
| 12676859 | 3 stacks | Borda dourada visível em Golpe Fantasma |
| 12688057 | Buff terminou | Apagado |

Não houve transição intermediária para contador desconhecido entre a leitura de 2 e a de 3, nem durante os aproximadamente 11,2 segundos do estado de 3 stacks nesse ciclo. Isso valida esse ciclo observado; não garante precisão universal.

Um segundo ciclo com **Regra ativa desmarcada** identificou 2 em `12805484` e 3 em `12807468`, mantendo o destaque apagado. O número 3 e a ausência da borda também foram conferidos na tela.

Verificações de interface: áreas escolhidas por dois cliques; configuração restaurada após reabrir; F8 retorna ao painel; F9 inicia a leitura; perda de foco informa indisponibilidade; fechar o seletor com Alt+F4 devolve o painel sem perder a configuração. As falhas encontradas durante a revisão foram corrigidas: mensagens de sessões antigas não alteram o estado atual, mudança de DPI do painel encerra a sessão anterior e fechamento do seletor não deixa a aplicação escondida.

## Testes automatizados e revisão

- **Debug e Release:** CMake/CTest, 2 de 2 suítes aprovadas em cada configuração. Builds finais sem warnings do compilador nos logs.
- **Regras e persistência:** 109 verificações, incluindo fonte da observação, limite exato da expiração, futuro/inválido, desconhecido versus ausente, regra desativada, Unicode, arquivo inválido e escrita de configuração. Diâmetro persistido restrito a 24..256 px.
- **Reconhecimento:** imagens originais, outros buffs como negativos, ausência de dígito, imagem uniforme/preta, posição/escala, relógio radial simulado e 22 recortes reais rotulados visualmente. Os 22 passaram: 7 com 2, 12 com 3, 1 sem número e 2 ausentes. Detalhes em `tests/fixtures/recognition-live/README.md`.
- **Captura isolada:** 314 frames válidos em uma janela de 10 segundos, ROI BGRA de 514 × 98 px, parada concluída. Isso confirma funcionamento e formato, não mede capacidade máxima nem desempenho de ponta a ponta.
- **Overlay:** harness nativo com um processo observador separado verificou que o hit testing atravessa um pixel opaco da borda até a janela do Albion. Controle negativo detectou a própria borda ao remover a transparência somente no harness; restaurá-la voltou a atingir o Albion. Janela compacta, foco preservado, visibilidade e exclusão/inclusão em captura foram conferidos. Nenhum clique sintético foi enviado nesse harness.
- **Revisões independentes:** agentes distintos revisaram captura/overlay, reconhecimento e integração/regras; captura e overlay tiveram implementação assistida pelo OpenCode no WSL. O coordenador compilou, integrou e operou o teste real.

O defeito reproduzido no contador vinha de deslocamentos de poucos pixels entre a caixa da identidade e os templates dos números. Ampliar apenas a tolerância espacial proporcional ao ícone corrigiu os recortes reais: de 13/22 para 22/22. Os limiares de aceitação foram preservados, e nenhum resultado anterior é mantido para esconder uma leitura incerta.

Logs locais completos ficam em `.orchestration` e nas pastas `build/windows/{Debug,Release}/Testing/Temporary`; são artefatos de trabalho, fora do Git. O registro integrado fica ao lado do executável em `diagnostics.csv` quando habilitado. A evidência textual resumida desta entrega acompanha este documento.

## Limites não validados

Monitor físico de 34 polegadas, outras escalas de HUD, HDR, monitor girado, tela cheia exclusiva e sessões longas de combate não foram testados. A POC entrega uma regra editável e os dígitos 2/3 desse preset; não é reconhecimento genérico de todo o catálogo de buffs. Não calcula os segundos restantes do relógio radial. Mudanças de HUD exigem calibração do usuário.
