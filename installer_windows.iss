; ==============================================================================
;   Vellum - Script Inno Setup para Instalador Windows (.exe)
;   Gera o instalador modular Vellum-Setup-Windows-v1.0.1.exe
; ==============================================================================

#define MyAppName "Vellum"
#define MyAppVersion "1.0.1"
#define MyAppPublisher "Vellum Team"
#define MyAppURL "https://github.com/LeinadSaia/Vellum"
#define MyAppExeName "Vellum.exe"

[Setup]
; Identificador único da aplicação
AppId={{9F8D346B-3C2E-4F8A-A2E1-72B9A098FE71}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=dist_installer
OutputBaseFilename=Vellum-Setup-Windows-v{#MyAppVersion}
SetupIconFile=release_windows\app_icon.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

; Configurações do Desinstalador (Limpo, Fácil e Amigável)
UninstallDisplayName=Desinstalar Vellum
UninstallDisplayIcon={app}\app_icon.ico
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "default"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupAppTitle=Instalador do Vellum
SetupWindowTitle=Instalador - %1
WelcomeLabel1=Bem-vindo ao Assistente de Instalação do Vellum
WelcomeLabel2=Este programa instalará o Vellum no seu computador.%n%nRecomenda-se fechar todos os outros aplicativos antes de continuar.
SelectDirLabel3=O assistente instalará o Vellum na seguinte pasta.
SelectDirBrowseLabel=Para continuar, clique em Avançar. Se desejar selecionar uma pasta diferente, clique em Procurar.
SelectTasksLabel2=Selecione as tarefas adicionais que deseja executar enquanto instala o Vellum:
ReadyLabel1=O assistente está pronto para começar a instalar o Vellum no seu computador.
ReadyLabel2a=Clique em Instalar para continuar com a instalação.
ClickNext=Clique em Avançar para continuar ou Cancelar para sair do instalador.
ButtonNext=&Avançar >
ButtonBack=< &Voltar
ButtonInstall=&Instalar
ButtonCancel=Cancelar
ButtonFinish=&Concluir

[CustomMessages]
CreateDesktopIcon=Criar atalho na Área de Trabalho
AdditionalIcons=Atalhos Adicionais:
LaunchProgram=Iniciar o %1

[Types]
Name: "recommended"; Description: "Instalação Padrão Recomendada (Mais rápida e pronta para uso)"; Flags: iscustom
Name: "full";        Description: "Instalação Completa (Permite selecionar Modelos Locais Ollama)"
Name: "custom";      Description: "Instalação Personalizada"

[Components]
Name: "core";          Description: "Núcleo do Vellum, Leitor de PDF e Backend (Obrigatório)"; Types: recommended full custom; Flags: fixed
Name: "ia_phi3";       Description: "Modelo Local phi3:mini (~2.2 GB) [Opcional - Requer Ollama no PC]"; Types: full custom; Flags: unchecked
Name: "ia_llama32";    Description: "Modelo Local llama3.2:3b (~2.0 GB) [Opcional - Requer Ollama no PC]"; Types: full custom; Flags: unchecked
Name: "ia_llama3";     Description: "Modelo Local llama3:8b (~4.7 GB) [Opcional - Requer Ollama no PC]"; Types: full custom; Flags: unchecked

[Tasks]
Name: "desktopicon"; Description: "Criar atalho na Área de Trabalho"; GroupDescription: "Atalhos Adicionais:"
Name: "associatepdf"; Description: "Associar o Vellum como leitor padrão para arquivos PDF (.pdf)"; GroupDescription: "Associações de Arquivo:"; Flags: unchecked

[Files]
; Binário principal e DLLs coletadas pelo windeployqt
Source: "release_windows\Vellum.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "release_windows\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "Vellum.exe"
Source: "download_models.bat"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"
Name: "{group}\Desinstalar Vellum"; Filename: "{uninstallexe}"; IconFilename: "{app}\app_icon.ico"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\app_icon.ico"; Tasks: desktopicon

[Registry]
; Associação com arquivos PDF (opcional via checkbox na instalação)
Root: HKA; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "Vellum.PDF"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associatepdf
Root: HKA; Subkey: "Software\Classes\Vellum.PDF"; ValueType: string; ValueName: ""; ValueData: "Documento PDF"; Flags: uninsdeletekey; Tasks: associatepdf
Root: HKA; Subkey: "Software\Classes\Vellum.PDF\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: associatepdf
Root: HKA; Subkey: "Software\Classes\Vellum.PDF\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: associatepdf

[UninstallDelete]
; Garante remoção completa de quaisquer arquivos residuais gerados durante o uso
Type: filesandordirs; Name: "{userappdata}\Vellum"
Type: filesandordirs; Name: "{app}\backend"
Type: filesandordirs; Name: "{app}"

[Run]
; Instalação condicional dos modelos Ollama selecionados pelo usuário (somente efetua pull se Ollama existir no PC)
Filename: "{app}\download_models.bat"; Parameters: "/phi3"; Components: ia_phi3; StatusMsg: "Verificando Ollama e configurando modelo phi3:mini..."; Flags: runhidden
Filename: "{app}\download_models.bat"; Parameters: "/llama32"; Components: ia_llama32; StatusMsg: "Verificando Ollama e configurando modelo llama3.2:3b..."; Flags: runhidden
Filename: "{app}\download_models.bat"; Parameters: "/llama3"; Components: ia_llama3; StatusMsg: "Verificando Ollama e configurando modelo llama3:8b..."; Flags: runhidden

; Iniciar o aplicativo ao finalizar
Filename: "{app}\{#MyAppExeName}"; Description: "Iniciar o Vellum agora"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ConfigDir: String;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    // Pergunta educadamente se deseja remover configurações salvas no registro
    if MsgBox('Deseja também remover as preferências e dados do Vellum salvos no seu computador para uma limpeza 100% completa?', mbConfirmation, MB_YESNO) = IDYES then
    begin
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\Vellum');
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\EnsinadorDeIngles');
      ConfigDir := ExpandConstant('{userappdata}\Vellum');
      if DirExists(ConfigDir) then
        DelTree(ConfigDir, True, True, True);
    end;
  end;
end;
