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
que o port já conhece. **Não é necessário configurar `host_directory` para
jogar normalmente. NÃO use `host_directory = ws://127.0.0.1:8787` em PCs
diferentes:** `127.0.0.1` é sempre o próprio computador, e não o do amigo.

**Player Hosted não faz mais fallback para Cloudflare.** A sala é criada
imediatamente **no servidor nativo dentro do dhd.exe**, com código sempre
`Hxxxxx`, mesmo sem conexão com a internet. Esse código funciona em outra
instância no mesmo PC sem diretório remoto. O diretório online só é necessário
para um amigo em outra rede encontrar seu endereço público usando esse código.

Se o diretório HTTPS estiver inacessível ou rejeitar o registro, o lobby
**continua sendo Player Hosted**, mas exibe uma mensagem de diagnóstico
("H-code rejected"/"H-code offline"). Não se conecta secretamente ao
servidor de corrida da Cloudflare. Reintenta o registro a cada ~4 segundos e
renova os registros aceitos aproximadamente a cada 20 segundos. O serviço
deve estar atualizado com os endpoints de registro e resolução.

O **único** uso da Cloudflare nesse modo, quando o diretório oficial está
disponível, é o mapeamento de código para endereço IP. **Nenhum ping de
jogabilidade, bike, pacote ou evento da corrida passa por ela.** Os códigos
com prefixo H são reservados ao Player Hosted; as novas salas oficiais
Cloudflare usam outros prefixos.

**Importante:** o sucesso do registro não testa se uma pessoa na internet
consegue realmente abrir o WebSocket para a porta 8787. Se o diretório
encontrar `Hxxxxx` mas a conexão não abrir, verifique encaminhamento de
porta, Firewall do Windows e CGNAT. O servidor nativo agora aceita IPv4 e
IPv6 (quando disponíveis), e usa TCP_NODELAY e polling de 10 ms para reduzir
o ping. Mesmo assim, o host precisa ter acesso de entrada a essa porta.
Uma conexão atrás de CGNAT sem rota pública não pode ser corrigida apenas
com o código da sala.

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

## Distinguir diretório, servidor e rota

- **Código Hxxxxx aparece**: a corrida está hospedada no `dhd.exe`. Não
  há fallback para o relay Cloudflare.
- **H-code rejected**: o serviço HTTPS respondeu com erro; confira se a
  versão atual de `online/server/` foi implantada na Cloudflare. Sem
  diretório o jogador remoto não pode descobrir o IP usando só seis letras.
- **H-code offline**: conexão HTTPS ou DNS indisponível. O jogo tentará
  registrar novamente automaticamente enquanto a sala existir.
- **Amigo consegue resolver o H-code, mas não entrar**: a conexão direta ao
  host falhou. Teste firewall do Windows, encaminhamento TCP 8787 do
  roteador, IPv4 público ou conectividade IPv6. CGNAT pode impedir
  conexões IPv4 de entrada independentemente da qualidade do código.
- **Código comum (sem H)**: lobby Cloudflare tradicional, independente
  do Player Hosted.

Este modo não instala regras de firewall nem abre portas do roteador
automaticamente. Não prometa conexão entre duas redes domésticas sem
verificar essa condição. A escolha `Host A Private Game` não expõe outros
serviços além do listener TCP 8787 do jogo.

## Testar em um computador

Sem configurar portas do roteador ou implantar o diretório, é possível
testar com duas instâncias do jogo no mesmo Windows:

1. Inicie o primeiro `dhd.exe`, hospede uma sala e anote o código Hxxxxx.
2. Execute uma segunda cópia do mesmo `dhd.exe` em outra pasta com
   `dhd.ini` próprio (a mesma ISO pode ser selecionada).
3. Use **ONLINE > Join With A Code** e digite o código. O cliente consulta
   primeiro o servidor nativo em `127.0.0.1:8787`; só depois tenta o
   diretório externo. Não precisa preencher IP nem iniciar CMD.
4. Teste seleção original de personagem, bike e roupa antes do Ready,
   depois Start, corrida e saída da sala. Ao terminar, teste **New Level** e
   **Try Again** pelo anfitrião sem desconectar os convidados.
5. Confirme que a porta 8787 deixa de responder após o anfitrião sair.

## Servidor público

As opções **Public Races**, **Create A Public Race**, **Create A Private
Race** e **Join With A Code** continuam preservadas. As salas oficiais
ainda usam Cloudflare para comunicação; **Player Hosted é um modo adicional**
e não afeta quem prefere o online original.

Esta implementação não contém servidores dedicados, matchmaking automático
entre NATs, moderação remota ou garantias de latência. O ganho de ping
depende da rota entre o convidado e o anfitrião.
