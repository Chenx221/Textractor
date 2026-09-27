// Russian translations

NATIVE_LANGUAGE = "Russian";
ATTACH = u8"Присоединить к игре";
LAUNCH = u8"Запустить игру";
CONFIG = u8"Настройки игры";
DETACH = u8"Отсоединить от игры";
FORGET = u8"Забыть игру";
ADD_HOOK = u8"Добавить хук";
REMOVE_HOOKS = u8"Удалить хук(и)";
SAVE_HOOKS = u8"Сохранить хук(и)";
SEARCH_FOR_HOOKS = u8"Найти хуки";
SETTINGS = u8"Настройки";
EXTENSIONS = u8"Расширения";
SELECT_PROCESS = u8"Выберете процесс";
ATTACH_INFO = u8R"(Если вы не видите процесс, к которому хотите присоединить, попробуйте запуск с правами администратора
Вы также можете ввести ID процесса)";
SELECT_PROCESS_INFO = u8"При ручном вводе имени файла процесса используйте абсолютный путь";
FROM_COMPUTER = u8"Найти в проводнике";
PROCESSES = u8"Процессы (*.exe)";
CODE_INFODUMP = u8R"(Введите код чтения
R{S|Q|V|M}[null_length<][codepage#]@addr
ИЛИ
Введите хук-код
H{A|B|W|H|S|Q|V|M}[F][null_length<][N][codepage#][padding+]data_offset[*deref_offset][:split_offset[*deref_offset]]@addr[:module[:func]]
Все цифры кроме codepage/null_length в hexadecimal
Кодировка по умолчанию - 932 (Shift-JIS), но может быть изменена в настройках
A/B: символ кодировки little/big endian
W: символ UTF-16
H: Два hex байта
S/Q/V/M: codepage/UTF-16/UTF-8/hex string
F: treat strings as full lines of text
N: не использовать контекст
null_length: длина null прерывателя, используемого в строке
padding: длина добавочных данных перед строкой (C struct { int64_t size; char string[500]; } needs padding = 8)
Отрицательное для data_offset/split_offset ссылается на регистры
-4 для EAX, -8 для ECX, -C для EDX, -10 для EBX, -14 для ESP, -18 для EBP, -1C для ESI, -20 для EDI
-C для RAX, -14 для RBX, -1C для RCX, -24 для RDX, и так далее для RSP, RBP, RSI, RDI, R8-R15
* значит указатель разницы+deref_offset)";
SAVE_SETTINGS = u8"Сохранить настройки";
EXTEN_WINDOW_INSTRUCTIONS = u8R"(Перетащите сюда (.dll) файлы расширений из проводника для их добавления
(Не работает при запуске от администратора)
Перетаскивайте по списку для изменения порядка
Нажмите клавишу удаления, чтобы удалить выбранное расширение)";
ADD_EXTENSION = u8"Добавить расширение";
INVALID_EXTENSION = u8"%1 - неверное расширение";
CONFIRM_EXTENSION_OVERWRITE = u8"Уже существует другая версия этого расширения, перезаписать его?";
EXTENSION_WRITE_ERROR = u8"Не удалось сохранить расширение";
USE_JP_LOCALE = u8"Симулировать японскую локаль?";
FAILED_TO_CREATE_CONFIG_FILE = u8"Не удалось создать файл настроек \"%1\"";
HOOK_SEARCH_UNSTABLE_WARNING = u8"Поиск хуков нестабилен! Игра может закрыться с ошибкой!";
SEARCH_CJK = u8"Поиск для Китайского/Японского/Корейского";
SEARCH_PATTERN = u8"Шаблон поиска (hex byte array)";
SEARCH_DURATION = u8"Продолжительность поиска (ms)";
SEARCH_MODULE = u8"Поиск по модулю";
PATTERN_OFFSET = u8"Смещение от начала шаблона";
MAX_HOOK_SEARCH_RECORDS = u8"Порог поиска";
MIN_ADDRESS = u8"Начальный адрес (hex)";
MAX_ADDRESS = u8"Конечный адрес (hex)";
STRING_OFFSET = u8"Смещение строки (hex)";
HOOK_SEARCH_FILTER = u8"Результат должен совпадать с этим regex";
TEXT = u8"Текст";
CODEPAGE = u8"Кодировка";
SEARCH_FOR_TEXT = u8"Поиск определенного текста";
START_HOOK_SEARCH = u8"Начать поиск хуков";
SAVE_SEARCH_RESULTS = u8"Сохранить результат поиска";
TEXT_FILES = u8"Текст (*.txt)";
DOUBLE_CLICK_TO_REMOVE_HOOK = u8"Двойной клик по хуку для его удаления";
FILTER_REPETITION = u8"Фильтр повторений";
AUTO_ATTACH = u8"Авто-присоединение";
ATTACH_SAVED_ONLY = u8"Авто-присоединение (только сохраненные)";
SHOW_SYSTEM_PROCESSES = u8"Показать системные процесы";
DEFAULT_CODEPAGE = u8"Кодировка по умолчанию";
FLUSH_DELAY = u8"Задержка сброса";
MAX_BUFFER_SIZE = u8"Максимальный размер буфера";
MAX_HISTORY_SIZE = u8"Макс. размер истории";
CONFIG_JP_LOCALE = u8"Запуск с JP локалью";
CONSOLE = L"Консоль";
CLIPBOARD = L"Буфер обмена";
ABOUT = L"Textractor " ARCH L" в." VERSION LR"( автор: Artikash (email: akashmozumdar@gmail.com)
Домашняя страница: https://github.com/Artikash/Textractor
Обучающее видео: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
Сообщайте о любых проблемах, желаемых для добавления функциях, или задавайте вопросы, касающиеся Textractor
Сделать это вы можете на домашней странице (секция issues) или через электронную почту
Исходный код доступен по лицензии GPLv3 на домашней странице проекта
Если эта программа вам понравилась, расскажите всем о ней :))";
CL_OPTIONS = LR"(использование: Textractor [-p{process ID|"process name"}]...
пример: Textractor -p4466 -p"My Game.exe" попробует присоединиться к процессу с ID 4466 или с именем My Game.exe)";
UPDATE_AVAILABLE = L"Доступна новая версия Textractor!\n\nПоследняя: %s\nВаша: %s\n\nНажмите «ОК», чтобы открыть страницу загрузки.";
ALREADY_INJECTED = L"Textractor: уже присоединен";
NEED_32_BIT = L"Textractor: несоответствие архитектуры: попробуйте Textractor x86 вместо этого";
NEED_64_BIT = L"Textractor: несоответствие архитектуры: только Textractor x64 может присоединиться к этому процессу";
INJECT_FAILED = L"Textractor: невозможно присоединиться";
LAUNCH_FAILED = L"Textractor: невозможно запустить";
INVALID_CODE = L"Textractor: неверный код";
INVALID_CODEPAGE = L"Textractor: невозможно конвертировать текст (неверная кодировка?)";
PIPE_CONNECTED = u8"Textractor: канал присоединен";
INSERTING_HOOK = u8"Textractor: вставка хука: %s";
REMOVING_HOOK = u8"Textractor: удаление хука: %s";
HOOK_FAILED = u8"Textractor: не удалось вставить хук";
TOO_MANY_HOOKS = u8"Textractor: слишком много хуков: невозможно вставить";
HOOK_SEARCH_STARTING = u8"Textractor: начало поиска";
NOT_ENOUGH_TEXT = u8"Textractor: не достаточно текста для точного поиска";
HOOK_SEARCH_INITIALIZED = u8"Textractor: поиск инициализирован с %zd хуками";
MAKE_GAME_PROCESS_TEXT = u8"Textractor: покликайте в игре, чтобы вызвать смену текста в течение %d секунд";
HOOK_SEARCH_FINISHED = u8"Textractor: поиск хуков завершен, %d результатов найдено";
OUT_OF_RECORDS_RETRY = u8"Textractor: записи для поиска закончились, повторите,если результаты неудовлетворительны (количество записей по умолчанию увеличено)";
FUNC_MISSING = u8"Textractor: функция отсутствует";
MODULE_MISSING = u8"Textractor: модуль отсутствует";
GARBAGE_MEMORY = u8"Textractor: память постоянно изменяется, бесполезно читать";
SEND_ERROR = u8"Textractor: Send ERROR (вероятно неверный H-code)";
READ_ERROR = u8"Textractor: Reader ERROR (вероятно неверный R-code)";
COULD_NOT_FIND = u8"Textractor: невозможно найти текст";
TRANSLATE_TO = u8"Переводить на";
TRANSLATE_SELECTED_THREAD_ONLY = u8"Переводить текст только в выбранном потоке";
RATE_LIMIT_ALL_THREADS = u8"Ограничение скорости для всех текстовых потоков";
RATE_LIMIT_SELECTED_THREAD = u8"Ограничение скорости выделенного текстового потока";
USE_TRANS_CACHE = u8"Использовать кеш перевода";
MAX_TRANSLATIONS_IN_TIMESPAN = u8"Ограничение частоты подсчёта токенов";
TIMESPAN = u8"Ограничение частоты задержки восстановления токенов (мс)";
TOO_MANY_TRANS_REQUESTS = L"Слишком много запросов для перевода: отклонено";
TRANSLATION_ERROR = L"Ошибка при переводе";
USE_PREV_SENTENCE_CONTEXT = u8"Использовать предыдущее предложение как контекст";
API_KEY = u8"Ключ API";
EXTRA_WINDOW_INFO = u8R"(Правый клик для изменения настроек
Нажмите и перетащите за края - для перемещения, или за правый-нижний угол - для изменения размера)";
MAX_SENTENCE_SIZE = u8"Максимальная длина предложения";
TOPMOST = u8"Поверх всех окон";
DICTIONARY = u8"Словарь";
DICTIONARY_INSTRUCTIONS = u8R"(Этот файл использован только для функции "Словарь" расширения Extra Window.
Он использует свой формат, специально для Textractor и не предназначен для рачного ввода.
Смотрите онлайн словарь в этом формате(https://github.com/Artikash/Textractor-Dictionaries/releases - начните здесь).
В качестве альтернативы, если вы программист, вы можете написать скрипт для конвертирования словаря из другого формата с информацией, приведенной ниже.
При наличии словаря, для просмотра текста в Экстра окне просто наведите курсор на него. Можно прокрутить все совпадающие определения.
Определения имеют следующий формат:|TERM|Hola<<ignored|TERM|hola|TERM|Bonjour|TERM|bonjour|DEFINITION|hello|END|
Термин и определение могут включать форматированный текст(https://doc.qt.io/qt-5/richtext-html-subset.html), который будет соответствующе отформатирован.
Перефразировки имеют следующий формат:|ROOT|1<<noun|INFLECTS TO|(\w*)s|NAME| plural|END|
Textractor проверит, совпадает ли термин с регексом перефразировки и при совпадении будет рекурсивно искать корневой термин..
Корневой термин генерируется путем замены каждого числа на соответствующую regex группу захвата(при этом 0 заменяется на полное совпадение).
Этот процесс может легко привести к бесконечным циклам и/или переполнению стека. Ваша задача - избежать этого.
regex перефразировок использует QRegularExpression (https://doc.qt.io/qt-5/qregularexpression.html) unicode синтаксис.
Textractor отобразит конечный корневой термин, а также все перефразировки, используемые для получения этого корневого термина.
Однако,текст в термине после << игнорируется при отображении. Это сделано для хранения информации о части речи.
У этого файла кодировка должна быть UTF-8.)";
SHOW_ORIGINAL = u8"Исходный текст";
SHOW_ORIGINAL_INFO = u8R"(Исходный текст будет скрыт
Работает только если это расширение используется после расширения перевода)";
SIZE_LOCK = u8"Фиксированный размер";
OPACITY = u8"Прозрачность";
BG_COLOR = u8"Цвет заднего фона";
TEXT_COLOR = u8"Цвет текста";
TEXT_OUTLINE = u8"Обводка текста";
OUTLINE_COLOR = u8"Цвет обводки";
OUTLINE_SIZE = u8"Толщина обводки";
OUTLINE_SIZE_INFO = u8"Размер в пикселях (рекомендуется оставить как 20% от размера шрифта)";
FONT = u8"Шрифт";
LUA_INTRO = u8R"(--[[
ProcessSentence вызывается каждый раз, когда Textractor получает предложение с текстом.

Param sentence: предложение полученое в Textractor (UTF-8).
Param sentenceInfo: таблица различной информации о предложении.

При возвращении строки предложение будет изменено на эту строку.
При возвращении нуля, предложение останется без изменения.

Это расширение использует несколько копий интерпретатора Lua для безопасности нити.
Модификации глобальных переменных из ProcessSentence не обязательно сохраняется.

Параметры в sentenceInfo:
"current select": равно 0, если предложение не находится в текстовой нити, выбранной в данный момент пользователем.
"process id": ID процесса, из которого предложение поступило. Равно 0, когда это консоль или буфер обмена.
"text number": номер текущей текстовой нити. Растет один за другим по мере создания текстовых нитей. 0 для консоли, 1 для буфера обмена.
--]]
function ProcessSentence(sentence, sentenceInfo)
  --Ваш код здесь...
end)";
LOAD_SCRIPT = u8"Загрузить скрипт";
LUA_ERROR = L"Ошибка Lua: %s";
REGEX_FILTER = u8"Фильтр Regex";
INVALID_REGEX = u8"Неверный regex";
CURRENT_FILTER = u8"Сейчас фильтруется: %1";
REPLACER_INSTRUCTIONS = LR"(Этот файл делает что-то только когда используется расширение "Replacer".
Команды для замены должны выглядеть так:
|ORIG|текст_оригинала|BECOMES|текст_замены|END|
Весь текст в этом файле вне команд заменителей будет проигнорирован.
Пробелы в текст_оригинала игнорируются, но текст_замены может содержать пробелы, новые строки и пр.
Этот файл должен быть в кодировке Unicode (UTF-16 little endian).)";
THREAD_LINKER = u8"Связыватель нитей";
LINK = u8"Связь";
THREAD_LINK_FROM = u8"Номер нити, от которой связывать";
THREAD_LINK_TO = u8"Номер нити, к которой привязывать";
