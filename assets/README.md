# Referências do reconhecimento

Recortes técnicos de 64 × 64 pixels extraídos dos PNGs que o usuário forneceu em 20/09/2026. Incluem somente ícones de buff; os screenshots completos e o vídeo não fazem parte dos assets.

- `assassin-none.png`: Espírito Assassino sem número visível. Origem `8d4b78fa-12a6-452d-a053-4efcfc0c84aa`, retângulo `[121,229,185,293)`.
- `assassin-3.png`: Espírito Assassino com contador 3. Origem `ef645756-ae4c-422e-9ba9-e9ec46b2a812`, mesmo retângulo.
- `assassin-2.png`: Espírito Assassino com contador 2. Origem `87d2633d-c43e-4cb2-93e7-d0ee81f11e0b`, retângulo `[9,5,73,69)`.
- `assassin-screen-3.png`: recorte independente do screenshot completo `d1e0c086-866c-4fd1-acc3-d9fcb43d1f87`, retângulo `[121,229,185,293)`, usado somente em teste.
- `assassin-clock.png`: referência separada do relógio, sem número, copiada pixel a pixel da ROI real `tests/fixtures/recognition-live/12036046-stacks-unknown.png`, retângulo `[35,11,99,75)`. Na região interna examinada ela coincide com os pixels mais iluminados observados no corpus. As referências de identidade acima já têm sombra e não substituem esta referência.
- `other-food.png` e `other-buff.png`: negativos reais do mesmo screenshot completo, retângulos `[190,229,254,293)` e `[260,229,324,293)`, usados somente em teste.

`iconSize` representa o diâmetro externo do ícone incluindo o aro. A identidade usa cromaticidade do desenho, ignorando o canto inferior direito. O contador usa silhuetas brancas dos exemplos reais 2 e 3. Ausência de número ou dígito não confiável permanece desconhecido; não se deduz stack 1.

Os testes também redimensionam esses recortes e simulam escurecimento. Essas transformações verificam regressões e não substituem validação ao vivo de outras resoluções/HUDs. Não há OCR genérico nem leitura de dígitos além de 2 e 3 nesta POC.

A leitura experimental do relógio compara os mesmos pixels com a referência iluminada, usando somente o disco interno e excluindo a área do contador. Só fornece uma fração quando observa contraste e uma frente radial coerente, do topo no sentido horário. Imagem uniforme, frente encoberta ou comparação ambígua deixam a fração desconhecida. Não há conversão em segundos nem extrapolação entre frames. Os prints atuais cobrem aproximadamente o primeiro quadrante; os testes sintéticos dos demais ângulos não validam uma volta inteira no jogo.

Os testes geométricos também exercitam ícones de 40, 48, 64 e 96 pixels. Em recortes muito pequenos, como 24 pixels, pode não haver suporte suficiente para medir o relógio; a fração permanece desconhecida. Isso não altera o reconhecimento de presença ou contador.
