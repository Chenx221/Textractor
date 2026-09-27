// Italian translations

NATIVE_LANGUAGE = "Italian";
ATTACH = u8"Collega al gioco";
LAUNCH = u8"Avvia gioco";
CONFIG = u8"Configura gioco";
DETACH = u8"Scollega dal gioco";
FORGET = u8"Dimentica gioco";
ADD_HOOK = u8"Aggiungi gancio";
REMOVE_HOOKS = u8"Rimuovi gancio/ci";
SAVE_HOOKS = u8"Salva gancio/ci";
SEARCH_FOR_HOOKS = u8"Ricerca ganci";
SETTINGS = u8"Impostazioni";
EXTENSIONS = u8"Estenzioni";
SELECT_PROCESS = u8"Seleziona processo";
ATTACH_INFO = u8R"(Se non vedi il processo che desideri allefare, prova ad eseguire come amministratore
Puoi anche digitare l'ID del processo)";
SELECT_PROCESS_INFO = u8"Se digiti manualmente il nome file del processo, si prega di utilizzare il percorso assoluto";
FROM_COMPUTER = u8"Seleziona dal computer";
PROCESSES = u8"Processi (*.exe)";
CODE_INFODUMP = u8R"(Digita il codice di lettura
R{S|Q|V|M}[null_length<][codepage#]@addr
OR
Digita il codice gancio
H{A|B|W|H|S|Q|V|M}[F][null_length<][N][codepage#][padding+]data_offset[*deref_offset][:split_offset[*deref_offset]]@addr[:module[:func]]
Tutti i numeri eccetto codepage/null_lengt nell'esadecimale
Codepage di base è 932 (Shift-JIS) ma può essere cambiato nelle impostazioni
A/B: carattere del codepage little/big endian
W: carattere UTF-16
H: Due byte esadecimali
S/Q/V/M: stringa codepage/UTF-16/UTF-8/hex
F: tratta le stringhe come linee di testo complete
N: non usa il contesto
null_lengt: lunghezza del terminatore null utilizzato per la stringa
padding: lunghezza del data padding prima della stringa (C struct { int64_t size; char string[500]; } needs padding = 8)
Negativi per data_offset/split_offset consulta i registri
-4 for EAX, -8 for ECX, -C for EDX, -10 for EBX, -14 for ESP, -18 for EBP, -1C for ESI, -20 for EDI
-C for RAX, -14 for RBX, -1C for RCX, -24 for RDX, and so on for RSP, RBP, RSI, RDI, R8-R15
* significa puntatore di deferenziazione+deref_offset)";
SAVE_SETTINGS = u8"Salva impostazioni";
EXTEN_WINDOW_INSTRUCTIONS = u8R"(Per aggiugnere un estenzione, clicca con il tasto destro la lista estenzioni
Alternativamente, trascina e rilascia il file estenzione dal tuo computer
Per riordinare le estenzioni, trascinali dentro la lista
(le estenzioni sono usate dall'alto verso il basso: l'ordine è IMPORTANTE)
Per rimuovere un estenzione, selezionala e premi rimuovi)";
ADD_EXTENSION = u8"Aggiungi estenzione";
INVALID_EXTENSION = u8"%1 è un estenzione non valida";
CONFIRM_EXTENSION_OVERWRITE = u8"Un'altra versione di questa estenzione esiste già, desidera cancellarla e sovvrascriverla?";
EXTENSION_WRITE_ERROR = u8"Impossibile salvare l'estenzione";
USE_JP_LOCALE = u8"Emulare l'impostazione locale giapponese?";
FAILED_TO_CREATE_CONFIG_FILE = u8"Impossibile creare il file di configurazione \"%1\"";
HOOK_SEARCH_UNSTABLE_WARNING = u8"Cercare i ganci è instabile! Preparati a un crash del tuo gioco!";
SEARCH_CJK = u8"Cerca per Cinese/Giapponese/Coreano";
SEARCH_PATTERN = u8"Cerca schema (matrice byte in esa)";
SEARCH_DURATION = u8"Cerca durata (ms)";
SEARCH_MODULE = u8"Cerca dentro il modulo";
PATTERN_OFFSET = u8"Offset dall'inizio dello schema";
MAX_HOOK_SEARCH_RECORDS = u8"Limite risultati di ricerca";
MIN_ADDRESS = u8"Indirizzo minimo (esa)";
MAX_ADDRESS = u8"Indirizzo massimo (esa)";
STRING_OFFSET = u8"Offset stringa (esa)";
HOOK_SEARCH_FILTER = u8"I risultati devono combaciare con questa espreressione regolare";
TEXT = u8"Testo";
CODEPAGE = u8"Codepage";
SEARCH_FOR_TEXT = u8"Cerca per un testo specifico";
START_HOOK_SEARCH = u8"Avvia ricerca gancio";
SAVE_SEARCH_RESULTS = u8"Salva i risultati ricerca";
TEXT_FILES = u8"Testo (*.txt)";
DOUBLE_CLICK_TO_REMOVE_HOOK = u8"Doppio click su un gancio per rimuoverlo";
FILTER_REPETITION = u8"Filtro ripetizione";
AUTO_ATTACH = u8"Allega automaticamente";
ATTACH_SAVED_ONLY = u8"Allega automaticamente (solamente salvato)";
SHOW_SYSTEM_PROCESSES = u8"Mostra i processi di sistema";
DEFAULT_CODEPAGE = u8"Codepage di base";
FLUSH_DELAY = u8"Ritardo flush";
FLUSH_DELAY_SPACING = u8"Spazia ritardo flush string";
MAX_BUFFER_SIZE = u8"Massima dimensione buffer";
MAX_HISTORY_SIZE = u8"Massima dimensione cronologia";
LIMIT_STRING_LENGTH = u8"Limita lunghezza stringa (0=Off)";
CONFIG_JP_LOCALE = u8"Avvia con il JP locale";
CONSOLE = L"Console";
CLIPBOARD = L"Appunti";
ABOUT = L"Textractor " ARCH L" v" VERSION LR"( creato da me: Artikash (email: akashmozumdar@gmail.com)
Pagina principale del progetto: https://github.com/Artikash/Textractor
Video tutorial: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
Contattatemi per ogni problema, richiesta futura, o domande legate a Textractor
Puoi farlo attraverso la pagina principale del progetto (sezione issues) o via email
Il codice sorgente è disponibile sotto il GPLv3 nella pagina principale
Al momento sono in cerca di un nuovo lavoro: contattatemi per email se conoscete qualcuno che ingaggia periti informatici statunitensi
Se ti piace questo progetto, parlane con tutti per favore :))";
CL_OPTIONS = LR"(utilizzo: Textractor [-p{process ID|"process name"}]...
esempio: Textractor -p4466 -p"My Game.exe" sta tentando di inniettare i processi con l'ID 4466 o con il nome My Game.exe)";
UPDATE_AVAILABLE = L"È disponibile una nuova versione di Textractor!\n\nUltima: %s\nLa tua: %s\n\nFai clic su OK per aprire la pagina di download.";
ALREADY_INJECTED = L"Textractor: già inniettato";
NEED_32_BIT = L"Textractor: incompatibilità di architettura: solo Textractor x86 può inniettare questo processo";
NEED_64_BIT = L"Textractor: incompatibilità di architettura: solo Textractor x64 può inniettare questo processo";
INJECT_FAILED = L"Textractor: impossibile inniettare";
LAUNCH_FAILED = L"Textractor: impossibile avviare";
INVALID_CODE = L"Textractor: codice invalido";
INVALID_CODEPAGE = L"impossibile convertire il testo (codepage invalido?)";
PIPE_CONNECTED = u8"Textractor: tubo connesso";
INSERTING_HOOK = u8"Textractor: aggiungi gancio: %s";
REMOVING_HOOK = u8"Textractor: rimuovi gancio: %s";
HOOK_FAILED = u8"Textractor: inserimento gancio non riuscito";
TOO_MANY_HOOKS = u8"Textractor: troppi ganci: impossibile inserirli";
HOOK_SEARCH_STARTING = u8"Textractor: avvia la ricerca";
NOT_ENOUGH_TEXT = u8"Textractor: testo insufficente per la ricerca accurata";
HOOK_SEARCH_INITIALIZED = u8"Textractor: ricerca inizializzata con %zd ganci";
MAKE_GAME_PROCESS_TEXT = u8"Textractor: clicca intorno al gioco per forzarlo nel testo del processo durante i prossimi %d secondi";
HOOK_SEARCH_FINISHED = u8"ricerca ganci conclusa, %d risultati trovati";
OUT_OF_RECORDS_RETRY = u8"registri di ricerca esauriti, riprova se i risultati sono scarsi (conto registri di base aumentato)";
FUNC_MISSING = u8"Textractor: funzione non presente";
MODULE_MISSING = u8"Textractor: modulo non presente";
GARBAGE_MEMORY = u8"Textractor: memoria è in costante cambiamento, inutila la lettura";
SEND_ERROR = u8"Textractor: Send ERROR (probabilmente un H-code incorretto)";
READ_ERROR = u8"Textractor: Reader ERROR (probabilmente un R-code incorretto)";
HIJACK_ERROR = u8"Textractor: ERRORE di Hijack";
COULD_NOT_FIND = u8"Textractor: impossibile trovare il testo";
TRANSLATE_TO = u8"Traduci a";
TRANSLATE_SELECTED_THREAD_ONLY = u8"Traduci solo il thread del testo selezionato";
RATE_LIMIT_ALL_THREADS = u8"Rate limit tutti i thread del testo";
RATE_LIMIT_SELECTED_THREAD = u8"Rate limit thread del testo selezionato";
USE_TRANS_CACHE = u8"Utilizza la cache di traduzione";
MAX_TRANSLATIONS_IN_TIMESPAN = u8"Numero di token del Rate Limit";
TIMESPAN = u8"Token del rate limit ripristina il ritardo (ms)";
TOO_MANY_TRANS_REQUESTS = L"Rate limit superato: rifiuta per fare altre richieste di traduzione";
TRANSLATION_ERROR = L"Errore durante la traduzione";
USE_PREV_SENTENCE_CONTEXT = u8"Utilizza la precedente sentenza come contesto";
API_KEY = u8"Chiave API";
EXTRA_WINDOW_INFO = u8R"(Tasto destro per cambiare le impostazioni
Clicca e trascina i bordi della finestra per muoverla, oppure nell'angolo in basso a destra per ridimensionare)";
MAX_SENTENCE_SIZE = u8"Dimensione massima sentenza";
TOPMOST = u8"Sempre in primo piano";
DICTIONARY = u8"Dizionario";
DICTIONARY_INSTRUCTIONS = u8R"(Questo file è utilizzato solo per la funzione "Dizionario" dell'estenzione Extra Window.
Utilizza un formato personalizzato spedifico per Textractor e non è pensato per essere critto manualmente.
Dovresti cercare per un dizionario in questo formato online (https://github.com/Artikash/Textractor-Dictionaries/releases è un buon posto per cominciare).
In alternativa, se sei un progammatore, puoi scrivere uno script per convertire un dizionario da un'altro formato con le informazioni sottostanti.
Una volta che hai il dizionario, cerca qualche testo in Extra Window, posizionaci sopra. Puoi scorrere fra tutte le definizioni corrispondenti.
Le definizioni sono formattate cosi: |TERM|Hola<<ignored|TERM|hola|TERM|Bonjour|TERM|bonjour|DEFINITION|hello|END|

Il termine e la definizione può includere rich text (https://doc.qt.io/qt-5/richtext-html-subset.html) che sarà formattato a dovere.
Le inflessioni sono formattate cosi: |ROOT|1<<noun|INFLECTS TO|(\w*)s|NAME| plural|END|
Textractor controllerà se un termine corrisponde il regex dell'inflessione e se cosi fosse, cercherà ricorsivamente per la radice.
La radice è generata rimpiazziando ogni numero con il regex del gruppo di cattura corrispondente (con 0 sostituito dall'intera corrispondenza).
Questo processo può risultare facilmente in cicli infiniti e/o in stack overflow. È il tuo compito impedirlo.
Il regex dell'inflessione utilizza la sintassi unicode QRegularExpression (https://doc.qt.io/qt-5/qregularexpression.html).
Textractor visualizzerà la radice finale cosi come tutte le inflessioni utilizzate per ottenere quella radice.
Tuttavia, il testo in un termine dopo << è ignorato quando è visualizzato. Questo è pensato per conservare l'informazione sulle parti del discorso.
Questo file deve essere codificato in UTF-8.)";
SHOW_ORIGINAL = u8"Testo originale";
SHOW_ORIGINAL_INFO = u8R"(Testo originale non sarà mostrato
Funziona solo se questa estenzione è usata direttamente dopo un'estensione di traduzione)";
ORIGINAL_AFTER_TRANSLATION = u8"Mostra testo originale dopo traduzione";
SIZE_LOCK = u8"Lock delle dimensione";
POSITION_LOCK = u8"Lock delle posizione";
CENTERED_TEXT = u8"Testo centrato";
AUTO_RESIZE_WINDOW_HEIGHT = u8"Auto resize altezza finestra";
CLICK_THROUGH = u8"Clicca attraverso\tAlt+X";
HIDE_MOUSEOVER = u8"Nascondi testo mouseover";
HIDE_TEXT = u8"Nascondi/Mostra testo\tAlt+T";
OPACITY = u8"Opacità";
BG_COLOR = u8"Colore dello sfondo";
TEXT_COLOR = u8"Colore del testo";
TEXT_OUTLINE = u8"Contorno del testo";
OUTLINE_COLOR = u8"Colore del contorno";
OUTLINE_SIZE = u8"Dimensione del contorno";
OUTLINE_SIZE_INFO = u8"Dimensione in pixel (consigliato di rimanere sotto il 20% della dimensione del font)";
FONT = u8"Font";
TIMER_HIDE_TEXT = u8"Timer nascondi testo";
TEXT_TIMEOUT = u8"Timeout (msec, 0=disattivato)";
TEXT_TIMEOUT_ADD_PER_CHAR = u8"Timeout aggiuntivo per carattere (msec)";
LUA_INTRO = u8R"(--[[
ProcessSentence è chiamato ogni volta che Textractor riceva una sentenza di testo.

Param sentence: sentenza ricevuta da Textractor (UTF-8).
Param sentenceInfo: tavola di informazioni miste sulla sentenza.

Se fai il return di una stringa, la sentenza di trasformera in quella stringa.
Se fai il return di nulla, la sentenza non sarà modificata.

Questa estenzione utilizza diverse copie dell'interprete Lua per la sicurezza del thread.
Modifiche alle variabili globali da ProcessSentence non sono garantite di persistere.

Proprietà in sentenceInfo:
"current select": 0 a meno che la sentenza è nel thread di testo attualmente scelto dall'utente.
"process id": ID del processo che da cui proviene la sentenza. 0 per console e per appunti.
"text number": numero dell'attuale thread di testo. Conta uno ad uno quando i thread di testo sono creati. 0 per console, 1 per appunti.
--]]
function ProcessSentence(sentence, sentenceInfo)
  --Tuo codice qui...
