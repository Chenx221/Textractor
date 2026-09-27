// Portuguese translations

NATIVE_LANGUAGE = "Portuguese";
ATTACH = u8"Anexar ao Jogo";
LAUNCH = u8"Iniciar Jogo";
DETACH = u8"Desconectar do Jogo";
ADD_HOOK = u8"Adicionar um Hook";
SAVE_HOOKS = u8"Salvar Hook(s)";
SETTINGS = u8"Opções";
EXTENSIONS = u8"Extensões";
SELECT_PROCESS = u8"Selecionar Processo";
ATTACH_INFO = u8R"(Se você não encontrar o processo o qual deseja anexar a, tente iniciar com permissão de Administrador. Você também pode digitar a ID do processo)";
FROM_COMPUTER = u8"Busque no Computador";
PROCESSES = u8"Executaveis (*.exe)";
SAVE_SETTINGS = u8"Salvar opções";
EXTEN_WINDOW_INSTRUCTIONS = u8R"(Arraste e solte a extensão (.dll) aqui para adicioná-la.
(Não funciona se estiver rodando como Administrador)
Arraste e solte dentro da lista para reordená-la.
(As extensões são utilizadas de cima para baixo: a ORDEM IMPORTA.)
Pressione delete com uma extensão selecionada para removê-la.)";
INVALID_EXTENSION = u8"%1 não é uma extensão válida.";
CONFIRM_EXTENSION_OVERWRITE = u8"Outra versão dessa extensão já existe, você gostaria de deletar e reescrevê-la?";
EXTENSION_WRITE_ERROR = u8"Falha na gravação da extensão";
USE_JP_LOCALE = u8"Emular o idioma Japonês?";
SHOW_SYSTEM_PROCESSES = u8"Mostrar Processos do sistema";
DEFAULT_CODEPAGE = u8"página de código padrão";
FLUSH_DELAY = u8"Delay do Flush";
MAX_BUFFER_SIZE = u8"Tamanho Máximo do Buffer";
MAX_HISTORY_SIZE = u8"Tamanho Máximo do Histórico";
CONSOLE = L"Terminal";
CLIPBOARD = L"Área de Transferência";
ABOUT = L"Textractor " ARCH L" v" VERSION LR"( Feito por mim: Artikash (e-mail: akashmozumdar@gmail.com)
Homepage do Projeto: https://github.com/Artikash/Textractor
Vídeo Tutorial: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
Por favor, em caso de problemas, requisição de recurso e/ou funções e de dúvidas, entrar em contato comigo. Use o Inglês para se comunicar.
Você pode fazê-lo por meio da Homepage do Projeto (na aba "Issues") ou via E-mail.
O código-fonte se encontra disponível na Homepage do projeto sob a licença GPLv3.
Se você gostou desse projeto, divulgue a todos :))";
UPDATE_AVAILABLE = L"Uma nova versão do Textractor está disponível!\n\nMais recente: %s\nA sua: %s\n\nClique em OK para abrir a página de download.";
ALREADY_INJECTED = L"Textractor: já está injetado";
NEED_32_BIT = L"Textractor: arquitetura errada: apenas o Textractor x86 pode injetar neste processo";
NEED_64_BIT = L"Textractor: arquitetura errada: apenas o Textractor x64 pode injetar neste processo";
INJECT_FAILED = L"Textractor: não pode injetar";
LAUNCH_FAILED = L"Textractor: não pode iniciar";
INVALID_CODE = L"Textractor: código inválido";
INVALID_CODEPAGE = L"Textractor: não pode converter o texto (página de código inválida?)";
PIPE_CONNECTED = u8"Textractor: pipe conectado";
INSERTING_HOOK = u8"Textractor: inserindo hook: %s";
REMOVING_HOOK = u8"Textractor: removendo hook: %s";
HOOK_FAILED = u8"Textractor: falha na inserção do hook";
TOO_MANY_HOOKS = u8"Textractor: há hooks de mais: não é possível inserir mais";
HOOK_SEARCH_STARTING = u8"Textractor: iniciando busca ";
NOT_ENOUGH_TEXT = u8"Textractor: não há texto suficiente para uma buscar precisa";
HOOK_SEARCH_INITIALIZED = u8"Textractor: busca inicializada com %zd hooks";
HOOK_SEARCH_FINISHED = u8"Textractor: busca por hooks finalizada, %d resultados encontrados";
FUNC_MISSING = u8"Textractor: função não encontrada";
MODULE_MISSING = u8"Textractor: módulo não presente";
SEND_ERROR = u8"Textractor: ERRO no envio (provavelmente um H-code incorreto)";
READ_ERROR = u8"Textractor:  ERRO na leitura (provavelmente um R-code incorreto)";
COULD_NOT_FIND = u8"Textractor: não foi possível encontrar texto";
TOO_MANY_TRANS_REQUESTS = L"Foram feitos pedidos de tradução demais: recusa na feitura de mais pedidos";
TRANSLATION_ERROR = L"Erro enquanto traduzindo";
EXTRA_WINDOW_INFO = u8R"(Clique com o botão direito para mudar as opções
Clique e arraste nas beiradas da janela para mover, ou no canto inferior direito para redimessionar)";
TOPMOST = u8"Sempre em cima";
SHOW_ORIGINAL = u8"Texto original";
SHOW_ORIGINAL_INFO = u8R"(Texto original não será mostrado
Apenas funciona se essa extensão for usada diretamente após uma extensão de tradução)";
SIZE_LOCK = u8"Travar o Tamanho";
BG_COLOR = u8"Cor de fundo";
TEXT_COLOR = u8"Cor do Texto";
FONT = u8"Fonte";
REGEX_FILTER = u8"Fíltro regex";
INVALID_REGEX = u8"Regex inválido";
CURRENT_FILTER = u8"Atualmente filtrando: %1";
REPLACER_INSTRUCTIONS = LR"(Este arquivo apenas faz algo quando a extensão "Replacer" está sendo utilizada.
Comandos de substituição devem ser formatos da seguinte maneira:
|ORIG|texto_original|BECOMES|texto_substituido|END|
Todo texto fora de um comando de substituição é ignorado.
Espaços contidos no texto original serão ignorados, mas o texto substituído pode conter espaços, novas línhas, etc.
Esse arquívo deve ser codifícado em (UTF-16 little endian).)";
THREAD_LINKER = u8"Ligador de Threads";
LINK = u8"Ligar";
THREAD_LINK_FROM = u8"Número do thread para ligar de";
THREAD_LINK_TO = u8"Número do thread para ligar para";
