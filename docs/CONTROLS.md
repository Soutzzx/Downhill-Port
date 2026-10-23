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

## Mouse e Ctrl esquerdo: binds de corrida

O mouse **não navega mais pelos menus**. A seleção por ponteiro foi removida
porque só funcionava em algumas telas e apontava para linhas erradas.

- Abra OPTIONS > PC SETTINGS > KEYBOARD BINDINGS > GAMEPLAY BINDINGS.
- Selecione **Punch** ou **Kick**, confirme com a tecla de menu, solte-a e
  **clique com o botão esquerdo ou direito do mouse**.
- Os botões esquerdo, direito, central, X1 e X2 são reconhecidos como
  **MOUSE LEFT**, **MOUSE RIGHT**, **MOUSE MIDDLE**, **MOUSE X1** e **MOUSE X2**.
- Também é possível selecionar **LEFT CTRL**, **RIGHT CTRL**,
  **LEFT SHIFT**, **RIGHT SHIFT** e **CAPS LOCK**. As duas teclas Shift são
  identificadas separadamente; Caps Lock atua quando a tecla é pressionada,
  não apenas por estar com as letras maiúsculas ativadas.
- Para atribuir Shift esquerdo ou Caps Lock, selecione uma ação, solte a tecla
  de confirmação, pressione a tecla desejada e solte-a. As teclas podem ser
  usadas tanto no MENU quanto em GAMEPLAY.
- Esses botões são válidos somente para os binds de **GAMEPLAY**.
  Menu continua usando teclado ou controle, para não perder os atalhos de
  navegação/confirmar.
- Os botões físicos do mouse só disparam essas ações **durante a corrida**
  e **quando a janela do jogo tem foco**.
- Ctrl+outra tecla é tratado como atalho e as outras teclas desse conjunto não
  são enviadas ao jogo. Isso evita interferir com Ctrl+Esc e similares.

Os binds permanecem salvos em `dhd.ini`, com os mesmos índices de gameplay
(`bind_00` a `bind_18`).

## Configurações antigas preservadas

- `bind_00` até `bind_18` em `dhd.ini` continuam sendo as teclas da **corrida**.
- `menu_bind_00` até `menu_bind_07` são as novas teclas do **menu**.
- Sem configuração de menu no arquivo, entram os padrões de seta, Space,
  Backspace, Enter e Tab.
- As configurações são salvas em `dhd.ini` junto do `dhd.exe`.

## Testes antes da distribuição

Esta alteração ainda precisa ser compilada e testada no Windows:

1. Em GAMEPLAY BINDINGS, alterar Punch para MOUSE RIGHT e Kick para
   MOUSE LEFT. Confirmar ambas as ações durante uma corrida.
2. Inverter os dois e confirmar que o jogo troca as teclas sem conflito.
3. Bindar LEFT CTRL para uma ação de corrida e verificar se ela responde.
4. Abrir menus e confirmar que nenhum clique no mouse navega ou ativa opções.
5. Reiniciar o jogo e confirmar a persistência dos binds no dhd.ini.
6. Confirmar que joystick físico e bindings de MENU continuam funcionando.
7. Testar jogar com foco fora da janela: mouse e teclas não devem atuar.

Não inclui ISO nem outros arquivos do jogo original.
