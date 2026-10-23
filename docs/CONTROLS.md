# Controles padrão do Downhill Port (Windows)

O teclado tem perfis independentes para **menus** e **corridas**. As configurações
abaixo são as que passam a vir de fábrica em uma instalação nova, baseadas
no preset enviado pelo mantenedor. O gamepad SDL3 permanece inalterado.

## Como configurar dentro do jogo

Em **OPTIONS > PC SETTINGS > KEYBOARD BINDINGS**, escolha
**MENU BINDINGS** ou **GAMEPLAY BINDINGS**.

1. Selecione a ação e confirme.
2. Solte a tecla usada para confirmar.
3. Pressione a nova tecla ou botão do mouse (somente GAMEPLAY).
4. Solte a tecla. A atribuição é salva em `dhd.ini`; Esc cancela a captura.

É possível repetir uma tecla entre **MENU** e **GAMEPLAY**, sem conflitos entre
os contextos. Duas ações do mesmo contexto trocam de tecla ao entrar em
conflito. **Restore Defaults** recupera os padrões listados abaixo.

## Menu: teclas padrão

| Ação | Tecla |
|---|---|
| Cima | W |
| Baixo | S |
| Esquerda | A |
| Direita | D |
| Confirmar | Espaço |
| Voltar | Backspace |
| Iniciar | Enter |
| Selecionar | Tab |

Esc também funciona como **Voltar** de segurança, mesmo se Back for alterado.
O mouse não navega pelos menus: a tentativa anterior de hover/clique direto
foi removida devido ao desalinhamento do cursor.

## Corrida: teclas e botões padrão

| Ação | Tecla ou botão |
|---|---|
| Virar à esquerda | A |
| Virar à direita | D |
| Inclinar para frente | Shift esquerdo |
| Inclinar para trás | Ctrl esquerdo |
| Pedalar | Seta para cima |
| Pedalar (alternativo) | X |
| Frear | S |
| Frear (alternativo) | Backspace |
| Pausar | Esc |
| Iniciar / pausar | Enter |
| Câmera / selecionar | Tab |
| Socar | Botão esquerdo do mouse |
| Chutar | Botão direito do mouse |
| Manobra L1 | Q |
| Manobra R1 | E |
| Manobra L2 | Z |
| Manobra R2 | Espaço |
| Sprint | W |
| Derrapagem | F |

Os botões centrais e laterais do mouse também podem ser mapeados
manualmente para ações de corrida. O leitor de entrada permite combinar
Ctrl esquerdo com direção, pedal e ataques ao correr. Alt e tecla Windows
continuam reservados para atalhos do sistema.

## Configurações de vídeo padrão

- Renderer: native; escala: 2×
- Display: borderless; tamanho de janela (caso usado): 1280×960
- Widescreen: ligado; picture: wide; filtro: linear
- Filmes de abertura: ativados (`skip_movies = 0`)
- FPS: 60; texturas substitutas: off; fonte: sharp
- Atualizações: perguntar (`auto_update = ask`)

## Perfis existentes e persistência

- `bind_00` a `bind_18` são os bindings de corrida.
- `menu_bind_00` a `menu_bind_07` são os bindings de menu.
- Um `dhd.ini` existente prevalece sobre os defaults do executável.
- Variáveis `DHD_*` existentes prevalecem sobre as opções do arquivo.
- O caminho da ISO não é pré-configurado: cada pessoa seleciona a própria
  cópia USA compatível de Downhill Domination (SCUS-97177).

## Testes antes de distribuir a nova versão

1. Em uma pasta sem `dhd.ini`, verificar WASD no menu e cliques para atacar
   na corrida.
2. Testar A/D junto com Ctrl esquerdo para inclinação.
3. Testar Shift esquerdo com direção e pedalar com a seta para cima.
4. Reiniciar o jogo para confirmar que as opções persistem.
5. Modificar uma ação, clicar em **Restore Defaults** e verificar o preset novo.
6. Confirmar que um `dhd.ini` antigo continua com as teclas escolhidas antes.
7. Confirmar que a seleção da ISO e o controle físico continuam funcionando.

As alterações precisam de uma nova compilação do port, mas não incluem nem
distribuem conteúdo do jogo original.
