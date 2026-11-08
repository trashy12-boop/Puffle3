# Atualização do cache — 2 novembro 2026

Atualização em código sobre 01.06, ainda sem novo PKG. Substituição da fila FIFO por clock: acertos marcam a entrada; entradas reutilizadas recebem uma segunda chance no descarte. Atualizações de uma chave existente preservam a posição da fila, eliminando o retain linear de até 1024 elementos. Pixels e verificação do destino permanecem iguais.

Teste isolado passou: fila limitada, substituição repetida, preservação de entrada reutilizada, pixels corretos e rejeição de destino diferente. Ensaio sintético: 100 mil substituições de 1024 entradas de 1 pixel. Antigo 1,08–1,55 s; novo 16–31 ms no host. Não representa FPS PS3. Logs e harness em validation/cache-replacement. A suíte completa do player e restauração da toolchain foram iniciadas, sem resultado final no momento deste checkpoint. O PKG anterior no Drive continua sendo a versão anterior a esta mudança.
