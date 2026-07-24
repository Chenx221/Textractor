#ifdef _WIN64
#define ARCH "x64"
#else
#define ARCH "x86"
#endif

enum class Language
{
	ENGLISH = 0,
	SPANISH = 1,
	SIMPLIFIED_CHINESE = 2,
	RUSSIAN = 3,
	INDONESIAN = 4,
	ITALIAN = 5,
	PORTUGUESE = 6,
	THAI = 7,
	KOREAN = 8,
	FRENCH = 9,
	TURKISH = 10,
	TRADITIONAL_CHINESE = 11,
	JAPANESE = 12,
	GERMAN = 13,
	VIETNAMESE = 14
};

Language CURRENT_LANGUAGE = Language::ENGLISH;

// Language display names for UI
const char* LANGUAGE_NAMES[] = {
	"English",
	"Español",
	"简体中文",
	"Русский",
	"Bahasa Indonesia",
	"Italiano",
	"Português",
	"ไทย",
	"한국어",
	"Français",
	"Türkçe",
	"繁體中文",
	"日本語",
	"Deutsch",
	"Tiếng Việt"
};

extern const int LANGUAGE_COUNT = sizeof(LANGUAGE_NAMES) / sizeof(LANGUAGE_NAMES[0]);

// If you are updating a previous translation see https://github.com/Artikash/Textractor/issues/313

