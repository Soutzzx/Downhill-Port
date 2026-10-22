# Executar no Windows com apenas dois cliques

A pessoa que baixar este repositório **não precisa de Python, CMake, LLVM, Visual Studio
ou ferramentas de compilação** para jogar. Ela precisa apenas de Windows 10/11,
acesso à internet na primeira execução e de sua própria ISO compatível de
Downhill Domination USA (SCUS-97177).

## Para quem vai jogar

1. Baixe o repositório pelo botão **Code > Download ZIP**, extraia e abra a pasta.
2. Dê dois cliques em **Play-Downhill.cmd**.
3. Se ainda não houver um executável instalado, o launcher encontra a versão
   pública mais recente em **Soutzzx/Downhill-Port > Releases**, verifica o SHA-256
   do pacote e extrai o jogo nativo para a pasta local **play/**.
4. O jogo abre e pergunta sua ISO USA na primeira vez. Selecione seu arquivo
   *.iso, que pode estar em outra pasta do computador.
5. Nas próximas vezes, abra **Play-Downhill.cmd** novamente. O caminho escolhido
   fica em **play/dhd.ini**; seu progresso fica em **play/saves/**.

Sem uma release publicada com os arquivos esperados, o launcher mostra uma
mensagem explicando o que está faltando — ele **não consegue criar um EXE**
sem a etapa de compilação realizada pelo desenvolvedor.

A ISO continua sendo necessária em cada inicialização do jogo, mas o usuário
não precisa selecioná-la outra vez enquanto o caminho salvo for válido.

**Nota de compatibilidade:** o port confere o conteúdo de SCUS_971.77
dentro da ISO e exige SHA-1
**4e25a222ad8a475b5d26c2e928e90b5f2b7075eb**.
O título ou código SCUS-97177 no nome do arquivo, por si só, não garante que
a imagem seja compatível. Não remova essa verificação.

## Para o desenvolvedor: primeira publicação

Você precisa publicar **uma vez** o executável que já compilou, para que
outros computadores possam obtê-lo automaticamente.

1. Conclua o build com seu próprio dump da edição USA e obtenha
   **build/port/clean/<revisao>/dhd.exe** junto de **manifest.json**.
2. Confira em **port/include/version.h** qual é o valor de DHD_VERSION.
   O executável deve ter sido compilado com essa mesma versão.
3. No PowerShell, na pasta do repositório, execute (substituindo o diretório
   pela revisão real):
   
       python port/tools/package_release.py --exe "build/port/clean/<revisao>/dhd.exe"

4. O empacotador confere o manifesto e faz uma verificação para não incluir
   dados do jogo, caminhos absolutos nem bibliotecas indevidas. Se falhar,
   não distribua o pacote até corrigir o motivo.
5. No GitHub, abra a aba **Releases > Draft a new release**; use a tag
   **v<VERSAO>** correspondente a DHD_VERSION (ex.: v1.0.0 para 1.0.0).
   Anexe os dois arquivos criados em **build/release/**:

       DownhillRevived-<VERSAO>-win.zip
       latest.json

6. Publique a release. Pronto: o launcher baixa o executável dessa release
   automaticamente no primeiro uso.

O manifesto **latest.json** é usado para verificar o endereço, versão e SHA-256
antes de instalar. O ZIP deve conservar a estrutura que o empacotador gera:
**DownhillRevived-<VERSAO>/dhd.exe** e os arquivos de licença/avisos.

O empacotador deste fork aponta para
**https://github.com/Soutzzx/Downhill-Port/releases**, inclusive nas URLs
de atualização. O launcher também configura este endereço no **dhd.ini**
recém-criado. Se o jogador já tiver configurações, o launcher não as substitui.

## O que não enviar ao repositório

**Não** remova as regras que ignoram as ISOs, o ELF e os recursos extraídos.
Mantenha **iso/**, **asm/**, **assets/**, **play/** e **build/** fora do Git.
A versão pronta para jogar é o arquivo de lançamento gerado pelo empacotador,
publicado como asset de uma Release, e não o jogo de PS2 original.

O port nativo ainda exige ISO local para modelos, áudio, menus, dados e
demais recursos do jogo. Não há BIOS nem emulador a instalar.

## Notas

- O launcher não muda mapeamentos de controle nem os comandos do teclado:
  essas melhorias são tratadas separadamente.
- Só baixar o código do Git **não produz o executável**. Ele é obtido da release.
- A opção **Play-Downhill.ps1 -CheckOnly** prepara/verifica a instalação, sem
  abrir o jogo; é útil para testes.
- O jogo tem um atualizador próprio, que lê o campo update_url do dhd.ini.
- Se uma versão nova for publicada com o mesmo número, o jogo pode não
  reconhecê-la como atualização; incremente DHD_VERSION antes de recompilar.
