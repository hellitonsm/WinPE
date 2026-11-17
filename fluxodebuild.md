Fluxo correto:
# 1. Montar/descompactar o boot.wim na pasta de trabalho
./wpetool/wpe-tool.sh workdir WinPE_amd64.iso wpe-work

#2. compilar (caso modifique)
x86_64-w64-mingw32-gcc wpetool/source/menu_iniciar.c -o wpetool/templates/menu_iniciar.exe -mwindows

# 3. Embutir os arquivos de templates/ no System32
./wpetool/wpe-tool.sh settemplate wpe-work/Windows/System32

# 3. Salvar e gerar nova ISO
./wpetool/wpe-tool.sh build wpe-work WinPE_menu.iso

# 4. Rodar a ISO gerada
./run-winpe-amd64.sh WinPE_menu.iso
