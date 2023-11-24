# Trocador de frames HDLC sobre RS485
Atividade da disciplina de Comunicação de Dados do curso de Engenharia de Controle e Automação do IFES Campus Serra.

## Descrição
Este repositório contém a implementação de um trocador de frames HDLC sobre a interface RS485, desenvolvido como parte do projeto da disciplina de Comunicação de Dados. O código foi projetado para permitir a comunicação entre um dispositivo primário e dois dispositivos secundários, utilizando o protocolo HDLC.

## Funcionamento Geral
O código do dispositivo primário e secundário é praticamente idêntico, com exceção do tratamento de mensagens após o recebimento (roteamento) e das iniciativas. A seguir, apresentamos o funcionamento geral do sistema.

### Primário
- O dispositivo primário pode enviar frames de informação (info) se não houverem enlaces estabelecidos no momento.
- Após o envio de um frame info, o primário aguarda a resposta do dispositivo secundário em forma de frames supervisórios (super), indicando o status da mensagem.
- Se o frame de resposta não for recebido em um determinado tempo, será feita uma nova tentativa de envio até um número de tentativas predefinido.
- O primário pode receber a resposta com um final = 0, indicando que virá em seguida um frame info. Caso venha um frame de informação, ele será tratado, e ocorrerá a resposta em formato de frame supervisório. O secundário também realiza tentativas até uma das condições de parada.

### Secundário
- O dispositivo secundário, fora de um enlace, aguarda por frames de informação (info) do primário.
- Após o recebimento de um frame info, o secundário responde com um frame supervisório (super), indicando se a mensagem foi recebida corretamente.
- Caso o conteúdo da mensagem exija uma resposta, o supervisório de resposta será enviado com final = 0, e um frame info com final = 1 será enviado em seguida.

## Envio de Mensagens
- As mensagens são enviadas encapsuladas em frames de informação (info).
- O protocolo HDLC é utilizado para delimitar os frames, com início e fim marcados pelo byte de flag (0x7E).
- O controle do frame info contém informações sobre os números de sequência (N(s) e N(r)), indicadores de pool ou final, e controle de fluxo.

## Recebimento de Mensagens
- Tanto o primário quanto o secundário estão disponíveis para receber frames de informação (info) a qualquer momento.
- Ao receber um frame info, o dispositivo responde obrigatoriamente com um frame supervisório (super) indicando o status da mensagem.

## Frames Supervisórios
- Os frames supervisórios (super) são utilizados para confirmar o recebimento de mensagens e indicar o status da comunicação.
- Existem dois tipos de frames supervisórios:
    - RR (Receive Ready): Indica que o dispositivo está pronto para receber a próxima mensagem.
    - REJ (Reject): Indica que houve algum erro na mensagem recebida, solicitando sua retransmissão.

# Execução e Explicação da Saída do Monitor Serial

A seguir, segue uma explicação da saída do monitor serial para o comportamento esperado do envio de mensagens com requisição de resposta.

'''
Primary Device - Initing...
My Address: 1
>> send:'ok?' to 0x02
SEND: 0x7E 0x02 0x19 0x6F 0x6B 0x3F 0x79 0xA5 0x7E n(s)=1 n(r)=1 ASCII:'ok?' P/F:1 (Info)
RECV: 0x7E 0x01 0x81 0x81 0x01 0x7E n(r)=1 CRC:OK P/F:0 (Super - RR)
>> recv: RR CRC:OK n(r)=1 from 0x02
RECV: 0x7E 0x01 0x19 0x6F 0x6B 0x4B 0x06 0x7E n(s)=1 n(r)=1 ASCII:'ok' CRC:OK P/F:1 (Info)
>> recv:'ok' CRC:OK n(s)=1 n(r)=1 from 0x02
>> resp: RR n(r)=1 to 0x02
SEND: 0x7E 0x02 0x81 0x83 0x01 0x7E n(r)=1 P/F:0 (Super - RR)
'''