const char* NATIVE_LANGUAGE = "English";
const char* ATTACH = u8"Attach to game";
const char* LAUNCH = u8"Launch game";
const char* CONFIG = u8"Configure game";
const char* DETACH = u8"Detach from game";
const char* FORGET = u8"Forget game";
const char* RESTART_AS_ADMIN = u8"Restart as Admin";
const char* ADD_HOOK = u8"Add hook";
const char* REMOVE_HOOKS = u8"Remove hook(s)";
const char* SAVE_HOOKS = u8"Save hook(s)";
const char* SEARCH_FOR_HOOKS = u8"Search for hooks";
const char* SETTINGS = u8"Settings";
const char* EXTENSIONS = u8"Extensions";
const char* SELECT_PROCESS = u8"Select process";
const char* ATTACH_INFO = u8R"(If you don't see the process you want to attach, try running with admin rights
You can also type in the process ID)";
const char* REFRESH = u8"Refresh";
const char* HIDE_INCOMPATIBLE_ARCHITECTURE = u8"Hide incompatible architecture";
const char* HIDE_NO_TITLE_PROCESS = u8"Hide processes without title";
const char* SELECT_PROCESS_INFO = u8"If you manually type in the process file name, use the absolute path";
const char* FROM_COMPUTER = u8"Select from computer";
const char* PROCESSES = u8"Processes (*.exe)";
const char* CODE_INFODUMP = u8R"(Enter read code
R{S|Q|V|M}[null_length<][codepage#]@addr
OR
Enter hook code
H{A|B|W|H|S|Q|V|M}[F][null_length<][N][codepage#][padding+]data_offset[*deref_offset][:split_offset[*deref_offset]]@addr[:module[:func]]
All numbers except codepage/null_length in hexadecimal
Default codepage is 932 (Shift-JIS) but this can be changed in settings
A/B: codepage char little/big endian
W: UTF-16 char
H: Two hex bytes
S/Q/V/M: codepage/UTF-16/UTF-8/hex string
F: treat strings as full lines of text
N: don't use context
null_length: length of null terminator used for string
padding: length of padding data before string (C struct { int64_t size; char string[500]; } needs padding = 8)
Negatives for data_offset/split_offset refer to registers
-4 for EAX, -8 for ECX, -C for EDX, -10 for EBX, -14 for ESP, -18 for EBP, -1C for ESI, -20 for EDI
-C for RAX, -14 for RBX, -1C for RCX, -24 for RDX, and so on for RSP, RBP, RSI, RDI, R8-R15
* means dereference pointer+deref_offset)";
const char* SAVE_SETTINGS = u8"Save settings";
const char* EXTEN_WINDOW_INSTRUCTIONS = u8R"(Right click the list to add or remove extensions
Drag and drop extensions within the list to reorder them
(Extensions are used from top to bottom: order DOES matter))";
const char* ADD_EXTENSION = u8"Add extension";
const char* REMOVE_EXTENSION = u8"Remove extension";
const char* INVALID_EXTENSION = u8"%1 is an invalid extension";
const char* CONFIRM_EXTENSION_OVERWRITE = u8"Another version of this extension already exists, do you want to delete and overwrite it?";
const char* EXTENSION_WRITE_ERROR = u8"Failed to save extension";
const char* USE_JP_LOCALE = u8"Emulate japanese locale?";
const char* FAILED_TO_CREATE_CONFIG_FILE = u8"Failed to create config file \"%1\"";
const char* HOOK_SEARCH_UNSTABLE_WARNING = u8"Searching for hooks is unstable! Be prepared for your game to crash!";
const char* HOOK_SEARCH_STARTING_VIEW_CONSOLE = u8"Initializing hook search - please check console for further instructions";
const char* SEARCH_CJK = u8"Search for Chinese/Japanese/Korean";
const char* SEARCH_PATTERN = u8"Search pattern (hex byte array)";
const char* SEARCH_DURATION = u8"Search duration (ms)";
const char* SEARCH_MODULE = u8"Search within module";
const char* PATTERN_OFFSET = u8"Offset from pattern start";
const char* MAX_HOOK_SEARCH_RECORDS = u8"Search result cap";
const char* MIN_ADDRESS = u8"Minimum address (hex)";
const char* MAX_ADDRESS = u8"Maximum address (hex)";
const char* STRING_OFFSET = u8"String offset (hex)";
const char* HOOK_SEARCH_FILTER = u8"Results must match this regex";
const char* TEXT = u8"Text";
const char* CODEPAGE = u8"Codepage";
const char* SEARCH_FOR_TEXT = u8"Search for specific text";
const char* START_HOOK_SEARCH = u8"Start hook search";
const char* SAVE_SEARCH_RESULTS = u8"Save search results";
const char* TEXT_FILES = u8"Text (*.txt)";
const char* DOUBLE_CLICK_TO_REMOVE_HOOK = u8"Double click a hook to remove it";
const char* FILTER_REPETITION = u8"Filter repetition";
const char* AUTO_ATTACH = u8"Auto attach";
const char* ATTACH_SAVED_ONLY = u8"Auto attach (saved only)";
const char* SHOW_SYSTEM_PROCESSES = u8"Show system processes";
const char* DEFAULT_CODEPAGE = u8"Default codepage";
const char* FLUSH_DELAY = u8"Flush delay";
const char* FLUSH_DELAY_SPACING = u8"Flush delay string spacing";
const char* FLUSH_DELAY_SPACING_OFF = u8"Off";
const char* FLUSH_DELAY_SPACING_SPACE = u8"Space";
const char* FLUSH_DELAY_SPACING_NEWLINE = u8"Newline";
const char* MAX_BUFFER_SIZE = u8"Max buffer size";
const char* MAX_HISTORY_SIZE = u8"Max history size";
const char* LIMIT_STRING_LENGTH = u8"Limit string length (0=Off)";
const char* CONFIG_JP_LOCALE = u8"Launch with JP locale";
const wchar_t* CONSOLE = L"Console";
const wchar_t* CLIPBOARD = L"Clipboard";
const wchar_t* ABOUT = L"Textractor " ARCH L" v" VERSION LR"( made by Artikash (email: akashmozumdar@gmail.com)
Project homepage: https://github.com/Artikash/Textractor
Tutorial video: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
FAQ: https://github.com/Artikash/Textractor/wiki/FAQ
Please contact Artikash with any problems, feature requests, or questions relating to Textractor
You can do so via the project homepage (issues section) or via email
Source code available under GPLv3 at project homepage
If you like this project, please tell everyone about it! It's time to put AGTH down :))";
const wchar_t* CL_OPTIONS = LR"(usage: Textractor [-p{process ID|"process name"}]...
example: Textractor -p4466 -p"My Game.exe" tries to inject processes with ID 4466 or with name My Game.exe)";
const wchar_t* UPDATE_AVAILABLE = L"Update available: download it from https://github.com/Artikash/Textractor/releases";
const wchar_t* ALREADY_INJECTED = L"Textractor: already injected";
const wchar_t* NEED_32_BIT = L"Textractor: architecture mismatch: only Textractor x86 can inject this process";
const wchar_t* NEED_64_BIT = L"Textractor: architecture mismatch: only Textractor x64 can inject this process";
const wchar_t* INJECT_FAILED = L"Textractor: couldn't inject";
const wchar_t* LAUNCH_FAILED = L"Textractor: couldn't launch";
const wchar_t* INVALID_CODE = L"Textractor: invalid code";
const wchar_t* INVALID_CODEPAGE = L"Textractor: couldn't convert text (invalid codepage?)";
const char* PIPE_CONNECTED = u8"Textractor: pipe connected";
const char* INSERTING_HOOK = u8"Textractor: inserting hook: %s";
const char* REMOVING_HOOK = u8"Textractor: removing hook: %s";
const char* HOOK_FAILED = u8"Textractor: failed to insert hook";
const char* TOO_MANY_HOOKS = u8"Textractor: too many hooks: can't insert";
const char* HOOK_SEARCH_STARTING = u8"Textractor: starting hook search";
const char* HOOK_SEARCH_INITIALIZING = u8"Textractor: initializing hook search (%f%%)";
const char* NOT_ENOUGH_TEXT = u8"Textractor: not enough text to search accurately";
const char* HOOK_SEARCH_INITIALIZED = u8"Textractor: initialized hook search with %zd hooks";
const char* MAKE_GAME_PROCESS_TEXT = u8"Textractor: please click around in the game to force it to process text during the next %d seconds";
const char* HOOK_SEARCH_FINISHED = u8"Textractor: hook search finished, %d results found";
const char* OUT_OF_RECORDS_RETRY = u8"Textractor: out of search records, please retry if results are poor (default record count increased)";
const char* FUNC_MISSING = u8"Textractor: function not present";
const char* MODULE_MISSING = u8"Textractor: module not present";
const char* GARBAGE_MEMORY = u8"Textractor: memory constantly changing, useless to read";
const char* SEND_ERROR = u8"Textractor: Send ERROR (likely an unstable/incorrect H-code)";
const char* READ_ERROR = u8"Textractor: Reader ERROR (likely an incorrect R-code)";
const char* HIJACK_ERROR = u8"Textractor: Hijack ERROR";
const char* COULD_NOT_FIND = u8"Textractor: could not find text";
const char* TRANSLATE_TO = u8"Translate to";
const char* TRANSLATE_FROM = u8"Translate from";
const char* CONFIGURE_TRANSLATION_SETTINGS = u8"Please configure the source/target language and API key in the external translation program.";
const char* PORT = u8"Port";
const char* TRANSLATE_VERSION = u8"Version";
const char* TRANSLATE_EDITON = u8"Translation Edition";
const char* FILTER_GARBAGE = u8"Filter garbage characters";
const char* TRANSLATE_SELECTED_THREAD_ONLY = u8"Translate selected text thread only";
const char* RATE_LIMIT_ALL_THREADS = u8"Use rate limiter";
const char* RATE_LIMIT_SELECTED_THREAD = u8"Rate limit selected text thread";
const char* USE_TRANS_CACHE = u8"Use translation cache";
const char* MAX_TRANSLATIONS_IN_TIMESPAN = u8"Max translation requests in timespan";
const char* TIMESPAN = u8"Timespan (ms)";
const wchar_t* SENTENCE_TOO_LARGE_TO_TRANS = L"Sentence too large to translate";
const wchar_t* TOO_MANY_TRANS_REQUESTS = L"Rate limit exceeded: refuse to make more translation requests";
const wchar_t* TRANSLATION_ERROR = L"Error while translating";
const char* USE_PREV_SENTENCE_CONTEXT = u8"Use previous sentence as context";
const char* API_KEY = u8"API key";
const char* CHROME_LOCATION = u8"Google Chrome file location";
const char* START_DEVTOOLS = u8"Start DevTools";
const char* STOP_DEVTOOLS = u8"Stop DevTools";
const char* HIDE_CHROME = u8"Hide Chrome window";
const char* DEVTOOLS_STATUS = u8"DevTools status";
const char* AUTO_START = u8"Start automatically";
const wchar_t* ERROR_START_CHROME = L"failed to start Chrome or to connect to it";
const char* EXTRA_WINDOW_INFO = u8R"(Right click to change settings
Click and drag on window edges to move, or the bottom right corner to resize)";
const char* MAX_SENTENCE_SIZE = u8"Max sentence size";
const char* TOPMOST = u8"Always on top";
const char* DICTIONARY = u8"Dictionary";
const char* DICTIONARY_INSTRUCTIONS = u8R"(This file is used only for the "Dictionary" feature of the Extra Window extension.
It uses a custom format specific to Textractor and is not meant to be written manually.
You should look for a dictionary in this format online (https://github.com/Artikash/Textractor-Dictionaries/releases is a good place to start).
Alternatively, if you're a programmer, you can write a script to convert a dictionary from another format with the info below.
Once you have a dictionary, to look up some text in Extra Window, hover over it. You can scroll through all the matching definitions.
Definitions are formatted like this:|TERM|Hola<<ignored|TERM|hola|TERM|Bonjour|TERM|bonjour|DEFINITION|hello|END|
The term and definition can include rich text (https://doc.qt.io/qt-5/richtext-html-subset.html) which will be formatted properly.
Inflections are formatted like this:|ROOT|1<<noun|INFLECTS TO|(\w*)s|NAME| plural|END|
Textractor will check if a term matches the inflection regex and if it does, will recursively search for the root term.
The root term is generated by replacing each number with the corresponding regex capture group (with 0 being replaced by the entire match).
This process can easily result in infinite loops and/or stack overflows. It's your job to avoid that.
Inflection regex uses QRegularExpression (https://doc.qt.io/qt-5/qregularexpression.html) unicode syntax.
Textractor will display the final root term as well as all inflections used to get to that root term.
However, the text in a term after << is ignored when displaying. This is intended to store part-of-speech information.
This file must be encoded in UTF-8.)";
const char* SHOW_ORIGINAL = u8"Original text";
const char* SHOW_ORIGINAL_INFO = u8R"(Original text will not be shown
Only works if this extension is used directly after a translation extension)";
const char* ORIGINAL_AFTER_TRANSLATION = u8"Original text after translation";
const char* SIZE_LOCK = u8"Size lock";
const char* POSITION_LOCK = u8"Position lock";
const char* CENTERED_TEXT = u8"Centered text";
const char* AUTO_RESIZE_WINDOW_HEIGHT = u8"Auto resize window height";
const char* CLICK_THROUGH = u8"Click through\tAlt+X";
const char* HIDE_MOUSEOVER = u8"Hide while mouse on top";
const char* HIDE_TEXT = u8"Hide/Show text\tAlt+T";
const char* OPACITY = u8"Opacity";
const char* BG_COLOR = u8"Background color";
const char* TEXT_COLOR = u8"Text color";
const char* TEXT_OUTLINE = u8"Text outline";
const char* OUTLINE_COLOR = u8"Outline color";
const char* OUTLINE_SIZE = u8"Outline size";
const char* OUTLINE_SIZE_INFO = u8"Size in pixels (recommended to stay below 20% of the font size)";
const char* FONT = u8"Font";
const char* CLEAR_CURRENT_THREAD = u8"Clear";
const char* SEARCH_GOOGLE = u8"Search on Google";
const char* TIMER_HIDE_TEXT = u8"Timer hide text";
const char* TEXT_TIMEOUT = u8"Timeout (msec, 0=disabled)";
const char* TEXT_TIMEOUT_ADD_PER_CHAR = u8"Additional timeout per char (msec)";
const char* LUA_INTRO = u8R"(--[[
ProcessSentence is called each time Textractor receives a sentence of text.

Param sentence: sentence received by Textractor (UTF-8).
Param sentenceInfo: table of miscellaneous info about the sentence.

If you return a string, the sentence will be turned into that string.
If you return nil, the sentence will be unmodified.

This extension uses several copies of the Lua interpreter for thread safety.
Modifications to global variables from ProcessSentence are not guaranteed to persist.

Properties in sentenceInfo:
"current select": 0 unless sentence is in the text thread currently selected by the user.
"process id": process ID that the sentence is coming from. 0 for console and clipboard.
"text number": number of the current text thread. Counts up one by one as text threads are created. 0 for console, 1 for clipboard.
--]]
function ProcessSentence(sentence, sentenceInfo)
  --Your code here...
