# Regressão da HUD ultrawide

Capturas reais de 21/09/2026, na ilha do personagem do usuário, com cliente
2560 × 1080. Cada PNG contém somente a origem `meu-personagem` (266 × 182),
sem redimensionamento, montagem ou alteração dos pixels. A medição salva
era 38 px; a busca exata em 40 px reconhece o ícone desta HUD.

| Arquivo | Captura de origem em `build/logs/live-read-2` | Conteúdo observado |
|---|---|---|
| absent-before.png | 75206968-1-unknown.png | Espírito ausente |
| present-no-counter.png | 75222218-1-unknown.png | Espírito presente, sem número |
| stacks-2.png | 75224250-1-unknown.png | Número 2 visível |
| stacks-3-start.png | 75226265-1-unknown.png | Número 3, início do trecho |
| stacks-3-middle.png | 75230312-1-unknown.png | Número 3, meio do trecho |
| stacks-3-end.png | 75235343-1-unknown.png | Número 3, fim do trecho |
| absent-after.png | 75236562-1-unknown.png | Espírito ausente após o efeito |

A regressão falhava nos três frames de 3 stacks com a busca exata em 38 px.
No frame central: confiança 0,749 em 38 px (ausente) e 0,944 em 40 px
(presente/3). A leitura com tolerância de até dois pixels deve reconhecer os
três frames, mantendo negativos ausentes e situações ambíguas desconhecidas.

O contador 2 é pequeno e permanece desconhecido na busca exata em 40 px.
O teste aceita 2 ou desconhecido, nunca 3. Sem número, os stacks devem ser
desconhecidos, com presença confirmada. Ligar o relógio não pode alterar
identidade/stacks. Estes PNGs não validam a fração temporal: a referência
embutida do relógio não se ajusta a este lote; não foram relaxados seus limites.
