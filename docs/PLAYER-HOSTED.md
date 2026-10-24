# Multiplayer local por LAN/Radmin

**Estado:** codigo alterado; build completo, teste entre dois PCs e verificacao do Defender ainda pendentes.

O jogo hospeda salas no proprio `dhd.exe`. Todos precisam da mesma versao do port e de uma ISO USA compativel. O transporte da corrida e direto entre os jogadores e o PC do anfitriao, via TCP 8787. Nao depende do Cloudflare.

## Sala publica

1. O anfitriao escolhe **ONLINE > Host A Public Race (Your PC)**.
2. Os outros jogadores escolhem **ONLINE > Public Races (LAN/Radmin)** e selecionam a sala. Nao precisam digitar o codigo.
3. A lista usa descoberta UDP 8788. Se a VPN nao encaminhar broadcast, cada convidado informa o IPv4 Radmin do anfitriao em **Host IP** e atualiza a lista. Continua sem precisar do codigo.
4. Os jogadores escolhem personagem, bike e roupa, marcam Ready, e o anfitriao inicia a corrida.

## Sala privada

1. O anfitriao escolhe **ONLINE > Host A Private Race (Your PC)** e compartilha o codigo `Hxxxxx` exibido.
2. Cada convidado informa o IPv4 Radmin do anfitriao em **Host IP** e escolhe **Join With A Code**.
3. Duas instancias no mesmo computador conseguem usar o codigo sem configurar IP, pelo `127.0.0.1`.

Salas privadas nao respondem a descoberta UDP. Salas publicas tambem exibem um codigo, mas ele nao e necessario para entrar pela lista. O host permite ate oito jogadores. Ao deixar a sala ou fechar o jogo, o servidor integrado para.

## Rede

O firewall do anfitriao precisa permitir TCP 8787; para listar salas publicas, tambem UDP 8788. Radmin VPN fornece a rede entre os computadores. O jogo nao instala regras de firewall nem abre portas do roteador automaticamente. `host_address` no `dhd.ini` e o mesmo campo Host IP do menu. Nas redes em que o broadcast funciona, ele pode ficar vazio.

A fonte do antigo servico Cloudflare permanece em `online/server/` para trabalho futuro, mas o fluxo de jogo atual nao registra codigos la nem envia pacotes a ele. Nenhum Node.js, npm, PowerShell ou servidor externo precisa ser instalado pelo jogador.