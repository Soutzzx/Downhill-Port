# Player Hosted: sala privada com o mesmo código de sempre (Windows)

**Status:** implementação experimental, precisa de testes reais de ponta a ponta e
de implantação do serviço de diretório para funcionar fora de uma rede local.
O multiplayer tradicional da Cloudflare **não foi substituído**.

## Para quem entra na partida

1. Abra o jogo atualizado, entre em **ONLINE > Join With A Code**.
2. Digite o **código de seis caracteres** informado pelo anfitrião.
3. O jogo consulta o diretório. Se o código for de **Player Hosted**, conecta
   ao computador do anfitrião. Se não estiver registrado, usa o servidor
   online normal, sem alterar as salas privadas existentes.
4. Escolha jogador e bicicleta, marque Ready e jogue como antes.

**Não é preciso informar IP ou porta para entrar.** A identificação por código
funciona depois de o diretório ser configurado e o PC do host estar acessível.

## Para quem hospeda — inicialização automática

**Não é mais necessário abrir `PlayerHost.cmd` manualmente.**

1. Abra `dhd.exe` e entre em **ONLINE > Host A Private Game (Your PC)**.
2. O jogo inicia o relay local em segundo plano, sem CMD visível, e mostra
   uma tela temporária com **Cancel** até `/v1/health` responder.
3. Quando estiver pronto, entra automaticamente no **RACE ROOM**, criando
   a sala e o código de seis caracteres.
4. Envie **apenas esse código** aos amigos. Eles usam o mesmo
   **ONLINE > Join With A Code** de sempre.
5. Ao sair da sala/voltar do multiplayer, o processo do servidor e seus
   subprocessos são encerrados. Se `dhd.exe` for fechado, o Windows também
   os encerra com um Job Object.

O primeiro uso pode demorar um pouco mais: o script instala dependências
com `npm ci` quando necessário. Se falhar, o jogo informa o problema;
as informações técnicas estão em `%TEMP%\\dhd-player-host.log`.
`PlayerHost.cmd` permanece disponível apenas para testes e depuração manual.

**Limite atual importante:** para hospedar, a máquina ainda precisa de
**Node.js 22+** e dos arquivos `online/player-host.ps1` e
`online/server/` (incluindo `package.json`, lockfile e código fonte).
O `dhd.exe` sozinho **não contém** esse runtime/servidor. O jogo procura
esses arquivos ao lado do executável ou em pastas pai, facilitando os
testes a partir do repositório. Uma distribuição pública deve incluir os
arquivos de host e ter o Node.js instalado/disponível, ou criar um
empacotamento autossuficiente separado.

Até **8 jogadores**, incluindo o host, utilizando o mesmo protocolo do
servidor atual. O servidor não renderiza oito jogos: recebe e redistribui
os estados da corrida.

O código é anunciado no serviço de diretório apenas quando há um jogador
hospedando e renovado periodicamente. Os registros vencem em até 90 segundos
se o host sair. O diretório **não transmite os dados da corrida**.

## Dependência importante: serviço de diretório

É preciso **implantar no Cloudflare Workers o `online/server` atualizado**,
com os novos endpoints:

- `POST /v1/player-host/register`: mapeia o código para o endereço público de
  origem do PC que registrou a sala (não aceita IP arbitrário no corpo).
- `GET /v1/player-host/XXXXXX`: resolve o código privado em `ws://host:8787`.

O host utiliza como padrão o endereço
`https://dhd-online.downhill-revived.workers.dev`. **A versão atualmente
implantada por terceiros pode não ter os novos endpoints.** Alterar o código
no GitHub **não** implanta mudanças nesse servidor. Sem acesso ao serviço
original, implante sua própria instância do `online/server` no Cloudflare.

Para usar seu diretório, inicie o host por PowerShell na raiz:

```powershell
powershell -ExecutionPolicy Bypass -File online/player-host.ps1 -RegistryUrl https://SEU-WORKER.workers.dev
```

Nos arquivos `dhd.ini` **dos clientes e do host**, coloque:

```ini
host_directory = wss://SEU-WORKER.workers.dev
```

Isso **não altera** a opção `server` do multiplayer tradicional. As salas
públicas continuam usando o servidor atual.

## Internet, ping e CGNAT

**Código de sala não elimina as restrições de rede.** O computador do host
precisa receber conexões TCP 8787:

- Permita o programa/porta no Windows Firewall.
- Configure o roteador para encaminhar **TCP 8787** ao IP local do host
  quando houver um IPv4 público acessível.
- Se a operadora usar **CGNAT**, encaminhar a porta normalmente não basta:
  pode ser necessário solicitar IP público à operadora ou usar um túnel/VPN
  que aceite conexões dos participantes.
- A sala pode estar registrada e ainda assim falhar ao conectar por bloqueio
  de porta ou NAT. Não existe fallback automático em túnel/relay nesta versão.
- Os jogadores conectam diretamente ao endereço do anfitrião; esse endereço
  fica acessível para quem conhecer um código de sala válido. Por enquanto,
  o transporte direto é `ws://` (sem TLS) e destina-se a testes com amigos
  de confiança.

O ping deve ser medido na corrida. Não há promessa de um valor máximo entre
Minas, Rio de Janeiro e Nordeste: depende da operadora, rota e upload do host.

## Testes recomendados

1. Primeiro mescle a correção do **Race Room** (PR #15) e a mudança de
   Player Hosted, gere um `dhd.exe` novo e instale o diretório atualizado.
2. Crie uma sala privada normal na Cloudflare, entre usando o código e
   verifique que ela continua funcionando.
3. Escolha `ONLINE > Host A Private Game (Your PC)`, sem abrir CMD;
   confirme a criação da sala. Consulte `%TEMP%\\dhd-player-host.log`
   se o servidor não iniciar ou o registro falhar.
4. Consulte `https://SEU-WORKER.workers.dev/v1/player-host/CODIGO` no
   navegador: deve retornar o endereço do host, sem exigir IP no jogo.
5. Tente entrar pelo código em outro PC/rede; depois teste Ready, Start,
   atualizações de outros ciclistas e desconexão do host.
6. Teste com 2, 4 e, por fim, **8** jogadores medindo ping e upload.

## O que não muda

- Salas **Public Races**, **Create A Public Race**, **Create A Private
  Race** e **Join With A Code** continuam disponíveis.
- A compatibilidade de versão e de dados originais de Downhill Domination
  USA continua obrigatória.
- Esta implementação não inclui ISO, modelos, texturas nem outros arquivos
  do jogo.
