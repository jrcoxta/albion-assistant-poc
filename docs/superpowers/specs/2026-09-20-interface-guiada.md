# Interface guiada e perfis de HUD

Design aprovado pelo usuário em “Faça isso”, após proposta em chat. Evoluir a POC nativa existente, mantendo leitura externa e a regra já validada.

Fluxo em quatro etapas: conectar, selecionar informações, configurar ação, conferir/iniciar. Painel com progresso, estados em linguagem simples, prévia e controles avançados separados do caminho principal. Perfis de HUD são independentes do nome do conjunto de armas. Preservar settings.ini legado e permitir salvar/carregar configurações “Notebook” e “Monitor 34” por arquivos INI. Sugerir perfis por dimensões do cliente, DPI e monitor conhecidos, sem trocar automaticamente nem deformar coordenadas entre proporções.

Seleção sobre uma captura congelada do cliente do jogo: região dos buffs e área de destaque por dois cliques; seleção do ícone por um clique assistido no preset com prévia ampliada e confirmação explícita. Se não houver sugestão segura, dois cliques definem o recorte manual; tamanho inferido da seleção, sem obrigar digitar pixels. Seleções não enviam entrada ao jogo. Confirmar/cancelar não modifica parcialmente uma calibração. Região de ícone deve ser quadrada e medir24..256px; áreas precisam permanecer dentro da imagem.

Regra como frase: Quando Espírito Assassino tiver3stacks, destacar Golpe Fantasma com borda dourada. Presente/ausente e2/3 permanecem disponíveis. Testar destaque ativa borda por5segundos, com aviso TESTE sobre o jogo; não depende do buff, encerra captura real durante o teste e nunca deixa leitura sintética no motor. F8/F9/cancelamento retornam de forma previsível. Estado expirado/incerto apaga leitura real como antes.

Uma troca de resolução/DPI detectada impede usar uma calibração incompatível. Oferecer carregar outra HUD ou recalibrar, preservando perfil salvo anterior. Mudança arbitrária do layout dentro do jogo não é detectável por dimensões; usuário seleciona perfil ou recalibra.

Validar regras e compatibilidade, persistência Unicode/migração, seleção multiescala em imagens reais e sintéticas, ausência/falso candidato e cancelamentos. Builds Debug/Release e teste pela UI nativa. Não alegar validação física em34polegadas. Preservar zip da primeira POC; gerar pacote novo v2 após revisão.
