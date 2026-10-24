# Segurança e falso positivo do Windows Defender

O Downhill Revived é um port comunitário do jogo. Este repositório compila o
`dhd.exe` a partir do código-fonte e **não fornece a ISO**. O multiplayer por
IP/Radmin VPN e o servidor integrado foram preservados nesta revisão.

## Detecção Trojan:Win32/Bearfoos.B!ml

Esse nome é uma classificação do Microsoft Defender, **não uma prova de
que o arquivo é seguro ou infectado**. Nunca instrua jogadores a desligar
o antivírus, adicionar exceções ou executar um arquivo em quarentena.

O port usa técnicas incomuns para reconstruir o ambiente de um programa
original de PlayStation 2 em Windows, incluindo mapa virtual de memória
fixo, um relançamento suspenso do *próprio* executável para reservar
endereços, e um atualizador que pode substituir o executável com uma
nova versão baixada após verificar SHA-256. O host integrado recebe conexões
WebSocket e o UPnP opcional tenta abrir uma porta no roteador. Essas
funcionalidades são implementadas abertamente no projeto, mas precisam
ser auditadas e podem atrair classificações por heurística. Não atribua
a detecção a um componente específico sem evidência.

## O que esta revisão garante

- O recurso VERSIONINFO identifica o `dhd.exe` como um programa do projeto
  independente Downhill Revived, sem fingir ser o editor original do jogo.
- `version.h` é a única fonte da versão textual e numérica; o build rejeita
  números divergentes.
- O verificador `release_safety.py` inspeciona a assinatura PE, número da
  versão e SHA-256 **sem executar o EXE**. O build completo exige que a
  versão embutida esteja correta antes de copiar um arquivo para distribuição.
- O atualizador requer HTTPS para os manifestos e arquivos ZIP, além de
  verificar o SHA-256 já exigido pelo programa.
- O Windows CI compila e valida o recurso PE em um executável de testes
  sem precisar da ISO. O CI também verifica o servidor nativo de até oito
  jogadores.

**Isso não prova ausência de malware nem garante remoção de alertas.**
Esta revisão não acrescenta assinatura Authenticode: para assinar é
necessário um certificado de assinatura de código legítimo do mantenedor.
Adicionar textos de versão não equivale a assinar o EXE.

## Como compilar e validar no seu Windows

Depois de fazer merge do PR e `git pull`, execute `Build-GUI.py` e escolha
**Build completo**. A verificação de versão acontece automaticamente.
Depois de um build concluído, ache o novo arquivo em
`build/port/clean/<commit>/dhd.exe` e rode:

```powershell
python port/tools/release_safety.py "build/port/clean/SEU_COMMIT/dhd.exe" --defender
```

O `--defender` solicita explicitamente ao Microsoft Defender para examinar
**apenas o executável informado**. Se o antivírus ainda detectar uma ameaça,
não distribua nem force sua execução. A Proteção do Windows pode colocá-lo
em quarentena. A saída também mostra o SHA-256 exato dessa compilação.

Evite passar a ISO ou arquivos pessoais a serviços externos. Para
pesquisar as detecções, você pode consultar o hash do executável com
autorização. Use **Submissão para Desenvolvedor de Software** no portal
oficial da Microsoft:

https://www.microsoft.com/en-us/wdsi/filesubmission

Envie apenas o arquivo de sua autoria para análise, selecione **Incorrectly
detected as malware** quando tiver evidência suficiente e inclua o código
de detecção, o hash SHA-256, a URL do repositório e a explicação do mapa de
memória/atualizador. Aguarde o resultado oficial antes de classificá-lo
como falso positivo confirmado.

## Distribuição transparente

Publique versões compiladas de um commit conhecido. Divulgue o SHA-256 e
changelog, mantenha fontes de download sob o seu controle, use HTTPS e,
quando possível, assinatura de código legítima. Não use packers,
encriptação para ocultar o executável, exclusões de antivírus ou scripts
que tentem contornar o Windows Defender.

O `dhd.exe` antigo enviado para análise tinha SHA-256:
`a34048e6441d266a8d60e2d41b5aa1642841bc9ef52eba5de0e22ec905acf3ee`.
Este hash identifica apenas aquela cópia recebida, não a revisão atual do
repositório nem futuros builds.
