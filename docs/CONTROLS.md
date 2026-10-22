# Remapeamento de teclas no PC (Windows)

O suporte a controles USB/Bluetooth (SDL3, XInput e Sony HID) não foi alterado.

## Onde configurar

Após gerar um novo dhd.exe com estas alterações, entre em:

**OPTIONS > PC SETTINGS > KEYBOARD BINDINGS**

São duas páginas com as ações de direção (esquerda/direita e inclinação),
pedalar, frear, golpes, manobras, câmera, pausa, sprint e derrapagem.

1. Escolha a ação e aperte Cross (Space na configuração padrão).
2. Solte a tecla de confirmação.
3. Pressione a nova tecla e solte-a. Esc cancela.
4. As teclas são salvas automaticamente no dhd.ini que fica junto ao jogo.

O atalho Esc para voltar/pausar permanece fixo para evitar perder
a possibilidade de cancelar o menu. Se uma nova tecla já pertencia
a outra ação, as duas são trocadas.

Na segunda página há a opção **Restore default keys**.

## Compatibilidade

- Todos os jogos têm os mesmos controles padrão se o jogador nunca configurar o teclado.
- As configurações novas são salvas como bind_00 até bind_18 no dhd.ini.
- O teclado continua enviando entradas de DualShock 2 à lógica original, de
  modo que o funcionamento do controle permanece independente.
- Há uma seção específica em docs/BUILDING.md para compilar o executável.

## Testes necessários antes do merge

Estas mudanças estão em um pull request experimental e precisam
ser compiladas e testadas em Windows com o jogo:

- Remapear a direção para WASD e confirmar movimentação durante a corrida.
- Remapear o pedal, soltar a tecla e garantir que não confirma menus involuntariamente.
- Reiniciar o jogo, verificar persistência no dhd.ini.
- Conferir se Esc cancela a captura e ainda permite sair dos menus.
- Restaurar padrões e verificar o teclado.
- Confirmar que o controle físico continua funcionando normalmente.

Não há ISO nem arquivos derivados do jogo no pull request.
