# Menu e corrida com teclas independentes (Windows)

O suporte existente a controles Xbox, DualSense, DualShock, Switch Pro e
controles genéricos não foi alterado. O mouse não é utilizado durante a corrida.

## Remapeamento dentro do jogo

Abra **OPTIONS > PC SETTINGS > KEYBOARD BINDINGS**.

- **MENU BINDINGS:** cima, baixo, esquerda, direita, confirmar, voltar, iniciar e selecionar.
- **GAMEPLAY BINDINGS:** direção, inclinação, pedal, freio, pausa, manobras, câmera,
  soco, chute, sprint e derrapagem, divididos em duas páginas.
- Selecione a ação, solte a tecla de confirmação e pressione a nova tecla.
- Esc cancela a captura; nos menus, Esc continua sendo uma tecla de segurança
  para voltar mesmo quando BACK foi remapeada.
- É permitido usar **a mesma tecla em uma ação de menu e outra de corrida**.
  Por exemplo: W sobe nos menus e inclina o ciclista na corrida, sem conflito.
- Duas ações do **mesmo** contexto continuam trocando de tecla em caso de
  conflito para evitar acionar dois comandos diferentes ao mesmo tempo.
- Cada grupo tem seu botão de restaurar padrões.

### Padrões de menu

| Ação | Tecla |
|---|---|
| Cima | Seta para cima |
| Baixo | Seta para baixo |
| Esquerda | Seta para esquerda |
| Direita | Seta para direita |
| Confirmar | Espaço |
| Voltar | Backspace (Esc também funciona sempre) |
| Iniciar | Enter |
| Selecionar | Tab |

Os padrões de corrida permanecem os da versão anterior, incluindo Esc para
pausar e as ações alternativas de pedal e freio. Pausa e voltar agora têm
contextos separados; mudar a pausa na corrida não muda a tecla de voltar do menu.

## Mouse: somente nos menus

O mouse nunca controla câmera, direção, ataques ou personagem durante a corrida.

- **Rolagem:** navega para cima/baixo em menus tradicionais e popups do port.
- **Clique esquerdo:** confirma a opção atualmente selecionada no menu original.
- **Clique direito:** volta/cancela nos menus.
- **Passar o cursor e clicar numa linha:** seleciona diretamente a linha nos
  menus popup do port, como PC SETTINGS, MENU BINDINGS e GAMEPLAY BINDINGS.

A seleção direta pelo ponteiro foi implementada para os popups da camada nativa
do port. As telas originais do PS2, incluindo a página inicial, têm layouts e
códigos de seleção diferentes e **ainda usam rolagem + clique para selecionar**.
Os limites visuais da seleção de popups são aproximados e precisam de teste no
Windows em resoluções e proporções de tela diferentes.

## Configurações antigas preservadas

- `bind_00` até `bind_18` em `dhd.ini` continuam sendo as teclas da **corrida**.
- `menu_bind_00` até `menu_bind_07` são as novas teclas do **menu**.
- Sem configuração de menu no arquivo, entram os padrões de seta, Space,
  Backspace, Enter e Tab.
- As configurações são salvas em `dhd.ini` junto do `dhd.exe`.

## Testes antes da distribuição

Este PR precisa de **compilação e testes reais no Windows**:

1. Alterar movimento de corrida para WASD sem afetar as setas do menu.
2. Colocar W para subir no menu e também inclinar na corrida; ambos devem funcionar.
3. Alterar confirmar no menu e pedal na corrida separadamente.
4. Alterar a pausa na corrida; Esc deve continuar permitindo sair dos menus.
5. Testar os botões de restaurar, reiniciar o jogo e conferir persistência.
6. Testar mouse com janela 4:3, 16:9 e borderless; cursor sobre cada linha dos
   menus de opções; rolagem e clique no menu inicial.
7. Confirmar que durante a corrida o mouse não influencia a jogabilidade.
8. Confirmar que um controle físico continua funcionando normalmente.

Sem arquivos originais do jogo ou ISO neste PR.
