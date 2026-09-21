# Relógio e contador na HUD de 40 px

Capturas reais de 21/09/2026, cliente 2560 × 1080. Os PNGs são cópias
literais da ROI `meu-personagem` (266 × 182), sem redimensionamento, obtidas
em `build/logs/clock-sequence-1`. O primeiro número do nome é o índice do
frame e o segundo é o instante monotônico da captura em milissegundos.
Tempo decorrido não é usado como resposta esperada do relógio.

| Frame | Evidência visual |
|---|---|
| 0, 473 | Espírito ausente; há outro status no mesmo local |
| 5 | Espírito sem número; já há uma pequena sombra no topo |
| 60, 70 | Número 2 visível |
| 120, 150, 170 | Número 3; frente avança no primeiro quadrante |
| 136, 318, 463 | Número 3 visível, mas referência antiga falhava no dígito |
| 190 | Número 3; frente voltou ao início depois da renovação |
| 238 | Número 3; disco iluminado na máscara usada pelo estimador |
| 260 | Número 3; frente visível após outra renovação |
| 400 | Frente encoberta pelo contador no quadrante inferior direito |
| 440, 460, 470 | Número 3; frente visível no quadrante inferior esquerdo |

Os assets nativos são recortes literais `(82, 94, 40, 40)`: `assassin-2-40`
vem do frame 60; `assassin-3-40` e `assassin-clock-40`, do frame 238. A
referência temporal foi conferida contra o máximo observado nos frames
5..459: todos os 285 pixels da máscara radial coincidem, sem sintetizar
ou modificar a imagem. O frame 5, embora pareça iluminado em miniatura,
difere em oito pixels dessa máscara e não serve de referência temporal.

Antes da correção, os frames 136/318/463 tinham identidade estável
(confiança próxima de 0,944), mas score do dígito 3 de 0,792899, abaixo do
limite 0,80. A correção conserva esse limite e a margem entre classes.
Variantes do mesmo valor não podem competir entre si como classes rivais.

Os intervalos do teste radial vêm dos quadrantes e frentes visíveis nos
prints. Uniforme, referência incompatível, referência parcialmente escura,
ausência e frente oculta devem continuar sem fração. Estes frames não
demonstram precisão de duração em segundos nem validam partes encobertas.

## Reexecução com amostras nativas

O probe de produção reexecutou os 1.105 frames da sequência em
`build/logs/vision-repair/dense-after.csv`: 54 leituras de 2 stacks, 361 de
3 stacks e 690 sem contador. Em relação à execução anterior, recuperou
34 leituras de 2 e 11 de 3, mantendo os limites 0,80 e 0,065. Os testes
versionados incluem negativos sem número/ausência e substituição de
amostras personalizadas, além dos frames que falhavam.

A referência temporal nativa forneceu fração em 331 frames (250 com
3 stacks); a execução anterior não fornecia nenhuma. Isso não representa
cobertura temporal completa: disco inteiramente iluminado, frente encoberta
pelo contador e suporte visual insuficiente continuam desconhecidos. O
estimador radial e seus limiares não foram relaxados. O destaque por stacks
não depende da disponibilidade da fração do relógio.

## Quedas evitáveis na grade pequena

Os frames 162–163, 228–230, 298–299, 368, 434 e 441 foram adicionados
como cópias literais da mesma sequência. Todos mantêm 3 stacks e uma
frente visível. Em um dos lados dessa frente há somente quatro pixels
disponíveis na janela angular, e todos os quatro concordam com a sombra;
o requisito fixo de cinco apagava indevidamente o aro. A regressão exige
fração e concordância com o ângulo visível (aproximadamente 77–78°, 179°
ou 192°). A correção exige quatro quando só há quatro disponíveis e
mantém cinco quando a grade oferece cinco ou mais. Menos de quatro não
é evidência suficiente. Máscara, contraste, ambiguidade e presença continuam
com os mesmos critérios. Isso não resolve a frente encoberta pelo número
nem o disco sem contraste logo depois de uma renovação.
