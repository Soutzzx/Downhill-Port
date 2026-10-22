# Construtor gráfico do Downhill Port (Windows)

Abra **Build-GUI.cmd** com dois cliques na pasta principal do repositório.
Também pode executar **python Build-GUI.py** pelo terminal do VS Code.
A interface usa Tkinter (vem com Python para Windows): não precisa instalar
Electron, PyQt ou outras dependências para abrir a janela.

## Botões

- **Selecionar ISO**: escolha sua própria ISO USA do Downhill Domination. Se ela
  estiver fora da pasta iso/, o programa oferece copiá-la para essa pasta.
  Você também pode deixar sua ISO na pasta iso/ manualmente.
- **Verificar ferramentas**: verifica Git, LLVM/Clang, CMake, 7-Zip e Python.
  O projeto também precisa do Visual Studio C++ Build Tools (bibliotecas x86 e
  Windows SDK).
- **Build completo**: verifica o ELF USA, instala os pacotes Python, faz o split
  somente se asm/ e assets/ estiverem ausentes, e executa o build limpo.
- **Continuar linkagem**: para quem já viu o build.py sair com código 0;
  reutiliza os objetos compilados existentes e passa --skip-compile ao script
  de build. **A linkagem ainda recompila funções MIPS convertidas para C,
  o que pode levar minutos.**
- **Cancelar**: solicita o encerramento da árvore de processos da compilação;
  objetos parciais podem exigir um novo build.
- **Abrir logs**: abre build/port/clean/logs/; a interface também registra a
  saída recebida em build/gui/build.log.
- **Abrir pasta do jogo**: após sucesso, abre
  build/port/clean/<revisão>/, onde fica dhd.exe.

## Barra de progresso

A porcentagem acompanha as etapas verificáveis e estima o avanço da compilação
inicial contando objetos .o gerados. **Não é uma estimativa de tempo.**
A fase de linkagem usa uma barra animada e mostra 70%+, porque o projeto
recompila iterativamente funções adicionais sem fornecer um total antecipado.
100% só é mostrado após o script de build retornar sucesso.

Não inicie uma segunda compilação em outro terminal enquanto a interface
estiver trabalhando. Se outra compilação já estiver aberta no VS Code,
termine-a antes de iniciar um novo build pela interface.

## Privacidade e direitos

As ISOs, o ELF SCUS_971.77, os arquivos asm//assets/ e os objetos
compilados são locais e ignorados pelo Git. **Não os envie nem adicione ao
repositório.** A aplicação usa os scripts do repositório e nunca envia os dados
do jogo. Na primeira execução, dependências e fontes da SDL3 podem ser baixadas
pelos próprios scripts do projeto.

Para explicação das dependências e do processo nativo, veja
[docs/BUILDING.md](docs/BUILDING.md).
