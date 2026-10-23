# Player Hosted — somente dhd.exe + ISO

**Status:** servidor nativo integrado ao executável. Ainda precisa ser compilado e
testado no Windows antes da publicação como versão estável.

## Como jogar

1. Execute **dhd.exe** e selecione sua ISO USA compatível.
2. Para hospedar, entre em **ONLINE > Host A Private Game (Your PC)**.
3. O próprio **dhd.exe** abre a porta TCP 8787, cria a sala privada e exibe
   um código de seis caracteres com prefixo **H** (ex.: `H7K9PA`).
4. Seus amigos entram normalmente em **ONLINE > Join With A Code** e
   digitam o código informado. Não precisam instalar ou executar servidor.
5. **Ready > Start The Race** como nas salas Cloudflare. Até **8 jogadores**,
   incluindo o anfitrião.
6. O servidor fecha automaticamente ao sair da sala ou quando o processo
   dhd.exe do anfitrião termina.

## Arquivos

A hospedagem **não precisa de Node.js, npm, PowerShell, Wrangler, scripts ou
pasta `online/`**. O servidor de salas/WebSocket, pings e retransmissão da
corrida agora é código nativo C compilado dentro do `dhd.exe`. A mesma
cópia do jogo serve para criar, entrar, jogar offline e hospedar.

**Distribuição de arquivos:** `dhd.exe` apenas, mais a **ISO própria e
compatível** que cada jogador seleciona. O projeto **não distribui ISO**
nem outros arquivos proprietários de Downhill Domination.

O código do servidor Cloudflare em `online/server/` continua no repositório
**somente para administrar o serviço público e o diretório dos códigos**,
não como dependência de execução do jogo.

## Limitações de conexão pela internet

O executável faz o transporte da corrida diretamente pelo host, mas um código
curto não pode carregar sozinho o IP público e a porta do host: um pequeno
**diretório HTTPS** associa temporariamente `Hxxxxx` ao IP:porta do anfitrião.
Ele tem os endpoints:

- `POST /v1/player-host/register`
- `GET /v1/player-host/Hxxxxx`

O serviço Cloudflare escolhido em `host_directory` no `dhd.ini` precisa
**estar implantado com esses endpoints**. Por padrão usa o endpoint oficial
que o port já conhece. O registro renova aproximadamente a cada 20 segundos
e expira após 90 segundos sem atualização. **Não transmite a corrida.**

O anfitrião precisa conseguir receber conexões TCP **8787**. Permita o
executável no Firewall do Windows e, em IPv4 com NAT, configure o roteador
para encaminhar a porta para o IP local do computador. Usuários sob
**CGNAT** podem precisar de IP público da operadora, VPN ou túnel.
O código da sala **não elimina** essas limitações. Não existe nesta versão
conexão automática via TURN/STUN ou relay alternativo.

A conexão direta usa WebSocket **sem TLS** (`ws://`), e quem obtiver um
código válido poderá descobrir o IP do host por meio do diretório.
Hospede apenas com pessoas de confiança até existir autenticação e suporte
de segurança mais avançado.

## Testar em um computador

Sem configurar portas do roteador ou implantar o diretório, é possível
testar com duas instâncias do jogo no mesmo Windows:

1. Inicie o primeiro `dhd.exe`, hospede uma sala e anote o código Hxxxxx.
2. Execute uma segunda cópia do mesmo `dhd.exe` em outra pasta com
   `dhd.ini` próprio (a mesma ISO pode ser selecionada).
3. Use **ONLINE > Join With A Code** e digite o código. O cliente consulta
   primeiro o servidor nativo em `127.0.0.1:8787`; só depois tenta o
   diretório externo. Não precisa preencher IP nem iniciar CMD.
4. Teste seleção de jogador, Ready, Start, corrida e saída da sala.
5. Confirme que a porta 8787 deixa de responder após o anfitrião sair.

## Servidor público

As opções **Public Races**, **Create A Public Race**, **Create A Private
Race** e **Join With A Code** continuam preservadas. As salas oficiais
ainda usam Cloudflare para comunicação; **Player Hosted é um modo adicional**
e não afeta quem prefere o online original.

Esta implementação não contém servidores dedicados, matchmaking automático
entre NATs, moderação remota ou garantias de latência. O ganho de ping
depende da rota entre o convidado e o anfitrião.