end)";
const char* LOAD_SCRIPT = u8"Load script";
const wchar_t* LUA_ERROR = L"Lua error: %s";
const char* REGEX_FILTER = u8"Regex Filter";
const char* INVALID_REGEX = u8"Invalid regex";
const char* CURRENT_FILTER = u8"Currently filtering: %1";
const wchar_t* REPLACER_INSTRUCTIONS = LR"(This file only does anything when the "Replacer" extension is used.
Replacement commands must be formatted like this:
|ORIG|original_text|BECOMES|replacement_text|END|
All text in this file outside of a replacement command is ignored.
A caret (^) acts as a wildcard that matches any other single character.
Whitespace in original_text is ignored, but replacement_text can contain spaces, newlines, etc.
This file must be encoded in Unicode (UTF-16 Little Endian).)";
const wchar_t* REGEX_REPLACER_INSTRUCTIONS = LR"(This file only does anything when the "Regex Replacer" extension is used.
Replacement commands must be formatted like this:
|REGEX|regular_expression|BECOMES|replacement_text|MODIFIER|modifiers|END|
replacement_text can reference capture groups with a $ followed by their number (e.g. $1 references first capture group).
modifiers can contain the following:
"g" the replacement is global.
"i" the replacement ignores the case.
If empty the replacement is only for the first match and case sensitive.
All text in this file outside of a replacement command is ignored.
This file must be encoded in Unicode (UTF-16 Little Endian).
Learn, build, & test Regular Expressions: https://regexr.com/)";
const char* THREAD_LINKER = u8"Thread Linker";
const char* LINK = u8"Link";
const char* UNLINK = u8"Unlink";
const char* THREAD_LINK_FROM = u8"Thread number to link from";
const char* THREAD_LINK_TO = u8"Thread number to link to";
const char* HEXADECIMAL = u8"Hexadecimal";
const char* LANGUAGE_SETTING = u8"Language (requires restart)";
const char* LANGUAGE_RESTART_NOTICE = u8"Language change will take effect after restarting Textractor";
const char* CHECK_UPDATE = u8"Check for texthook updates on startup";
const char* ORIGINAL_FONT = u8"OriginalFont";
const char* TRANSLATED_FONT = u8"TranslatedFont";
const wchar_t* WINHTTP_TIMEOUT = L"request timed out";
const wchar_t* WINHTTP_NAME_NOT_RESOLVED = L"DNS resolve failed";
const wchar_t* WINHTTP_CANNOT_CONNECT = L"cannot connect to server";
const wchar_t* WINHTTP_CONNECTION_ERROR = L"connection dropped";
const wchar_t* WINHTTP_SECURE_FAILURE = L"TLS/SSL handshake failed";
const wchar_t* WINHTTP_INVALID_SERVER_RESPONSE = L"invalid server response";
const wchar_t* WINHTTP_OPERATION_CANCELLED = L"request cancelled";
const wchar_t* UNKNOWN_ERROR = L"unknown error";
const wchar_t* CHECKING_UPDATE = L"Checking for updates...";
const wchar_t* OPEN_TEXTHOOK_FAILED = L"Failed to open texthook.dll.";
const wchar_t* CHECK_UPDATE_FAILED = L"Update check failed: %s";
const wchar_t* CHECK_UPDATE_FAILED_CODE = L"Update check failed: %s (code=%u).";
const wchar_t* CHECK_UPDATE_FAILED_HTTP = L"Update check failed: HTTP status %u.";
const wchar_t* CHECK_UPDATE_FAILED_EMPTY_RESPONSE = L"Update check failed: empty server response.";
const wchar_t* CHECK_UPDATE_FAILED_MALFORMED_RESPONSE = L"Update check failed: malformed JSON response.";
const wchar_t* CHECK_UPDATE_FAILED_UNKNOWN = L"Update check failed: server returned an unknown error.";
const wchar_t* CHECK_UPDATE_FAILED_UNEXPECTED_RESPONSE = L"Unexpected response from update server.\nTry visiting the update server in an external browser to see what's happening: https://api.iloli.one";
const wchar_t* TEXTHOOK_UPDATE_AVAILABLE = L"Texthook update found!\nCurrent: %s, New: %s\nDownload: %s";
const wchar_t* TEXTHOOK_IS_LATEST = L"Texthook is up to date.";
const wchar_t* TENCENT_AUTH_PROMPT = L"Please fill in the SecretId|SecretKey for Tencent Cloud Translation in the API key field";
const wchar_t* LINGOCLOUD_AUTH_PROMPT = L"Please fill in the API token for Lingocloud Translation in the API key field";
const wchar_t* TRANSLATION_NO_TRANSLATION_FOUND = L"No translation found in response";
const wchar_t* TRANSLATION_JSON_PARSE_ERROR = L"JSON parse error";
const wchar_t* TRANSLATION_KEY_ERROR = L"Key error in translation map";
const wchar_t* TRANSLATION_EXCEPTION_OCCURRED = L"Exception occurred";
const wchar_t* TRANSLATION_UNKNOWN_ERROR = L"Unknown error occurred during translation";
const wchar_t* TRANSLATION_EMPTY_RESPONSE = L"Empty response from translation API";
const wchar_t* TRANSLATION_RESPONSE_PARSE_DATA_EMPTY = L"Empty data line after parsing response";
const wchar_t* TRANSLATION_INVALID_JSON_FORMAT = L"Invalid JSON response format";
const wchar_t* TRANSLATION_UNEXPECTED_STRUCTURE = L"Unexpected response structure";
const wchar_t* TRANSLATION_INVALID_DATA_STRUCTURE = L"Invalid translation data structure";
const wchar_t* TRANSLATION_EMPTY_ARRAY = L"Empty translation array";
const wchar_t* TRANSLATION_INVALID_ENTRY = L"Invalid translation entry";
const wchar_t* TRANSLATION_EMPTY_SEGMENTS = L"Empty translation segments";
const wchar_t* TRANSLATION_NO_TEXT_IN_SEGMENTS = L"No translation text found in segments";
const wchar_t* TRANSLATION_RETRY_LIMIT_REACHED = L"retry limit reached";
const wchar_t* ALIYUN_CSRF_TOKEN_NOT_FOUND = L"CSRF token not found in response";
const wchar_t* ALIYUN_INVALID_API_KEY_FORMAT = L"Invalid API key format. Expected: accessKeyId|accessKeySecret";
const wchar_t* ALIYUN_API_ERROR = L"Aliyun API Error";
const wchar_t* BING_TOKEN_RESPONSE_EMPTY = L"token response is empty";
const wchar_t* BING_COULD_NOT_ACQUIRE_TOKEN = L"could not acquire token";
const wchar_t* BING_TOKEN_NOT_FOUND = L"token not found";
const wchar_t* BAIDU_MD5_SIGNATURE_FAILED = L"MD5 signature generation failed.";
const char* INCLUDE_PREVIOUS_CONTEXT = u8"Include previous sentences as context";
const char* AI_TEMPERATURE = u8"AI Temperature";
const char* AI_SYSTEM_PROMPT = u8"AI system prompt";
const char* AI_API_PATH = u8"AI API path";
const char* AI_API_HOST = u8"AI API host";
const char* AI_MODEL = u8"AI model";
const char* AI_PROVIDER = u8"AI provider";
const char* CUSTOM_TRANSLATE_TO = u8"Custom translate to";
const char* CUSTOM_TRANSLATE_FROM = u8"Custom translate from";
const char* AI_THINKING = u8"AI thinking mode";
const char* AI_REASONING_EFFORT = u8"AI reasoning effort";

