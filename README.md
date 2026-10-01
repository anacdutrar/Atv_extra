# Estimativa de altitude com ESP32-S3, BMP180 e TFLite Micro

Atividade acadêmica de IA embarcada que estima altitude a partir da temperatura
e da pressão medidas por um BMP180. O firmware executa no ESP32-S3 um modelo de
regressão totalmente quantizado em `int8`, treinado no notebook incluído neste
repositório.

Este projeto é uma demonstração didática, não um altímetro certificado. A
pressão atmosférica também varia com o clima e o modelo é limitado às cidades,
ao período histórico e às faixas presentes no dataset.

## Funcionamento

O BMP180 é lido a cada dois segundos. O firmware:

1. obtém temperatura em graus Celsius e pressão em pascals;
2. normaliza as duas entradas com os parâmetros calculados somente no treino;
3. quantiza as entradas para `int8`;
4. executa o modelo com TensorFlow Lite Micro;
5. desquantiza e desnormaliza a saída para metros.

O modelo possui 489 parâmetros, três camadas densas (`24`, `16` e `1`
neurônios) e ocupa 3.736 bytes no arquivo `.tflite`.

## Ligações no ESP32-S3

| BMP180 | ESP32-S3 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 9 |
| SCL | GPIO 8 |

O circuito de simulação está em [`diagram.json`](diagram.json).

## Estrutura

```text
.
├── components/bmp180/              # Driver local corrigido e licença MIT
├── main/
│   ├── main.cc                      # Sensor, laço e saída serial
│   ├── altitude_model.cc/.h         # Pré e pós-processamento e inferência
│   ├── model_data.cc/.h             # Modelo incorporado ao firmware
│   └── extra_altitude_normalization.h
├── model/extra_altitude_int8.tflite
├── diagram.json
└── wokwi.toml
```

O CSV final não foi incluído porque possui aproximadamente 44 MB e é
reconstruído pelo notebook a partir das fontes públicas.

## Requisitos

- ESP-IDF 6.1 ou versão compatível com ESP32-S3;
- Python e ferramentas instaladas pelo ESP-IDF;
- acesso à internet no primeiro `reconfigure`, para baixar TFLite Micro,
  `i2cdev` e os auxiliares declarados nos manifests;
- extensão do Wokwi no VS Code, caso seja utilizada a simulação.

## Compilação

Abra um terminal com o ambiente ESP-IDF carregado e execute:

```bash
idf.py set-target esp32s3
idf.py reconfigure
idf.py build
```

O driver corrigido do BMP180 está em `components/bmp180`, portanto não depende
de uma alteração manual dentro de `managed_components`. O CMake do projeto
também neutraliza a definição `ESP_NN` do componente TFLite Micro, mantendo a
configuração validada nesta atividade.

Para uma placa física:

```bash
idf.py -p PORT flash monitor
```

Substitua `PORT` pela porta serial correspondente. Para encerrar o monitor,
pressione `Ctrl+]`.

## Wokwi

Após o build, abra `diagram.json` e inicie a simulação. O `wokwi.toml` aponta
para os artefatos da pasta `build`.

Saída esperada:

```text
altitude_model: Modelo carregado (3736 bytes), entrada int8 (...), saida int8 (...)
extra_altitude: BMP180 pronto: SDA=GPIO9, SCL=GPIO8
extra_altitude: Temperatura: 28.80 C | Pressao: 91000 Pa (910.00 hPa) | Altitude estimada: 937.48 m
```

## Faixa recomendada para a demonstração

Para evitar extrapolação e saturação do `int8`, utilize preferencialmente:

| Grandeza | Faixa recomendada |
|---|---:|
| Temperatura | aproximadamente `-9,5 °C` a `43,3 °C` |
| Pressão | aproximadamente `60.200 Pa` a `103.510 Pa` |
| Altitude representada pelo dataset | `0 m` a `4.356 m` |

O dataset completo contém temperaturas entre `-22 °C` e `45,3 °C` e pressões
entre `58.930 Pa` e `103.510 Pa`, mas a calibração `int8` com 1.000 amostras
representativas resultou numa faixa efetiva um pouco menor. Fora dela, entradas
diferentes podem receber o mesmo valor quantizado. Por isso, pressões muito
baixas repetem valores próximos de `4.350 m`, e temperaturas como `74,5 °C` não
constituem um teste válido do modelo.

A saída também possui resolução aproximada de `17,32 m` por nível `int8`.
Pequenas mudanças podem, portanto, produzir a mesma altitude impressa.

## Modelo


O modelo binário original está em
[`model/extra_altitude_int8.tflite`](model/extra_altitude_int8.tflite). A cópia
embutida no ESP32-S3 está em `main/model_data.cc`.

## Correção local do BMP180

O driver deriva de `esp-idf-lib/bmp180` 1.0.7. A leitura bruta de temperatura
do sensor é um valor de 16 bits sem sinal. No driver original ela era lida em
`int16_t`, podendo se tornar negativa acima de `32767`. A versão local usa
`uint16_t` nessa leitura; os coeficientes que o datasheet define com sinal
continuam utilizando `int16_t`.

O componente preserva os avisos originais e a licença MIT no arquivo
[`components/bmp180/LICENSE`](components/bmp180/LICENSE).

