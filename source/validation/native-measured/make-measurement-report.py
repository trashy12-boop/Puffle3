from pathlib import Path
import json,re,statistics,hashlib
w=Path.cwd();e=w/'evidence';out=w/'deliverables';out.mkdir(exist_ok=True)
def load(n):return json.loads((e/n).read_text())
files=['software03-mario-summary.json','new-mario-summary.json','new-bill-menu-summary.json','new-bill-gameplay-summary.json','new-bill-gameplay-steady-summary.json','baseline-mix-rsx-summary.json','new-mix-rsx-summary.json','new-mix-software-summary.json']
labels=['Mario — 01.03 software, tela inicial animada','Mario — 01.05 gradientes, tela inicial até falhar','Bullet Bill — 01.05 diagnóstico, menu','Bullet Bill — 01.05 diagnóstico, entrada na partida','Bullet Bill — após as duas amostras de transição','The Right Mix — 01.05 anterior, RSX','The Right Mix — 01.05 diagnóstico, RSX','The Right Mix — 01.05 diagnóstico, software']
rows=[]
for label,name in zip(labels,files):
 r=load(name);rows.append(f"| {label} | {r['samples']} | {r['mean_draws_per_s']:.2f} | {r['mean_render_ms']:.2f} | {r['mean_tick_ms']:.2f} | {r['mean_audio_pump_ms']:.2f} |")
