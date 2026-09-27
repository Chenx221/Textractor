// French translations

NATIVE_LANGUAGE = "French";
ATTACH = u8"Attacher le jeu";
LAUNCH = u8"Lancer le jeu";
CONFIG = u8"Configure le jeu";
DETACH = u8"Detacher du jeu";
FORGET = u8"Oublier le jeu";
ADD_HOOK = u8"Ajouter un hook";
REMOVE_HOOKS = u8"Enlever un hook(s)";
SAVE_HOOKS = u8"Sauvegarder un hook(s)";
SEARCH_FOR_HOOKS = u8"Rechercher des hooks";
SETTINGS = u8"Paramètres";
EXTENSIONS = u8"Extensions";
SELECT_PROCESS = u8"Selectionner le processus";
ATTACH_INFO = u8R"(Si vous ne voyez pas le processus que vous souhaitez joindre, essayez de l'exécuter avec les droits d'administrateur
Vous pouvez également saisir l'ID de processus)";
SELECT_PROCESS_INFO = u8"Si vous saisissez manuellement le nom du fichier de processus, veuillez utiliser le chemin exact";
FROM_COMPUTER = u8"Sélectionner depuis l'ordinateur";
PROCESSES = u8"Processus (*.exe)";
CODE_INFODUMP = u8R"(Entrez le read code
R{S|Q|V|M}[null_length<][codepage#]@addr
OU
Entrez le hook code
H{A|B|W|H|S|Q|V|M}[F][null_length<][N][codepage#][padding+]data_offset[*deref_offset][:split_offset[*deref_offset]]@addr[:module[:func]]
Tous les nombres sauf codepage/null_length sont en hexadécimal
Le codepage par défaut est 932 (Shift-JIS) mais cela peut être modifié dans les paramètres
A/B: codepage char little/big endian
W: UTF-16 char
H: Two hex bytes
S/Q/V/M: codepage/UTF-16/UTF-8/hex string
F: treat strings as full lines of text
N: n'utilise pas de contexte
null_length: length of null terminator used for string
padding: length of padding data before string (C struct { int64_t size; char string[500]; } needs padding = 8)
Les valeures négatives pour data_offset/split_offset font références aux registres
-4 pour EAX, -8 pour ECX, -C pour EDX, -10 pour EBX, -14 pour ESP, -18 pour EBP, -1C pour ESI, -20 pour EDI
-C pour RAX, -14 pour RBX, -1C pour RCX, -24 pour RDX, and so on for RSP, RBP, RSI, RDI, R8-R15
* means dereference pointer+deref_offset)";
SAVE_SETTINGS = u8"Sauvergarder les paramètres";
EXTEN_WINDOW_INSTRUCTIONS = u8R"(Pour ajouter une extension, cliquez avec le bouton droit sur la liste des extensions
Vous pouvez également faire glisser et déposer le fichier d'extension depuis votre ordinateur
Pour réorganiser les extensions, faites-les glisser et déposez-les dans la liste
(Les extensions sont utilisées de haut en bas: l'ordre est IMPORTANT)
Pour supprimer une extension, sélectionnez-la et appuyez sur supprimer)";
ADD_EXTENSION = u8"Ajouter une extension";
INVALID_EXTENSION = u8"%1 C'est une extension invalide";
CONFIRM_EXTENSION_OVERWRITE = u8"Une autre version de cette extension existe déjà. Voulez-vous la supprimer et la remplacer?";
EXTENSION_WRITE_ERROR = u8"Impossible d'enregistrer l'extension";
USE_JP_LOCALE = u8"Émuler les paramètres régionaux japonais?";
FAILED_TO_CREATE_CONFIG_FILE = u8"Impossible de créer le fichier de configuration \"%1\"";
HOOK_SEARCH_UNSTABLE_WARNING = u8"La recherche de crochets est instable! Soyez prêt à ce que votre jeu plante!";
SEARCH_CJK = u8"Rechercher pour Chinois/Japonais/Coréen";
SEARCH_PATTERN = u8"Modèle de recherche (tableau d'octets hexadécimaux)";
SEARCH_DURATION = u8"Durée de la recherche (ms)";
SEARCH_MODULE = u8"Recherche sans module";
PATTERN_OFFSET = u8"Décalage par rapport au début du modèle";
MAX_HOOK_SEARCH_RECORDS = u8"Limite du résultat de la recherche";
MIN_ADDRESS = u8"Minimum d'adresses (hex)";
MAX_ADDRESS = u8"Maximum d'adresses (hex)";
STRING_OFFSET = u8"Décalage de la chaîne (hex)";
HOOK_SEARCH_FILTER = u8"Les résultats doivent correspondre à ce regex";
TEXT = u8"Texte";
CODEPAGE = u8"Code de page";
SEARCH_FOR_TEXT = u8"Rechercher un texte spécifique";
START_HOOK_SEARCH = u8"Lancer la recherche de hook";
SAVE_SEARCH_RESULTS = u8"Sauvergarder les résultats de la recherche";
TEXT_FILES = u8"Texte (*.txt)";
DOUBLE_CLICK_TO_REMOVE_HOOK = u8"Double cliquer sur un hook pour l'enlever";
FILTER_REPETITION = u8"Répétition de filtre";
AUTO_ATTACH = u8"Attachement Automatique";
ATTACH_SAVED_ONLY = u8"Attachement Automatique (Sauvergardé seulement)";
SHOW_SYSTEM_PROCESSES = u8"Montrer les processus système";
DEFAULT_CODEPAGE = u8"Page de code de base";
FLUSH_DELAY = u8"Retard de vidage";
MAX_BUFFER_SIZE = u8"Taille maximale du tampon";
MAX_HISTORY_SIZE = u8"Taille d'historique maximale";
CONFIG_JP_LOCALE = u8"Lancement avec les paramètres régionaux JP";
CONSOLE = L"Console";
CLIPBOARD = L"Presse-papier";
ABOUT = L"Textractor " ARCH L" v" VERSION LR"( Fait par moi: Artikash (email: akashmozumdar@gmail.com)
Page d'acceuil du projet: https://github.com/Artikash/Textractor
Vidéo tuto: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
FAQ: https://github.com/Artikash/Textractor/wiki/FAQ
Veuillez me contacter pour tout problème, demande de fonctionnalité ou question concernant Textractor
Vous pouvez le faire via la page d'accueil du projet (section problèmes) ou par e-mail
Code source disponible sous GPLv3 sur la page d'accueil du projet
Si vous aimez ce projet, parlez-en à tout le monde :))";
CL_OPTIONS = LR"(usage: Textractor [-p{process ID|"process name"}]...
example: Textractor -p4466 -p"My Game.exe" tries to inject processes with ID 4466 or with name My Game.exe)";
UPDATE_AVAILABLE = L"Une nouvelle version de Textractor est disponible !\n\nDernière : %s\nLa vôtre : %s\n\nCliquez sur OK pour ouvrir la page de téléchargement.";
ALREADY_INJECTED = L"Textractor: déjà injecté";
NEED_32_BIT = L"Textractor: incompatibilité d'architecture: seul Textractor x86 peut injecter ce processus";
NEED_64_BIT = L"Textractor: incompatibilité d'architecture: seul Textractor x64 peut injecter ce processus";
INJECT_FAILED = L"Textractor: ne peut pas injecter";
LAUNCH_FAILED = L"Textractor: ne peut pas lancer";
INVALID_CODE = L"Textractor: code invalide";
INVALID_CODEPAGE = L"Textractor: impossible de convertir le texte (page de code non valide?)";
PIPE_CONNECTED = u8"Textractor: tuyau connecté";
INSERTING_HOOK = u8"Textractor: insertion du hook: %s";
REMOVING_HOOK = u8"Textractor: enlève le hook: %s";
HOOK_FAILED = u8"Textractor: n'a pas réussi à insérer un hook";
TOO_MANY_HOOKS = u8"Textractor: trop de hooks: impossible d'insérer";
HOOK_SEARCH_STARTING = u8"Textractor: démarrage de la recherche";
NOT_ENOUGH_TEXT = u8"Textractor: pas assez de texte pour effectuer une recherche précise";
HOOK_SEARCH_INITIALIZED = u8"Textractor: la recherche a été initialisé avec %zd hooks";
MAKE_GAME_PROCESS_TEXT = u8"Textractor: veuillez cliquer dans le jeu pour le forcer à traiter le texte lors de la prochaine %d seconds";
HOOK_SEARCH_FINISHED = u8"Textractor: la recherche du hook est finie, %d results found";
OUT_OF_RECORDS_RETRY = u8"Textractor: hors des enregistrements de recherche, veuillez réessayer si les résultats sont médiocres (le nombre d'enregistrements par défaut a augmenté)";
FUNC_MISSING = u8"Textractor: function non présente";
MODULE_MISSING = u8"Textractor: module non présente";
GARBAGE_MEMORY = u8"Textractor: mémoire en constante évolution, inutile à lire";
SEND_ERROR = u8"Textractor: envoyer ERREUR (comme un H-code instable/incorrect)";
READ_ERROR = u8"Textractor: Lire ERREUR (comme un R-code incorrect)";
HIJACK_ERROR = u8"Textractor: ERREUR Hijack";
COULD_NOT_FIND = u8"Textractor: ne peut pas trouver le texte";
TRANSLATE_TO = u8"Traduire à";
TRANSLATE_SELECTED_THREAD_ONLY = u8"Traduire uniquement le thread sélectionné";
RATE_LIMIT_ALL_THREADS = u8"Taux limite tout les threads de texte";
RATE_LIMIT_SELECTED_THREAD = u8"Limite de débit du thread de texte sélectionné";
USE_TRANS_CACHE = u8"Utiliser le cache de traduction";
MAX_TRANSLATIONS_IN_TIMESPAN = u8"Nombre de tokens du limiteur de débit";
TIMESPAN = u8"Délai de restauration du token du limiteur de débit (ms)";
TOO_MANY_TRANS_REQUESTS = L"Limite de taux dépassée: refus de faire plus de demande de traduction";
TRANSLATION_ERROR = L"Une erreur est survenue pendant la traduction";
USE_PREV_SENTENCE_CONTEXT = u8"Utiliser la phrase précédente comme contexte";
API_KEY = u8"API key";
EXTRA_WINDOW_INFO = u8R"(Clic droit pour modifier les paramètres
Cliquez et faites glisser sur les bords de la fenêtre pour vous déplacer ou dans le coin inférieur droit pour redimensionner)";
MAX_SENTENCE_SIZE = u8"Taille maximale de la phrase";
TOPMOST = u8"Toujours au dessus";
DICTIONARY = u8"Dictionnaire";
DICTIONARY_INSTRUCTIONS = u8R"(Ce fichier est utilisé uniquement pour la fonction "Dictionnaire" de l'extension Extra Window.
Il utilise un format personnalisé spécifique à Textractor et n'est pas destiné à être écrit manuellement.
Vous devriez rechercher un dictionnaire dans ce format en ligne (https://github.com/Artikash/Textractor-Dictionaries/releases est un bon point de dépar).
Alternativement, si vous êtes programmeur, vous pouvez écrire un script pour convertir un dictionnaire d'un autre format avec les informations ci-dessous.
Une fois que vous avez un dictionnaire, pour rechercher du texte dans Extra Window, survolez-le. Vous pouvez faire défiler toutes les définitions correspondantes.
Les definitions sont formattés comme ceci:|TERM|Hola<<ignored|TERM|hola|TERM|Bonjour|TERM|bonjour|DEFINITION|hello|END|
Le terme et la définition peuvent inclure du texte enrichi (https://doc.qt.io/qt-5/richtext-html-subset.html) qui sera correctement formaté.
Les inflexions sont formatées comme ceci:|ROOT|1<<noun|INFLECTS TO|(\w*)s|NAME| plural|END|
Textractor vérifiera si un terme correspond à l'expression rationnelle d'inflexion et si c'est le cas, recherchera récursivement le terme racine.
Le terme racine est généré en remplaçant chaque nombre par le groupe de capture regex correspondant (0 étant remplacé par la correspondance entière).
Ce processus peut facilement entraîner des boucles infinies et / ou des débordements de pile. C'est votre travail d'éviter cela.
L'expression regex d'inflexion utilise la syntaxe unicode QRegularExpression (https://doc.qt.io/qt-5/qregularexpression.html).
Textractor affichera le terme racine final ainsi que toutes les inflexions utilisées pour arriver à ce terme racine.
Cependant, le texte d'un terme après << est ignoré lors de l'affichage. Ceci est destiné à stocker des informations sur une partie de la parole.
Ce fichier doit être encodé en UTF-8.)";
SHOW_ORIGINAL = u8"Texte Original";
SHOW_ORIGINAL_INFO = u8R"(Le texte d'origine ne sera pas affiché
Fonctionne uniquement si cette extension est utilisée directement après une extension de traduction)";
SIZE_LOCK = u8"Verouiller la taille";
OPACITY = u8"Opacité";
BG_COLOR = u8"Couleur d'arrière-plan";
TEXT_COLOR = u8"Couleur du texte";
TEXT_OUTLINE = u8"Contour du texte";
OUTLINE_COLOR = u8"Couleur du contour";
OUTLINE_SIZE = u8"Taille du contour";
OUTLINE_SIZE_INFO = u8"Taille en pixels (recommandé de rester en dessous de 20% de la taille de la police)";
FONT = u8"Police";
LUA_INTRO = u8R"(--[[
ProcessSentence est appelée chaque fois que Textractor reçoit une phrase de texte.
Phrase param: phrase reçue par Textractor (UTF-8).
Param sentenceInfo: tableau d'informations diverses sur la phrase.
Si vous retournez une chaîne, la phrase sera transformée en cette chaîne.
Si vous renvoyez zéro, la phrase ne sera pas modifiée.
Cette extension utilise plusieurs copies de l'interpréteur Lua pour la sécurité des threads.
Les modifications apportées aux variables globales à partir de ProcessSentence ne sont pas garanties de persister.
Properties in sentenceInfo:
"current select": 0 unless sentence is in the text thread currently selected by the user.
"process id": process ID that the sentence is coming from. 0 for console and clipboard.
"text number": number of the current text thread. Counts up one by one as text threads are created. 0 for console, 1 for clipboard.
--]]
function ProcessSentence(sentence, sentenceInfo)
  --Your code here...
end)";
LOAD_SCRIPT = u8"Charger le script";
LUA_ERROR = L"Erreur Lua: %s";
REGEX_FILTER = u8"Filtre regex";
INVALID_REGEX = u8"Regex invalide";
CURRENT_FILTER = u8"En train de filtrer: %1";
REPLACER_INSTRUCTIONS = LR"(Ce fichier ne fait rien lorsque l'extension "Replacer" est utilisée.
Les commandes de remplacement doivent être formatées comme:
|ORIG|original_text|BECOMES|replacement_text|END|
Tout le texte de ce fichier en dehors d'une commande de remplacement est ignoré.
Un caret (^) agit comme un caractère générique qui correspond à tout autre caractère unique.
Les espaces dans original_text sont ignorés, mais remplacement_text peut contenir des espaces, des nouvelles lignes, etc.
Ce fichier doit être encodé en Unicode (UTF-16 Little Endian).)";
THREAD_LINKER = u8"Lien du thread";
LINK = u8"Lien";
THREAD_LINK_FROM = u8"Nombre du thread du lien depuis";
THREAD_LINK_TO = u8"Nombre du thread du lien a";
HEXADECIMAL = u8"Hexadécimal";