void Localize()
{
	switch (CURRENT_LANGUAGE)
	{
		case Language::SPANISH:
			#include "languages/spanish.cpp"
			break;
		case Language::SIMPLIFIED_CHINESE:
			#include "languages/schinese.cpp"
			break;
		case Language::RUSSIAN:
			#include "languages/russian.cpp"
			break;
		case Language::INDONESIAN:
			#include "languages/indonesian.cpp"
			break;
		case Language::ITALIAN:
			#include "languages/italian.cpp"
			break;
		case Language::PORTUGUESE:
			#include "languages/portuguese.cpp"
			break;
		case Language::THAI:
			#include "languages/thai.cpp"
			break;
		case Language::KOREAN:
			#include "languages/korean.cpp"
			break;
		case Language::FRENCH:
			#include "languages/french.cpp"
			break;
		case Language::TURKISH:
			#include "languages/turkish.cpp"
			break;
		case Language::TRADITIONAL_CHINESE:
			#include "languages/tchinese.cpp"
			break;
		case Language::JAPANESE:
			#include "languages/japanese.cpp"
			break;
		case Language::GERMAN:
			#include "languages/german.cpp"
			break;
		case Language::VIETNAMESE:
			#include "languages/vietnamese.cpp"
			break;
		case Language::ENGLISH:
		default:
			break;
	}
}

// Initialize localization on startup using a static initializer class
// This is cleaner than using a dummy variable and provides better code clarity
namespace {
	struct LocalizationInitializer {
		LocalizationInitializer() { Localize(); }
	};
	[[maybe_unused]] LocalizationInitializer localizationInitializer;
}