host=load('host-current/summary.json');hostrows=[]
for r in host:hostrows.append(f"| {r['game']} | {r['frames']} | {r['logic_ms']:.3f} | {r['render_ms']:.3f} | {r['audio_ms']:.3f} | {r['total_p95_ms']:.3f} | {r['worst_ms']:.3f} |")
report='''# Puffle 3: medições executadas no RPCS3

1 de novembro de 2026, horário de Brasília. Atualização posterior ao relatório da 01.05.

**Agora houve execução dos executáveis PS3 e dos três jogos no RPCS3. O ganho prático solicitado não foi demonstrado. A versão nova apresentou falta de memória no Super Mario 63. The Right Mix continua com imagem e interação incorretas. O PKG de desenvolvimento anterior não deve ser considerado uma atualização estável validada.**

## Condições e limites

- RPCS3 0.0.43-20551-ad468a13, firmware 4.93, Linux x86-64, host com cerca de 9,7 GiB de RAM.
- PPU `Interpreter (static)`, SPU LLVM, pré-compilação LLVM desligada, Vulkan por software: llvmpipe/LLVM 20.1.2, Mesa 25.2.8. Shader Mode `Legacy Recompiler (single-threaded)`.
- Cena Flash em viewport 320×240, frontend nativo e caminho RSX do próprio port. Sem modificação da lógica dos jogos para aumentar FPS.
- O backend de áudio Cubeb não encontrou dispositivo físico. O mixer do aplicativo foi executado e registrou PCM no anel de saída PS3, mas reprodução sonora e seu impacto em um PS3 real não foram validados.
- Os valores nativos vêm dos contadores existentes no aplicativo, agrupados em intervalos de aproximadamente dois segundos. “Quadros/s” significa desenhos do jogo por segundo (`Draw`), não o FPS do frontend do RPCS3. Render, tick e audio têm seus próprios denominadores; não devem ser somados como uma decomposição exata de cada quadro.
- Tick inclui VM, timeline e atualização relacionada. Não é uma medição isolada de opcodes AVM1. O profiling atual não separa GC, propriedades e display list dentro dele.
- Os números **não são FPS de PS3 real**. O interpretador do RPCS3 e o Vulkan por software alteram fortemente os custos. A comparação é observacional, com cenas animadas, janelas de duração diferente e uma execução por condição; não sustenta percentuais de melhora.
- A imagem foi inspecionada para identificar menu, partida e a falha. Scripts de controle, logs brutos, hashes e marcadores de fase acompanham os resultados.

## Resultados nativos medidos

Médias das amostras disponíveis na fase identificada. A entrada na partida de Bullet Bill inclui a transição a partir do menu.

| Executável e cena | Amostras | Desenhos/s | Render ms/chamada | Tick ms/chamada | Áudio ms/pump |
|---|---:|---:|---:|---:|---:|
'''+ '\n'.join(rows)+'''

### Super Mario 63

A animação da tela inicial foi alcançada tanto pela 01.03 original quanto pelo novo executável. As médias ficaram próximas: aproximadamente 3,8 e 3,9 desenhos/s. Na nova versão, o conjunto de 8 amostras após o marcador de tela inicial teve mínimo 2,4 e máximo 5,0 desenhos/s. Esses são extremos de intervalos, não pior tempo de quadro.

A nova versão caiu em `std::alloc::rust_oom`. O trap em `0x0048a058` foi localizado com `ppu-addr2line` no ELF com símbolos. A pilha passa por `MovieClipPath::new_from_path`, execução de ações AVM1, `Player::run_frame` e `rp_player_tick`. O ponto onde uma alocação falha não identifica sozinho quem reteve toda a memória.

Uma segunda execução, com logs de heap no C e sem a tecla Z da primeira sequência, também falhou. O TTY registrou: `malloc request=14208 align=16 live=124362986 peak=124368638`. Isso equivale a cerca de 118,6 MiB de heap Rust vivo. A falha veio do malloc, não da guarda de 128 MiB. É necessário examinar a memória total do processo, retenção e fragmentação; elevar a guarda não é uma correção validada. O relatório anterior dizia que o comportamento nativo não tinha sido testado; esta evidência nova substitui essa limitação e revela uma falha concreta.

O renderer registrou fallback por máscaras na tela inicial, com zero submissões de malhas RSX nesses intervalos. O cache de gradientes não pode acelerar desenhos que nem chegam ao RSX. Renderização e runtime variam com a animação; em um intervalo, render consumiu cerca de 325 ms e tick 29 ms; em outros, ambos foram relevantes.

A 01.03 ficou executando a fase marcada por cerca de 57 segundos sem esse trap durante a coleta. A versão nova teve coleta interrompida pelo erro, portanto seus 3,9 desenhos/s não são uma vitória de performance ou estabilidade. A sequência nova incluiu uma tecla Z durante a apresentação anterior à tela inicial; não se afirma que as animações estavam no mesmo quadro.

### Bullet Bill

O menu ficou em aproximadamente 31,9 desenhos/s, com render e tick abaixo de 1 ms em média. A partida foi iniciada com o controle de mouse emulado e confirmada na imagem. Houve queda forte: a fase coletada ficou em aproximadamente 6 desenhos/s, incluindo a transição; intervalos da partida chegaram a 3,3–3,9 desenhos/s.

Na partida, houve intervalos com tick de 113–225 ms e render de 5–29 ms. Os contadores mostraram fallback por máscaras, mas o tick foi o custo maior nesses exemplos. Isso é evidência de que otimizar somente uploads ou desenho não resolve esse trecho. A medição ainda não distingue colisões, scripts, MovieClips e GC dentro do tick.

### The Right Mix

O executável PS3 reproduziu o fundo preto. Forçar software também deixou a imagem incorreta. Houve tentativas de mouse emulado sobre a garrafa, sem confirmar o comportamento esperado. **Não há correção de compatibilidade validada.**

O host com a mesma SWF mostrou o menu vermelho e, com o replay, a cena completa; a imagem nativa diverge. Assim, o diagnóstico não pode se limitar ao RSX. A causa pode envolver runtime, estado da timeline, desenho e/ou hit testing; ainda não foi isolada.

RSX e software ficaram perto de 18 desenhos/s. O renderer de software custou mais por chamada, mas ambos couberam no limite de atualização observado dessa cena. FPS igual não significa imagem correta.

## O que a otimização de gradientes realmente mudou

O executável 01.05 anterior foi recuperado do PKG salvo no Drive e instalado para comparação. Seu payload EBOOT corresponde ao hash `4d31783b721efe5d07fd9e4e8e4bab0ed73c60ad18e6e892757848fc5d28f1ab`; os 16 bytes adicionais do instalador não fazem parte desse payload.

Na cena estável de The Right Mix, o executável anterior registrou **468 uploads / 468 KiB por intervalo de cerca de dois segundos**. Com identidades persistentes dos gradientes, houve **zero uploads depois do aquecimento**, mantendo a reutilização. Os intervalos comparados têm o mesmo número de desenhos e vértices submetidos: 3.384 draws e 641.304 vértices.

As médias de desenhos/s ficaram em aproximadamente **17,97 antes e 17,96 depois**. Não houve ganho prático de FPS comprovado. Os logs confirmam que o cache é usado no caminho PS3, mas a compatibilidade visual continua errada e a cena já opera perto de seu ritmo observado. A média do tick não é uma comparação controlada antes/depois: houve movimentos e cliques no ensaio novo.

## Medições separadas no host

Replay de entradas com relógio Flash controlado, em Linux x86-64, build release, renderer de software, áudio misturado, 320×240. Foi executado sem RPCS3 simultâneo. São milissegundos por frame simulado, incluindo carregamento e transições; não representam FPS nativo. Uma rodada por jogo.

| Jogo | Frames | Tick médio ms | Render médio ms | Mixer médio ms | P95 total ms | Pior total ms |
|---|---:|---:|---:|---:|---:|---:|
'''+ '\n'.join(hostrows)+'''

Os CSVs têm tempos por frame do host. No PS3/RPCS3, o instrumento agregado existente não fornece distribuição por quadro ou P95 nativo, então esses valores não foram inventados.

## Comparação de versões e disponibilidade

- 01.03: PKG original recuperado e executado nesta sessão; comparação direta do Mario registrada.
- 01.04 RSX: fonte recuperada, mas o PKG original não foi recuperado nesta sessão. Não há nova média nativa da 01.04 neste relatório. A observação histórica do usuário — desempenho parecido com 01.03 — permanece observação do usuário.
- 01.05 anterior: PKG recuperado e executado; comparação de uploads de The Right Mix registrada.
- 01.05 com gradientes: executável entregue foi executado no Mario e falhou. Bullet Bill e Right Mix foram medidos numa recompilação diagnóstica com a mesma biblioteca Rust/renderer, acrescentando logs de heap no C. Essa recompilação não é uma nova release de performance.
- Nenhum teste em hardware PS3 real foi realizado aqui. SPUs e JIT não foram implementados ou medidos.

## Ambiente restaurado e bloqueios esclarecidos

O primeiro modo Vulkan, com interpretador de shaders, consumiu cerca de 8 GB e uma tentativa terminou em -9 antes do app. O mesmo crescimento ocorreu com o PPU interpretado. Alterar o modo de shaders permitiu iniciar o aplicativo. Assim, a descrição anterior de “falha de análise do executável” era incompleta; o trabalho do Vulkan/lavapipe durante a inicialização também precisava ser considerado.

O nome `Interpreter (precise)` foi rejeitado pelo RPCS3 desta versão. O modo válido confirmado no código e nos logs é `Interpreter (static)`. Os ensaios que falharam na preparação não entram nos números de FPS dos jogos.

## Próximas correções exigidas pelos dados

1. Diagnosticar e reduzir o consumo de memória do Mario; o allocator do port tem guarda de 128 MiB para o heap Rust. Foram acrescentados logs de heap e de falha de alocação para distinguir essa guarda de falha do malloc. Não se aumentou o limite às cegas.
2. Resolver o fallback por máscaras com gerenciamento de memória e correção visual; o RSX não é usado nos intervalos relevantes do menu do Mario e da partida do Bill.
3. Perfilar o tick do Bill por operações AVM1 e atualização da cena, pois ele domina a partida no ensaio.
4. Isolar a divergência do Right Mix, reproduzida nos dois renderers, antes de declarar a entrada ou o fundo corrigidos.
5. Repetir a aceitação com pré-compilação e áudio adequados e, depois, em hardware real. Nenhuma previsão de FPS substitui isso.

O pedido de medir foi executado. Os resultados não validam a versão nova como a otimização ampla solicitada, e nenhum novo PKG foi oferecido como solução nesta etapa.
'''
(out/'Puffle3_Medicoes_Reais_RPC S3.md'.replace(' ','')).write_text(report)
print('report',len(report))
