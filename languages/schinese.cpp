// Chinese (Simplified) translations

NATIVE_LANGUAGE = "Chinese (Simplified)";
ATTACH = u8"附加到游戏";
LAUNCH = u8"启动游戏";
CONFIG = u8"配置游戏";
DETACH = u8"从游戏分离";
FORGET = u8"移除游戏";
RESTART_AS_ADMIN = u8"以管理员权限重启";
ADD_HOOK = u8"添加钩子";
REMOVE_HOOKS = u8"移除钩子";
SAVE_HOOKS = u8"保存钩子";
SEARCH_FOR_HOOKS = u8"搜索钩子";
SETTINGS = u8"设置";
EXTENSIONS = u8"扩展";
SELECT_PROCESS = u8"选择进程";
ATTACH_INFO = u8R"(如果没看见想要附加的进程，请尝试使用管理员权限运行Textractor
也可以手动输入进程ID)";
REFRESH = u8"刷新";
HIDE_INCOMPATIBLE_ARCHITECTURE = u8"隐藏架构不匹配的进程";
HIDE_NO_TITLE_PROCESS = u8"隐藏无标题的进程";
SELECT_PROCESS_INFO = u8"如果手动输入游戏名，请使用绝对路径";
FROM_COMPUTER = u8"从资源管理器中选择";
PROCESSES = u8"可执行文件 (*.exe)";
CODE_INFODUMP = u8R"(输入 R-code
R{S|Q|V|M}[null_length<][codepage#]@addr
OR
输入 H-code
H{A|B|W|H|S|Q|V|M}[F][null_length<][N][codepage#][padding+]data_offset[*deref_offset][:split_offset[*deref_offset]]@addr[:module[:func]]
除 codepage/null_length 外的所有数字均为十六进制。
默认代码页为 932 (Shift-JIS)，可在设置中修改。
A/B: 小端/大端代码页字符
W: UTF-16 字符
H: 两个十六进制字节
S/Q/V/M: 代码页/UTF-16/UTF-8/十六进制字符串
F: 将字符串视为完整行
N: 不使用上下文
null_length: 字符串使用的空终止符长度
padding: 字符串前的填充数据长度 (例如 C 结构体 { int64_t size; char string[500]; } 需要 padding = 8)
data_offset/split_offset 的负值代表寄存器：
-4:EAX, -8:ECX, -C:EDX, -10:EBX, -14:ESP, -18:EBP, -1C:ESI, -20:EDI
-C:RAX, -14:RBX, -1C:RCX, -24:RDX, RSP, RBP, RSI, RDI, R8-R15 剩下自己推
* 表示对指针+deref_offset进行取值(dereference))";
SAVE_SETTINGS = u8"保存设置";
EXTEN_WINDOW_INSTRUCTIONS = u8R"(在列表中单击右键以添加或移除扩展
在列表中拖拽扩展可以调整插件的执行顺序, 扩展会从上往下依次执行
排在下方的扩展只能接收到由排在上方的扩展处理过后的文本)";
ADD_EXTENSION = u8"添加扩展";
REMOVE_EXTENSION = u8"移除扩展";
INVALID_EXTENSION = u8"%1 是一个无效扩展";
CONFIRM_EXTENSION_OVERWRITE = u8"此扩展的另一个版本已启用，是否删除并覆盖它?";
EXTENSION_WRITE_ERROR = u8"保存扩展失败";
USE_JP_LOCALE = u8"模拟日本区域设置?";
FAILED_TO_CREATE_CONFIG_FILE = u8"无法创建配置文件 \"%1\"";
HOOK_SEARCH_UNSTABLE_WARNING = u8"搜索钩子的功能是不稳定的! 可能会导致你的游戏崩溃! ";
HOOK_SEARCH_STARTING_VIEW_CONSOLE = u8"正在初始化钩子搜索 - 请查看控制台以获取更多提示";
SEARCH_CJK = u8"搜索中文/日文/韩文";
SEARCH_PATTERN = u8"搜索匹配特征 (hex byte array)";
SEARCH_DURATION = u8"搜索持续时间 (ms)";
SEARCH_MODULE = u8"搜索指定模块";
PATTERN_OFFSET = u8"相对于特征地址的偏移值";
MAX_HOOK_SEARCH_RECORDS = u8"搜索结果达到上限";
MIN_ADDRESS = u8"起始地址 (hex)";
MAX_ADDRESS = u8"结束地址 (hex)";
STRING_OFFSET = u8"字符串偏移值 (hex)";
HOOK_SEARCH_FILTER = u8"结果必须匹配的正则表达式";
TEXT = u8"文本";
CODEPAGE = u8"代码页";
SEARCH_FOR_TEXT = u8"搜索指定文本";
START_HOOK_SEARCH = u8"开始搜索钩子";
SAVE_SEARCH_RESULTS = u8"保存搜索结果";
TEXT_FILES = u8"文本文档 (*.txt)";
DOUBLE_CLICK_TO_REMOVE_HOOK = u8"双击移除不需要的钩子";
FILTER_REPETITION = u8"过滤重复文本";
AUTO_ATTACH = u8"自动附加";
ATTACH_SAVED_ONLY = u8"自动附加 (仅限保存过配置的游戏)";
SHOW_SYSTEM_PROCESSES = u8"显示系统进程";
DEFAULT_CODEPAGE = u8"默认代码页";
FLUSH_DELAY = u8"刷新延迟";
FLUSH_DELAY_SPACING = u8"字符串间插入分隔符";
FLUSH_DELAY_SPACING_OFF = u8"关闭";
FLUSH_DELAY_SPACING_SPACE = u8"空格";
FLUSH_DELAY_SPACING_NEWLINE = u8"换行";
MAX_BUFFER_SIZE = u8"最大缓冲区长度";
MAX_HISTORY_SIZE = u8"最大缓存文本长度";
LIMIT_STRING_LENGTH = u8"限制字符串长度 (0=关闭)";
CONFIG_JP_LOCALE = u8"模拟日本区域启动游戏";
CONSOLE = L"控制台";
CLIPBOARD = L"剪贴板";
ABOUT = L"Textractor " ARCH L" v" VERSION LR"( 作者: Artikash (email: akashmozumdar@gmail.com)
项目主页: https://github.com/Artikash/Textractor
教程视频: https://github.com/Artikash/Textractor/blob/master/docs/TUTORIAL.md
常见问题： https://github.com/Artikash/Textractor/wiki/FAQ
如果有任何关于 Textractor 的困难，功能请求或问题，请联系我
可以通过项目主页 (Issues 页面) 或通过邮件来联系
项目主页提供基于 GPLv3 协议的源代码
如果你喜欢这个项目，请分享给其他人！是时候让 AGTH 退休了 :))";
UPDATE_AVAILABLE = L"有可用的更新: 请从 https://github.com/Artikash/Textractor/releases 下载";
ALREADY_INJECTED = L"Textractor: 已经注入";
NEED_32_BIT = L"Textractor: 架构不匹配: 请尝试使用32位版本的Textractor";
NEED_64_BIT = L"Textractor: 架构不匹配: 请尝试使用64位版本的Textractor";
INJECT_FAILED = L"Textractor: 无法注入";
LAUNCH_FAILED = L"Textractor: 无法启动";
INVALID_CODE = L"Textractor: 无效代码";
INVALID_CODEPAGE = L"Textractor: 无法转换文本 (无效的代码页?)";
PIPE_CONNECTED = u8"Textractor: 管道已连接";
INSERTING_HOOK = u8"Textractor: 注入钩子: %s";
REMOVING_HOOK = u8"Textractor: 移除钩子: %s";
HOOK_FAILED = u8"Textractor: 钩子注入失败";
TOO_MANY_HOOKS = u8"Textractor: 钩子数量已达上限: 无法注入";
HOOK_SEARCH_STARTING = u8"Textractor: 开始搜索钩子";
HOOK_SEARCH_INITIALIZING = u8"Textractor: 初始化钩子搜索 (%f%%)";
NOT_ENOUGH_TEXT = u8"Textractor: 文本长度不足, 无法精确搜索";
HOOK_SEARCH_INITIALIZED = u8"Textractor: 搜索初始化完成, 创建了 %zd 个钩子";
MAKE_GAME_PROCESS_TEXT = u8"Textractor: 请点击游戏区域, 在接下来的 %d 秒内使游戏强制处理文本";
HOOK_SEARCH_FINISHED = u8"Textractor: 钩子搜索完毕, 找到了 %d 条结果";
OUT_OF_RECORDS_RETRY = u8"Textractor: 搜索结果已达上限, 如果结果不理想, 请重试(默认最大记录数增加)";
FUNC_MISSING = u8"Textractor: 函数不存在";
MODULE_MISSING = u8"Textractor: 模块不存在";
GARBAGE_MEMORY = u8"Textractor: 内存一直在变，读了也没用";
SEND_ERROR = u8"Textractor: Sender 错误 (H码可能不正确)";
READ_ERROR = u8"Textractor: Reader 错误 (R码可能不正确)";
HIJACK_ERROR = u8"Textractor: Hijack 错误";
COULD_NOT_FIND = u8"Textractor: 无法找到文本";
TRANSLATE_TO = u8"目标语言";
TRANSLATE_FROM = u8"源语言";
CONFIGURE_TRANSLATION_SETTINGS = u8"请在外部翻译程序中配置源/目标语言和API key";
PORT = u8"端口";
TRANSLATE_VERSION = u8"版本";
TRANSLATE_EDITON = u8"翻译版本";
FILTER_GARBAGE = u8"过滤无意义字符";
TRANSLATE_SELECTED_THREAD_ONLY = u8"仅翻译当前选择的文本线程";
RATE_LIMIT_ALL_THREADS = u8"限制请求频率";
RATE_LIMIT_SELECTED_THREAD = u8"限制选中线程的请求频率";
USE_TRANS_CACHE = u8"使用缓存的翻译结果";
MAX_TRANSLATIONS_IN_TIMESPAN = u8"单位时间内最大请求次数";
TIMESPAN = u8"单位时间长度 (ms)";
SENTENCE_TOO_LARGE_TO_TRANS = L"要翻译的段落太长了";
TOO_MANY_TRANS_REQUESTS = L"超出频率限制: 拒绝发出翻译请求";
TRANSLATION_ERROR = L"翻译时出错";
USE_PREV_SENTENCE_CONTEXT = u8"使用之前的句子作为上下文";
API_KEY = u8"API key";
CHROME_LOCATION = u8"Google Chrome 安装位置";
START_DEVTOOLS = u8"启动 DevTools";
STOP_DEVTOOLS = u8"停止 DevTools";
HIDE_CHROME = u8"隐藏 Chrome 窗口";
DEVTOOLS_STATUS = u8"DevTools 状态";
AUTO_START = u8"自动启动";
ERROR_START_CHROME = L"无法启动或连接到 Chrome";
EXTRA_WINDOW_INFO = u8R"(在此点击右键以修改设置
在窗口边缘点击并拖拽来移动, 在窗口右下角点击并拖拽来调整大小)";
MAX_SENTENCE_SIZE = u8"最大文本长度";
TOPMOST = u8"窗口总是置顶";
DICTIONARY = u8"字典";
DICTIONARY_INSTRUCTIONS = u8R"(此文件仅用于 Extra Window 扩展的“字典”功能。
它使用 Textractor 特有的自定义格式，不建议手动编写。
你应该在线寻找此格式的词典（推荐：https://github.com/Artikash/Textractor-Dictionaries/releases）。
或者，如果你是程序员，可以参考以下信息编写脚本转换其他格式的词典。
安装词典后，在 Extra Window 中将鼠标悬停在文本上即可查词，可滚动查看所有匹配项。
定义格式如下：|TERM|Hola<<ignored|TERM|hola|TERM|Bonjour|TERM|bonjour|DEFINITION|hello|END|
术语和定义支持富文本 (HTML子集)，将正确显示格式。
词形变化格式：|ROOT|1<<noun|INFLECTS TO|(\w*)s|NAME| plural|END|
Textractor 会检查术语是否匹配正则，若匹配则递归搜索根词。
根词通过替换正则捕获组生成（0代表完整匹配）。
注意：此过程可能导致死循环或栈溢出，请务必小心。
正则表达式使用 QRegularExpression 的 Unicode 语法。
Textractor 将显示最终的根词（原形），以及在追溯该根词过程中所涉及的所有词形变化。
<< 之后的内容在显示时会被忽略，通常用于存储词性信息。
文件必须使用 UTF-8 编码。)";
SHOW_ORIGINAL = u8"显示原文";
SHOW_ORIGINAL_INFO = u8R"(原文将被隐藏
仅当此扩展位于翻译扩展之后使用时才有效)";
ORIGINAL_AFTER_TRANSLATION = u8"把原文显示在翻译文本的后面";
SIZE_LOCK = u8"锁定窗口大小";
POSITION_LOCK = u8"锁定窗口位置";
CENTERED_TEXT = u8"居中显示文本";
AUTO_RESIZE_WINDOW_HEIGHT = u8"自动改变窗口高度";
CLICK_THROUGH = u8"点击穿透\tAlt+X";
HIDE_MOUSEOVER = u8"鼠标经过时隐藏窗口";
HIDE_TEXT = u8"隐藏/显示文本\tAlt+T";
OPACITY = u8"透明度";
BG_COLOR = u8"背景颜色";
TEXT_COLOR = u8"文本颜色";
TEXT_OUTLINE = u8"文字描边";
OUTLINE_COLOR = u8"文字描边颜色";
OUTLINE_SIZE = u8"文字描边大小";
OUTLINE_SIZE_INFO = u8"描边的单位为px (建议保持在字体大小的20%以下)";
FONT = u8"字体";
CLEAR_CURRENT_THREAD = u8"清空";
SEARCH_GOOGLE = u8"使用 Google 搜索";
TIMER_HIDE_TEXT = u8"定时隐藏文本";
TEXT_TIMEOUT = u8"文本停留时间 (毫秒, 0=禁用)";
TEXT_TIMEOUT_ADD_PER_CHAR = u8"每个字符额外增加的显示时长 (毫秒)";
LUA_INTRO = u8R"(--[[
ProcessSentence 是Textractor每接收到一句文本时都会调用的函数.

