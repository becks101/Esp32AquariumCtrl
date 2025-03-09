# ESP32 Relay Control System

Um sistema completo de controle de relés com ESP32, display OLED e interface web.

## Visão Geral

Este projeto permite configurar e controlar até 8 relés usando um ESP32, com as seguintes características:

- 🕒 Programação de horários de ligar/desligar para cada relé
- 📱 Interface web para controle remoto
- 📊 Display OLED para visualização de status
- 💾 Armazenamento de configurações na EEPROM
- 🔄 Múltiplas opções de WiFi para conexão confiável

## Hardware Necessário

- ESP32 (qualquer modelo compatível)
- Display OLED SSD1306 (128x64)
- Até 8 relés (ou módulos de relé)
- Teclado matricial 2x4 (ou botões individuais)
- Cabos de conexão

## Conexões

### Display OLED
- SDA: GPIO21 (padrão I2C)
- SCL: GPIO22 (padrão I2C)
- VCC: 3.3V
- GND: GND

### Teclado Matricial
- Linhas: GPIO12, GPIO14
- Colunas: GPIO26, GPIO27, GPIO33, GPIO25

### Relés
- Relé 1: GPIO15
- Relé 2: GPIO2
- Relé 3: GPIO4
- Relé 4: GPIO16
- Relé 5: GPIO17
- Relé 6: GPIO5
- Relé 7: GPIO18
- Relé 8: GPIO19

## Instalação

1. Instale o Arduino IDE (https://www.arduino.cc/en/software)
2. Adicione suporte para ESP32 ao Arduino IDE (https://github.com/espressif/arduino-esp32)
3. Instale as bibliotecas necessárias:
   - Adafruit_GFX
   - Adafruit_SSD1306
   - ArduinoJson
   - ESPAsyncWebServer
   - AsyncTCP
4. Abra o arquivo `.ino` no Arduino IDE
5. Configure suas redes WiFi no código (edite os arrays `ssids` e `passwords`)
6. Conecte o ESP32 via USB e faça o upload do código

## Uso

### Configuração via Display OLED

O sistema possui dois menus principais:

**Menu 1 (Principal):**
- Mostra o tempo atual
- Exibe o status de cada relé e o tempo restante até a próxima mudança de estado
- Pressione botões 1-4 para alternar os estados dos relés 1-4
- Pressione 'R' para ir para o Menu 2
- Pressione e segure 'L' para salvar configurações na EEPROM

**Menu 2 (Configuração):**
- Permite configurar horários para cada relé
- Use 'L' e 'R' para navegar entre os campos
- Use 'U' e 'D' para alterar valores
- Campos:
  - Hora de ligar
  - Minuto de ligar
  - Hora de desligar
  - Minuto de desligar
  - Usar programação (O = sim, X = não)
  - Estado atual (O = ligado, X = desligado)

### Interface Web

Para acessar a interface web:
1. Conecte-se à mesma rede WiFi do ESP32
2. Abra um navegador e digite o endereço IP do ESP32 (mostrado no display durante a inicialização)
3. Use a interface para:
   - Ver o status de todos os relés
   - Configurar horários de ligar/desligar
   - Alternar estados dos relés manualmente
   - Salvar configurações na EEPROM

## API REST

O sistema também fornece uma API REST para integração com outros sistemas:

- `GET /api/relays` - Obtém informações de todos os relés
- `GET /api/update?relay=[ID]&field=[CAMPO]&value=[VALOR]` - Atualiza configuração de relé
- `GET /api/save` - Salva configurações na EEPROM

Veja a documentação completa da API no arquivo [Web_API_Documentation.md](./Web_API_Documentation.md).

## Resolução de Problemas

### Sistema não conecta ao WiFi
- Verifique se os SSIDs e senhas estão corretos no código
- Certifique-se de que o ESP32 está no alcance do roteador
- Tente adicionar mais opções de redes no array de SSIDs

### Horário incorreto
- Verifique se o ESP32 tem acesso à internet para sincronização NTP
- Ajuste a configuração de fuso horário (`gmtOffset_sec`) se necessário
- Reinicie o dispositivo para forçar uma sincronização

### Relés não respondem
- Verifique as conexões físicas
- Confirme se os pinos GPIO estão definidos corretamente
- Verifique se os relés são acionados em LOW (padrão do código) ou HIGH

### Display OLED não funciona
- Verifique as conexões I2C
- Confirme se o endereço do display está correto (0x3C é o padrão)
- Tente um endereço alternativo (0x3D) se necessário

## Personalizações

- Para adicionar mais redes WiFi, aumente o valor de `NUM_NETWORKS` e adicione entradas aos arrays `ssids` e `passwords`
- Para ajustar o fuso horário, modifique o valor de `gmtOffset_sec` (em segundos)
- Para alterar o servidor NTP, modifique as variáveis `ntpServer` e `ntpServerBackup`

## Licença

Este projeto é disponibilizado como código aberto sob a licença MIT.
