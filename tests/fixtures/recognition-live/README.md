# Regressão de contador em captura real

22 ROIs PNG de 514 × 98 px capturadas pelo aplicativo em 20/09/2026, com diâmetro calibrado de 64 px e HUD em 2880 × 1800. Contêm somente a região dos buffs e uma pequena faixa do cenário. Nenhum vídeo foi usado.

Os nomes preservam o timestamp e a **saída antiga** do aplicativo; `stacks-unknown` no nome não é o rótulo esperado. Os rótulos de teste foram conferidos visualmente:

- `12025625`, `12047375`: Espírito Assassino ausente.
- `12036046`: presente, sem número visível.
- `12038093` até `12040093`: sete imagens com número 2.
- `12040156` até `12046609`: doze imagens com número 3. Na primeira a transição visual já ocorreu.

Estas amostras reproduzem o erro de alinhamento: a busca de identidade alternava a caixa entre `(33,12)` e `(35,11)` conforme o relógio, enquanto o contador só aceitava deslocamento de um pixel. Os templates originais de 2 e 3 também possuem pequenos deslocamentos relativos ao aro. O teste exige leitura independente por imagem; nunca mantém o contador do frame anterior.

O conjunto é uma regressão do problema observado, não uma estimativa estatística de precisão em outros HUDs, resoluções ou condições.

## Relógio radial experimental

O recorte `[35,11,99,75)` de `12036046-stacks-unknown.png` foi preservado em `assets/assassin-clock.png` como referência iluminada independente da identidade. Os testes conferem igualdade dos pixels de origem. Na máscara interna sem contador, essa imagem coincide com o máximo de brilho observado nas 20 ROIs positivas.

Os casos de relógio usam intervalos largos de fração compatíveis com a posição visível da frente: `12038609` e `12046609` no começo do primeiro quadrante; `12039187` mais adiante nesse quadrante. Não são rótulos de duração ou uma estimativa de precisão. O corpus não mostra o restante da volta. Há recuos da frente mantendo 3 stacks, portanto uma renovação não é inferida a partir do contador. Frente encoberta, ausência de contraste ou referência já sombreada devem produzir fração desconhecida.