### Comportamento Esperado - Primary Device 0x01
1. Initing...: Indica o início da execução do dispositivo primário.
2. My Address: 1: Mostra o endereço atribuído ao dispositivo primário (1).
3. send:'ok?' to 0x02: Envia a mensagem 'ok?' para o dispositivo secundário com endereço 0x02.
4. SEND: Exibe o frame de informação enviado, indicando o número de sequência (n(s)), número de recebimento (n(r)), conteúdo ASCII 5. ('ok?'), e se é um frame final (P/F).
5. RECV: Indica a recepção de um frame supervisório (super) do dispositivo secundário, confirmando a prontidão para receber a próxima mensagem (RR).
6. recv: RR CRC:OK n(r)=1 from 0x02: Mostra a confirmação da recepção do dispositivo secundário (0x02).
7. RECV: Indica a recepção de um frame de informação do dispositivo secundário, mostrando o conteúdo ('ok').
8. recv:'ok' CRC:OK n(s)=1 n(r)=1 from 0x02: Confirma a recepção da mensagem 'ok' do dispositivo secundário.
9. resp: RR n(r)=1 to 0x02: Responde ao dispositivo secundário confirmando a prontidão para receber a próxima mensagem (RR).
10. SEND: Envia um frame supervisório (super) para o dispositivo secundário, indicando prontidão para receber a próxima mensagem (RR).

**********

```
### Secondary Device - Initing...
My Address: 2
RECV: 0x7E 0x02 0x19 0x6F 0x6B 0x3F 0x79 0xA5 0x7E n(s)=1 n(r)=1 ASCII:'ok?' CRC:OK P/F:1 (Info)
>> recv:'ok?' CRC:OK n(s)=1 n(r)=1 from 0x01
>> resp: RR n(r)=1 to 0x01
SEND: 0x7E 0x01 0x81 0x81 0x01 0x7E n(r)=1 P/F:0 (Super - RR)
>> send:'ok' to Primary
SEND: 0x7E 0x01 0x19 0x6F 0x6B 0x4B 0x06 0x7E n(s)=1 n(r)=1 ASCII:'ok' P/F:1 (Info)
RECV: 0x7E 0x02 0x81 0x83 0x01 0x7E n(r)=1 CRC:OK P/F:0 (Super - RR)
>> recv: RR CRC:OK n(r)=1 from 0x01
```

### Comportamento Esperado - Secondary Device 0x02
1. Initing...: Indica o início da execução do dispositivo secundário 1.
2. My Address: 2: Mostra o endereço atribuído ao dispositivo secundário 1 (2).
3. RECV: Indica a recepção de um frame de informação do dispositivo primário, mostrando o conteúdo ('ok?').
4. recv:'ok?' CRC:OK n(s)=1 n(r)=1 from 0x01: Confirma a recepção da mensagem 'ok?' do dispositivo primário (0x01).
5. resp: RR n(r)=1 to 0x01: Responde ao dispositivo primário confirmando a prontidão para receber a próxima mensagem (RR).
6. SEND: Envia um frame de informação para o dispositivo primário, indicando o conteúdo ('ok') e que é um frame final (P/F).
7. RECV: Indica a recepção de um frame supervisório (super) do dispositivo primário, confirmando a prontidão para receber a próxima mensagem (RR).
8. recv: RR CRC:OK n(r)=1 from 0x01: Confirma a prontidão do dispositivo primário para receber a próxima mensagem.

**********

```
Secondary Device - Initing...
My Address: 3
RECV: 0x7E 0x02 0x19 0x6F 0x6B 0x3F 0x79 0xA5 0x7E <drop>
RECV: 0x7E 0x01 0x81 0x81 0x01 0x7E <drop>
RECV: 0x7E 0x01 0x19 0x6F 0x6B 0x4B 0x06 0x7E <drop>
RECV: 0x7E 0x02 0x81 0x83 0x01 0x7E <drop>
```

### Comportamento Esperado - Secondary Device 0x03
1. Initing...: Indica o início da execução do dispositivo secundário 2.
2. My Address: 3: Mostra o endereço atribuído ao dispositivo secundário 2 (3).
3. RECV: Indica a tentativa de recepção de um frame de informação do dispositivo primário, mas o frame é descartado.
4. RECV: Indica a tentativa de recepção de um frame supervisório (super) do dispositivo primário, mas o frame é descartado.
5. RECV: Indica a tentativa de recepção de um frame de informação do dispositivo primário, mas o frame é descartado.
6. RECV: Indica a tentativa de recepção de um frame supervisório (super) do dispositivo primário, mas o frame é descartado.

> Nota: Este README fornece uma visão geral do projeto. Consulte a documentação interna do código-fonte para informações detalhadas sobre a implementação.