end)";
LOAD_SCRIPT = u8"Carica script";
LUA_ERROR = L"Errore Lua: %s";
REGEX_FILTER = u8"Filtro regex";
INVALID_REGEX = u8"Regex non valido";
CURRENT_FILTER = u8"Filtraggio attuale: %1";
REPLACER_INSTRUCTIONS = LR"(Questo file fa qualcosa solo quando l'estenzione "Replacer" è utilizzata.
I comandi di rimpiazzo devono essere formattati cosi:
|ORIG|testo_originale|BECOMES|testo_sostituito|END|
Tutto il testo in questo file all'infuori di un comando di rimpiazzo è ignorato.
La spaziatura nel testo_originale è ignorato, ma testo_sostituito può contenere spaziature, ritorni a capo, ecc.
Questo file deve essere codificato in Unicode (UTF-16 Little Endian).)";
REGEX_REPLACER_INSTRUCTIONS = LR"(Questo file fa qualcosa solo quando l'estenzione "Regex Replacer" è utilizzata.
I comandi di sostituzione devono essere formattati cosi:
|REGEX|espressione_regolare|BECOMES|testo_sostituito|MODIFIER|modificatori|END|
Il parametro "MODIFIER" può contenere i seguenti modificatori:
"g" la sostituzione è globale.
"i" la sostituzione ignora maiuscole/minuscole.
Se il modificatore è vuoto, la sostituzione viene applicata alla sola prima corrispondenza
e fa distinzione tra maiuscole e minuscole.
Tutto il testo in questo file all'infuori di un comando di sostituzione è ignorato.
Questo file deve essere codificato in Unicode (UTF-16 Little Endian).
Apprendere, creare e testare Espressioni Regolari: https://regexr.com/)";
THREAD_LINKER = u8"Collegatore di thread";
LINK = u8"Collegamento";
THREAD_LINK_FROM = u8"Numero di thread da cui collegarsi";
THREAD_LINK_TO = u8"Numero di thread a cui collegarsi";
HEXADECIMAL = u8"Esadecimale";