参数 sentence:  Textractor接收到的文本 (UTF-8), 数据类型为 string.
参数 sentenceInfo: 用于保存文本相关信息的表, 数据类型为 tabel.

如果你返回一个字符串，文本将被转换为该字符串.
如果返回 nil, 该文本将不会被修改.

此扩展使用了几个Lua解释器的副本用于保证线程安全.
在 ProcessSentence 函数中对全局变量的修改可能不会生效.

sentenceInfo 表有以下关键字索引:
current select: 除非文本属于用户当前选择的文本线程, 否则为0. 数据类型为 number.
process id: 这句文本所属的进程ID. 0 表示控制台与剪贴板线程. 数据类型为 number.
text number: 当前选择的文本线程的ID. 这是在创建文本线程时自动递增的计数器. 0 为控制台, 1 为剪贴板. 数据类型为 number.
--]]
function ProcessSentence(sentence, sentenceInfo)
  --在此处添加你的代码...
end)";
LOAD_SCRIPT = u8"加载脚本";
LUA_ERROR = L"Lua 错误";
REGEX_FILTER = u8"正则表达式过滤器";
INVALID_REGEX = u8"无效的正则表达式";
CURRENT_FILTER = u8"当前过滤中: %1";
REPLACER_INSTRUCTIONS = LR"(使用"Replace"扩展时会使用此文件
替换指令必须遵循以下格式:
|ORIG|原文|BECOMES|替代文本|END|
此文件中替换指令之外的所有文本都会被忽略.
符号 ^ 可以代替任意一个字符.
原文中的空白字符将被忽略，但是替代文本可以包含空白字符、换行符等.
此文件必须使用 Unicode 编码 (UTF-16 little endian).)";
REGEX_REPLACER_INSTRUCTIONS = LR"(使用"Regex Replace"扩展时会使用此文件
替换指令必须遵循以下格式:
|REGEX|正则表达式|BECOMES|替代文本|MODIFIER|修饰符|END|
替代文本可以通过 $[编号] 来引用捕获分组 (例如: $1 引用第1个捕获分组).
修饰符可以包含以下选项:
g 全局替换.
i 忽略大小写差异.
如果替代文本为空, 那么只会替换第1个匹配的到结果, 而且强制启用大小写敏感.
此文件中替换指令之外的所有文本都会被忽略.
此文件必须使用 Unicode 编码 (UTF-16 little endian).
你可以通过下列链接来学习编写和在线测试正则表达式:
  https://www.runoob.com/regexp/regexp-intro.html
  https://regexr.com/)";
THREAD_LINKER = u8"线程链接器";
LINK = u8"链接";
UNLINK = u8"断开";
THREAD_LINK_FROM = u8"需要链接的线程ID";
THREAD_LINK_TO = u8"链接到的线程ID";
HEXADECIMAL = u8"十六进制";
LANGUAGE_SETTING = u8"语言 (需重启)";
LANGUAGE_RESTART_NOTICE = u8"语言更改将在重启 Textractor 后生效";
CHECK_UPDATE = u8"启动时检查 texthook 更新";
ORIGINAL_FONT = u8"原文语言字体";
TRANSLATED_FONT = u8"翻译语言字体";
WINHTTP_TIMEOUT = L"请求超时";
WINHTTP_NAME_NOT_RESOLVED = L"DNS 解析失败";
WINHTTP_CANNOT_CONNECT = L"无法连接到服务器";
WINHTTP_CONNECTION_ERROR = L"连接已断开";
WINHTTP_SECURE_FAILURE = L"TLS/SSL 握手失败";
WINHTTP_INVALID_SERVER_RESPONSE = L"无效的服务器响应";
WINHTTP_OPERATION_CANCELLED = L"操作已取消";
UNKNOWN_ERROR = L"未知错误";
CHECKING_UPDATE = L"正在检查更新...";
OPEN_TEXTHOOK_FAILED = L"无法打开 texthook.dll";
CHECK_UPDATE_FAILED = L"检查更新失败: %s";
CHECK_UPDATE_FAILED_CODE = L"检查更新失败: %s (代码=%u)";
CHECK_UPDATE_FAILED_HTTP = L"检查更新失败: HTTP 状态码 %u";
CHECK_UPDATE_FAILED_EMPTY_RESPONSE = L"检查更新失败: 服务器响应为空";
CHECK_UPDATE_FAILED_MALFORMED_RESPONSE = L"检查更新失败: JSON 格式错误";
CHECK_UPDATE_FAILED_UNKNOWN = L"检查更新失败: 服务器返回了未知错误";
CHECK_UPDATE_FAILED_UNEXPECTED_RESPONSE = L"更新服务器响应异常。\n请尝试在浏览器中访问更新服务器查看详情: https://api.iloli.one";
TEXTHOOK_UPDATE_AVAILABLE = L"发现 Texthook 更新！\n当前版本: %s, 最新版本: %s\n下载地址: %s";
TEXTHOOK_IS_LATEST = L"Texthook 已是最新版本。";
TENCENT_AUTH_PROMPT = L"请在 API key 栏位中填写腾讯云翻译的 SecretId|SecretKey";
LINGOCLOUD_AUTH_PROMPT = L"请在 API key 栏位中填写彩云小译的 API 令牌 (Token)";
TRANSLATION_NO_TRANSLATION_FOUND = L"响应中未找到翻译结果";
TRANSLATION_JSON_PARSE_ERROR = L"JSON 解析错误";
TRANSLATION_KEY_ERROR = L"翻译映射表中的键 (Key) 错误";
TRANSLATION_EXCEPTION_OCCURRED = L"发生异常";
TRANSLATION_UNKNOWN_ERROR = L"翻译过程中发生未知错误";
TRANSLATION_EMPTY_RESPONSE = L"翻译 API 响应为空";
TRANSLATION_RESPONSE_PARSE_DATA_EMPTY = L"解析响应后数据行内容为空";
TRANSLATION_INVALID_JSON_FORMAT = L"无效的 JSON 响应格式";
TRANSLATION_UNEXPECTED_STRUCTURE = L"非预期的响应结构";
TRANSLATION_INVALID_DATA_STRUCTURE = L"无效的翻译数据结构";
TRANSLATION_EMPTY_ARRAY = L"翻译数组为空";
TRANSLATION_INVALID_ENTRY = L"无效的翻译条目";
TRANSLATION_EMPTY_SEGMENTS = L"翻译分段 (Segments) 为空";
TRANSLATION_NO_TEXT_IN_SEGMENTS = L"分段中未找到翻译文本";
TRANSLATION_RETRY_LIMIT_REACHED = L"已达到重试次数上限";
ALIYUN_CSRF_TOKEN_NOT_FOUND = L"响应中未找到 CSRF 令牌";
ALIYUN_INVALID_API_KEY_FORMAT = L"API key 格式无效。应为: accessKeyId|accessKeySecret";
ALIYUN_API_ERROR = L"阿里云 API 错误";
BING_TOKEN_RESPONSE_EMPTY = L"令牌 (Token) 响应为空";
BING_COULD_NOT_ACQUIRE_TOKEN = L"无法获取令牌";
BING_TOKEN_NOT_FOUND = L"未找到令牌";
BAIDU_MD5_SIGNATURE_FAILED = L"MD5 签名生成失败。";
INCLUDE_PREVIOUS_CONTEXT = u8"将之前的段落视为上下文";
AI_TEMPERATURE = u8"AI 温度";
AI_SYSTEM_PROMPT = u8"AI 系统提示词";
AI_API_PATH = u8"AI API 路径";
AI_API_HOST = u8"AI API 主机";
AI_MODEL = u8"AI 模型";
AI_PROVIDER = u8"AI 提供商";
CUSTOM_TRANSLATE_TO = u8"自定义目标语言";
CUSTOM_TRANSLATE_FROM = u8"自定义源语言";
AI_THINKING = u8"思考模式";
AI_REASONING_EFFORT = u8"思考强度";